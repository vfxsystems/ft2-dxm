/* BEM (UN05, MikMod) loader. Supports modules converted from XM only!
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

#define MAX_TRACKS (256*32)

#define FLAG_XMPERIODS 1
#define FLAG_LINEARSLIDES 2

#ifdef _MSC_VER // please don't mess with these structs!
#pragma pack(push)
#pragma pack(1)
#endif
typedef struct bemHdr_t
{
	char id[4];
	uint8_t numchn;
	uint16_t numpos;
	uint16_t reppos;
	uint16_t numpat;
	uint16_t numtrk;
	uint16_t numins;
	uint8_t initspeed;
	uint8_t inittempo;
	uint8_t positions[256];
	uint8_t panning[32];
	uint8_t flags;
}
#ifdef __GNUC__
__attribute__((packed))
#endif
bemHdr_t;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

enum
{
	UNI_NOTE = 1,
	UNI_INSTRUMENT,
	UNI_PTEFFECT0,
	UNI_PTEFFECT1,
	UNI_PTEFFECT2,
	UNI_PTEFFECT3,
	UNI_PTEFFECT4,
	UNI_PTEFFECT5,
	UNI_PTEFFECT6,
	UNI_PTEFFECT7,
	UNI_PTEFFECT8,
	UNI_PTEFFECT9,
	UNI_PTEFFECTA,
	UNI_PTEFFECTB,
	UNI_PTEFFECTC,
	UNI_PTEFFECTD,
	UNI_PTEFFECTE,
	UNI_PTEFFECTF,
	UNI_S3MEFFECTA,
	UNI_S3MEFFECTD,
	UNI_S3MEFFECTE,
	UNI_S3MEFFECTF,
	UNI_S3MEFFECTI,
	UNI_S3MEFFECTQ,
	UNI_S3MEFFECTT,
	UNI_XMEFFECTA,
	UNI_XMEFFECTG,
	UNI_XMEFFECTH,
	UNI_XMEFFECTP,

	UNI_LAST
};

static const uint8_t xmEfxTab[] = { 10, 16, 17, 25 }; // A, G, H, P

static bool readExact(FILE *f, void *dst, size_t bytes)
{
	return bytes == 0 || fread(dst, 1, bytes, f) == bytes;
}

static bool readU8(FILE *f, uint8_t *value)
{
	const int c = fgetc(f);
	if (c == EOF) return false;
	*value = (uint8_t)c;
	return true;
}

static bool decodeBEMTrack(const uint8_t *data, size_t dataSize, note_t *out, uint16_t numRows)
{
	size_t pos = 0;
	uint16_t outRow = 0;

	while (pos < dataSize)
	{
		const uint8_t descriptor = data[pos++];
		if (descriptor == 0)
			return true;

		const uint8_t encodedSize = descriptor & 0x1F;
		if (encodedSize == 0)
			return false;

		const size_t opcodeBytes = encodedSize - 1;
		if (opcodeBytes > dataSize - pos)
			return false;

		const uint8_t repeatCount = 1 + (descriptor >> 5);
		if (repeatCount > numRows - outRow)
			return false;

		for (uint8_t repeat = 0; repeat < repeatCount; repeat++)
		{
			note_t *note = &out[outRow++];
			size_t opcodePos = pos;
			const size_t opcodeEnd = pos + opcodeBytes;

			while (opcodePos < opcodeEnd)
			{
				const uint8_t opcode = data[opcodePos++];
				if (opcode == 0)
					break;
				if (opcode >= UNI_LAST || opcodePos >= opcodeEnd)
					return false;

				const uint8_t value = data[opcodePos++];
				if (opcode == UNI_NOTE)
				{
					if (value == UINT8_MAX) return false;
					note->note = 1 + value;
				}
				else if (opcode == UNI_INSTRUMENT)
				{
					if (value == UINT8_MAX) return false;
					note->instr = 1 + value;
				}
				else if (opcode >= UNI_PTEFFECT0 && opcode <= UNI_PTEFFECTF)
				{
					note->efx = opcode - UNI_PTEFFECT0;
					note->efxData = value;
				}
				else if (opcode >= UNI_XMEFFECTA && opcode <= UNI_XMEFFECTP)
				{
					note->efx = xmEfxTab[opcode-UNI_XMEFFECTA];
					note->efxData = value;
				}
			}
		}

		pos += opcodeBytes;
	}

	return true;
}

static void freeDecodedTracks(note_t **tracks, uint16_t numTracks)
{
	for (uint16_t i = 0; i < numTracks; i++)
		free(tracks[i]);
}

static char *readString(FILE *f)
{
	uint16_t length;
	if (fread(&length, sizeof (length), 1, f) != 1)
		return NULL;

	char *out = (char *)malloc(length+1);
	if (out == NULL)
		return NULL;

	if (fread(out, 1, length, f) != length)
	{
		free(out);
		return NULL;
	}
	out[length] = '\0';

	return out;
}

bool detectBEM(FILE *f)
{
	if (f == NULL) return false;

	const long oldPos = ftell(f);
	if (oldPos < 0) return false;

	if (fseek(f, 0, SEEK_SET) != 0) return false;
	char ID[64];
	memset(ID, 0, sizeof (ID));
	if (fread(ID, 1, 4, f) != 4)
		goto error;
	if (memcmp(ID, "UN05", 4) != 0)
		goto error;

	if (fseek(f, 0x131, SEEK_SET) != 0)
		goto error;

	const int flagsByte = fgetc(f);
	if (flagsByte == EOF) goto error;
	uint8_t flags = (uint8_t)flagsByte;
	if ((flags & FLAG_XMPERIODS) == 0)
		goto error;

	if (fseek(f, 0x132, SEEK_SET) != 0)
		goto error;

	uint16_t strLength = 0;
	if (fread(&strLength, sizeof (strLength), 1, f) != 1)
		goto error;
	if (strLength == 0 || strLength > 512)
		goto error;

	if (fseek(f, strLength+2, SEEK_CUR) != 0)
		goto error;

	if (fread(ID, 1, sizeof (ID), f) != sizeof (ID))
		goto error;
	if (memcmp(ID, "FastTracker v2.00", 17) != 0)
		goto error;

	return fseek(f, oldPos, SEEK_SET) == 0;

error:
	(void)fseek(f, oldPos, SEEK_SET);
	return false;
}

bool loadBEM(FILE *f, uint32_t filesize)
{
	bemHdr_t h;

	if (filesize < sizeof (h))
	{
		loaderMsgBox("Error: This file is either not a module, or is not supported.");
		return false;
	}

	if (fread(&h, 1, sizeof (h), f) != sizeof (h))
		return false;

	if (h.numpos == 0 || h.numpos > 256 || h.numpat == 0 || h.numpat > 256 ||
		h.numchn == 0 || h.numchn > 32 || h.numtrk == 0 || h.numtrk > MAX_TRACKS ||
		h.numins > MAX_INST)
	{
		loaderMsgBox("Error loading BEM: The module is corrupt!");
		return false;
	}

	char *songName = readString(f);
	if (songName == NULL)
		return false;

	strncpy(songTmp.name, songName, 20);
	songTmp.name[20] = '\0';
	free(songName);
	uint16_t strLength;
	if (fread(&strLength, sizeof (strLength), 1, f) != 1 ||
		fseek(f, strLength, SEEK_CUR) != 0 ||
		fread(&strLength, sizeof (strLength), 1, f) != 1 ||
		fseek(f, strLength, SEEK_CUR) != 0)
	{
		loaderMsgBox("Error loading BEM: Truncated metadata!");
		return false;
	}

	tmpLinearPeriodsFlag = !!(h.flags & FLAG_LINEARSLIDES);

	songTmp.numChannels = h.numchn;
	songTmp.songLength = h.numpos;
	songTmp.songLoopStart = h.reppos;
	songTmp.BPM = h.inittempo;
	songTmp.speed = h.initspeed;
	
	memcpy(songTmp.orders, h.positions, 256);

	// load instruments
	for (int16_t i = 0; i < h.numins; i++)
	{
		if (!allocateTmpInstr(1 + i))
		{
			loaderMsgBox("Not enough memory!");
			return false;
		}

		instr_t *ins = instrTmp[1 + i];

		uint8_t numSamples;
		if (!readU8(f, &numSamples) || numSamples > MAX_SMP_PER_INST ||
			!readExact(f, ins->note2SampleLUT, sizeof (ins->note2SampleLUT)))
			return false;
		ins->numSamples = numSamples;

		if (!readU8(f, &ins->volEnvFlags) || !readU8(f, &ins->volEnvLength) ||
			!readU8(f, &ins->volEnvSustain) || !readU8(f, &ins->volEnvLoopStart) ||
			!readU8(f, &ins->volEnvLoopEnd) ||
			!readExact(f, ins->volEnvPoints, sizeof (ins->volEnvPoints))) return false;

		if (!readU8(f, &ins->panEnvFlags) || !readU8(f, &ins->panEnvLength) ||
			!readU8(f, &ins->panEnvSustain) || !readU8(f, &ins->panEnvLoopStart) ||
			!readU8(f, &ins->panEnvLoopEnd) ||
			!readExact(f, ins->panEnvPoints, sizeof (ins->panEnvPoints))) return false;

		if (!readU8(f, &ins->autoVibType) || !readU8(f, &ins->autoVibSweep) ||
			!readU8(f, &ins->autoVibDepth) || !readU8(f, &ins->autoVibRate) ||
			!readExact(f, &ins->fadeout, sizeof (ins->fadeout))) return false;

		char *insName = readString(f);
		if (insName == NULL)
			return false;

		uint32_t insNameLen = (uint32_t)strlen(insName);
		if (insNameLen > 22)
			insNameLen = 22;

		memcpy(songTmp.instrName[1+i], insName, insNameLen);
		free(insName);

		for (int32_t j = 0; j < ins->numSamples; j++)
		{
			sample_t *s = &ins->smp[j];

			uint8_t finetune, reserved, relativeNote, volume, panning;
			if (!readU8(f, &finetune) || !readU8(f, &reserved) ||
				!readU8(f, &relativeNote) || !readU8(f, &volume) || !readU8(f, &panning))
				return false;
			(void)reserved;
			s->finetune = (int8_t)(finetune ^ 0x80);
			s->relativeNote = (int8_t)relativeNote;
			s->volume = volume;
			s->panning = panning;
			if (!readExact(f, &s->length, sizeof (s->length)) ||
				!readExact(f, &s->loopStart, sizeof (s->loopStart))) return false;
			uint32_t loopEnd;
			if (!readExact(f, &loopEnd, sizeof (loopEnd))) return false;

			uint16_t flags;
			if (!readExact(f, &flags, sizeof (flags))) return false;
			if (s->length < 0 || s->length > MAX_SAMPLE_LEN || s->loopStart < 0 ||
				loopEnd < (uint32_t)s->loopStart || loopEnd > (uint32_t)s->length)
				return false;
			s->loopLength = (int32_t)(loopEnd - (uint32_t)s->loopStart);
			if (flags &  1) s->flags |= SAMPLE_16BIT;
			if (flags & 16) s->flags |= LOOP_FWD;
			if (flags & 32) s->flags |= LOOP_BIDI;

			char *smpName = readString(f);
			if (smpName == NULL)
				return false;
			
			uint32_t smpNameLen = (uint32_t)strlen(smpName);
			if (smpNameLen > 22)
				smpNameLen = 22;

			memcpy(s->name, smpName, smpNameLen);
			free(smpName);
		}
	}

	// load tracks

	uint16_t rowsInPattern[256];
	uint16_t trackList[256*32];
	if (!readExact(f, rowsInPattern, (size_t)h.numpat * sizeof (rowsInPattern[0])) ||
		!readExact(f, trackList, (size_t)h.numpat * h.numchn * sizeof (trackList[0])))
		return false;

	uint16_t trackRows[MAX_TRACKS] = { 0 };
	for (uint16_t pattern = 0; pattern < h.numpat; pattern++)
	{
		if (rowsInPattern[pattern] > 256)
			return false;

		for (uint8_t channel = 0; channel < h.numchn; channel++)
		{
			const uint16_t track = trackList[(pattern * h.numchn) + channel];
			if (track >= h.numtrk)
				return false;
			if (trackRows[track] < rowsInPattern[pattern])
				trackRows[track] = rowsInPattern[pattern];
		}
	}

	note_t *decodedTrack[MAX_TRACKS] = { NULL };
	for (int32_t i = 0; i < h.numtrk; i++)
	{
		uint16_t trackBytesInFile;
		if (!readExact(f, &trackBytesInFile, sizeof (trackBytesInFile)))
			goto trackError;
		if (trackBytesInFile == 0)
		{
			loaderMsgBox("Error loading BEM: This module is corrupt!");
			goto trackError;
		}

		uint8_t *encodedTrack = (uint8_t *)malloc(trackBytesInFile);
		if (encodedTrack == NULL)
		{
			loaderMsgBox("Not enough memory!");
			goto trackError;
		}
		if (!readExact(f, encodedTrack, trackBytesInFile))
		{
			free(encodedTrack);
			goto trackError;
		}

		if (trackRows[i] > 0)
		{
			decodedTrack[i] = (note_t *)calloc(trackRows[i], sizeof (note_t));
			if (decodedTrack[i] == NULL)
			{
				free(encodedTrack);
				loaderMsgBox("Not enough memory!");
				goto trackError;
			}

			if (!decodeBEMTrack(encodedTrack, trackBytesInFile, decodedTrack[i], trackRows[i]))
			{
				free(encodedTrack);
				loaderMsgBox("Error loading BEM: Invalid track data!");
				goto trackError;
			}
		}

		free(encodedTrack);
	}

	// create patterns from tracks
	for (int32_t i = 0; i < h.numpat; i++)
	{
		uint16_t numRows = rowsInPattern[i];
		if (numRows == 0 || numRows > 256)
			continue;

		if (!allocateTmpPatt(i, numRows))
		{
			loaderMsgBox("Not enough memory!");
			freeDecodedTracks(decodedTrack, h.numtrk);
			return false;
		}

		note_t *dst = patternTmp[i];
		for (int32_t j = 0; j < h.numchn; j++)
		{
			note_t *src = (note_t *)decodedTrack[trackList[(i * h.numchn) + j]];
			if (src != NULL)
			{
				for (int32_t k = 0; k < numRows; k++)
					dst[(k * MAX_CHANNELS) + j] = src[k];
			}
		}
	}
	freeDecodedTracks(decodedTrack, h.numtrk);

	// load sample data
	for (int32_t i = 0; i < h.numins; i++)
	{
		instr_t *ins = instrTmp[1 + i];
		if (ins == NULL)
			continue;

		for (int32_t j = 0; j < ins->numSamples; j++)
		{
			sample_t *s = &ins->smp[j];

			bool sampleIs16Bit = !!(s->flags & SAMPLE_16BIT);
			if (!allocateSmpData(s, s->length, sampleIs16Bit, false))
			{
				loaderMsgBox("Not enough memory!");
				return false;
			}

			if (fread(s->dataPtr, 1 + sampleIs16Bit, s->length, f) != (size_t)s->length)
				return false;
			delta2Samp(s->dataPtr, s->length, s->flags);
		}
	}

	return true;

trackError:
	freeDecodedTracks(decodedTrack, h.numtrk);
	return false;
}

#ifdef FT2_STABILITY_TESTS
bool runBEMLoaderRegressionTests(void)
{
	note_t notes[2] = { 0 };
	const uint8_t repeatedNote[] = { 0x23, UNI_NOTE, 59, 0 };
	if (!decodeBEMTrack(repeatedNote, sizeof (repeatedNote), notes, 2) ||
		notes[0].note != 60 || notes[1].note != 60)
		return false;

	const uint8_t zeroLengthDescriptor[] = { 0x20 };
	const uint8_t truncatedValue[] = { 2, UNI_NOTE };
	const uint8_t tooManyRows[] = { 0x23, UNI_NOTE, 59 };
	return !decodeBEMTrack(zeroLengthDescriptor, sizeof (zeroLengthDescriptor), notes, 2) &&
		!decodeBEMTrack(truncatedValue, sizeof (truncatedValue), notes, 2) &&
		!decodeBEMTrack(tooManyRows, sizeof (tooManyRows), notes, 1);
}
#endif
