/* (Lossy) Impulse Tracker module loader.
**
** It makes little sense to convert this format to XM, as it results
** in severe conversion losses. The reason I wrote this loader anyway,
** is so that you can import IT files to extract samples, pattern data
** and so on.
**
** Note: Data sanitation is done in the last stage
** of module loading, so you don't need to do that here.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../ft2_header.h"
#include "../ft2_module_loader.h"
#include "../ft2_sample_ed.h"
#include "../ft2_sysreqs.h"

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct itHdr_t
{
	char ID[4], songName[26];
	uint16_t rowHighlight, ordNum, insNum, smpNum, patNum, cwtv, cmwt, flags, special;
	uint8_t globalVol, mixingVol, speed, BPM, panSep, pitchWheelDepth;
	uint16_t msgLen;
	uint32_t msgOffs, reserved;
	uint8_t initialPans[64], initialVols[64];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itHdr_t;

typedef struct envNode_t
{
	int8_t magnitude;
	uint16_t tick;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
envNode_t;

typedef struct env_t
{
	uint8_t flags, num, loopBegin, loopEnd, sustainLoopBegin, sustainLoopEnd;
	envNode_t nodePoints[25];
	uint8_t reserved;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
env_t;

typedef struct itInsHdr_t
{
	char ID[4], dosFilename[12+1];
	uint8_t NNA, DCT, DCA;
	uint16_t fadeOut;
	uint8_t pitchPanSep, pitchPanCenter, globVol, defPan, randVol, randPan;
	uint16_t trackerVer;
	uint8_t numSamples, res1;
	char instrumentName[26];
	uint8_t filterCutoff, filterResonance, midiChn, midiProg;
	uint16_t midiBank;
	uint16_t smpNoteTable[120];
	env_t volEnv, panEnv, pitchEnv;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itInsHdr_t;

typedef struct itOldInsHdr_t
{
	char ID[4], dosFilename[12+1];
	uint8_t volEnvFlags, volEnvLoopBegin, volEnvLoopEnd, volEnvSusLoopBegin, volEnvSusLoopEnd;
	uint16_t res1, fadeOut;
	uint8_t NNA, DNC;
	uint16_t trackerVer;
	uint8_t numSamples, res2;
	char instrumentName[26];
	uint8_t res3[6];
	uint16_t smpNoteTable[120];
	uint8_t volEnv[200];
	uint16_t volEnvPoints[25];
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itOldInsHdr_t;

typedef struct itSmpHdr_t
{
	char ID[4], dosFilename[12+1];
	uint8_t globVol, flags, vol;
	char sampleName[26];
	uint8_t cvt, defPan;
	uint32_t length, loopBegin, loopEnd, c5Speed, sustainLoopBegin, sustainLoopEnd, offsetInFile;
	uint8_t autoVibratoSpeed, autoVibratoDepth, autoVibratoRate, autoVibratoWaveform;
}
#ifdef __GNUC__
__attribute__ ((packed))
#endif
itSmpHdr_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

static uint8_t decompBuffer[65536];
static uint8_t volPortaConv[9] = { 1, 4, 8, 16, 32, 64, 96, 128, 255 };

static bool readExact(FILE *f, void *dst, size_t bytes)
{
	return bytes == 0 || fread(dst, 1, bytes, f) == bytes;
}

static bool fileRangeValid(uint32_t offset, uint64_t length, uint32_t filesize)
{
	return offset <= filesize && length <= (uint64_t)filesize - offset;
}

static bool seekTo(FILE *f, uint32_t offset, uint32_t filesize)
{
	return offset <= filesize && fseek(f, (long)offset, SEEK_SET) == 0;
}

static bool decodeITPattern(const uint8_t *data, size_t dataSize, note_t *patt,
	uint16_t numRows, uint32_t *highestChannel)
{
	uint8_t lastMask[64] = { 0 };
	note_t lastNote[64] = { 0 };
	size_t pos = 0;
	uint16_t row = 0;

	while (pos < dataSize && row < numRows)
	{
		const uint8_t descriptor = data[pos++];
		if (descriptor == 0)
		{
			row++;
			continue;
		}

		const uint8_t encodedChannel = descriptor & 0x7F;
		if (encodedChannel == 0 || encodedChannel > 64)
			return false;
		const uint8_t ch = encodedChannel - 1;
		if (ch > *highestChannel)
			*highestChannel = ch;

		note_t discardedNote = { 0 };
		note_t *noteOut = ch >= MAX_CHANNELS ? &discardedNote : &patt[(row * MAX_CHANNELS) + ch];

		if (descriptor & 0x80)
		{
			if (pos >= dataSize) return false;
			lastMask[ch] = data[pos++];
		}

		const uint8_t mask = lastMask[ch];
		if (mask & 0x10) noteOut->note = lastNote[ch].note;
		if (mask & 0x20) noteOut->instr = lastNote[ch].instr;
		if (mask & 0x40) noteOut->vol = lastNote[ch].vol;
		if (mask & 0x80)
		{
			noteOut->efx = lastNote[ch].efx;
			noteOut->efxData = lastNote[ch].efxData;
		}

		if (mask & 1)
		{
			if (pos >= dataSize) return false;
			uint8_t note = data[pos++];
			if (note < 120)
			{
				note++;
				if (note < 12 || note >= 96+12)
					note = 0;
				else
					note -= 12;
			}
			else if (note != 254)
			{
				note = NOTE_OFF;
			}

			if (note > NOTE_OFF && note != 254)
				note = 0;
			noteOut->note = lastNote[ch].note = note;
		}

		if (mask & 2)
		{
			if (pos >= dataSize) return false;
			uint8_t instrument = data[pos++];
			if (instrument > MAX_INST) instrument = 0;
			noteOut->instr = lastNote[ch].instr = instrument;
		}

		if (mask & 4)
		{
			if (pos >= dataSize || data[pos] == UINT8_MAX) return false;
			noteOut->vol = lastNote[ch].vol = 1 + data[pos++];
		}

		if (mask & 8)
		{
			if (dataSize - pos < 2) return false;
			noteOut->efx = lastNote[ch].efx = data[pos++];
			noteOut->efxData = lastNote[ch].efxData = data[pos++];
		}
	}

	return row == numRows;
}

static bool loadCompressed16BitSample(FILE *f, sample_t *s, bool deltaEncoded);
static bool loadCompressed8BitSample(FILE *f, sample_t *s, bool deltaEncoded);
static void setAutoVibrato(instr_t *ins, itSmpHdr_t *itSmp);
static bool loadSample(FILE *f, sample_t *s, itSmpHdr_t *itSmp, uint32_t filesize);

bool loadIT(FILE *f, uint32_t filesize)
{
	uint32_t insOffs[256], smpOffs[256], patOffs[256];
	itSmpHdr_t *itSmp, smpHdrs[256] = { 0 };
	itHdr_t itHdr;

	if (filesize < sizeof (itHdr))
	{
		loaderMsgBox("This IT module is not supported or is corrupt!");
		goto error;
	}

	if (!readExact(f, &itHdr, sizeof (itHdr)) || memcmp(itHdr.ID, "IMPM", 4) != 0)
		goto error;

	if (itHdr.ordNum > 257 || itHdr.insNum > 256 || itHdr.smpNum > 256 || itHdr.patNum > 256)
	{
		loaderMsgBox("This IT module is not supported or is corrupt!");
		goto error;
	}

	tmpLinearPeriodsFlag = !!(itHdr.flags & 8);

	songTmp.pattNum = itHdr.patNum;
	songTmp.speed = itHdr.speed;
	songTmp.BPM = itHdr.BPM;

	memcpy(songTmp.name, itHdr.songName, 20);
	songTmp.name[20] = '\0';

	bool oldFormat = (itHdr.cmwt < 0x200);
	bool songUsesInstruments = !!(itHdr.flags & 4);
	bool oldEffects = !!(itHdr.flags & 16);
	bool compatGxx = !!(itHdr.flags & 32);

	uint8_t orderList[257];
	if (!readExact(f, orderList, itHdr.ordNum))
		goto error;

	// read order list
	for (uint16_t i = 0; i < itHdr.ordNum && i < MAX_ORDERS; i++)
	{
		const uint8_t patt = orderList[i];
		if (patt == 254) // separator ("+++"), skip it
			continue;

		if (patt == 255) // end of pattern list
			break;
		if (patt >= itHdr.patNum)
			goto error;

		songTmp.orders[songTmp.songLength] = patt;

		songTmp.songLength++;
		if (songTmp.songLength == MAX_ORDERS-1)
			break;
	}

	// read file pointers
	const uint64_t offsetTableBytes = ((uint64_t)itHdr.insNum + itHdr.smpNum + itHdr.patNum) * sizeof (uint32_t);
	const uint32_t offsetTablePos = (uint32_t)sizeof (itHdr) + itHdr.ordNum;
	if (!fileRangeValid(offsetTablePos, offsetTableBytes, filesize) ||
		!seekTo(f, offsetTablePos, filesize) ||
		!readExact(f, insOffs, (size_t)itHdr.insNum * sizeof (insOffs[0])) ||
		!readExact(f, smpOffs, (size_t)itHdr.smpNum * sizeof (smpOffs[0])) ||
		!readExact(f, patOffs, (size_t)itHdr.patNum * sizeof (patOffs[0])))
		goto error;

	for (int32_t i = 0; i < itHdr.smpNum; i++)
	{
		if (!fileRangeValid(smpOffs[i], sizeof (itSmpHdr_t), filesize) ||
			!seekTo(f, smpOffs[i], filesize) ||
			!readExact(f, &smpHdrs[i], sizeof (itSmpHdr_t)) ||
			memcmp(smpHdrs[i].ID, "IMPS", 4) != 0)
			goto error;
	}

	if (!songUsesInstruments) // read samples (as instruments)
	{
		int32_t numIns = MIN(itHdr.smpNum, MAX_INST);

		itSmp = smpHdrs;
		for (int16_t i = 0; i < numIns; i++, itSmp++)
		{
			if (!allocateTmpInstr(1 + i))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			instr_t *ins = instrTmp[1+i];
			sample_t *s = &ins->smp[0];

			memcpy(songTmp.instrName[1+i], itSmp->sampleName, 22);
			songTmp.instrName[1+i][22] = '\0';

			ins->numSamples = (itSmp->length > 0) ? 1 : 0;
			if (ins->numSamples > 0)
			{
				setAutoVibrato(ins, itSmp);

				if (!loadSample(f, s, itSmp, filesize))
				{
					loaderMsgBox("Not enough memory!");
					goto error;
				}
			}
		}
	}
	else if (oldFormat) // read instruments (old format version)
	{
		itOldInsHdr_t itIns;

		int32_t numIns = MIN(itHdr.insNum, MAX_INST);
		for (int16_t i = 0; i < numIns; i++)
		{
			if (!fileRangeValid(insOffs[i], sizeof (itIns), filesize) ||
				!seekTo(f, insOffs[i], filesize) || !readExact(f, &itIns, sizeof (itIns)) ||
				memcmp(itIns.ID, "IMPI", 4) != 0)
				goto error;

			if (!allocateTmpInstr(1 + i))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			instr_t *ins = instrTmp[1+i];

			memcpy(songTmp.instrName[1+i], itIns.instrumentName, 22);
			songTmp.instrName[1+i][22] = '\0';

			ins->fadeout = itIns.fadeOut * 64; // 0..64 -> 0..4096
			if (ins->fadeout > 4095)
				ins->fadeout = 4095;

			// find out what samples to load into this XM instrument header

			int16_t numSamples = 0;
			uint8_t sampleList[MAX_SMP_PER_INST];

			bool sampleAdded[256];
			memset(sampleList, 0, sizeof (sampleList));
			memset(sampleAdded, 0, sizeof (sampleAdded));

			for (int32_t j = 0; j < 96; j++)
			{
				uint8_t sample = itIns.smpNoteTable[12+j] >> 8;
				if (sample > itHdr.smpNum)
					goto error;
				if (sample > 0 && !sampleAdded[sample-1] && numSamples < MAX_SMP_PER_INST)
				{
					sampleAdded[sample-1] = true;
					sampleList[numSamples] = sample-1;
					numSamples++;
				}
			}

			/* If instrument only has one sample, copy over the sample's
			** auto-vibrato parameters to this instrument.
			*/
			bool singleSample = true;
			if (numSamples > 1)
			{
				uint8_t firstSample = sampleList[0];
				for (int32_t j = 1; j < numSamples; j++)
				{
					if (sampleList[j] != firstSample)
					{
						singleSample = false;
						break;
					}
				}
			}

			if (singleSample && numSamples == 1)
				setAutoVibrato(ins, &smpHdrs[sampleList[0]]);

			// create new note-to-sample table
			for (int32_t j = 0; j < 8*12; j++)
			{
				uint8_t inSmp = itIns.smpNoteTable[(1 * 12) + j] >> 8;

				uint8_t outSmp = 0;
				if (inSmp > 0)
				{
					inSmp--;
					for (; outSmp < numSamples; outSmp++)
					{
						if (inSmp == sampleList[outSmp])
							break;
					}

					if (outSmp >= numSamples)
						outSmp = 0;
				}

				ins->note2SampleLUT[j] = outSmp;
			}

			// load volume envelope
			if (itIns.volEnvFlags & 1)
			{
				bool volEnvLoopOn = !!(itIns.volEnvFlags & 2);
				bool volEnvSusOn = !!(itIns.volEnvFlags & 4);

				ins->volEnvFlags |= ENV_ENABLED;
				if (volEnvLoopOn) ins->volEnvFlags |= ENV_LOOP;
				if (volEnvSusOn) ins->volEnvFlags |= ENV_SUSTAIN;
				
				ins->volEnvLoopStart = MIN(itIns.volEnvLoopBegin, 11);
				ins->volEnvLoopEnd = MIN(itIns.volEnvLoopEnd, 11);
				ins->volEnvSustain = MIN(itIns.volEnvSusLoopEnd, 11);

				// hack: if sus loop only, set as normal loop + set sustain point
				if (!volEnvLoopOn && volEnvSusOn)
				{
					ins->volEnvLoopStart = MIN(itIns.volEnvSusLoopBegin, 11);
					ins->volEnvLoopEnd = MIN(itIns.volEnvSusLoopEnd, 11);
					ins->volEnvSustain = MIN(itIns.volEnvSusLoopEnd, 11);
					ins->volEnvFlags |= ENV_LOOP + ENV_SUSTAIN;
				}

				int32_t j = 0;
				for (; j < 12; j++)
				{
					if (itIns.volEnvPoints[j] >> 8 == 0xFF)
						break; // end of volume envelope

					ins->volEnvPoints[j][0] = itIns.volEnvPoints[j] & 0xFF;
					ins->volEnvPoints[j][1] = itIns.volEnvPoints[j] >> 8;
				}
				ins->volEnvLength = (uint8_t)j;

				// increase loop end point tick by one to better match IT style env looping
				if (ins->volEnvFlags & ENV_LOOP)
					ins->volEnvPoints[ins->volEnvLoopEnd][0]++;
			}

			ins->numSamples = numSamples;
			if (ins->numSamples > 0)
			{
				sample_t *s = ins->smp;
				for (int32_t j = 0; j < ins->numSamples; j++, s++)
				{
					if (!loadSample(f, s, &smpHdrs[sampleList[j]], filesize))
					{
						loaderMsgBox("Not enough memory!");
						goto error;
					}
				}
			}
		}
	}
	else // read instruments (later format version)
	{
		itInsHdr_t itIns;

		int32_t numIns = MIN(itHdr.insNum, MAX_INST);
		for (int16_t i = 0; i < numIns; i++)
		{
			if (!fileRangeValid(insOffs[i], sizeof (itIns), filesize) ||
				!seekTo(f, insOffs[i], filesize) || !readExact(f, &itIns, sizeof (itIns)) ||
				memcmp(itIns.ID, "IMPI", 4) != 0)
				goto error;

			if (!allocateTmpInstr(1 + i))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			instr_t *ins = instrTmp[1+i];

			memcpy(songTmp.instrName[1+i], itIns.instrumentName, 22);
			songTmp.instrName[1+i][22] = '\0';

			ins->fadeout = itIns.fadeOut * 32; // 0..128 -> 0..4096
			if (ins->fadeout > 4095)
				ins->fadeout = 4095;

			// find out what samples to load into this XM instrument header

			int16_t numSamples = 0;
			uint8_t sampleList[MAX_SMP_PER_INST];

			bool sampleAdded[256];
			memset(sampleList, 0, sizeof (sampleList));
			memset(sampleAdded, 0, sizeof (sampleAdded));

			for (int32_t j = 0; j < 96; j++)
			{
				uint8_t sample = itIns.smpNoteTable[12+j] >> 8;
				if (sample > itHdr.smpNum)
					goto error;
				if (sample > 0 && !sampleAdded[sample-1] && numSamples < MAX_SMP_PER_INST)
				{
					sampleAdded[sample-1] = true;
					sampleList[numSamples] = sample-1;
					numSamples++;
				}
			}

			/* If instrument only has one sample, copy over the sample's
			** auto-vibrato parameters to this instrument.
			*/
			bool singleSample = true;
			if (numSamples > 1)
			{
				uint8_t firstSample = sampleList[0];
				for (int32_t j = 1; j < numSamples; j++)
				{
					if (sampleList[j] != firstSample)
					{
						singleSample = false;
						break;
					}
				}
			}

			if (singleSample && numSamples == 1)
				setAutoVibrato(ins, &smpHdrs[sampleList[0]]);

			// create new note-to-sample table
			for (int32_t j = 0; j < 8*12; j++)
			{
				uint8_t inSmp = itIns.smpNoteTable[(1 * 12) + j] >> 8;

				uint8_t outSmp = 0;
				if (inSmp > 0)
				{
					inSmp--;
					for (; outSmp < numSamples; outSmp++)
					{
						if (inSmp == sampleList[outSmp])
							break;
					}

					if (outSmp >= numSamples)
						outSmp = 0;
				}

				ins->note2SampleLUT[j] = outSmp;
			}

			// load volume envelope
			env_t *volEnv = &itIns.volEnv;
			bool volEnvEnabled = !!(volEnv->flags & 1);
			if (volEnvEnabled && volEnv->num > 0)
			{
				bool volEnvLoopOn = !!(volEnv->flags & 2);
				bool volEnvSusOn = !!(volEnv->flags & 4);

				ins->volEnvFlags |= ENV_ENABLED;
				if (volEnvLoopOn) ins->volEnvFlags |= ENV_LOOP;
				if (volEnvSusOn) ins->volEnvFlags |= ENV_SUSTAIN;
				
				ins->volEnvLength = MIN(volEnv->num, 12);
				ins->volEnvLoopStart = MIN(volEnv->loopBegin, 11);
				ins->volEnvLoopEnd = MIN(volEnv->loopEnd, 11);
				ins->volEnvSustain = MIN(volEnv->sustainLoopEnd, 11);

				// hack: if sus loop only, set as normal loop + set sustain point
				if (!volEnvLoopOn && volEnvSusOn)
				{
					ins->volEnvLoopStart = MIN(volEnv->sustainLoopBegin, 11);
					ins->volEnvLoopEnd = MIN(volEnv->sustainLoopEnd, 11);
					ins->volEnvSustain = MIN(volEnv->sustainLoopEnd, 11);
					ins->volEnvFlags |= ENV_LOOP + ENV_SUSTAIN;
				}

				for (int32_t j = 0; j < ins->volEnvLength; j++)
				{
					ins->volEnvPoints[j][0] = volEnv->nodePoints[j].tick;
					ins->volEnvPoints[j][1] = volEnv->nodePoints[j].magnitude;
				}

				// increase loop end point tick by one to better match IT style env looping
				if (ins->volEnvFlags & ENV_LOOP)
					ins->volEnvPoints[ins->volEnvLoopEnd][0]++;
			}

			// load pan envelope
			env_t *panEnv = &itIns.panEnv;
			bool panEnvEnabled = !!(panEnv->flags & 1);
			if (panEnvEnabled && panEnv->num > 0)
			{
				bool panEnvLoopOn = !!(panEnv->flags & 2);
				bool panEnvSusOn = !!(panEnv->flags & 4);

				ins->panEnvFlags |= ENV_ENABLED;
				if (panEnvLoopOn) ins->panEnvFlags |= ENV_LOOP;
				if (panEnvSusOn) ins->panEnvFlags |= ENV_SUSTAIN;
				
				ins->panEnvLength = MIN(panEnv->num, 12);
				ins->panEnvLoopStart = MIN(panEnv->loopBegin, 11);
				ins->panEnvLoopEnd = MIN(panEnv->loopEnd, 11);
				ins->panEnvSustain = MIN(panEnv->sustainLoopEnd, 11);

				// hack: if sus loop only, set as normal loop + set sustain point
				if (!panEnvLoopOn && panEnvSusOn)
				{
					ins->panEnvLoopStart = MIN(panEnv->sustainLoopBegin, 11);
					ins->panEnvLoopEnd = MIN(panEnv->sustainLoopEnd, 11);
					ins->panEnvSustain = MIN(panEnv->sustainLoopEnd, 11);
					ins->panEnvFlags |= ENV_LOOP + ENV_SUSTAIN;
				}

				for (int32_t j = 0; j < ins->panEnvLength; j++)
				{
					ins->panEnvPoints[j][0] = panEnv->nodePoints[j].tick;
					ins->panEnvPoints[j][1] = panEnv->nodePoints[j].magnitude + 32;
				}

				// increase loop end point tick by one to better match IT style env looping
				if (ins->panEnvFlags & ENV_LOOP)
					ins->panEnvPoints[ins->panEnvLoopEnd][0] = panEnv->nodePoints[ins->panEnvLoopEnd].tick + 1;
			}

			ins->numSamples = numSamples;
			if (ins->numSamples > 0)
			{
				sample_t *s = ins->smp;
				for (int32_t j = 0; j < ins->numSamples; j++, s++)
				{
					if (!loadSample(f, s, &smpHdrs[sampleList[j]], filesize))
					{
						loaderMsgBox("Not enough memory!");
						goto error;
					}
				}
			}
		}
	}

	// load pattern data

	uint32_t numChannels = 0;
	for (int32_t i = 0; i < songTmp.pattNum; i++)
	{
		if (patOffs[i] == 0)
			continue;
		if (!fileRangeValid(patOffs[i], 8, filesize) || !seekTo(f, patOffs[i], filesize))
			goto error;

		uint16_t length, numRows;
		uint8_t reserved[4];
		if (!readExact(f, &length, sizeof (length)) || !readExact(f, &numRows, sizeof (numRows)) ||
			!readExact(f, reserved, sizeof (reserved)) ||
			!fileRangeValid(patOffs[i] + 8, length, filesize))
			goto error;

		if (numRows > MAX_PATT_LEN)
			goto error;
		if (numRows == 0)
			continue;
		if (length == 0)
			goto error;

		uint8_t *packedPattern = (uint8_t *)malloc(length);
		if (packedPattern == NULL)
		{
			loaderMsgBox("Not enough memory!");
			goto error;
		}
		if (!readExact(f, packedPattern, length))
		{
			free(packedPattern);
			goto error;
		}

		if (!allocateTmpPatt(i, numRows))
		{
			free(packedPattern);
			loaderMsgBox("Not enough memory!");
			goto error;
		}

		const bool decoded = decodeITPattern(packedPattern, length, patternTmp[i], numRows, &numChannels);
		free(packedPattern);
		if (!decoded)
			goto error;
	}
	numChannels++;

	songTmp.numChannels = MIN((numChannels + 1) & ~1, MAX_CHANNELS);

	// convert pattern data

	uint8_t lastInstr[MAX_CHANNELS], lastGInstr[MAX_CHANNELS];
	uint8_t lastDxy[MAX_CHANNELS], lastExy[MAX_CHANNELS], lastFxy[MAX_CHANNELS];
	uint8_t lastJxy[MAX_CHANNELS], lastKxy[MAX_CHANNELS], lastLxy[MAX_CHANNELS];
	uint8_t lastOxx[MAX_CHANNELS];

	memset(lastInstr, 0, sizeof (lastInstr));
	memset(lastGInstr, 0, sizeof (lastGInstr));
	memset(lastDxy, 0, sizeof (lastDxy));
	memset(lastExy, 0, sizeof (lastExy));
	memset(lastFxy, 0, sizeof (lastFxy));
	memset(lastJxy, 0, sizeof (lastJxy));
	memset(lastKxy, 0, sizeof (lastKxy));
	memset(lastLxy, 0, sizeof (lastLxy));
	memset(lastOxx, 0, sizeof (lastOxx));

	for (int32_t i = 0; i < songTmp.pattNum; i++)
	{
		note_t *p = patternTmp[i];
		if (p == NULL)
			continue;

		for (int32_t j = 0; j < patternNumRowsTmp[i]; j++)
		{
			for (int32_t ch = 0; ch < songTmp.numChannels; ch++, p++)
			{
				if (p->instr > 0)
					lastInstr[ch] = p->instr;

				// effect
				if (p->efx != 0)
				{
					const uint8_t itEfx = 'A' + (p->efx - 1);
					switch (itEfx)
					{
						case 'A': // set speed
						{
							if (p->efxData == 0) // A00 is ignored in IT
							{
								p->efx = p->efxData = 0;
							}
							else
							{
								p->efx = 0xF;
								if (p->efxData > 31)
									p->efxData = 31;
							}
						}
						break;

						case 'B': p->efx = 0xB; break; // position jump
						case 'C': p->efx = 0xD; break; // pattern break

						case 'D': // volume slide
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastDxy[ch] & 0x0F) == 0x0F || (lastDxy[ch] >> 4) == 0x0F;
								if (lastWasFineSlide)
									p->efxData = lastDxy[ch];
							}
							else
							{
								lastDxy[ch] = p->efxData;
							}

							if ((p->efxData & 0x0F) == 0x0F && (p->efxData >> 4) > 0)
							{
								p->efx = 0xE;
								p->efxData = 0xA0 + (p->efxData >> 4);
							}
							else if ((p->efxData >> 4) == 0x0F && (p->efxData & 0x0F) > 0)
							{
								p->efx = 0xE;
								p->efxData = 0xB0 + (p->efxData & 0x0F);
							}
							else
							{
								p->efx = 0xA;
							}
						}
						break;

						case 'E': // portamento down
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastExy[ch] & 0x0F) == 0x0F || (lastExy[ch] >> 4) == 0x0F;
								bool lastWasExtraFineSlide = (lastExy[ch] & 0x0F) == 0x0E || (lastExy[ch] >> 4) == 0x0E;

								if (lastWasFineSlide || lastWasExtraFineSlide)
									p->efxData = lastExy[ch];
							}
							else
							{
								lastExy[ch] = p->efxData;
							}

							if (p->efxData < 224)
							{
								p->efx = 0x2;
							}
							else if ((p->efxData >> 4) == 0x0E)
							{
								p->efx = 16 + ('X' - 'G');
								p->efxData = 0x20 + (p->efxData & 0x0F);
							}
							else if ((p->efxData >> 4) == 0x0F)
							{
								p->efx = 0xE;
								p->efxData = 0x20 + (p->efxData & 0x0F);
							}
						}
						break;

						case 'F': // portamento up
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastFxy[ch] & 0x0F) == 0x0F || (lastFxy[ch] >> 4) == 0x0F;
								bool lastWasExtraFineSlide = (lastFxy[ch] & 0x0F) == 0x0E || (lastFxy[ch] >> 4) == 0x0E;

								if (lastWasFineSlide || lastWasExtraFineSlide)
									p->efxData = lastFxy[ch];
							}
							else
							{
								lastFxy[ch] = p->efxData;
							}

							if (p->efxData < 224)
							{
								p->efx = 0x1;
							}
							else if ((p->efxData >> 4) == 0x0E)
							{
								p->efx = 16 + ('X' - 'G');
								p->efxData = 0x10 + (p->efxData & 0x0F);
							}
							else if ((p->efxData >> 4) == 0x0F)
							{
								p->efx = 0xE;
								p->efxData = 0x10 + (p->efxData & 0x0F);
							}
						}
						break;

						case 'G': // tone portamento
						{
							p->efx = 3;

							// remove illegal slides (this is not quite right, but good enough)
							if (!compatGxx && p->instr != 0 && p->instr != lastGInstr[ch])
								p->efx = p->efxData = 0;
						}
						break;

						case 'H': // vibrato
						{
							p->efx = 4;
							if (!oldEffects && p->efxData > 0)
								p->efxData = (p->efxData & 0xF0) | ((p->efxData & 0x0F) >> 1);
						}
						break;

						case 'I': // tremor
						{
							p->efx = 16 + ('T' - 'G');

							int8_t onTime = p->efxData >> 4;
							if (onTime > 0) // closer to IT2 (but still off)
								onTime--;

							int8_t offTime = p->efxData & 0x0F;
							if (offTime > 0) // ---
								offTime--;

							p->efxData = (onTime << 4) | offTime;
						}
						break;

						case 'J': // arpeggio
						{
							p->efx = 0;

							if (p->efxData != 0)
								p->efxData = lastJxy[ch] = (p->efxData >> 4) | (p->efxData << 4); // swap order (FT2 = reversed)
							else
								p->efxData = lastJxy[ch];
						}
						break;

						case 'K': // volume slide + vibrato
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastKxy[ch] & 0x0F) == 0x0F || (lastKxy[ch] >> 4) == 0x0F;
								if (lastWasFineSlide)
									p->efxData = lastKxy[ch];
							}
							else
							{
								lastKxy[ch] = p->efxData;
							}

							if ((p->efxData & 0x0F) == 0x0F && (p->efxData >> 4) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+203; // IT2 vibrato of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xA0 + (p->efxData >> 4);
							}
							else if ((p->efxData >> 4) == 0x0F && (p->efxData & 0x0F) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+203; // IT2 vibrato of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xB0 + (p->efxData & 0x0F);
							}
							else
							{
								p->efx = 0x6;
							}
						}
						break;

						case 'L': // volume slide + tone portamento
						{
							if (p->efxData == 0)
							{
								bool lastWasFineSlide = (lastLxy[ch] & 0x0F) == 0x0F || (lastLxy[ch] >> 4) == 0x0F;
								if (lastWasFineSlide)
									p->efxData = lastLxy[ch];
							}
							else
							{
								lastLxy[ch] = p->efxData;
							}

							if ((p->efxData & 0x0F) == 0x0F && (p->efxData >> 4) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+193; // IT2 tone portamento of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xA0 + (p->efxData >> 4);
							}
							else if ((p->efxData >> 4) == 0x0F && (p->efxData & 0x0F) > 0)
							{
								if (p->vol == 0)
									p->vol = 1+193; // IT2 tone portamento of param 0 (to be converted)

								p->efx = 0xE;
								p->efxData = 0xB0 + (p->efxData & 0x0F);
							}
							else
							{
								p->efx = 0x5;
							}
						}
						break;

						case 'O': // set sample offset
						{
							p->efx = 0x9;

							if (p->efxData > 0)
								lastOxx[ch] = p->efxData;

							// handle cases where the sample offset is after the end of the sample
							if (lastInstr[ch] > 0 && lastOxx[ch] > 0 && p->note > 0 && p->note <= 96)
							{
								instr_t *ins = instrTmp[lastInstr[ch]];
								if (ins != NULL)
								{
									const uint8_t sample = ins->note2SampleLUT[p->note-1];
									if (sample < MAX_SMP_PER_INST)
									{
										sample_t *s = &ins->smp[sample];
										if (s->length > 0)
										{
											const bool loopEnabled = (GET_LOOPTYPE(s->flags) != LOOP_DISABLED);
											const uint32_t sampleEnd = loopEnabled ? s->loopStart+s->loopLength : s->length;

											if (lastOxx[ch]*256UL >= sampleEnd)
											{
												if (oldEffects)
												{
													if (loopEnabled)
														p->efxData = (uint8_t)(sampleEnd >> 8);
												}
												else
												{
													p->efx = p->efxData = 0;
												}
											}
										}
									}
								}
							}
						}
						break;

						case 'P': // panning slide
						{
							p->efx = 16 + ('P' - 'G');

							if ((p->efxData >> 4) == 0)
							{
								uint8_t param = (((p->efxData & 0x0F) * 255) + 32) / 64;
								if (param > 15)
									param = 15;

								p->efxData = param << 4;
							}
							else if ((p->efxData & 0x0F) == 0)
							{
								uint8_t param = (((p->efxData >> 4) * 255) + 32) / 64;
								if (param > 15)
									param = 15;

								p->efxData = param;
							}
						}
						break;

						case 'Q': // note retrigger
						{
							p->efx = 16 + ('R' - 'G');

							if ((p->efxData & 0xF0) == 0x00)
								p->efxData |= 0x80;
						}
						break;

						case 'R': // tremolo
						{
							p->efx = 7;
							p->efxData = (p->efxData & 0xF0) | ((p->efxData & 0x0F) >> 1);
						}
						break;

						case 'S': // special effects
						{
							switch (p->efxData >> 4)
							{
								case 0x1: p->efx = 0xE3; break; // set glissando control

								case 0x3: // set vibrato waveform
								{
									if ((p->efxData & 0x0F) > 2)
										p->efx = p->efxData = 0;
									else
										p->efx = 0xE4;
								}
								break;

								case 0x4: // set tremolo waveform
								{
									if ((p->efxData & 0x0F) > 2)
										p->efx = p->efxData = 0;
									else
										p->efx = 0xE7;
								}
								break;

								case 0x8:
									p->efx = 0x08;
									p->efxData = (p->efxData << 4) | (p->efxData & 0x0F);
								break;

								case 0xB: p->efx = 0xE6; break; // pattern loop
								case 0xC: p->efx = 0xEC; break; // note cut
								case 0xD: p->efx = 0xED; break; // note delay
								case 0xE: p->efx = 0xEE; break; // pattern delay

								default:
									p->efx = p->efxData = 0;
								break;
							}
						}
						break;

						case 'T': // set tempo (BPM)
						{
							p->efx = 0xF;
							if (p->efxData < 32)
								p->efx = p->efxData = 0; // tempo slide is not supported
						}
						break;

						case 'V': // set global volume
						{
							p->efx = 16 + ('G' - 'G');
							p->efxData >>= 1; // IT2 g.vol. ranges 0..128, FT2 g.vol. ranges 0..64

							if (p->efxData > 64)
								p->efxData = 64;
						}
						break;

						case 'W': // global volume slide
						{
							p->efx = 16 + ('H' - 'G');

							// IT2 g.vol. ranges 0..128, FT2 g.vol. ranges 0..64
							if (p->efxData >> 4 == 0)
							{
								uint8_t param = p->efxData & 0x0F;
								if (param > 1)
									p->efxData = param >> 1;
							}
							else if ((p->efxData & 0x0F) == 0)
							{
								uint8_t param = p->efxData >> 4;
								if (param > 1)
									p->efxData = (param >> 1) << 4;
							}
						}
						break;

						case 'X': p->efx = 8; break; // set 8-bit panning

						default:
							p->efx = p->efxData = 0;
						break;
					}
				}
				else
				{
					p->efxData = 0;
				}

				if (p->instr != 0 && p->efx != 0x3)
					lastGInstr[ch] = p->instr;

				// volume column
				if (p->vol > 0)
				{
					p->vol--;
					if (p->vol <= 64) // set volume
					{
						p->vol += 0x10;
					}
					else if (p->vol <= 74) // fine volume slide up
					{
						p->vol = 0x90 + (p->vol - 65);
					}
					else if (p->vol <= 84) // fine volume slide down
					{
						p->vol = 0x80 + (p->vol - 75);
					}
					else if (p->vol <= 94) // volume slide up
					{
						p->vol = 0x70 + (p->vol - 85);
					}
					else if (p->vol <= 104) // volume slide down
					{
						p->vol = 0x60 + (p->vol - 95);
					}
					else if (p->vol <= 114) // pitch slide down
					{
						uint8_t param = p->vol - 105;
						p->vol = 0;

						if (p->efx == 0 && p->efxData == 0)
						{
							p->efx = 2;
							p->efxData = param * 4;
						}
					}
					else if (p->vol <= 124) // pitch slide up
					{
						uint8_t param = p->vol - 115;
						p->vol = 0;

						if (p->efx == 0 && p->efxData == 0)
						{
							p->efx = 1;
							p->efxData = param * 4;
						}
					}
					else if (p->vol <= 192) // set panning
					{
						p->vol = 0xC0 + (((p->vol - 128) * 15) / 64);
					}
					else if (p->vol >= 193 && p->vol <= 202) // portamento
					{
						uint8_t param = p->vol - 193;
					
						if (p->efx == 0 && p->efxData == 0)
						{
							p->vol = 0;

							p->efx = 3;
							p->efxData = (param == 0) ? 0 : volPortaConv[param-1];
						}
						else
						{
							p->vol = 0xF0 + param;
						}
					}
					else if (p->vol <= 212) // vibrato
					{
						p->vol = 0xB0 + (p->vol - 203);
					}
				}

				// note
				if (p->note == 254) // note cut
				{
					p->note = 0;
					if (p->efx == 0 && p->efxData == 0)
					{
						// EC0 (instant note cut)
						p->efx = 0xE;
						p->efxData = 0xC0;
					}
					else if (p->vol == 0)
					{
						// volume command vol 0
						p->vol = 0x10;
					}
				}
			}

			p += MAX_CHANNELS - songTmp.numChannels;
		}
	}

	// removing this message is considered a criminal act!!!
	loaderMsgBox("Loading of this format has severe issues. Don't use this for listening to .ITs!");

	return true;

