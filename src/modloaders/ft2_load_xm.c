/* Fasttracker II (or compatible) XM loader
**
** Note: Data sanitation is done in the last stage
** of module loading, so you don't need to do that here.
**
** See also: DXM loader (ft2_load_dxm.c) for extended XM+DSP+WAV+TF4 format
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_module_loader.h"
#include "../ft2_sample_ed.h"
#include "../ft2_tables.h"
#include "../ft2_sysreqs.h"

// External flag to indicate if we're loading a DXM file
extern bool isDXMFormat;

static uint8_t packedPattData[65536];

/* ModPlug Tracker & OpenMPT supports up to 32 samples per instrument for XMs -  we don't.
** For such modules, we use a temporary array here to store the extra sample data lengths
** we need to skip to be able to load the file (we lose the extra samples, though...).
*/
static uint32_t extraSampleDataLengths[257][32-MAX_SMP_PER_INST];

static bool readExact(FILE *f, void *dst, size_t bytes)
{
	return bytes == 0 || fread(dst, 1, bytes, f) == bytes;
}

static bool skipBytes(FILE *f, uint64_t bytes, uint32_t filesize)
{
	const long position = ftell(f);
	if (position < 0 || (uint64_t)position > filesize || bytes > filesize - (uint64_t)position)
		return false;
	return fseek(f, (long)bytes, SEEK_CUR) == 0;
}

static bool seekTo(FILE *f, uint64_t offset, uint32_t filesize)
{
	return offset <= filesize && fseek(f, (long)offset, SEEK_SET) == 0;
}

static bool loadInstrHeader(FILE *f, uint16_t i, uint32_t filesize);
static bool loadInstrSample(FILE *f, uint16_t i, uint32_t filesize);
static bool unpackPatt(note_t *dst, const uint8_t *src, size_t srcSize,
	uint16_t sourceRows, uint16_t outputRows, uint16_t numChannels);
static bool loadPatterns(FILE *f, uint16_t numPatterns, uint16_t xmVersion,
	uint16_t numChannels, uint32_t filesize);
static bool loadADPCMSample(FILE *f, sample_t *s); // ModPlug Tracker

static void decodeStereoSample(sample_t *s, const void *encodedData, bool sample16Bit, bool preserveStereo)
{
	if (sample16Bit)
	{
		const int16_t *source = (const int16_t *)encodedData;
		int16_t *dstL = (int16_t *)s->dataPtrL;
		int16_t *dstR = preserveStereo ? (int16_t *)s->dataPtrR : NULL;
		int16_t deltaL = 0, deltaR = 0;
		for (int32_t frame = 0; frame < s->length; frame++)
		{
			deltaL += source[frame];
			deltaR += source[s->length + frame];
			dstL[frame] = preserveStereo ? deltaL : (int16_t)(((int32_t)deltaL + deltaR) / 2);
			if (dstR != NULL) dstR[frame] = deltaR;
		}
	}
	else
	{
		const int8_t *source = (const int8_t *)encodedData;
		int8_t *dstL = s->dataPtrL;
		int8_t *dstR = preserveStereo ? s->dataPtrR : NULL;
		int8_t deltaL = 0, deltaR = 0;
		for (int32_t frame = 0; frame < s->length; frame++)
		{
			deltaL += source[frame];
			deltaR += source[s->length + frame];
			dstL[frame] = preserveStereo ? deltaL : (int8_t)(((int16_t)deltaL + deltaR) / 2);
			if (dstR != NULL) dstR[frame] = deltaR;
		}
	}
}

