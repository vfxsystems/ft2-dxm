/* Apple AIFF/AIFC sample loader
**
** Volume and loop sanitation is performed by the common sample-loading path.
** Do not close the file handle here.
*/

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

static bool readExact(FILE *f, void *dst, size_t bytes)
{
	return bytes == 0 || fread(dst, 1, bytes, f) == bytes;
}

static bool seekTo(FILE *f, uint64_t offset, uint32_t filesize)
{
	return offset <= filesize && fseek(f, (long)offset, SEEK_SET) == 0;
}

static uint16_t readBE16(const uint8_t *src)
{
	return (uint16_t)((src[0] << 8) | src[1]);
}

static uint32_t readBE32(const uint8_t *src)
{
	return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
		((uint32_t)src[2] << 8) | src[3];
}

static uint64_t readInteger(const uint8_t *src, uint32_t bytes, bool littleEndian)
{
	uint64_t value = 0;
	if (littleEndian)
	{
		for (uint32_t i = 0; i < bytes; i++)
			value |= (uint64_t)src[i] << (i * 8);
	}
	else
	{
		for (uint32_t i = 0; i < bytes; i++)
			value = (value << 8) | src[i];
	}
	return value;
}

static double decodeExtended80(const uint8_t *src)
{
	const bool negative = (src[0] & 0x80) != 0;
	const uint16_t exponent = (uint16_t)(((src[0] & 0x7F) << 8) | src[1]);
	const uint64_t mantissa = readInteger(src + 2, 8, false);
	if (negative || exponent == 0x7FFF || mantissa == 0)
		return 0.0;

	const int unbiasedExponent = (exponent == 0 ? 1 : exponent) - 16383;
	const double value = ldexp((double)mantissa, unbiasedExponent - 63);
	return isfinite(value) && value > 0.0 ? value : 0.0;
}

static double decodeIntegerSample(const uint8_t *src, uint16_t bitDepth,
	bool littleEndian, bool signedSample)
{
	const uint64_t raw = readInteger(src, bitDepth / 8, littleEndian);
	const uint64_t signBit = UINT64_C(1) << (bitDepth - 1);
	if (!signedSample)
		return (double)raw - (double)signBit;

	if (raw & signBit)
		return (double)((int64_t)raw - (int64_t)(signBit << 1));
	return (double)raw;
}

static double decodeFloatSample(const uint8_t *src, uint16_t bitDepth, bool littleEndian)
{
	if (bitDepth == 32)
	{
		const uint32_t bits = (uint32_t)readInteger(src, 4, littleEndian);
		float value;
		memcpy(&value, &bits, sizeof (value));
		return isfinite(value) ? value : 0.0;
	}

	const uint64_t bits = readInteger(src, 8, littleEndian);
	double value;
	memcpy(&value, &bits, sizeof (value));
	return isfinite(value) ? value : 0.0;
}

static int16_t scaleToSigned16(double sample, double gain)
{
	double scaled = sample * gain;
	if (scaled > INT16_MAX) scaled = INT16_MAX;
	if (scaled < INT16_MIN) scaled = INT16_MIN;
	return (int16_t)scaled;
}