error:
	return false;
}

typedef struct itBitReader_t
{
	const uint8_t *data;
	size_t size, bitPosition;
}
itBitReader_t;

static bool readBits(itBitReader_t *reader, uint8_t count, uint32_t *value)
{
	const size_t availableBits = reader->size * 8;
	if (count == 0 || count > 24 || count > availableBits ||
		reader->bitPosition > availableBits - count)
		return false;

	uint32_t result = 0;
	for (uint8_t bit = 0; bit < count; bit++)
	{
		const size_t sourceBit = reader->bitPosition + bit;
		result |= ((reader->data[sourceBit >> 3] >> (sourceBit & 7)) & 1u) << bit;
	}
	reader->bitPosition += count;
	*value = result;
	return true;
}

static int32_t signExtend(uint32_t value, uint8_t width)
{
	const uint32_t signBit = 1u << (width - 1);
	return (value & signBit) ? (int32_t)(value - (1u << width)) : (int32_t)value;
}

static bool decompress16BitData(int16_t *dst, uint32_t sampleCount,
	const uint8_t *src, size_t srcSize)
{
	itBitReader_t reader = { src, srcSize, 0 };
	uint16_t lastValue = 0;
	uint8_t bitDepth = 17;

	while (sampleCount > 0)
	{
		uint32_t value;
		if (!readBits(&reader, bitDepth, &value))
			return false;

		if (bitDepth <= 6 && value == (1u << (bitDepth - 1)))
		{
			uint32_t widthCode;
			if (!readBits(&reader, 4, &widthCode)) return false;
			uint8_t newDepth = 1 + (uint8_t)widthCode;
			if (newDepth >= bitDepth) newDepth++;
			if (newDepth == 0 || newDepth > 17) return false;
			bitDepth = newDepth;
			continue;
		}

		if (bitDepth <= 16)
		{
			if (bitDepth > 6)
			{
				const uint32_t border = (1u << (bitDepth - 1)) - 9;
				if (value > border && value <= border + 16)
				{
					uint8_t newDepth = (uint8_t)(value - border);
					if (newDepth >= bitDepth) newDepth++;
					if (newDepth == 0 || newDepth > 17) return false;
					bitDepth = newDepth;
					continue;
				}
			}

			lastValue += (uint16_t)signExtend(value, bitDepth);
		}
		else if (value & 0x10000)
		{
			const uint32_t newDepth = (value & 0xFFFF) + 1;
			if (newDepth == 0 || newDepth > 17) return false;
			bitDepth = (uint8_t)newDepth;
			continue;
		}
		else
		{
			lastValue += (uint16_t)value;
		}

		*dst++ = (int16_t)lastValue;
		sampleCount--;
	}

	return true;
}

