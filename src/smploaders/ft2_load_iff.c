/* IFF (Amiga/FT2) sample loader
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_audio.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

static bool readExact(FILE *f, void *dst, size_t bytes)
{
	return bytes == 0 || fread(dst, 1, bytes, f) == bytes;
}

static bool seekTo(FILE *f, uint32_t offset, uint32_t filesize)
{
	return offset <= filesize && fseek(f, (long)offset, SEEK_SET) == 0;
}

bool loadIFF(FILE *f, uint32_t filesize)
{
	uint8_t fileHeader[12];
	uint32_t length, volume, loopStart, loopLength, sampleRate;
	sample_t *s = &tmpSmp;

	if (filesize < 12)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	if (!seekTo(f, 0, filesize) || !readExact(f, fileHeader, sizeof (fileHeader)) ||
		memcmp(fileHeader, "FORM", 4) != 0 ||
		(memcmp(&fileHeader[8], "8SVX", 4) != 0 && memcmp(&fileHeader[8], "16SV", 4) != 0))
		return false;
	bool sample16Bit = memcmp(&fileHeader[8], "16SV", 4) == 0;

	uint32_t vhdrPtr = 0, vhdrLen = 0;
	uint32_t bodyPtr = 0, bodyLen = 0;
	uint32_t namePtr = 0, nameLen = 0;
	bool bodyConsumesRemainder = false;

	if (!seekTo(f, 12, filesize))
		return false;
	while (true)
	{
		const long chunkHeaderPos = ftell(f);
		if (chunkHeaderPos < 0) return false;
		if ((uint64_t)chunkHeaderPos + 8 > filesize) break;

		uint32_t blockName, blockSize;
		if (!readExact(f, &blockName, sizeof (blockName)) ||
			!readExact(f, &blockSize, sizeof (blockSize)))
			return false;

		blockName = SWAP32(blockName);
		blockSize = SWAP32(blockSize);

		const long payloadPos = ftell(f);
		if (payloadPos < 0 || (uint64_t)payloadPos + blockSize + (blockSize & 1u) > filesize)
			return false;

		switch (blockName)
		{
			case 0x56484452: // VHDR
			{
				vhdrPtr = (uint32_t)payloadPos;
				vhdrLen = blockSize;
			}
			break;

			case 0x4E414D45: // NAME
			{
				namePtr = (uint32_t)payloadPos;
				nameLen = blockSize;
			}
			break;

			case 0x424F4459: // BODY
			{
				bodyPtr = (uint32_t)payloadPos;
				bodyLen = blockSize;
				bodyConsumesRemainder = blockSize == 0;
			}
			break;

			default: break;
		}

		if (bodyConsumesRemainder)
			break;
		const uint64_t paddedBlockSize = (uint64_t)blockSize + (blockSize & 1u);
		if (fseek(f, (long)paddedBlockSize, SEEK_CUR) != 0)
			return false;
	}

	if (vhdrPtr == 0 || vhdrLen < 20 || bodyPtr == 0)
	{
		loaderMsgBox("Error loading sample: The sample is not supported or is invalid!");
		return false;
	}

	// kludge for some really strange IFFs
	if (bodyLen == 0)
		bodyLen = filesize - bodyPtr;

	if (bodyLen > filesize-bodyPtr)
		bodyLen = filesize - bodyPtr;

	uint8_t skipped[5], sampleType;
	uint16_t sampleRate16;
	if (!seekTo(f, vhdrPtr, filesize) ||
		!readExact(f, &loopStart, sizeof (loopStart)) ||
		!readExact(f, &loopLength, sizeof (loopLength)) ||
		!readExact(f, skipped, 4) ||
		!readExact(f, &sampleRate16, sizeof (sampleRate16)) ||
		!readExact(f, skipped, 1) || !readExact(f, &sampleType, sizeof (sampleType)))
		return false;
	loopStart = SWAP32(loopStart);
	loopLength = SWAP32(loopLength);
	sampleRate = SWAP16(sampleRate16);

	if (sampleType != 0) // sample type
	{
		loaderMsgBox("Error loading sample: The sample is not supported!");
		return false;
	}

	if (!readExact(f, &volume, sizeof (volume))) return false;
	volume = SWAP32(volume);
	if (volume > 65535)
		volume = 65535;

	volume = (volume + 512) / 1024; // rounded

	length = bodyLen;
	if (sample16Bit)
	{
		length >>= 1;
		loopStart >>= 1;
		loopLength >>= 1;
	}

	s->length = (int32_t)MIN(length, MAX_SAMPLE_LEN);

	if (!allocateSmpData(s, s->length, sample16Bit, false))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	const size_t bytesToRead = (size_t)s->length << sample16Bit;
	if (!seekTo(f, bodyPtr, filesize) || !readExact(f, s->dataPtr, bytesToRead))
	{
		loaderMsgBox("General I/O error during loading! Is the file in use?");
		return false;
	}

	if (loopStart <= (uint32_t)s->length && loopLength <= (uint32_t)s->length-loopStart)
	{
		s->loopStart = (int32_t)loopStart;
		s->loopLength = (int32_t)loopLength;
	}

	if (s->loopLength > 0)
		s->flags |= LOOP_FWD;

	if (sample16Bit)
		s->flags |= SAMPLE_16BIT;

	s->volume = (uint8_t)volume;
	s->panning = 128;

	setSampleC4Hz(s, sampleRate);

	// set name
	if (namePtr != 0 && nameLen > 0)
	{
		if (nameLen > 22)
			nameLen = 22;

		if (!seekTo(f, namePtr, filesize) || !readExact(f, s->name, nameLen))
			return false;
		s->name[nameLen] = '\0';

		smpFilenameSet = true;
	}

	return true;
}

#ifdef FT2_STABILITY_TESTS
bool runIFFLoaderRegressionTests(void)
{
	const uint8_t validIFF[] =
	{
		'F','O','R','M', 0,0,0,42, '8','S','V','X',
		'V','H','D','R', 0,0,0,20,
		0,0,0,0, 0,0,0,0, 0,0,0,0, 0x41,0x56, 1,0, 0,1,0,0,
		'B','O','D','Y', 0,0,0,1, 0x7F,0
	};
	FILE *f = tmpfile();
	if (f == NULL) return false;
	bool ok = fwrite(validIFF, 1, sizeof (validIFF), f) == sizeof (validIFF) &&
		fseek(f, 0, SEEK_SET) == 0 && loadIFF(f, sizeof (validIFF)) &&
		tmpSmp.length == 1 && tmpSmp.dataPtr[0] == 0x7F;
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	fclose(f);
	if (!ok) return false;

	uint8_t truncatedIFF[sizeof (validIFF)];
	memcpy(truncatedIFF, validIFF, sizeof (truncatedIFF));
	truncatedIFF[47] = 2; // BODY declares two bytes, but only one data byte remains before padding
	f = tmpfile();
	if (f == NULL) return false;
	ok = fwrite(truncatedIFF, 1, sizeof (truncatedIFF)-1, f) == sizeof (truncatedIFF)-1 &&
		fseek(f, 0, SEEK_SET) == 0 && !loadIFF(f, sizeof (truncatedIFF)-1);
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	fclose(f);
	return ok;
}
#endif
