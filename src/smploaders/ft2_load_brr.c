/* Super Nintendo BRR sample loader (based on work by _astriid_, but heavily modified)
**
** Note: Vol/loop sanitation is done in the last stage
** of sample loading, so you don't need to do that here.
** Do NOT close the file handle!
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"
#include "../ft2_sample_loader.h"

#define BRR_RATIO(x) (((x) * 16) / 9)

static int16_t s1, s2;

bool detectBRR(FILE *f)
{
	if (f == NULL)
		return false;

	const long oldPos = ftell(f);
	if (oldPos < 0 || fseek(f, 0, SEEK_END) != 0)
		return false;
	const long fileEnd = ftell(f);
	if (fileEnd < 0 || (uint64_t)fileEnd > UINT32_MAX)
		goto error;
	const uint32_t filesize = (uint32_t)fileEnd;

	const uint32_t filesizeMod9 = filesize % 9;

	if (filesize < 11 || filesize > 65536 || (filesizeMod9 != 0 && filesizeMod9 != 2))
		goto error; // definitely not a BRR file

	if (fseek(f, 0, SEEK_SET) != 0)
		goto error;

	uint32_t blockBytes = filesize;
	if (filesizeMod9 == 2) // skip loop block word
	{
		if (fseek(f, 2, SEEK_CUR) != 0)
			goto error;
		blockBytes -= 2;
	}

	uint32_t numBlocks = blockBytes / 9;

	// if the first block is the last, this is very unlikely to be a real BRR sample
	int headerByte = fgetc(f);
	if (headerByte == EOF) goto error;
	uint8_t header = (uint8_t)headerByte;
	if (header & 1)
		goto error;

	/* Test the shift range of a few blocks.
	** While it's possible to have a shift value above 12 (illegal),
	** it's rare in dumped BRR samples. I have personally seen 13,
	** but never above, so let's test for >13 then.
	*/
	uint32_t blocksToTest = 8;
	if (blocksToTest > numBlocks)
		blocksToTest = numBlocks;

	for (uint32_t i = 0; i < blocksToTest; i++)
	{
		const uint8_t shift = header >> 4;
		if (shift > 13)
			goto error;

		if (i + 1 < blocksToTest)
		{
			if (fseek(f, 8, SEEK_CUR) != 0) goto error;
			headerByte = fgetc(f);
			if (headerByte == EOF) goto error;
			header = (uint8_t)headerByte;
		}
	}

	return fseek(f, oldPos, SEEK_SET) == 0;

error:
	(void)fseek(f, oldPos, SEEK_SET);
	return false;
}

static int16_t decodeSample(int8_t nybble, int32_t shift, int32_t filter)
{
	int32_t smp = ((int32_t)nybble * (1 << shift)) >> 1;
	if (shift >= 13)
		smp &= ~2047; // invalid shift clamping

	switch (filter)
	{
		default: break;

		case 1:
			smp += (s1 * 15) >> 4;
			break;

		case 2:
			smp += (s1 * 61) >> 5;
			smp -= (s2 * 15) >> 4;
			break;

		case 3:
			smp += (s1 * 115) >> 6;
			smp -= (s2 *  13) >> 4;
			break;
	}

	// clamp as 16-bit, even if we decode into a 15-bit sample
	smp = CLAMP(smp, -32768, 32767);

	// 15-bit clip
	if (smp & 16384)
		smp |= ~16383;
	else
		smp &= 16383;

	// shuffle last samples (store as 15-bit sample)
	s2 = s1;
	s1 = (int16_t)smp;

	return (int16_t)(smp * 2); // multiply by two to get 16-bit scale
}

bool loadBRR(FILE *f, uint32_t filesize)
{
	sample_t *s = &tmpSmp;
	if (filesize < 9 || filesize > 65536 || (filesize % 9 != 0 && filesize % 9 != 2))
		return false;

	uint32_t blockBytes = filesize, loopStart = 0;
	if ((filesize % 9) == 2) // loop header present
	{
		uint16_t loopStartBlock;
		if (fread(&loopStartBlock, sizeof (loopStartBlock), 1, f) != 1)
			return false;
		loopStart = BRR_RATIO(loopStartBlock);
		blockBytes -= 2;
	}

	uint32_t sampleLength = BRR_RATIO(blockBytes);
	if (!allocateSmpData(s, sampleLength, true, false))
	{
		loaderMsgBox("Not enough memory!");
		return false;
	}

	uint32_t shift = 0, filter = 0;
	bool loopFlag = false, endFlag = false;

	s1 = s2 = 0; // clear last BRR samples (for decoding)

	int16_t *ptr16 = (int16_t *)s->dataPtr;
	for (uint32_t i = 0; i < blockBytes; i++)
	{
		const uint32_t blockOffset = i % 9;
		const int nextByte = fgetc(f);
		if (nextByte == EOF) return false;
		const uint8_t byte = (uint8_t)nextByte;

		if (blockOffset == 0) // this byte is the BRR header
		{
			shift = byte >> 4;
			filter = (byte & 0x0C) >> 2;
			loopFlag = !!(byte & 0x02);
			endFlag = !!(byte & 0x01);
			continue;
		}

		// decode samples
		*ptr16++ = decodeSample((int8_t)byte >> 4, shift, filter);
		*ptr16++ = decodeSample((int8_t)(byte << 4) >> 4, shift, filter);

		if (endFlag && blockOffset == 8)
		{
			sampleLength = BRR_RATIO(i+1);
			break;
		}
	}

	s->volume = 64;
	s->panning = 128;
	s->flags |= SAMPLE_16BIT;
	s->length = sampleLength;

	if (loopFlag) // XXX: Maybe this is not how to do it..?
	{
		if (loopStart >= sampleLength)
			return false;
		s->flags |= LOOP_FWD;
		s->loopStart = loopStart;
		s->loopLength = sampleLength - loopStart;
	}

	return true;
}

#ifdef FT2_STABILITY_TESTS
bool runBRRLoaderRegressionTests(void)
{
	const uint8_t validBlock[9] = { 1, 0, 0, 0, 0, 0, 0, 0, 0 };
	FILE *f = tmpfile();
	if (f == NULL) return false;
	bool ok = fwrite(validBlock, 1, sizeof (validBlock), f) == sizeof (validBlock) &&
		fseek(f, 0, SEEK_SET) == 0 && loadBRR(f, sizeof (validBlock)) &&
		tmpSmp.length == 16 && (tmpSmp.flags & SAMPLE_16BIT) != 0;
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	fclose(f);
	if (!ok) return false;

	f = tmpfile();
	if (f == NULL) return false;
	ok = fwrite(validBlock, 1, sizeof (validBlock)-1, f) == sizeof (validBlock)-1 &&
		fseek(f, 0, SEEK_SET) == 0 && !loadBRR(f, sizeof (validBlock));
	freeSmpData(&tmpSmp);
	memset(&tmpSmp, 0, sizeof (tmpSmp));
	fclose(f);

	s1 = s2 = 0;
	return ok && decodeSample(-8, 12, 0) == INT16_MIN;
}
#endif
