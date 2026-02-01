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
static uint32_t extraSampleLengths[32-MAX_SMP_PER_INST];

static bool loadInstrHeader(FILE *f, uint16_t i);
static bool loadInstrSample(FILE *f, uint16_t i);
static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t antChn);
static bool loadPatterns(FILE *f, uint16_t antPtn, uint16_t xmVersion);
static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t antChn);
static void loadADPCMSample(FILE *f, sample_t *s); // ModPlug Tracker

bool loadXM(FILE *f, uint32_t filesize)
{
	xmHdr_t h;

	if (filesize < sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (fread(&h, 1, sizeof (h), f) != sizeof (h))
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

	if (h.numInstr > 256) // if >128 instruments, we fake-load up to 128 extra instruments and discard them
	{
		loaderMsgBox("Error loading XM: This file is corrupt.");
		return false;
	}

	fseek(f, 60 + h.headerSize, SEEK_SET);
	if (filesize != 336 && feof(f)) // 336 in length at this point = empty XM
	{
		loaderMsgBox("Error loading XM: The module is empty!");
		return false;
	}

	memcpy(songTmp.name, h.name, 20);
	songTmp.name[20] = '\0';

	songTmp.songLength = h.numOrders;
	songTmp.songLoopStart = h.songLoopStart;
	songTmp.numChannels = (uint8_t)h.numChannels;
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

	if (h.version < 0x0104)
	{
		// XM v1.02 and XM v1.03

		for (uint16_t i = 1; i <= h.numInstr; i++)
		{
			if (!loadInstrHeader(f, i))
				return false;
		}

		if (!loadPatterns(f, h.numPatterns, h.version))
			return false;

		for (uint16_t i = 1; i <= h.numInstr; i++)
		{
			if (!loadInstrSample(f, i))
				return false;
		}
	}
	else
	{
		// XM v1.04 (latest version)

		if (!loadPatterns(f, h.numPatterns, h.version))
			return false;

		for (uint16_t i = 1; i <= h.numInstr; i++)
		{
			if (!loadInstrHeader(f, i))
				return false;

			if (!loadInstrSample(f, i))
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

	if (songTmp.numChannels > MAX_CHANNELS)
	{
		songTmp.numChannels = MAX_CHANNELS;
		loaderMsgBox("Warning: Module contains >32 channels. The extra channels will be discarded!");
	}

	if (h.numInstr > MAX_INST)
		loaderMsgBox("Warning: Module contains >128 instruments. The extra instruments will be discarded!");

	if (instrHasMoreThan16Samples)
		loaderMsgBox("Warning: Module contains instrument(s) with >16 samples. The extra samples will be discarded!");

	return true;
}

static bool loadInstrHeader(FILE *f, uint16_t i)
{
	uint32_t readSize;
	xmInsHdr_t ih;
	instr_t *ins;
	xmSmpHdr_t *src;
	sample_t *s;
	bool tf4Flag = false; // will be set after we read the instrument header

	memset(extraSampleLengths, 0, sizeof (extraSampleLengths));
	memset(&ih, 0, sizeof (ih));

	fread(&readSize, 4, 1, f);
	fseek(f, -4, SEEK_CUR);

	// yes, some XMs can have a header size of 0, and it usually means 263 bytes (INSTR_HEADER_SIZE)
	if (readSize == 0 || readSize > INSTR_HEADER_SIZE)
		readSize = INSTR_HEADER_SIZE;

	if ((int32_t)readSize < 0)
	{
		loaderMsgBox("Error loading XM: This file is corrupt!");
		return false;
	}

	fread(&ih, readSize, 1, f); // read instrument header

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
	if (ih.instrSize > INSTR_HEADER_SIZE)
		fseek(f, ih.instrSize-INSTR_HEADER_SIZE, SEEK_CUR);

	if (ih.numSamples < 0 || ih.numSamples > 32)
	{
		loaderMsgBox("Error loading XM: This file is corrupt (or not supported)!");
		return false;
	}

	if (i <= MAX_INST) // copy over instrument names
		memcpy(songTmp.instrName[i], ih.name, 22);

	if (ih.numSamples > 0 && ih.numSamples <= 32)
	{
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

		if (fread(ih.smp, sampleHeadersToRead * sizeof (xmSmpHdr_t), 1, f) != 1)
		{
			loaderMsgBox("General I/O error during loading!");
			return false;
		}

		// if instrument contains more than 16 sample headers (unsupported), skip them
		if (ih.numSamples > MAX_SMP_PER_INST) // can only be 0..32 at this point
		{
			const int32_t samplesToSkip = ih.numSamples-MAX_SMP_PER_INST;
			for (int32_t j = 0; j < samplesToSkip; j++)
			{
				fread(&extraSampleLengths[j], 4, 1, f); // used for skipping data in loadInstrSample()
				fseek(f, sizeof (xmSmpHdr_t)-4, SEEK_CUR);
			}
		}

		for (int32_t j = 0; j < sampleHeadersToRead; j++)
		{
			s = &instrTmp[i]->smp[j];
			src = &ih.smp[j];

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

static bool loadInstrSample(FILE *f, uint16_t i)
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
			if (s->length > 0)
				fseek(f, s->length, SEEK_CUR);
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

				if (sample16Bit) // we use units of samples (not bytes like in FT2)
				{
					s->length >>= 1;
					s->loopStart >>= 1;
					s->loopLength >>= 1;
				}

				if (s->length > MAX_SAMPLE_LEN)
					s->length = MAX_SAMPLE_LEN;

				// Always allocate stereo samples properly (DXM is primary format)
				bool allocateAsStereo = stereoSample;
				
				if (!allocateSmpData(s, s->length, sample16Bit, allocateAsStereo))
				{
					loaderMsgBox("Not enough memory!");
					return false;
				}

				if (adpcmSample)
				{
					loadADPCMSample(f, s);
				}
				else
				{
					const int32_t sampleLengthInBytes = SAMPLE_LENGTH_BYTES(s);
					
					// XM import: Convert stereo samples to mono for compatibility
					// Stereo samples are only supported in DXM format
					// But preserve stereo samples when loading DXM files
					if (stereoSample && !isDXMFormat)
					{
						// For XM import, convert stereo to mono by reading interleaved data
						// and mixing L+R channels to mono
						int32_t bytesPerSample = sample16Bit ? 2 : 1;
						int32_t totalSamples = s->length;
						int32_t stereoBytes = totalSamples * bytesPerSample;
						
						// Read interleaved stereo data into temporary buffer
						void *tempBuffer = malloc(stereoBytes);
						if (!tempBuffer)
						{
							loaderMsgBox("Not enough memory!");
							return false;
						}
						
						fread(tempBuffer, 1, stereoBytes, f);
						
						// Convert interleaved stereo to mono by mixing L+R
						if (sample16Bit)
						{
							int16_t *temp16 = (int16_t *)tempBuffer;
							int16_t *dst = (int16_t *)s->dataPtrL;
							int32_t numStereoSamples = totalSamples / 2;
							for (int32_t i = 0; i < numStereoSamples; i++)
							{
								// Mix L+R channels to mono
								dst[i] = (temp16[i * 2] + temp16[i * 2 + 1]) / 2;
							}
						}
						else
						{
							int8_t *temp8 = (int8_t *)tempBuffer;
							int8_t *dst = (int8_t *)s->dataPtrL;
							int32_t numStereoSamples = totalSamples / 2;
							for (int32_t i = 0; i < numStereoSamples; i++)
							{
								// Mix L+R channels to mono
								dst[i] = (temp8[i * 2] + temp8[i * 2 + 1]) / 2;
							}
						}
						
						free(tempBuffer);
						
						// Update sample length and remove stereo flag
						s->length = totalSamples / 2;
						s->flags &= ~SAMPLE_STEREO; // Remove stereo flag for XM compatibility
						
						// Delta decode mono sample
						delta2Samp(s->dataPtrL, s->length, s->flags);
					}
					else if (stereoSample && isDXMFormat)
					{
						// Stereo sample for DXM format - read stereo data
						int32_t bytesPerSample = sample16Bit ? 2 : 1;
						int32_t totalBytes = s->length * bytesPerSample;
						
						// Read interleaved stereo data
						void *tempBuffer = malloc(totalBytes * 2);
						if (!tempBuffer)
						{
							loaderMsgBox("Not enough memory!");
							return false;
						}
						
						fread(tempBuffer, 1, totalBytes * 2, f);
						
						// Deinterleave to planar format
						if (sample16Bit)
						{
							int16_t *temp16 = (int16_t *)tempBuffer;
							int16_t *dstL = (int16_t *)s->dataPtrL;
							int16_t *dstR = (int16_t *)s->dataPtrR;
							for (int32_t i = 0; i < s->length; i++)
							{
								dstL[i] = temp16[i * 2];     // L channel
								dstR[i] = temp16[i * 2 + 1]; // R channel
							}
						}
						else
						{
							int8_t *temp8 = (int8_t *)tempBuffer;
							int8_t *dstL = (int8_t *)s->dataPtrL;
							int8_t *dstR = (int8_t *)s->dataPtrR;
							for (int32_t i = 0; i < s->length; i++)
							{
								dstL[i] = temp8[i * 2];     // L channel
								dstR[i] = temp8[i * 2 + 1]; // R channel
							}
						}
						
						free(tempBuffer);
						
						// Note: No delta decoding needed for DXM stereo samples
						// The DXM loader handles the sample data correctly
					}
					else
					{
						// Mono sample - read as normal
						fread(s->dataPtr, 1, sampleLengthInBytes, f);
						delta2Samp(s->dataPtr, s->length, s->flags);
					}

					if (sampleLengthInBytes < lengthInFile)
						fseek(f, lengthInFile-sampleLengthInBytes, SEEK_CUR);
				}
			}


		}
	}

	// skip sample headers if we have more than 16 samples in instrument
	if (instrTmp[i]->numSamples > MAX_SMP_PER_INST)
	{
		const int32_t samplesToSkip = instrTmp[i]->numSamples-MAX_SMP_PER_INST;
		for (i = 0; i < samplesToSkip; i++)
		{
			if (extraSampleLengths[i] > 0)
				fseek(f, extraSampleLengths[i], SEEK_CUR); 
		}
	}

	return true;
}

static bool loadPatterns(FILE *f, uint16_t antPtn, uint16_t xmVersion)
{
	uint8_t tmpLen;
	xmPatHdr_t ph;

	bool pattLenWarn = false;
	for (uint16_t i = 0; i < antPtn; i++)
	{
		if (fread(&ph.headerSize, 4, 1, f) != 1)
			goto pattCorrupt;

		if (fread(&ph.type, 1, 1, f) != 1)
			goto pattCorrupt;

		ph.numRows = 0;
		if (xmVersion == 0x0102)
		{
			if (fread(&tmpLen, 1, 1, f) != 1)
				goto pattCorrupt;

			if (fread(&ph.dataSize, 2, 1, f) != 1)
				goto pattCorrupt;

			ph.numRows = tmpLen + 1; // +1 in v1.02

			if (ph.headerSize > 8)
				fseek(f, ph.headerSize - 8, SEEK_CUR);
		}
		else
		{
			if (fread(&ph.numRows, 2, 1, f) != 1)
				goto pattCorrupt;

			if (fread(&ph.dataSize, 2, 1, f) != 1)
				goto pattCorrupt;

			if (ph.headerSize > 9)
				fseek(f, ph.headerSize - 9, SEEK_CUR);
		}

		if (feof(f))
			goto pattCorrupt;

		patternNumRowsTmp[i] = ph.numRows;
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

			if (fread(packedPattData, 1, ph.dataSize, f) != ph.dataSize)
				goto pattCorrupt;

			unpackPatt((uint8_t *)patternTmp[i], packedPattData, patternNumRowsTmp[i], songTmp.numChannels);
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

static void unpackPatt(uint8_t *dst, uint8_t *src, uint16_t len, int32_t antChn)
{
	int32_t j;

	if (dst == NULL)
		return;

	const int32_t srcEnd = len * (sizeof (note_t) * antChn);
	int32_t srcIdx = 0;

	int32_t numChannels = antChn;
	if (numChannels > MAX_CHANNELS)
		numChannels = MAX_CHANNELS;

	const int32_t pitch = sizeof (note_t) * (MAX_CHANNELS - antChn);
	for (int32_t i = 0; i < len; i++)
	{
		for (j = 0; j < numChannels; j++)
		{
			if (srcIdx >= srcEnd)
				return; // error!

			const uint8_t note = *src++;
			if (note & 0x80)
			{
				*dst++ = (note & 0x01) ? *src++ : 0;
				*dst++ = (note & 0x02) ? *src++ : 0;
				*dst++ = (note & 0x04) ? *src++ : 0;
				*dst++ = (note & 0x08) ? *src++ : 0;
				*dst++ = (note & 0x10) ? *src++ : 0;
			}
			else
			{
				*dst++ = note;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
				*dst++ = *src++;
			}

			srcIdx += sizeof (note_t);
		}

		// if more than 32 channels, skip rest of the channels for this row
		for (; j < antChn; j++)
		{
			if (srcIdx >= srcEnd)
				return; // error!

			const uint8_t note = *src++;
			if (note & 0x80)
			{
				if (note & 0x01) src++;
				if (note & 0x02) src++;
				if (note & 0x04) src++;
				if (note & 0x08) src++;
				if (note & 0x10) src++;
			}
			else
			{
				src++;
				src++;
				src++;
				src++;
			}

			srcIdx += sizeof (note_t);
		}

		// if song has <32 channels, align pointer to next row (skip unused channels)
		if (antChn < MAX_CHANNELS)
			dst += pitch;
	}
}

static void loadADPCMSample(FILE *f, sample_t *s) // ModPlug Tracker
{
	int8_t deltaLUT[16];
	fread(deltaLUT, 1, 16, f);

	int8_t *dataPtr = s->dataPtr;
	const int32_t dataLength = (s->length + 1) / 2;

	int8_t currSample = 0;
	for (int32_t i = 0; i < dataLength; i++)
	{
		const uint8_t nibbles = (uint8_t)fgetc(f);

		currSample += deltaLUT[nibbles & 0x0F];
		*dataPtr++ = currSample;

		currSample += deltaLUT[nibbles >> 4];
		*dataPtr++ = currSample;
	}
}