static bool decompress8BitData(int8_t *dst, uint32_t sampleCount,
	const uint8_t *src, size_t srcSize)
{
	itBitReader_t reader = { src, srcSize, 0 };
	uint8_t lastValue = 0;
	uint8_t bitDepth = 9;

	while (sampleCount > 0)
	{
		uint32_t value;
		if (!readBits(&reader, bitDepth, &value))
			return false;

		if (bitDepth <= 6 && value == (1u << (bitDepth - 1)))
		{
			uint32_t widthCode;
			if (!readBits(&reader, 3, &widthCode)) return false;
			uint8_t newDepth = 1 + (uint8_t)widthCode;
			if (newDepth >= bitDepth) newDepth++;
			if (newDepth == 0 || newDepth > 9) return false;
			bitDepth = newDepth;
			continue;
		}

		if (bitDepth <= 8)
		{
			if (bitDepth > 6)
			{
				const uint32_t border = (0xFFu >> (9 - bitDepth)) - 4;
				if (value > border && value <= border + 8)
				{
					uint8_t newDepth = (uint8_t)(value - border);
					if (newDepth >= bitDepth) newDepth++;
					if (newDepth == 0 || newDepth > 9) return false;
					bitDepth = newDepth;
					continue;
				}
			}

			lastValue += (uint8_t)signExtend(value, bitDepth);
		}
		else if (value & 0x100)
		{
			const uint32_t newDepth = (value & 0xFF) + 1;
			if (newDepth == 0 || newDepth > 9) return false;
			bitDepth = (uint8_t)newDepth;
			continue;
		}
		else
		{
			lastValue += (uint8_t)value;
		}

		*dst++ = (int8_t)lastValue;
		sampleCount--;
	}

	return true;
}