bool loadXM(FILE *f, uint32_t filesize)
{
	xmHdr_t h;

	if (filesize < sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (!readExact(f, &h, sizeof (h)) || memcmp(h.ID, "Extended Module: ", 17) != 0 || h.x1A != 0x1A)
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (h.version < 0x0102 || h.version > 0x0104)
	{
		loaderMsgBox("Error loading XM: Unsupported file version (v%01X.%02X).", (h.version >> 8) & 15, h.version & 0xFF);
		return false;
	}

	if (h.numOrders > MAX_ORDERS)
	{
		loaderMsgBox("Error loading XM: The song has more than 256 orders!");
		return false;
	}

	if (h.numPatterns > MAX_PATTERNS)
	{
		loaderMsgBox("Error loading XM: The song has more than 256 patterns!");
		return false;
	}

	if (h.numChannels == 0)
	{
		loaderMsgBox("Error loading XM: This file is corrupt.");
		return false;
	}
	if (h.numChannels > 255)
	{
		loaderMsgBox("Error loading XM: More than 255 channels are not supported.");
		return false;
	}

	if (h.numInstr > 256) // if >128 instruments, we fake-load up to 128 extra instruments and discard them
	{
		loaderMsgBox("Error loading XM: This file is corrupt.");
		return false;
	}

	if (h.headerSize < 276 || !seekTo(f, 60ULL + (uint32_t)h.headerSize, filesize))
	{
		loaderMsgBox("Error loading XM: Invalid or truncated module header!");
		return false;
	}

	memcpy(songTmp.name, h.name, 20);
	songTmp.name[20] = '\0';

	songTmp.songLength = h.numOrders;
	songTmp.songLoopStart = h.songLoopStart;
	songTmp.numChannels = (uint8_t)MIN(h.numChannels, MAX_CHANNELS);
	songTmp.BPM = h.BPM;
	songTmp.speed = h.speed;
	tmpLinearPeriodsFlag = h.flags & 1;

	if (songTmp.songLength == 0)
		songTmp.songLength = 1; // songTmp.songTab is already empty
	else
		memcpy(songTmp.orders, h.orders, songTmp.songLength);

	// some strange XMs have the order list padded with 0xFF, remove them!
	for (int16_t j = 255; j >= 0; j--)
	{
		if (songTmp.orders[j] != 0xFF)
			break;

		if (songTmp.songLength > j)
			songTmp.songLength = j;
	}

	// even though XM supports 256 orders, FT2 supports only 255...
	if (songTmp.songLength > 255)
		songTmp.songLength = 255;
	if (h.numPatterns > 0)
	{
		for (uint16_t order = 0; order < songTmp.songLength; order++)
		{
			if (songTmp.orders[order] >= h.numPatterns)
			{
				loaderMsgBox("Error loading XM: Order list references a missing pattern!");
				return false;
			}
		}
	}

	if (h.version < 0x0104)
	{
		// XM v1.02 and XM v1.03

		for (uint16_t i = 1; i <= h.numInstr; i++)
		{
			if (!loadInstrHeader(f, i, filesize))
				return false;
		}

		if (!loadPatterns(f, h.numPatterns, h.version, h.numChannels, filesize))
			return false;

		for (uint16_t i = 1; i <= h.numInstr; i++)
		{
			if (!loadInstrSample(f, i, filesize))
				return false;
		}
	}
	else
	{
		// XM v1.04 (latest version)

		if (!loadPatterns(f, h.numPatterns, h.version, h.numChannels, filesize))
			return false;

		for (uint16_t i = 1; i <= h.numInstr; i++)
		{
			if (!loadInstrHeader(f, i, filesize))
				return false;

			if (!loadInstrSample(f, i, filesize))
				return false;
		}
	}

	// if we temporarily loaded more than 128 instruments, clear the extra allocated memory
	if (h.numInstr > MAX_INST)
	{
		for (int32_t i = MAX_INST+1; i <= h.numInstr; i++)
		{
			if (instrTmp[i] != NULL)
			{
				free(instrTmp[i]);
				instrTmp[i] = NULL;
			}
		}
	}

	/* We support loading XMs with up to 32 samples per instrument (ModPlug/OpenMPT),
	** but only the first 16 will be loaded. Now make sure we set the number of samples
	** back to max 16 in the headers before loading is done.
	*/
	bool instrHasMoreThan16Samples = false;
	for (int32_t i = 1; i <= MAX_INST; i++)
	{
		if (instrTmp[i] != NULL && instrTmp[i]->numSamples > MAX_SMP_PER_INST)
		{
			instrHasMoreThan16Samples = true;
			instrTmp[i]->numSamples = MAX_SMP_PER_INST;
		}
	}

	if (h.numChannels > MAX_CHANNELS)
	{
		loaderMsgBox("Warning: Module contains >32 channels. The extra channels will be discarded!");
	}

	if (h.numInstr > MAX_INST)
		loaderMsgBox("Warning: Module contains >128 instruments. The extra instruments will be discarded!");

	if (instrHasMoreThan16Samples)
		loaderMsgBox("Warning: Module contains instrument(s) with >16 samples. The extra samples will be discarded!");

	return true;
}

static bool loadInstrHeader(FILE *f, uint16_t i, uint32_t filesize)
{
	uint32_t readSize;
	xmInsHdr_t ih;
	instr_t *ins;
	xmSmpHdr_t *src;
	sample_t *s;
	bool tf4Flag = false; // will be set after we read the instrument header

	memset(extraSampleDataLengths[i], 0, sizeof (extraSampleDataLengths[i]));
	memset(&ih, 0, sizeof (ih));

	const long headerStart = ftell(f);
	if (headerStart < 0 || !readExact(f, &readSize, sizeof (readSize)))
		return false;

	// yes, some XMs can have a header size of 0, and it usually means 263 bytes (INSTR_HEADER_SIZE)
	if (readSize == 0 || readSize > INSTR_HEADER_SIZE)
		readSize = INSTR_HEADER_SIZE;
	else if (readSize < 29)
	{
		loaderMsgBox("Error loading XM: This file is corrupt!");
		return false;
	}

	if (!seekTo(f, (uint64_t)headerStart, filesize) || !readExact(f, &ih, readSize))
		return false;
	if (ih.numSamples < 0 || ih.numSamples > 32 || (ih.numSamples > 0 && readSize < 33))
	{
		loaderMsgBox("Error loading XM: This file is corrupt (or not supported)!");
		return false;
	}

	// we can now safely pick up the stored Tunefish-flag
	tf4Flag = (ih.junk[0] != 0);

	/* Ensure we have an instrument structure allocated if it carries
	 * samples, or if it is a TF4 synth (zero-sample instrument with the flag).
	 */
	if ((ih.numSamples > 0 || tf4Flag) && instrTmp[i] == NULL)
	{
		if (!allocateTmpInstr(i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}
	}

	// FT2 bugfix: skip instrument header data if instrSize is above INSTR_HEADER_SIZE
	if (ih.instrSize > INSTR_HEADER_SIZE && !skipBytes(f, ih.instrSize-INSTR_HEADER_SIZE, filesize))
	{
		loaderMsgBox("Error loading XM: Truncated instrument header!");
		return false;
	}

	if (i <= MAX_INST) // copy over instrument names
		memcpy(songTmp.instrName[i], ih.name, 22);

	if (ih.numSamples > 0 && ih.numSamples <= 32)
	{
		if (ih.sampleSize < (int32_t)sizeof (xmSmpHdr_t))
		{
			loaderMsgBox("Error loading XM: Invalid sample header size!");
			return false;
		}

		// instrTmp[i] is guaranteed to be allocated at this point

		// copy instrument header elements to our instrument struct

		ins = instrTmp[i];
		memcpy(ins->note2SampleLUT, ih.note2SampleLUT, 96);
		memcpy(ins->volEnvPoints, ih.volEnvPoints, 12*2*sizeof(int16_t));
		memcpy(ins->panEnvPoints, ih.panEnvPoints, 12*2*sizeof(int16_t));
		ins->volEnvLength = ih.volEnvLength;
		ins->panEnvLength = ih.panEnvLength;
		ins->volEnvSustain = ih.volEnvSustain;
		ins->volEnvLoopStart = ih.volEnvLoopStart;
		ins->volEnvLoopEnd = ih.volEnvLoopEnd;
		ins->panEnvSustain = ih.panEnvSustain;
		ins->panEnvLoopStart = ih.panEnvLoopStart;
		ins->panEnvLoopEnd = ih.panEnvLoopEnd;
		ins->volEnvFlags = ih.volEnvFlags;
		ins->panEnvFlags = ih.panEnvFlags;
		ins->autoVibType = ih.vibType;
		ins->autoVibSweep = ih.vibSweep;
		ins->autoVibDepth = ih.vibDepth;
		ins->autoVibRate = ih.vibRate;
		ins->fadeout = ih.fadeout;
		ins->midiOn = (ih.midiOn == 1) ? true : false;
		ins->midiChannel = ih.midiChannel;
		ins->midiProgram = ih.midiProgram;
		ins->midiBend = ih.midiBend;
		ins->mute = (ih.mute == 1) ? true : false; // correct logic, don't change this!
		ins->useTF4 = tf4Flag;
		ins->numSamples = ih.numSamples; // used in loadInstrSample()

		int32_t sampleHeadersToRead = ih.numSamples;
		if (sampleHeadersToRead > MAX_SMP_PER_INST)
			sampleHeadersToRead = MAX_SMP_PER_INST;

		for (int32_t j = 0; j < ih.numSamples; j++)
		{
			xmSmpHdr_t sampleHeader;
			if (!readExact(f, &sampleHeader, sizeof (sampleHeader)) ||
				!skipBytes(f, (uint32_t)ih.sampleSize-sizeof (sampleHeader), filesize))
			{
				loaderMsgBox("General I/O error during loading!");
				return false;
			}

			if (j < sampleHeadersToRead)
				ih.smp[j] = sampleHeader;
			else
				extraSampleDataLengths[i][j-MAX_SMP_PER_INST] =
					(sampleHeader.nameLength == 0xAD && !(sampleHeader.flags & (SAMPLE_16BIT | SAMPLE_STEREO)))
					? (uint32_t)(16 + (((uint64_t)sampleHeader.length + 1) / 2)) : sampleHeader.length;
		}

		for (int32_t j = 0; j < sampleHeadersToRead; j++)
		{
			s = &instrTmp[i]->smp[j];
			src = &ih.smp[j];
			if (src->length > INT32_MAX || src->loopStart > src->length ||
				(uint64_t)src->loopStart + src->loopLength > src->length)
			{
				loaderMsgBox("Error loading XM: Invalid sample bounds!");
				return false;
			}

			// copy sample header elements to our sample struct

			s->length = src->length;
			s->loopStart = src->loopStart;
			s->loopLength = src->loopLength;
			s->volume = src->volume;
			s->finetune = src->finetune;
			s->flags = src->flags;
			s->panning = src->panning;
			s->relativeNote = src->relativeNote;

			/* If the sample is 8-bit mono and nameLength (reserved) is 0xAD,
			** then this is a 4-bit ADPCM compressed sample (ModPlug Tracker).
			*/
			if (src->nameLength == 0xAD && !(src->flags & (SAMPLE_16BIT | SAMPLE_STEREO)))
				s->flags |= SAMPLE_ADPCM;

			memcpy(s->name, src->name, 22);

			// dst->dataPtr is set up later
		}
	}
	else if (tf4Flag)
	{
		/* Zero-sample TF4 instrument – instrument structure has already been
		 * allocated above (if it was missing). Just mark the flag.
		 */
		instrTmp[i]->useTF4 = true;
	}

	return true;
}

static bool loadInstrSample(FILE *f, uint16_t i, uint32_t filesize)
{
	if (instrTmp[i] == NULL)
		return true; // empty instrument, let's just pretend it got loaded successfully

	uint16_t k = instrTmp[i]->numSamples;
	if (k > MAX_SMP_PER_INST)
		k = MAX_SMP_PER_INST;

	sample_t *s = instrTmp[i]->smp;

	if (i > MAX_INST) // insNum > 128, just skip sample data
	{
		for (uint16_t j = 0; j < k; j++, s++)
		{
			uint64_t encodedLength = (uint32_t)s->length;
			if (s->flags & SAMPLE_ADPCM)
				encodedLength = 16 + ((encodedLength + 1) / 2);
			if (encodedLength > 0 && !skipBytes(f, encodedLength, filesize))
				return false;
		}
	}
	else
	{
		for (uint16_t j = 0; j < k; j++, s++)
		{
			if (s->length <= 0)
			{
				s->length = 0;
				s->loopStart = 0;
				s->loopLength = 0;
				// Don't clear flags for DXM format - they will be restored later
				if (!isDXMFormat)
					s->flags = 0;
			}
			else
			{
				const int32_t lengthInFile = s->length;

				bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
				bool stereoSample = !!(s->flags & SAMPLE_STEREO);
				bool adpcmSample = !!(s->flags & SAMPLE_ADPCM); // ModPlug Tracker

				if (sample16Bit) // convert bytes to sample values
				{
					if ((s->length & 1) != 0 || (s->loopStart & 1) != 0 || (s->loopLength & 1) != 0)
						return false;
					s->length >>= 1;
					s->loopStart >>= 1;
					s->loopLength >>= 1;
				}
				if (stereoSample)
				{
					if ((s->length & 1) != 0 || (s->loopStart & 1) != 0 || (s->loopLength & 1) != 0)
						return false;
					s->length >>= 1;
					s->loopStart >>= 1;
					s->loopLength >>= 1;
				}

				if (s->length > MAX_SAMPLE_LEN)
					s->length = MAX_SAMPLE_LEN;

				const bool allocateAsStereo = stereoSample && isDXMFormat;
				
				if (!allocateSmpData(s, s->length, sample16Bit, allocateAsStereo))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				if (adpcmSample)
				{
					if (!loadADPCMSample(f, s)) return false;
					const size_t encodedBytesRead = ((size_t)s->length + 1) / 2;
					const size_t encodedBytesInFile = ((size_t)lengthInFile + 1) / 2;
					if (encodedBytesRead < encodedBytesInFile &&
						!skipBytes(f, encodedBytesInFile-encodedBytesRead, filesize))
						return false;
				}
				else
				{
					const size_t bytesPerSample = sample16Bit ? 2u : 1u;
					const size_t channelCount = stereoSample ? 2u : 1u;
					const size_t sampleLengthInBytes = (size_t)s->length * bytesPerSample * channelCount;
					if (sampleLengthInBytes > (size_t)lengthInFile)
						return false;
					
					if (stereoSample)
					{
						void *tempBuffer = malloc(sampleLengthInBytes);
						if (!tempBuffer)
						{
							loaderMsgBox("Not enough memory!");
							return false;
						}
						if (!readExact(f, tempBuffer, sampleLengthInBytes))
						{
							free(tempBuffer);
							return false;
						}

						decodeStereoSample(s, tempBuffer, sample16Bit, allocateAsStereo);
						free(tempBuffer);
						if (!allocateAsStereo) s->flags &= ~SAMPLE_STEREO;
					}
					else
					{
						if (!readExact(f, s->dataPtr, sampleLengthInBytes)) return false;
						delta2Samp(s->dataPtr, s->length, s->flags);
					}

					if (sampleLengthInBytes < (size_t)lengthInFile &&
						!skipBytes(f, (size_t)lengthInFile-sampleLengthInBytes, filesize))
						return false;
				}
			}


		}
	}

	// skip sample headers if we have more than 16 samples in instrument
	if (instrTmp[i]->numSamples > MAX_SMP_PER_INST)
	{
		const int32_t samplesToSkip = instrTmp[i]->numSamples-MAX_SMP_PER_INST;
		for (uint16_t extra = 0; extra < samplesToSkip; extra++)
		{
			if (extraSampleDataLengths[i][extra] > 0 &&
				!skipBytes(f, extraSampleDataLengths[i][extra], filesize))
				return false;
		}
	}

	return true;
}

static bool loadPatterns(FILE *f, uint16_t numPatterns, uint16_t xmVersion,
	uint16_t numChannels, uint32_t filesize)
{
	uint8_t tmpLen;
	xmPatHdr_t ph;

	bool pattLenWarn = false;
	for (uint16_t i = 0; i < numPatterns; i++)
	{
		if (!readExact(f, &ph.headerSize, sizeof (ph.headerSize)))
			goto pattCorrupt;

		if (!readExact(f, &ph.type, sizeof (ph.type)))
			goto pattCorrupt;
		if (ph.type != 0)
			goto pattCorrupt;

		ph.numRows = 0;
		if (xmVersion == 0x0102)
		{
			if (!readExact(f, &tmpLen, sizeof (tmpLen)))
				goto pattCorrupt;

			if (!readExact(f, &ph.dataSize, sizeof (ph.dataSize)))
				goto pattCorrupt;

			ph.numRows = tmpLen + 1; // +1 in v1.02

			if (ph.headerSize < 8 || !skipBytes(f, (uint32_t)ph.headerSize - 8, filesize))
				goto pattCorrupt;
		}
		else
		{
			if (!readExact(f, &ph.numRows, sizeof (ph.numRows)))
				goto pattCorrupt;

			if (!readExact(f, &ph.dataSize, sizeof (ph.dataSize)))
				goto pattCorrupt;

			if (ph.headerSize < 9 || !skipBytes(f, (uint32_t)ph.headerSize - 9, filesize))
				goto pattCorrupt;
		}

		if (ph.numRows <= 0)
			goto pattCorrupt;

		const uint16_t sourceRows = (uint16_t)ph.numRows;
		patternNumRowsTmp[i] = sourceRows;
		if (patternNumRowsTmp[i] > MAX_PATT_LEN)
		{
			patternNumRowsTmp[i] = MAX_PATT_LEN;
			pattLenWarn = true;
		}

		if (ph.dataSize > 0)
		{
			if (!allocateTmpPatt(i, patternNumRowsTmp[i]))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			if (!readExact(f, packedPattData, ph.dataSize))
				goto pattCorrupt;

			if (!unpackPatt(patternTmp[i], packedPattData, ph.dataSize,
				sourceRows, patternNumRowsTmp[i], numChannels))
				goto pattCorrupt;
			clearUnusedChannels(patternTmp[i], patternNumRowsTmp[i], songTmp.numChannels);
		}

		if (tmpPatternEmpty(i))
		{
			if (patternTmp[i] != NULL)
			{
				free(patternTmp[i]);
				patternTmp[i] = NULL;
			}

			patternNumRowsTmp[i] = 64;
		}
	}

	if (pattLenWarn)
		loaderMsgBox("This module contains pattern(s) with a length above 256! They will be truncated.");

	return true;

pattCorrupt:
	loaderMsgBox("Error loading XM: This file is corrupt!");
	return false;
}

static bool unpackPatt(note_t *dst, const uint8_t *src, size_t srcSize,
	uint16_t sourceRows, uint16_t outputRows, uint16_t numChannels)
{
	if (dst == NULL || src == NULL || numChannels == 0)
		return false;

	size_t pos = 0;
	for (uint16_t row = 0; row < sourceRows; row++)
	{
		for (uint16_t channel = 0; channel < numChannels; channel++)
		{
			if (pos >= srcSize)
				return false;

			note_t decoded = { 0 };
			const uint8_t descriptor = src[pos++];
			if (descriptor & 0x80)
			{
				const uint8_t fields = descriptor & 0x1F;
				uint8_t fieldBytes = 0;
				for (uint8_t mask = 1; mask <= 0x10; mask <<= 1)
					fieldBytes += !!(fields & mask);
				if (fieldBytes > srcSize - pos)
					return false;

				if (fields & 0x01) decoded.note = src[pos++];
				if (fields & 0x02) decoded.instr = src[pos++];
				if (fields & 0x04) decoded.vol = src[pos++];
				if (fields & 0x08) decoded.efx = src[pos++];
				if (fields & 0x10) decoded.efxData = src[pos++];
			}
			else
			{
				if (srcSize - pos < 4)
					return false;
				decoded.note = descriptor;
				decoded.instr = src[pos++];
				decoded.vol = src[pos++];
				decoded.efx = src[pos++];
				decoded.efxData = src[pos++];
			}

			if (row < outputRows && channel < MAX_CHANNELS)
				dst[(row * MAX_CHANNELS) + channel] = decoded;
		}
	}

	return true;
}

static bool loadADPCMSample(FILE *f, sample_t *s) // ModPlug Tracker
{
	int8_t deltaLUT[16];
	if (!readExact(f, deltaLUT, sizeof (deltaLUT)))
		return false;

	int8_t *dataPtr = s->dataPtr;
	const int32_t dataLength = (s->length + 1) / 2;

	int8_t currSample = 0;
	for (int32_t i = 0; i < dataLength; i++)
	{
		uint8_t nibbles;
		if (!readExact(f, &nibbles, sizeof (nibbles)))
			return false;

		currSample += deltaLUT[nibbles & 0x0F];
		*dataPtr++ = currSample;

		if ((i * 2) + 1 < s->length)
		{
			currSample += deltaLUT[nibbles >> 4];
			*dataPtr++ = currSample;
		}
	}

	return true;
}

#ifdef FT2_STABILITY_TESTS
bool runXMLoaderRegressionTests(void)
{
	note_t pattern[2 * MAX_CHANNELS] = { 0 };
	const uint8_t validPattern[] = { 0x9F, 48, 2, 0x30, 0x0F, 0x7D, 49, 3, 0x31, 0x0E, 0x22 };
	if (!unpackPatt(pattern, validPattern, sizeof (validPattern), 1, 1, 2) ||
		pattern[0].note != 48 || pattern[0].instr != 2 || pattern[0].efxData != 0x7D ||
		pattern[1].note != 49 || pattern[1].instr != 3 || pattern[1].efxData != 0x22)
		return false;

	const uint8_t truncatedPacked[] = { 0x9F, 48 };
	const uint8_t truncatedPlain[] = { 48, 2, 0x30, 0x0F };
	if (unpackPatt(pattern, truncatedPacked, sizeof (truncatedPacked), 1, 1, 1) ||
		unpackPatt(pattern, truncatedPlain, sizeof (truncatedPlain), 1, 1, 1))
		return false;

	uint8_t emptyWidePattern[33];
	memset(emptyWidePattern, 0x80, sizeof (emptyWidePattern));
	if (!unpackPatt(pattern, emptyWidePattern, sizeof (emptyWidePattern), 1, 1, 33))
		return false;
	uint32_t randomState = 0x584D5041;
	uint8_t fuzzPattern[64];
	for (int32_t iteration = 0; iteration < 256; iteration++)
	{
		for (size_t i = 0; i < sizeof (fuzzPattern); i++)
		{
			randomState = (randomState * 1664525) + 1013904223;
			fuzzPattern[i] = (uint8_t)(randomState >> 24);
		}
		memset(pattern, 0, sizeof (pattern));
		(void)unpackPatt(pattern, fuzzPattern, sizeof (fuzzPattern), 2, 2, 4);
	}

	const int8_t planarDeltas[6] = { 1, 1, 1, 10, 10, 10 };
	int8_t left[3] = { 0 }, right[3] = { 0 }, mono[3] = { 0 };
	sample_t stereoSample = { 0 };
	stereoSample.length = 3;
	stereoSample.dataPtrL = left;
	stereoSample.dataPtrR = right;
	decodeStereoSample(&stereoSample, planarDeltas, false, true);
	if (left[0] != 1 || left[1] != 2 || left[2] != 3 ||
		right[0] != 10 || right[1] != 20 || right[2] != 30)
		return false;
	stereoSample.dataPtrL = mono;
	stereoSample.dataPtrR = NULL;
	decodeStereoSample(&stereoSample, planarDeltas, false, false);
	if (mono[0] != 5 || mono[1] != 11 || mono[2] != 16)
		return false;

	FILE *f = tmpfile();
	if (f == NULL) return false;
	int8_t adpcmData[4] = { 0, 0, 0, 42 };
	int8_t deltaTable[16] = { 0 };
	deltaTable[1] = 1;
	const uint8_t nibbles[2] = { 0x11, 0x01 };
	sample_t sample = { 0 };
	sample.length = 3;
	sample.dataPtr = adpcmData;
	bool ok = fwrite(deltaTable, 1, sizeof (deltaTable), f) == sizeof (deltaTable) &&
		fwrite(nibbles, 1, sizeof (nibbles), f) == sizeof (nibbles) &&
		fseek(f, 0, SEEK_SET) == 0 && loadADPCMSample(f, &sample) &&
		adpcmData[0] == 1 && adpcmData[1] == 2 && adpcmData[2] == 3 && adpcmData[3] == 42;
	fclose(f);
	if (!ok) return false;

	f = tmpfile();
	if (f == NULL) return false;
	memset(adpcmData, 0, sizeof (adpcmData));
	ok = fwrite(deltaTable, 1, sizeof (deltaTable), f) == sizeof (deltaTable) &&
		fseek(f, 0, SEEK_SET) == 0 && !loadADPCMSample(f, &sample);
	fclose(f);
	return ok;
}
#endif