bool loadAIFF(FILE *f, uint32_t filesize)
{
	if (filesize < 12)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	uint8_t formHeader[12];
	if (!seekTo(f, 0, filesize) || !readExact(f, formHeader, sizeof (formHeader)) ||
		memcmp(formHeader, "FORM", 4) != 0 ||
		(memcmp(formHeader + 8, "AIFF", 4) != 0 && memcmp(formHeader + 8, "AIFC", 4) != 0))
		return false;

	const bool isAIFC = memcmp(formHeader + 8, "AIFC", 4) == 0;
	const uint64_t formEnd = (uint64_t)readBE32(formHeader + 4) + 8;
	if (formEnd < 12 || formEnd > filesize)
		return false;

	uint32_t commPtr = 0, commLen = 0, ssndPtr = 0, ssndLen = 0;
	if (!seekTo(f, 12, filesize)) return false;
	while (true)
	{
		const long headerPos = ftell(f);
		if (headerPos < 0) return false;
		if ((uint64_t)headerPos + 8 > formEnd) break;

		uint8_t chunkHeader[8];
		if (!readExact(f, chunkHeader, sizeof (chunkHeader))) return false;
		const uint32_t chunkSize = readBE32(chunkHeader + 4);
		const long payloadPos = ftell(f);
		if (payloadPos < 0) return false;
		const uint64_t chunkEnd = (uint64_t)payloadPos + chunkSize + (chunkSize & 1u);
		if (chunkEnd > formEnd) return false;

		if (memcmp(chunkHeader, "COMM", 4) == 0)
		{
			commPtr = (uint32_t)payloadPos;
			commLen = chunkSize;
		}
		else if (memcmp(chunkHeader, "SSND", 4) == 0)
		{
			ssndPtr = (uint32_t)payloadPos;
			ssndLen = chunkSize;
		}

		if (!seekTo(f, chunkEnd, filesize)) return false;
	}

	if (commPtr == 0 || commLen < (isAIFC ? 22u : 18u) || ssndPtr == 0 || ssndLen < 8)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	uint8_t common[22];
	const size_t commonBytes = isAIFC ? sizeof (common) : 18;
	if (!seekTo(f, commPtr, filesize) || !readExact(f, common, commonBytes))
		return false;

	const uint16_t numChannels = readBE16(common);
	const uint32_t sampleLength = readBE32(common + 2);
	const uint16_t bitDepth = readBE16(common + 6);
	const double sampleRate = decodeExtended80(common + 8);
	if ((numChannels != 1 && numChannels != 2) || sampleLength == 0 ||
		sampleLength > MAX_SAMPLE_LEN || sampleRate <= 0.0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	bool floatSample = false, signedSample = true, littleEndian = false;
	if (isAIFC)
	{
		const uint8_t *compression = common + 18;
		if (memcmp(compression, "NONE", 4) == 0 || memcmp(compression, "twos", 4) == 0)
			signedSample = true;
		else if (memcmp(compression, "sowt", 4) == 0)
		{
			signedSample = true;
			littleEndian = true;
		}
		else if (memcmp(compression, "raw ", 4) == 0)
			signedSample = false;
		else if (memcmp(compression, "FL32", 4) == 0 || memcmp(compression, "fl32", 4) == 0)
		{
			floatSample = true;
			if (bitDepth != 32) return false;
		}
		else if (memcmp(compression, "FL64", 4) == 0 || memcmp(compression, "fl64", 4) == 0)
		{
			floatSample = true;
			if (bitDepth != 64) return false;
		}
		else
		{
			loaderMsgBox("Error loading sample: Unsupported AIFF compression type!");
			return false;
		}
	}

	if ((!floatSample && bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32) ||
		(floatSample && bitDepth != 32 && bitDepth != 64))
	{
		loaderMsgBox("Error loading sample: Unsupported AIFF bit depth!");
		return false;
	}

	uint8_t soundHeader[8];
	if (!seekTo(f, ssndPtr, filesize) || !readExact(f, soundHeader, sizeof (soundHeader)))
		return false;
	const uint32_t dataOffset = readBE32(soundHeader);
	if ((uint64_t)dataOffset + 8 > ssndLen)
		return false;

	const uint32_t bytesPerSample = bitDepth / 8;
	const uint32_t bytesPerFrame = bytesPerSample * numChannels;
	const uint64_t dataBytes = (uint64_t)sampleLength * bytesPerFrame;
	const uint64_t availableBytes = ssndLen - 8u - dataOffset;
	const uint64_t dataPtr = (uint64_t)ssndPtr + 8 + dataOffset;
	if (dataBytes == 0 || dataBytes > availableBytes || dataPtr + dataBytes > filesize)
		return false;

	int16_t stereoMode = -1;
	if (numChannels == 2)
	{
		stereoMode = loaderSysReq(4, "System request", "This is a stereo sample.", NULL);
		if (stereoMode != STEREO_SAMPLE_READ_LEFT && stereoMode != STEREO_SAMPLE_READ_RIGHT &&
			stereoMode != STEREO_SAMPLE_CONVERT)
			return false;
	}
	const bool stereoOutput = numChannels == 2 && stereoMode == STEREO_SAMPLE_CONVERT;
	const uint32_t selectedChannel = stereoMode == STEREO_SAMPLE_READ_RIGHT ? 1 : 0;
	const bool output16Bit = bitDepth != 8 || floatSample;

	uint8_t *sourceData = (uint8_t *)malloc((size_t)dataBytes);
	if (sourceData == NULL)
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}
	if (!seekTo(f, dataPtr, filesize) || !readExact(f, sourceData, (size_t)dataBytes))
	{
		free(sourceData);
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	sample_t *s = &tmpSmp;
	if (!allocateSmpData(s, sampleLength, output16Bit, stereoOutput))
	{
		free(sourceData);
		loaderMsgBox("Not enough memory!");
		return false;
	}

	if (!output16Bit)
	{
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			s->dataPtrL[i] = (int8_t)decodeIntegerSample(frame + selectedChannel, 8, false, signedSample);
			if (stereoOutput)
				s->dataPtrR[i] = (int8_t)decodeIntegerSample(frame + 1, 8, false, signedSample);
		}
	}
	else if (!floatSample && bitDepth == 16)
	{
		int16_t *left = (int16_t *)s->dataPtrL;
		int16_t *right = (int16_t *)s->dataPtrR;
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			left[i] = (int16_t)decodeIntegerSample(frame + (selectedChannel * 2), 16, littleEndian, signedSample);
			if (stereoOutput)
				right[i] = (int16_t)decodeIntegerSample(frame + 2, 16, littleEndian, signedSample);
		}
	}
	else
	{
		double peak = 0.0;
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			const double left = floatSample
				? decodeFloatSample(frame + (selectedChannel * bytesPerSample), bitDepth, littleEndian)
				: decodeIntegerSample(frame + (selectedChannel * bytesPerSample), bitDepth, littleEndian, signedSample);
			if (fabs(left) > peak) peak = fabs(left);
			if (stereoOutput)
			{
				const double right = floatSample
					? decodeFloatSample(frame + bytesPerSample, bitDepth, littleEndian)
					: decodeIntegerSample(frame + bytesPerSample, bitDepth, littleEndian, signedSample);
				if (fabs(right) > peak) peak = fabs(right);
			}
		}

		const double gain = peak > 0.0 ? (double)INT16_MAX / peak : 0.0;
		int16_t *leftOut = (int16_t *)s->dataPtrL;
		int16_t *rightOut = (int16_t *)s->dataPtrR;
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			const double left = floatSample
				? decodeFloatSample(frame + (selectedChannel * bytesPerSample), bitDepth, littleEndian)
				: decodeIntegerSample(frame + (selectedChannel * bytesPerSample), bitDepth, littleEndian, signedSample);
			leftOut[i] = scaleToSigned16(left, gain);
			if (stereoOutput)
			{
				const double right = floatSample
					? decodeFloatSample(frame + bytesPerSample, bitDepth, littleEndian)
					: decodeIntegerSample(frame + bytesPerSample, bitDepth, littleEndian, signedSample);
				rightOut[i] = scaleToSigned16(right, gain);
			}
		}
	}
	free(sourceData);

	s->flags = (output16Bit ? SAMPLE_16BIT : 0) | (stereoOutput ? SAMPLE_STEREO : 0);
	s->length = sampleLength;
	s->volume = 64;
	s->panning = 128;
	setSampleC4Hz(s, sampleRate);
	return true;
}