static bool loadCompressed16BitSample(FILE *f, sample_t *s, bool deltaEncoded)
{
	int8_t *dstPtr = (int8_t *)s->dataPtr;

	uint32_t i = s->length * 2;
	while (i > 0)
	{
		uint32_t bytesToUnpack = 32768;
		if (bytesToUnpack > i)
			bytesToUnpack = i;

		uint16_t packedLen;
		if (!readExact(f, &packedLen, sizeof (packedLen)) || packedLen == 0 ||
			!readExact(f, decompBuffer, packedLen))
			return false;

		if (!decompress16BitData((int16_t *)dstPtr, bytesToUnpack >> 1, decompBuffer, packedLen))
			return false;

		if (deltaEncoded) // convert from delta values to PCM
		{
			int16_t *ptr16 = (int16_t *)dstPtr;
			int16_t lastSmp16 = 0; // yes, reset this every block!

			const uint32_t length = bytesToUnpack >> 1;
			for (uint32_t j = 0; j < length; j++)
			{
				lastSmp16 += ptr16[j];
				ptr16[j] = lastSmp16;
			}
		}

		dstPtr += bytesToUnpack;
		i -= bytesToUnpack;
	}

	return true;
}

static bool loadCompressed8BitSample(FILE *f, sample_t *s, bool deltaEncoded)
{
	int8_t *dstPtr = (int8_t *)s->dataPtr;

	uint32_t i = s->length;
	while (i > 0)
	{
		uint32_t bytesToUnpack = 32768;
		if (bytesToUnpack > i)
			bytesToUnpack = i;

		uint16_t packedLen;
		if (!readExact(f, &packedLen, sizeof (packedLen)) || packedLen == 0 ||
			!readExact(f, decompBuffer, packedLen))
			return false;

		if (!decompress8BitData(dstPtr, bytesToUnpack, decompBuffer, packedLen))
			return false;

		if (deltaEncoded) // convert from delta values to PCM
		{
			int8_t lastSmp8 = 0; // yes, reset this every block!
			for (uint32_t j = 0; j < bytesToUnpack; j++)
			{
				lastSmp8 += dstPtr[j];
				dstPtr[j] = lastSmp8;
			}
		}

		dstPtr += bytesToUnpack;
		i -= bytesToUnpack;
	}

	return true;
}

