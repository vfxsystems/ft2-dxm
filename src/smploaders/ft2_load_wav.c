/* WAV sample loader
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

enum
{
	WAV_FORMAT_PCM = 1,
	WAV_FORMAT_IEEE_FLOAT = 3
};

static bool readExact(FILE *f, void *dst, size_t bytes)
{
	return bytes == 0 || fread(dst, 1, bytes, f) == bytes;
}

static bool seekTo(FILE *f, uint64_t offset, uint32_t filesize)
{
	return offset <= filesize && fseek(f, (long)offset, SEEK_SET) == 0;
}

static uint16_t readLE16(const uint8_t *src)
{
	return (uint16_t)(src[0] | (src[1] << 8));
}

static uint32_t readLE32(const uint8_t *src)
{
	return (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
		((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static double decodeHighResolutionSample(const uint8_t *src, uint16_t audioFormat, uint16_t bitsPerSample)
{
	if (audioFormat == WAV_FORMAT_IEEE_FLOAT)
	{
		if (bitsPerSample == 32)
		{
			const uint32_t bits = readLE32(src);
			float sample;
			memcpy(&sample, &bits, sizeof (sample));
			return isfinite(sample) ? sample : 0.0;
		}

		uint64_t bits = (uint64_t)readLE32(src) | ((uint64_t)readLE32(src + 4) << 32);
		double sample;
		memcpy(&sample, &bits, sizeof (sample));
		return isfinite(sample) ? sample : 0.0;
	}

	if (bitsPerSample == 24)
	{
		uint32_t value = (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16);
		if (value & 0x00800000u)
			value |= 0xFF000000u;
		return (double)(int32_t)value;
	}

	return (double)(int32_t)readLE32(src);
}

static int16_t scaleToSigned16(double sample, double gain)
{
	double scaled = sample * gain;
	if (scaled > INT16_MAX) scaled = INT16_MAX;
	if (scaled < INT16_MIN) scaled = INT16_MIN;
	return (int16_t)scaled;
}

bool loadWAV(FILE *f, uint32_t filesize)
{
	uint16_t audioFormat, numChannels, bitsPerSample;
	uint32_t sampleRate, sampleLength;
	sample_t *s = &tmpSmp;

	if (filesize < 12)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}
	uint8_t riffHeader[12];
	if (!seekTo(f, 0, filesize) || !readExact(f, riffHeader, sizeof (riffHeader)) ||
		memcmp(riffHeader, "RIFF", 4) != 0 || memcmp(&riffHeader[8], "WAVE", 4) != 0)
		return false;

	uint32_t fmtPtr  = 0, fmtLen  = 0;
	uint32_t dataPtr = 0, dataLen = 0;
	uint32_t inamPtr = 0, inamLen = 0;
	uint32_t xtraPtr = 0, xtraLen = 0;
	uint32_t smplPtr = 0, smplLen = 0;

	// look for wanted chunks and set up pointers + lengths
	if (!seekTo(f, 12, filesize)) return false;
	while (true)
	{
		const long chunkHeaderPos = ftell(f);
		if (chunkHeaderPos < 0) return false;
		if ((uint64_t)chunkHeaderPos + 8 > filesize) break;

		uint32_t chunkID, chunkSize;
		if (!readExact(f, &chunkID, sizeof (chunkID)) || !readExact(f, &chunkSize, sizeof (chunkSize)))
			return false;
		const long payloadPos = ftell(f);
		if (payloadPos < 0) return false;
		const uint64_t chunkEnd = (uint64_t)payloadPos + chunkSize + (chunkSize & 1u);
		if (chunkEnd > filesize) return false;

		switch (chunkID)
		{
			case 0x20746D66: // "fmt "
			{
				fmtPtr = (uint32_t)payloadPos;
				fmtLen = chunkSize;
			}
			break;

			case 0x61746164: // "data"
			{
				dataPtr = (uint32_t)payloadPos;
				dataLen = chunkSize;
			}
			break;

			case 0x5453494C: // "LIST"
			{
				if (chunkSize >= 4)
				{
					uint32_t listType;
					if (!readExact(f, &listType, sizeof (listType))) return false;
					if (listType == 0x4F464E49) // "INFO"
					{
						const uint64_t listEnd = (uint64_t)payloadPos + chunkSize;
						while (true)
						{
							const long infoHeaderPos = ftell(f);
							if (infoHeaderPos < 0) return false;
							if ((uint64_t)infoHeaderPos + 8 > listEnd) break;
							uint32_t infoID, infoSize;
							if (!readExact(f, &infoID, sizeof (infoID)) ||
								!readExact(f, &infoSize, sizeof (infoSize))) return false;
							const long infoPayloadPos = ftell(f);
							if (infoPayloadPos < 0) return false;
							const uint64_t infoEnd = (uint64_t)infoPayloadPos + infoSize + (infoSize & 1u);
							if (infoEnd > listEnd) return false;

							if (infoID == 0x4D414E49) // "INAM"
							{
								inamPtr = (uint32_t)infoPayloadPos;
								inamLen = infoSize;
							}
							if (!seekTo(f, infoEnd, filesize)) return false;
						}
					}
				}
			}
			break;

			case 0x61727478: // "xtra"
			{
				xtraPtr = (uint32_t)payloadPos;
				xtraLen = chunkSize;
			}
			break;

			case 0x6C706D73: // "smpl"
			{
				smplPtr = (uint32_t)payloadPos;
				smplLen = chunkSize;
			}
			break;

			default: break;
		}

		if (!seekTo(f, chunkEnd, filesize)) return false;
	}

	// we need at least "fmt " and "data" - check if we found them sanely
	if (fmtPtr == 0 || fmtLen < 16 || dataPtr == 0 || dataLen == 0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	// ---- READ "fmt " CHUNK ----
	uint8_t fmtSkipped[6];
	if (!seekTo(f, fmtPtr, filesize) || !readExact(f, &audioFormat, sizeof (audioFormat)) ||
		!readExact(f, &numChannels, sizeof (numChannels)) ||
		!readExact(f, &sampleRate, sizeof (sampleRate)) ||
		!readExact(f, fmtSkipped, sizeof (fmtSkipped)) ||
		!readExact(f, &bitsPerSample, sizeof (bitsPerSample)))
		return false;
	// test if the WAV is compatible with our loader
	if (audioFormat != WAV_FORMAT_PCM && audioFormat != WAV_FORMAT_IEEE_FLOAT)
	{
		loaderMsgBox("Error loading sample: The sample is not supported!");
		return false;
	}
	if (audioFormat == WAV_FORMAT_PCM && bitsPerSample == 64)
	{
		loaderMsgBox("Error loading sample: Unsupported bitdepth!");
		return false;
	}

	const uint32_t bytesPerSample = bitsPerSample / 8;
	const uint32_t bytesPerFrame = numChannels * bytesPerSample;
	if (sampleRate == 0 || bytesPerFrame == 0 || dataLen < bytesPerFrame || dataLen % bytesPerFrame != 0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}
	sampleLength = dataLen / bytesPerFrame;
	if (sampleLength > MAX_SAMPLE_LEN)
	{
		loaderMsgBox("Error loading sample: The sample is too long!");
		return false;
	}

	if (numChannels == 0 || numChannels > 2)
	{
		loaderMsgBox("Error loading sample: Unsupported number of channels!");
		return false;
	}

	if (audioFormat == WAV_FORMAT_IEEE_FLOAT && bitsPerSample != 32 && bitsPerSample != 64)
	{
		loaderMsgBox("Error loading sample: Unsupported bitdepth!");
		return false;
	}

	if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32 && bitsPerSample != 64)
	{
		loaderMsgBox("Error loading sample: Unsupported bitdepth!");
		return false;
	}

	// ---- READ SAMPLE DATA ----
	if (!seekTo(f, dataPtr, filesize)) return false;

	int16_t stereoSampleLoadMode = -1;
	if (numChannels == 2)
	{
		stereoSampleLoadMode = loaderSysReq(4, "System request", "This is a stereo sample.", NULL);
		if (stereoSampleLoadMode != STEREO_SAMPLE_READ_LEFT &&
			stereoSampleLoadMode != STEREO_SAMPLE_READ_RIGHT &&
			stereoSampleLoadMode != STEREO_SAMPLE_CONVERT)
			return false;
	}

	const bool stereoOutput = numChannels == 2 && stereoSampleLoadMode == STEREO_SAMPLE_CONVERT;
	const uint32_t selectedChannel = stereoSampleLoadMode == STEREO_SAMPLE_READ_RIGHT ? 1 : 0;
	uint8_t *sourceData = (uint8_t *)malloc(dataLen);
	if (sourceData == NULL)
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}
	if (!readExact(f, sourceData, dataLen))
	{
		free(sourceData);
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	const bool output16Bit = bitsPerSample != 8;
	if (!allocateSmpData(s, sampleLength, output16Bit, stereoOutput))
	{
		free(sourceData);
		loaderMsgBox("Not enough memory!");
		return false;
	}

	if (bitsPerSample == 8)
	{
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			s->dataPtrL[i] = frame[selectedChannel] ^ 0x80;
			if (stereoOutput)
				s->dataPtrR[i] = frame[1] ^ 0x80;
		}
	}
	else if (bitsPerSample == 16)
	{
		int16_t *left = (int16_t *)s->dataPtrL;
		int16_t *right = (int16_t *)s->dataPtrR;
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			left[i] = (int16_t)readLE16(frame + (selectedChannel * 2));
			if (stereoOutput)
				right[i] = (int16_t)readLE16(frame + 2);
		}
	}
	else
	{
		double peak = 0.0;
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			const double left = fabs(decodeHighResolutionSample(
				frame + (selectedChannel * bytesPerSample), audioFormat, bitsPerSample));
			if (left > peak) peak = left;
			if (stereoOutput)
			{
				const double right = fabs(decodeHighResolutionSample(
					frame + bytesPerSample, audioFormat, bitsPerSample));
				if (right > peak) peak = right;
			}
		}

		const double gain = peak > 0.0 ? (double)INT16_MAX / peak : 0.0;
		int16_t *left = (int16_t *)s->dataPtrL;
		int16_t *right = (int16_t *)s->dataPtrR;
		for (uint32_t i = 0; i < sampleLength; i++)
		{
			const uint8_t *frame = sourceData + ((size_t)i * bytesPerFrame);
			left[i] = scaleToSigned16(decodeHighResolutionSample(
				frame + (selectedChannel * bytesPerSample), audioFormat, bitsPerSample), gain);
			if (stereoOutput)
				right[i] = scaleToSigned16(decodeHighResolutionSample(
					frame + bytesPerSample, audioFormat, bitsPerSample), gain);
		}
	}
	free(sourceData);

	s->flags = (output16Bit ? SAMPLE_16BIT : 0) | (stereoOutput ? SAMPLE_STEREO : 0);
	s->length = sampleLength;


	setSampleC4Hz(s, sampleRate);

	s->volume = 64;
	s->panning = 128;

	// ---- READ "smpl" chunk ----
	if (smplPtr != 0 && smplLen >= 52)
	{
		uint32_t numLoops, loopType, loopStart, loopEnd;

		if (!seekTo(f, (uint64_t)smplPtr + 28, filesize) ||
			!readExact(f, &numLoops, sizeof (numLoops)))
			return false;
		if (numLoops == 1)
		{
			if (!seekTo(f, (uint64_t)smplPtr + 40, filesize) ||
				!readExact(f, &loopType, sizeof (loopType)) ||
				!readExact(f, &loopStart, sizeof (loopStart)) ||
				!readExact(f, &loopEnd, sizeof (loopEnd)))
				return false;

			if (loopEnd != UINT32_MAX)
				loopEnd++;
			if (loopStart < loopEnd && loopEnd <= sampleLength)
			{
				s->loopStart = loopStart;
				s->loopLength = loopEnd - loopStart;
				s->flags |= (loopType == 0) ? LOOP_FWD : LOOP_BIDI;
			}
		}
	}
	// ---------------------------

	// ---- READ "xtra" chunk ----
	if (xtraPtr != 0 && xtraLen >= 8)
	{
		uint16_t tmpPan, tmpVol;
		uint32_t xtraFlags;

		if (!seekTo(f, xtraPtr, filesize) ||
			!readExact(f, &xtraFlags, sizeof (xtraFlags)) ||
			!readExact(f, &tmpPan, sizeof (tmpPan)) ||
			!readExact(f, &tmpVol, sizeof (tmpVol)))
			return false;

		// panning (0..256)
		if (xtraFlags & 0x20) // set panning flag
		{
			if (tmpPan > 255)
				tmpPan = 255;

			s->panning = (uint8_t)tmpPan;
		}

		// volume (0..256)
		if (tmpVol > 256)
			tmpVol = 256;

		s->volume = (uint8_t)((tmpVol + 2) / 4); // 0..256 -> 0..64 (rounded)
	}
	// ---------------------------

	// ---- READ "INAM" chunk ----
	if (inamPtr != 0 && inamLen > 0)
	{
		if (inamLen > 22)
			inamLen = 22;

		if (!seekTo(f, inamPtr, filesize) || !readExact(f, s->name, inamLen))
			return false;
		s->name[inamLen] = '\0';

		smpFilenameSet = true;
	}

	return true;
}

#ifdef FT2_STABILITY_TESTS
static void ignoreWAVLoaderMessage(const char *message, ...)
{
	(void)message;
}

static int16_t chooseWAVStereo(int16_t type, const char *headline, const char *text, void (*callback)(void))
{
	(void)type;
	(void)headline;
	(void)text;
	(void)callback;
	return STEREO_SAMPLE_CONVERT;
}

static bool loadWAVFixture(const uint8_t *data, size_t size, bool expectedResult)
{
	FILE *f = tmpfile();
	if (f == NULL) return false;
	void (*oldLoaderMsgBox)(const char *, ...) = loaderMsgBox;
	int16_t (*oldLoaderSysReq)(int16_t, const char *, const char *, void (*)(void)) = loaderSysReq;
	loaderMsgBox = ignoreWAVLoaderMessage;
	loaderSysReq = chooseWAVStereo;
	const bool result = fwrite(data, 1, size, f) == size &&
		fseek(f, 0, SEEK_SET) == 0 && loadWAV(f, (uint32_t)size) == expectedResult;
	loaderMsgBox = oldLoaderMsgBox;
	loaderSysReq = oldLoaderSysReq;
	fclose(f);
	return result;
}

bool runWAVLoaderRegressionTests(void)
{
	const uint8_t validWAV[] =
	{
		'R','I','F','F', 38,0,0,0, 'W','A','V','E',
		'f','m','t',' ', 16,0,0,0,
		1,0, 1,0, 0x40,0x1F,0,0, 0x40,0x1F,0,0, 1,0, 8,0,
		'd','a','t','a', 2,0,0,0, 0,255
	};
	bool ok = loadWAVFixture(validWAV, sizeof (validWAV), true) &&
		tmpSmp.length == 2 && tmpSmp.dataPtrL != NULL &&
		tmpSmp.dataPtrL[0] == INT8_MIN && tmpSmp.dataPtrL[1] == INT8_MAX &&
		(tmpSmp.flags & (SAMPLE_16BIT | SAMPLE_STEREO)) == 0;
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	if (!ok) return false;

	const uint8_t stereo24WAV[] =
	{
		'R','I','F','F', 48,0,0,0, 'W','A','V','E',
		'f','m','t',' ', 16,0,0,0,
		1,0, 2,0, 0x40,0x1F,0,0, 0x80,0xBB,0,0, 6,0, 24,0,
		'd','a','t','a', 12,0,0,0,
		0xFF,0xFF,0x7F, 0,0,0, 0,0,0, 0,0,0x80
	};
	ok = loadWAVFixture(stereo24WAV, sizeof (stereo24WAV), true) &&
		tmpSmp.length == 2 &&
		(tmpSmp.flags & (SAMPLE_16BIT | SAMPLE_STEREO)) == (SAMPLE_16BIT | SAMPLE_STEREO) &&
		tmpSmp.dataPtrR != NULL &&
		((int16_t *)tmpSmp.dataPtrL)[0] > 32000 && ((int16_t *)tmpSmp.dataPtrL)[1] == 0 &&
		((int16_t *)tmpSmp.dataPtrR)[0] == 0 && ((int16_t *)tmpSmp.dataPtrR)[1] < -32000;
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	if (!ok) return false;

	uint8_t truncatedWAV[sizeof (validWAV)];
	memcpy(truncatedWAV, validWAV, sizeof (truncatedWAV));
	truncatedWAV[40] = 4; // data chunk declares four bytes but only contains two
	ok = loadWAVFixture(truncatedWAV, sizeof (truncatedWAV), false);
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	if (!ok) return false;

	uint8_t invalidDepthWAV[sizeof (validWAV)];
	memcpy(invalidDepthWAV, validWAV, sizeof (invalidDepthWAV));
	invalidDepthWAV[34] = 0;
	ok = loadWAVFixture(invalidDepthWAV, sizeof (invalidDepthWAV), false);
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	return ok;
}
#endif