#ifdef FT2_STABILITY_TESTS
static void ignoreAIFFLoaderMessage(const char *message, ...)
{
	(void)message;
}

static int16_t chooseAIFFStereo(int16_t type, const char *headline, const char *text, void (*callback)(void))
{
	(void)type;
	(void)headline;
	(void)text;
	(void)callback;
	return STEREO_SAMPLE_CONVERT;
}

static bool loadAIFFFixture(const uint8_t *data, size_t size, bool expectedResult)
{
	FILE *f = tmpfile();
	if (f == NULL) return false;
	void (*oldLoaderMsgBox)(const char *, ...) = loaderMsgBox;
	int16_t (*oldLoaderSysReq)(int16_t, const char *, const char *, void (*)(void)) = loaderSysReq;
	loaderMsgBox = ignoreAIFFLoaderMessage;
	loaderSysReq = chooseAIFFStereo;
	const bool result = fwrite(data, 1, size, f) == size &&
		fseek(f, 0, SEEK_SET) == 0 && loadAIFF(f, (uint32_t)size) == expectedResult;
	loaderMsgBox = oldLoaderMsgBox;
	loaderSysReq = oldLoaderSysReq;
	fclose(f);
	return result;
}

bool runAIFFLoaderRegressionTests(void)
{
	const uint8_t monoAIFF[] =
	{
		'F','O','R','M', 0,0,0,48, 'A','I','F','F',
		'C','O','M','M', 0,0,0,18,
		0,1, 0,0,0,2, 0,8, 0x40,0x0B,0xFA,0,0,0,0,0,0,0,
		'S','S','N','D', 0,0,0,10, 0,0,0,0, 0,0,0,0, 0x80,0x7F
	};
	bool ok = loadAIFFFixture(monoAIFF, sizeof (monoAIFF), true) &&
		tmpSmp.length == 2 && tmpSmp.dataPtrL != NULL &&
		tmpSmp.dataPtrL[0] == INT8_MIN && tmpSmp.dataPtrL[1] == INT8_MAX &&
		(tmpSmp.flags & (SAMPLE_16BIT | SAMPLE_STEREO)) == 0;
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	if (!ok) return false;

	const uint8_t stereo24AIFF[] =
	{
		'F','O','R','M', 0,0,0,58, 'A','I','F','F',
		'C','O','M','M', 0,0,0,18,
		0,2, 0,0,0,2, 0,24, 0x40,0x0B,0xFA,0,0,0,0,0,0,0,
		'S','S','N','D', 0,0,0,20, 0,0,0,0, 0,0,0,0,
		0x7F,0xFF,0xFF, 0,0,0, 0,0,0, 0x80,0,0
	};
	ok = loadAIFFFixture(stereo24AIFF, sizeof (stereo24AIFF), true) &&
		tmpSmp.length == 2 &&
		(tmpSmp.flags & (SAMPLE_16BIT | SAMPLE_STEREO)) == (SAMPLE_16BIT | SAMPLE_STEREO) &&
		tmpSmp.dataPtrR != NULL &&
		((int16_t *)tmpSmp.dataPtrL)[0] > 32000 && ((int16_t *)tmpSmp.dataPtrL)[1] == 0 &&
		((int16_t *)tmpSmp.dataPtrR)[0] == 0 && ((int16_t *)tmpSmp.dataPtrR)[1] < -32000;
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	if (!ok) return false;

	uint8_t truncatedAIFF[sizeof (monoAIFF)];
	memcpy(truncatedAIFF, monoAIFF, sizeof (truncatedAIFF));
	truncatedAIFF[45] = 12; // SSND declares more payload than FORM contains
	ok = loadAIFFFixture(truncatedAIFF, sizeof (truncatedAIFF), false);
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	return ok;
}
#endif