static void setAutoVibrato(instr_t *ins, itSmpHdr_t *itSmp)
{
	ins->autoVibType = itSmp->autoVibratoWaveform;
	if (ins->autoVibType > 3 || itSmp->autoVibratoRate == 0)
	{
		// turn off auto-vibrato
		ins->autoVibDepth = ins->autoVibRate = ins->autoVibSweep = ins->autoVibType = 0;
		return;
	}

	ins->autoVibRate = itSmp->autoVibratoSpeed;
	if (ins->autoVibRate > 63)
		ins->autoVibRate = 63;

	int32_t autoVibSweep = ((itSmp->autoVibratoDepth * 256) + 128) / itSmp->autoVibratoRate;
	if (autoVibSweep > 255)
		autoVibSweep = 255;
	ins->autoVibSweep = (uint8_t)autoVibSweep;

	ins->autoVibDepth = itSmp->autoVibratoDepth;
	if (ins->autoVibDepth > 15)
		ins->autoVibDepth = 15;
}

static bool loadSample(FILE *f, sample_t *s, itSmpHdr_t *itSmp, uint32_t filesize)
{
	bool sampleIs16Bit = !!(itSmp->flags & 2);
	bool compressed = !!(itSmp->flags & 8);
	bool hasLoop = !!(itSmp->flags & 16);
	bool bidiLoop = !!(itSmp->flags & 64);
	bool signedSamples = !!(itSmp->cvt & 1);
	bool deltaEncoded = !!(itSmp->cvt & 4);

	if (sampleIs16Bit)
		s->flags |= SAMPLE_16BIT;

	if (hasLoop)
		s->flags |= bidiLoop ? LOOP_BIDI : LOOP_FWD;

	if (itSmp->length > MAX_SAMPLE_LEN || itSmp->loopBegin > itSmp->length ||
		itSmp->loopEnd < itSmp->loopBegin || itSmp->loopEnd > itSmp->length)
		return false;

	s->length = (int32_t)itSmp->length;
	s->loopStart = (int32_t)itSmp->loopBegin;
	s->loopLength = (int32_t)(itSmp->loopEnd - itSmp->loopBegin);
	s->volume = itSmp->vol;

	s->panning = 128;
	if (itSmp->defPan & 128) // use panning?
	{
		int32_t pan = (itSmp->defPan & 127) * 4; // 0..64 -> 0..256
		if (pan > 255)
			pan = 255;

		s->panning = (uint8_t)pan;
	}

	memcpy(s->name, itSmp->sampleName, 22);
	s->name[22] = '\0';

	setSampleC4Hz(s, itSmp->c5Speed);

	if (s->length == 0)
		return true; // empty sample, skip data loading
	if (itSmp->offsetInFile == 0)
		return false;

	const uint64_t sampleBytes = (uint64_t)s->length << sampleIs16Bit;
	if ((!compressed && !fileRangeValid(itSmp->offsetInFile, sampleBytes, filesize)) ||
		(compressed && !fileRangeValid(itSmp->offsetInFile, sizeof (uint16_t), filesize)))
		return false;

	if (!allocateSmpData(s, s->length, sampleIs16Bit, false))
		return false;

	// begin sample loading

	if (!seekTo(f, itSmp->offsetInFile, filesize))
		return false;

	if (compressed)
	{
		if (sampleIs16Bit)
		{
			if (!loadCompressed16BitSample(f, s, deltaEncoded)) return false;
		}
		else if (!loadCompressed8BitSample(f, s, deltaEncoded))
		{
			return false;
		}
	}
	else
	{
		if (!readExact(f, s->dataPtr, (size_t)sampleBytes))
			return false;

		if (!signedSamples)
		{
			if (sampleIs16Bit)
			{
				int16_t *ptr16 = (int16_t *)s->dataPtr;
				for (int32_t i = 0; i < s->length; i++)
					ptr16[i] ^= 0x8000;
			}
			else
			{
				int8_t *ptr8 = (int8_t *)s->dataPtr;
				for (int32_t i = 0; i < s->length; i++)
					ptr8[i] ^= 0x80;
			}
		}
	}

	return true;
}

#ifdef FT2_STABILITY_TESTS
bool runITLoaderRegressionTests(void)
{
	note_t pattern[MAX_CHANNELS] = { 0 };
	uint32_t highestChannel = 0;
	const uint8_t validPattern[] = { 0x81, 0x03, 59, 2, 0 };
	if (!decodeITPattern(validPattern, sizeof (validPattern), pattern, 1, &highestChannel) ||
		pattern[0].note != 48 || pattern[0].instr != 2 || highestChannel != 0)
		return false;

	const uint8_t truncatedPattern[] = { 0x81, 0x03, 59 };
	const uint8_t invalidChannel[] = { 0xC1, 0, 0 };
	if (decodeITPattern(truncatedPattern, sizeof (truncatedPattern), pattern, 1, &highestChannel) ||
		decodeITPattern(invalidChannel, sizeof (invalidChannel), pattern, 1, &highestChannel))
		return false;

	int8_t sample8 = 1;
	const uint8_t zero8[] = { 0, 0 };
	const uint8_t invalidWidth8[] = { 0xFF, 0x01 };
	const uint8_t widthChange8[] = { 0x07, 0x03, 0x00 }; // 9-bit width marker, then an 8-bit +1 delta
	if (!decompress8BitData(&sample8, 1, zero8, sizeof (zero8)) || sample8 != 0 ||
		decompress8BitData(&sample8, 1, zero8, 1) ||
		decompress8BitData(&sample8, 1, invalidWidth8, sizeof (invalidWidth8)) ||
		!decompress8BitData(&sample8, 1, widthChange8, sizeof (widthChange8)) || sample8 != 1)
		return false;

	int16_t sample16 = 1;
	const uint8_t zero16[] = { 0, 0, 0 };
	const uint8_t widthChange16[] = { 0x0F, 0x00, 0x03, 0x00, 0x00 }; // 17-bit marker, then 16-bit +1
	if (!decompress16BitData(&sample16, 1, zero16, sizeof (zero16)) || sample16 != 0 ||
		decompress16BitData(&sample16, 1, zero16, 2) ||
		!decompress16BitData(&sample16, 1, widthChange16, sizeof (widthChange16)) || sample16 != 1)
		return false;

	uint32_t randomState = 0x49544C44;
	uint8_t fuzzData[8];
	for (int32_t iteration = 0; iteration < 256; iteration++)
	{
		for (size_t i = 0; i < sizeof (fuzzData); i++)
		{
			randomState = (randomState * 1664525) + 1013904223;
			fuzzData[i] = (uint8_t)(randomState >> 24);
		}

		note_t fuzzPattern[2 * MAX_CHANNELS] = { 0 };
		int8_t fuzzSample8[4] = { 0 };
		int16_t fuzzSample16[4] = { 0 };
		highestChannel = 0;
		(void)decodeITPattern(fuzzData, sizeof (fuzzData), fuzzPattern, 2, &highestChannel);
		(void)decompress8BitData(fuzzSample8, 4, fuzzData, sizeof (fuzzData));
		(void)decompress16BitData(fuzzSample16, 4, fuzzData, sizeof (fuzzData));
	}

	return true;
}
#endif
