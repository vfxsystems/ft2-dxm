// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ft2_header.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_mouse.h"
#include "ft2_sample_ed.h"
#include "ft2_inst_ed.h"
#include "ft2_replayer.h"
#include "ft2_module_loader.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "ft2_mixer.h"  // for mixerCh and MAX_MIXER_CHANNELS
#include "ft2_dsp.h"    // for dspEffectInstance_t, DSP_MAX_SLOTS, dspGetParamInfo, masterEffects
#include "ft2_diskop.h" // for module save mode constants
#include "ft2_synth.h"
#include "ft2_dexed.h"
#include "ft2_v2.h"
#include "ft2_ostirus.h"
#include "ft2_macro_map.h"

#ifdef FT2_DXM_SAVE_TRACE
#define DXM_SAVE_TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#define DXM_SAVE_TRACE(...) ((void)0)
#endif
// Tunefish4 synth helper
extern void* createInstrumentInstance(int instrID);

static int8_t smpChunkBuf[1024];
static uint8_t packedPattData[65536], modPattData[64*32*4];
static SDL_Thread *thread;

static const char modIDs[32][5] =
{
	"1CHN", "2CHN", "3CHN", "4CHN", "5CHN", "6CHN", "7CHN", "8CHN",
	"9CHN", "10CH", "11CH", "12CH", "13CH", "14CH", "15CH", "16CH",
	"17CH", "18CH", "19CH", "20CH", "21CH", "22CH", "23CH", "24CH",
	"25CH", "26CH", "27CH", "28CH", "29CH", "30CH", "31CH", "32CH"
};

static uint16_t packPatt(uint8_t *writePtr, uint8_t *pattPtr, uint16_t numRows);

static UNICHAR *makeDxmTemporaryPath(const UNICHAR *destination)
{
    if (destination == NULL) return NULL;
    const size_t length = UNICHAR_STRLEN(destination);
    UNICHAR *path = (UNICHAR *)malloc((length + 5u) * sizeof(UNICHAR));
    if (path == NULL) return NULL;
    UNICHAR_STRCPY(path, destination);
#ifdef _WIN32
    UNICHAR_STRCAT(path, L".tmp");
#else
    UNICHAR_STRCAT(path, ".tmp");
#endif
    return path;
}

static bool commitDxmTemporaryFile(const UNICHAR *temporaryPath, const UNICHAR *destination)
{
#ifdef _WIN32
    return MoveFileExW(temporaryPath, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return UNICHAR_RENAME(temporaryPath, destination) == 0;
#endif
}

static bool abortDxmSave(FILE *f, UNICHAR *temporaryPath, const char *message)
{
    if (f != NULL) fclose(f);
    if (temporaryPath != NULL) {
        UNICHAR_REMOVE(temporaryPath);
        free(temporaryPath);
    }
    if (message != NULL)
        okBoxThreadSafe(0, "System message", message, NULL);
    return false;
}

static bool writeDxmU32At(FILE *f, long fieldPosition, long returnPosition, uint32_t value)
{
    if (f == NULL || fieldPosition < 0 || returnPosition < 0) return false;
    if (fseek(f, fieldPosition, SEEK_SET) != 0) return false;
    if (fwrite(&value, sizeof(value), 1, f) != 1) return false;
    return fseek(f, returnPosition, SEEK_SET) == 0;
}

static bool finishDxmChunk(FILE *f, long lengthPosition, long dataStart,
                           long dataEnd, uint32_t *lengthOut)
{
    if (dataStart < 0 || dataEnd < dataStart) return false;
    const uint64_t length = (uint64_t)(dataEnd - dataStart);
    if (length > UINT32_MAX) return false;

    const uint32_t length32 = (uint32_t)length;
    if (!writeDxmU32At(f, lengthPosition, dataEnd, length32)) return false;
    if (lengthOut != NULL) *lengthOut = length32;
    return true;
}

static void setDxmSamplesFixed(bool fixed)
{
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins) continue;
        for (int smpIdx = 0; smpIdx < 16; smpIdx++) {
            sample_t *s = &ins->smp[smpIdx];
            if (s->dataPtrL != NULL) {
                if (fixed) fixSample(s);
                else unfixSample(s);
            }
        }
    }
}
// Helper: Write DSP state (mixer and master chains) to FILE*, return bytes written
static size_t writeDSPStateChunk(FILE *f)
{
    size_t bytesWritten = 0;
    // write mixer channel DSP chains (32 channels)
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++)
    {
        dspEffectInstance_t *chain = stereoMixerCh[ch].effects;
        for (int slot = 0; slot < DSP_MAX_SLOTS; slot++)
        {
            dspEffectInstance_t *e = &chain[slot];
            uint8_t type = (uint8_t)e->type;
            fwrite(&type, 1, 1, f); bytesWritten += 1;
            int nParams = 0;
            dspGetParamInfo(e->type, &nParams);
            uint8_t np = (uint8_t)nParams;
            fwrite(&np, 1, 1, f); bytesWritten += 1;
            float *p = (float *)&e->params;
            for (int i = 0; i < nParams; i++)
            {
                fwrite(&p[i], sizeof(float), 1, f); bytesWritten += sizeof(float);
            }
        }
    }
    // write master chain
    for (int slot = 0; slot < DSP_MAX_SLOTS; slot++)
    {
        dspEffectInstance_t *e = &masterEffects[slot];
        uint8_t type = (uint8_t)e->type;
        fwrite(&type, 1, 1, f); bytesWritten += 1;
        int nParams = 0;
        dspGetParamInfo(e->type, &nParams);
        uint8_t np = (uint8_t)nParams;
        fwrite(&np, 1, 1, f); bytesWritten += 1;
        float *p = (float *)&e->params;
        for (int i = 0; i < nParams; i++)
        {
            fwrite(&p[i], sizeof(float), 1, f); bytesWritten += sizeof(float);
        }
    }
    return bytesWritten;
}
// XM saving removed - DXM is the primary format with stereo support
// Forward declaration to match header
bool saveDXM(UNICHAR *filenameU);
// Remove saveMOD and all MOD-related code

static int32_t SDLCALL saveMusicThread(void *ptr)
{
	DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Starting save thread...\n");
	assert(editor.tmpFilenameU != NULL);
	if (editor.tmpFilenameU == NULL)
		return false;
	DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Pausing audio...\n");
	pauseAudio();
	// Always save as DXM format (DXM is the primary format with stereo support)
	DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Calling saveDXM...\n");
	bool success = saveDXM(editor.tmpFilenameU);
	if (!success)
	{
		DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Save failed!\n");
		okBoxThreadSafe(0, "System message", "Error saving module!", NULL);
	}
	else
	{
		DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Save completed successfully!\n");
	}
	DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Resuming audio...\n");
	resumeAudio();
	DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Audio resumed successfully\n");
	DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Thread exiting...\n");
	// Turn off mouse animation when thread exits
	mouseAnimOff();
	DXM_SAVE_TRACE("[DXM-SAVE-THREAD] Mouse animation turned off\n");
	return success;
	(void)ptr;
}

void saveMusic(UNICHAR *filenameU)
{
	DXM_SAVE_TRACE("[DXM-SAVE-MAIN] Starting save process...\n");
	UNICHAR_STRCPY(editor.tmpFilenameU, filenameU);
	DXM_SAVE_TRACE("[DXM-SAVE-MAIN] Turning on mouse animation...\n");
	mouseAnimOn();
	DXM_SAVE_TRACE("[DXM-SAVE-MAIN] Creating save thread...\n");
	thread = SDL_CreateThread(saveMusicThread, NULL, NULL);
	if (thread == NULL)
	{
		DXM_SAVE_TRACE("[DXM-SAVE-MAIN] Failed to create thread!\n");
		okBoxThreadSafe(0, "System message", "Couldn't create thread!", NULL);
		return;
	}
	DXM_SAVE_TRACE("[DXM-SAVE-MAIN] Detaching thread...\n");
	SDL_DetachThread(thread);
	DXM_SAVE_TRACE("[DXM-SAVE-MAIN] Thread detached successfully\n");
	DXM_SAVE_TRACE("[DXM-SAVE-MAIN] Save process initiated...\n");
}

static uint16_t packPatt(uint8_t *writePtr, uint8_t *pattPtr, uint16_t numRows)
{
	uint8_t bytes[5];

	if (pattPtr == NULL)
		return 0;

	uint16_t totalPackLen = 0;

	const int32_t pitch = sizeof (note_t) * (MAX_CHANNELS - song.numChannels);
	for (int32_t row = 0; row < numRows; row++)
	{
		for (int32_t chn = 0; chn < song.numChannels; chn++)
		{
			bytes[0] = *pattPtr++;
			bytes[1] = *pattPtr++;
			bytes[2] = *pattPtr++;
			bytes[3] = *pattPtr++;
			bytes[4] = *pattPtr++;

			uint8_t *firstBytePtr = writePtr++;

			uint8_t packBits = 0;
			if (bytes[0] > 0) { packBits |= 1; *writePtr++ = bytes[0]; } // note
			if (bytes[1] > 0) { packBits |= 2; *writePtr++ = bytes[1]; } // instrument
			if (bytes[2] > 0) { packBits |= 4; *writePtr++ = bytes[2]; } // volume column
			if (bytes[3] > 0) { packBits |= 8; *writePtr++ = bytes[3]; } // effect

			if (packBits == 15) // first four bits set?
			{
				// no packing needed, write pattern data as is

				// point to first byte (and overwrite data)
				writePtr = firstBytePtr;

				*writePtr++ = bytes[0];
				*writePtr++ = bytes[1];
				*writePtr++ = bytes[2];
				*writePtr++ = bytes[3];
				*writePtr++ = bytes[4];

				totalPackLen += 5;
				continue;
			}

			if (bytes[4] > 0) { packBits |= 16; *writePtr++ = bytes[4]; } // effect parameter

			*firstBytePtr = packBits | 128; // write pack bits byte
			totalPackLen += (uint16_t)(writePtr - firstBytePtr); // bytes writen
		}

		// skip unused channels (unpacked patterns always have 32 channels)
		pattPtr += pitch;
	}

	return totalPackLen;
}
// Write module sample data without allocating from the save thread.
static bool writeAllSamplesDXMWAV(FILE *f, size_t *bytesWrittenOut)
{
    size_t bytesWritten = 0;
    uint16_t numSamples = 0;
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins) continue;
        for (int smpIdx = 0; smpIdx < 16; smpIdx++) {
            sample_t *s = &ins->smp[smpIdx];
            if (s->length > 0 && s->dataPtrL != NULL)
                numSamples++;
        }
    }
    if (fwrite(&numSamples, sizeof(numSamples), 1, f) != 1)
        return false;
    bytesWritten += sizeof(numSamples);

    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins) continue;
        for (int smpIdx = 0; smpIdx < 16; smpIdx++) {
            sample_t *s = &ins->smp[smpIdx];
            if (s->length <= 0 || s->dataPtrL == NULL)
                continue;

            if (s->length > MAX_SAMPLE_LEN || ((s->flags & SAMPLE_STEREO) && s->dataPtrR == NULL))
                return false;

            uint8_t flags = 0;
            bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
            bool stereo = !!(s->flags & SAMPLE_STEREO);
            if (sample16Bit) flags |= 1;
            if (stereo) flags |= 2;
            const uint8_t instrument = (uint8_t)instrIdx;
            const uint8_t sample = (uint8_t)smpIdx;
            uint32_t frames = s->length;
            if (fwrite(&instrument, sizeof(instrument), 1, f) != 1 ||
                fwrite(&sample, sizeof(sample), 1, f) != 1 ||
                fwrite(&flags, sizeof(flags), 1, f) != 1 ||
                fwrite(&frames, sizeof(frames), 1, f) != 1)
                return false;
            bytesWritten += 3 + sizeof(frames);

            const size_t bytesPerSample = sample16Bit ? 2u : 1u;
            if (!stereo) {
                const size_t payloadSize = (size_t)frames * bytesPerSample;
                if (fwrite(s->dataPtrL, 1, payloadSize, f) != payloadSize)
                    return false;
                bytesWritten += payloadSize;
            } else {
                const size_t framesPerBlock = sizeof(smpChunkBuf) / (2u * bytesPerSample);
                uint32_t frame = 0;
                while (frame < frames) {
                    size_t blockFrames = frames - frame;
                    if (blockFrames > framesPerBlock) blockFrames = framesPerBlock;

                    if (sample16Bit) {
                        int16_t *dst = (int16_t *)smpChunkBuf;
                        const int16_t *left = (const int16_t *)s->dataPtrL;
                        const int16_t *right = (const int16_t *)s->dataPtrR;
                        for (size_t i = 0; i < blockFrames; ++i) {
                            dst[i * 2] = left[frame + i];
                            dst[i * 2 + 1] = right[frame + i];
                        }
                    } else {
                        for (size_t i = 0; i < blockFrames; ++i) {
                            smpChunkBuf[i * 2] = s->dataPtrL[frame + i];
                            smpChunkBuf[i * 2 + 1] = s->dataPtrR[frame + i];
                        }
                    }

                    const size_t blockBytes = blockFrames * 2u * bytesPerSample;
                    if (fwrite(smpChunkBuf, 1, blockBytes, f) != blockBytes)
                        return false;
                    bytesWritten += blockBytes;
                    frame += (uint32_t)blockFrames;
                }
            }
        }
    }
    if (bytesWrittenOut != NULL) *bytesWrittenOut = bytesWritten;
    return true;
}
// Helper: Write all Tunefish4 instrument states to FILE*, return bytes written
static size_t writeAllTF4StatesDXM(FILE *f)
{
    size_t bytesWritten = 0;
    uint8_t numTF4 = 0;
    // Tunefish4 parameter count (adjust if needed)
    #define DXM_TF4_PARAM_COUNT 128
    // First, count all TF4 instruments
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (ins && ins->useTF4) {
            numTF4++;
            DXM_SAVE_TRACE("[DXM-SAVE] Found TF4 instrument %d\n", instrIdx);
        }
    }
    DXM_SAVE_TRACE("[DXM-SAVE] Total TF4 instruments to save: %d\n", numTF4);
    fwrite(&numTF4, sizeof(uint8_t), 1, f); bytesWritten += sizeof(uint8_t);
    // Now write each TF4 instrument
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useTF4)
            continue;
        // Write instrument index
        uint8_t idx = (uint8_t)instrIdx;
        fwrite(&idx, sizeof(uint8_t), 1, f); bytesWritten += 1;
        // Write instrument name (use sample 0 name)
        uint8_t nameLen = (uint8_t)strnlen(ins->smp[0].name, 22);
        fwrite(&nameLen, sizeof(uint8_t), 1, f); bytesWritten += 1;
        fwrite(ins->smp[0].name, sizeof(char), nameLen, f); bytesWritten += nameLen;
        // Write TF4 params (float array)
        float *params = ins->tf4Params;
        DXM_SAVE_TRACE("[DXM-SAVE] TF4 instr=%d name='%.*s'\n", instrIdx, nameLen, ins->smp[0].name);
        DXM_SAVE_TRACE("[DXM-SAVE] TF4 params[0..3]: %.3f %.3f %.3f %.3f\n", params[0], params[1], params[2], params[3]);
        fwrite(params, sizeof(float), DXM_TF4_PARAM_COUNT, f); bytesWritten += DXM_TF4_PARAM_COUNT * sizeof(float);
    }
    return bytesWritten;
}
// Helper: Write DXM instrument metadata to distinguish from XM instruments
static size_t writeDXMInstrumentMetadata(FILE *f)
{
    size_t bytesWritten = 0;
    uint8_t numDXMInstruments = 0;
    
    // Count DXM instruments (those with stereo samples or TF4/Dexed/V2)
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins) continue;
        
        // Check if this is a DXM instrument (has stereo samples or TF4/Dexed/V2)
        bool isDXMInstrument = false;
        if (ins->useTF4 || ins->useDexed || ins->useV2 || ins->useOsTirus || ins->osTirusPreset != 0xFFFF || ins->osTirusSlot != 0xFF) {
            isDXMInstrument = true;
        } else {
            // Check for stereo samples
            for (int smpIdx = 0; smpIdx < 16; smpIdx++) {
                sample_t *s = &ins->smp[smpIdx];
                if (s->length > 0 && (s->flags & SAMPLE_STEREO)) {
                    isDXMInstrument = true;
                    break;
                }
            }
        }
        
        if (isDXMInstrument) {
            numDXMInstruments++;
        }
    }
    
    fwrite(&numDXMInstruments, sizeof(uint8_t), 1, f); bytesWritten += sizeof(uint8_t);
    
    // Write DXM instrument indices
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins) continue;
        
        bool isDXMInstrument = false;
        if (ins->useTF4 || ins->useDexed || ins->useV2 || ins->useOsTirus || ins->osTirusPreset != 0xFFFF || ins->osTirusSlot != 0xFF) {
            isDXMInstrument = true;
        } else {
            for (int smpIdx = 0; smpIdx < 16; smpIdx++) {
                sample_t *s = &ins->smp[smpIdx];
                if (s->length > 0 && (s->flags & SAMPLE_STEREO)) {
                    isDXMInstrument = true;
                    break;
                }
            }
        }
        
        if (isDXMInstrument) {
            uint8_t idx = (uint8_t)instrIdx;
            fwrite(&idx, sizeof(uint8_t), 1, f); bytesWritten += 1;
            
            // Write instrument type flags
            uint8_t flags = 0;
            if (ins->useTF4) flags |= 1; // TF4 instrument
            for (int smpIdx = 0; smpIdx < 16; smpIdx++) {
                sample_t *s = &ins->smp[smpIdx];
                if (s->length > 0 && (s->flags & SAMPLE_STEREO)) {
                    flags |= 2; // Has stereo samples
                    break;
                }
            }
            if (ins->useDexed) flags |= 4; // Dexed instrument
            if (ins->useV2) flags |= 8;    // V2 instrument
            const bool hasOsTirusState = ins->useOsTirus || ins->osTirusPreset != 0xFFFF || ins->osTirusSlot != 0xFF;
            if (hasOsTirusState) flags |= 16; // OsTIrus instrument/state present
            fwrite(&flags, sizeof(uint8_t), 1, f); bytesWritten += 1;

            if (hasOsTirusState) {
                uint16_t preset = ins->osTirusPreset;
                uint8_t slot = ins->osTirusSlot;
                fwrite(&preset, sizeof(uint16_t), 1, f); bytesWritten += sizeof(uint16_t);
                fwrite(&slot, sizeof(uint8_t), 1, f); bytesWritten += sizeof(uint8_t);
            }
        }
    }
    
    return bytesWritten;
}

static bool hasOsTirusArpState(const instr_t *ins)
{
    if (!ins)
        return false;

    for (int i = 0; i < 16; ++i)
    {
        if (ins->osTirusArpStepGate[i] != 0 ||
            ins->osTirusArpStepVelocity[i] != 0 ||
            ins->osTirusArpStepLength[i] != 0)
        {
            return true;
        }
    }

    return false;
}

static size_t writeOsTirusArpStateChunk(FILE *f)
{
    size_t bytesWritten = 0;
    uint8_t numArpInstruments = 0;

    for (int instrIdx = 1; instrIdx <= MAX_INST; ++instrIdx)
    {
        const instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useOsTirus)
            continue;

        if (hasOsTirusArpState(ins))
            ++numArpInstruments;
    }

    fwrite(&numArpInstruments, sizeof(uint8_t), 1, f);
    bytesWritten += sizeof(uint8_t);

    for (int instrIdx = 1; instrIdx <= MAX_INST; ++instrIdx)
    {
        const instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useOsTirus || !hasOsTirusArpState(ins))
            continue;

        const uint8_t idx = (uint8_t)instrIdx;
        fwrite(&idx, sizeof(uint8_t), 1, f);
        bytesWritten += sizeof(uint8_t);

        fwrite(ins->osTirusArpStepGate, sizeof(uint8_t), 16, f);
        bytesWritten += 16;
        fwrite(ins->osTirusArpStepVelocity, sizeof(uint8_t), 16, f);
        bytesWritten += 16;
        fwrite(ins->osTirusArpStepLength, sizeof(uint8_t), 16, f);
        bytesWritten += 16;
    }

    return bytesWritten;
}
// Helper: Refresh all TF4 parameter arrays from the synth engine before saving
static void refreshAllTF4ParamsFromSynth(void)
{
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useTF4) continue;
        /* Ensure we query a valid TF4 instance (auto-create if missing) */
        createInstrumentInstance(instrIdx);
        for (int p = 0; p < 128; ++p)
        {
            /* Tunefish synth API uses zero-based instrument IDs (0..127),
             * while FT2 instruments are numbered 1..128. Convert here.
             */
            ins->tf4Params[p] = ft2_synth_get_param(instrIdx, p);
        }
    }
}
// Helper: Refresh all Dexed parameter arrays from the synth engine before saving
static void refreshAllDexedParamsFromSynth(void)
{
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useDexed) continue;
        
        ft2_dx_get_patch_data(instrIdx, ins->dxParams, 155);
    }
}

// Helper: Write all Dexed instrument states to FILE*, return bytes written
static void writeAllDexedStatesDXM(FILE *f)
{
    uint8_t numDexed = 0;
    
    // First, count all Dexed instruments
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (ins && ins->useDexed) {
            numDexed++;
        }
    }
    
    fwrite(&numDexed, sizeof(uint8_t), 1, f);

    // Now write each Dexed instrument's patch
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useDexed)
            continue;
        
        // Write instrument index
        uint8_t idx = (uint8_t)instrIdx;
        fwrite(&idx, sizeof(uint8_t), 1, f);
        
        // Write Dexed params (155-byte blob)
        fwrite(ins->dxParams, sizeof(uint8_t), 155, f);
    }
}

// Helper: Write all V2 instrument states to FILE*, return bytes written
static bool writeAllV2StatesDXM(FILE *f, size_t *bytesWrittenOut)
{
    size_t bytesWritten = 0;
    uint8_t numV2 = 0;

    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (ins && ins->useV2) {
            numV2++;
        }
    }

    if (fwrite(&numV2, sizeof(numV2), 1, f) != 1) return false;
    bytesWritten += sizeof(numV2);

    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useV2)
            continue;

        size_t blobSize = ft2_v2_serialize_state(instrIdx, NULL, 0);
        uint8_t idx = (uint8_t)instrIdx;
        if (blobSize == 0 || blobSize > 16u * 1024u * 1024u || blobSize > UINT32_MAX)
            return false;
        uint8_t *blob = (uint8_t *)malloc(blobSize);
        if (blob == NULL) return false;
        if (ft2_v2_serialize_state(instrIdx, blob, blobSize) != blobSize) {
            free(blob);
            return false;
        }
        const uint32_t len = (uint32_t)blobSize;
        const bool wrote = fwrite(&idx, sizeof(idx), 1, f) == 1 &&
            fwrite(&len, sizeof(len), 1, f) == 1 && fwrite(blob, 1, len, f) == len;
        free(blob);
        if (!wrote) return false;
        bytesWritten += sizeof(idx) + sizeof(len) + len;
    }

    if (bytesWrittenOut != NULL) *bytesWrittenOut = bytesWritten;
    return true;
}

static bool writeAllOsTirusStatesDXM(FILE *f, size_t *bytesWrittenOut)
{
    size_t bytesWritten = 0;
    uint8_t numOsTirus = 0;

    for (int instrIdx = 1; instrIdx <= MAX_INST; ++instrIdx)
    {
        instr_t *ins = instr[instrIdx];
        if (ins && ins->useOsTirus)
            ++numOsTirus;
    }

    if (fwrite(&numOsTirus, sizeof(numOsTirus), 1, f) != 1) return false;
    bytesWritten += sizeof(numOsTirus);

    for (int instrIdx = 1; instrIdx <= MAX_INST; ++instrIdx)
    {
        instr_t *ins = instr[instrIdx];
        if (!ins || !ins->useOsTirus)
            continue;

        const size_t blobSize = ft2_ostirus_serialize_state(instrIdx, NULL, 0);
        uint8_t idx = (uint8_t)instrIdx;
        if (blobSize == 0 || blobSize > 16u * 1024u * 1024u || blobSize > UINT32_MAX)
            return false;
        uint8_t *blob = (uint8_t *)malloc(blobSize);
        if (blob == NULL) return false;
        if (ft2_ostirus_serialize_state(instrIdx, blob, blobSize) != blobSize) {
            free(blob);
            return false;
        }
        const uint32_t len = (uint32_t)blobSize;
        const bool wrote = fwrite(&idx, sizeof(idx), 1, f) == 1 &&
            fwrite(&len, sizeof(len), 1, f) == 1 && fwrite(blob, 1, len, f) == len;
        free(blob);
        if (!wrote) return false;
        bytesWritten += sizeof(idx) + sizeof(len) + len;
    }

    if (bytesWrittenOut != NULL) *bytesWrittenOut = bytesWritten;
    return true;
}

// --- DXM (Extended XM) Saver ---
bool saveDXM(UNICHAR *filenameU)
{
	if (filenameU == NULL)
		return false;

	DXM_SAVE_TRACE("[DXM-SAVE] Starting DXM save...\n");
	
	// Note: isDXMFormat is only used during loading, not saving
	// Stereo samples are handled directly in the save function
	
	DXM_SAVE_TRACE("[DXM-SAVE] Refreshing TF4 params...\n");
	refreshAllTF4ParamsFromSynth();
	DXM_SAVE_TRACE("[DXM-SAVE] Refreshing Dexed params...\n");
	refreshAllDexedParamsFromSynth();
        // Open file for writing
    DXM_SAVE_TRACE("[DXM-SAVE] Opening file for writing...\n");
    UNICHAR *temporaryPath = makeDxmTemporaryPath(filenameU);
    if (temporaryPath == NULL) {
        okBoxThreadSafe(0, "System message", "Out of memory preparing module save!", NULL);
        return false;
    }
    FILE *f = UNICHAR_FOPEN(temporaryPath, "wb");
    if (f == NULL)
    {
        free(temporaryPath);
        okBoxThreadSafe(0, "System message", "Error opening file for saving, is it in use?", NULL);
        return false;
    }
    // 1. Write DXM magic header
    DXM_SAVE_TRACE("[DXM-SAVE] Writing DXM magic header...\n");
    const char dxmMagic[4] = {'D','X','M','0'};
    if (fwrite(dxmMagic, 1, 4, f) != 4)
    {
        return abortDxmSave(f, temporaryPath, "Error writing DXM header!");
    }
    // 2. Reserve space for XM data size (4 bytes)
    DXM_SAVE_TRACE("[DXM-SAVE] Reserving XM size space...\n");
    uint32_t xmSize = 0;
    long xmSizePos = ftell(f);
    if (fwrite(&xmSize, 1, 4, f) != 4)
    {
        return abortDxmSave(f, temporaryPath, "Error reserving XM size!");
    }
    // 3. Write XM data directly (without stereo samples - those go in WAV chunk)
    DXM_SAVE_TRACE("[DXM-SAVE] Writing XM header...\n");
    // Write XM header
    xmHdr_t h;
    memcpy(h.ID, "Extended Module: ", 17);
    
    // song name
    int32_t nameLength = (int32_t)strlen(song.name);
    if (nameLength > 20) nameLength = 20;
    memset(h.name, ' ', 20);
    if (nameLength > 0) memcpy(h.name, song.name, nameLength);
    
    h.x1A = 0x1A;
    
    // program/tracker name
    nameLength = (int32_t)strlen(PROG_NAME_STR);
    if (nameLength > 20) nameLength = 20;
    memset(h.progName, ' ', 20);
    if (nameLength > 0) memcpy(h.progName, PROG_NAME_STR, nameLength);
    
    h.version = 0x0104;
    h.headerSize = 20 + 256;
    h.numOrders = song.songLength;
    h.songLoopStart = song.songLoopStart;
    h.numChannels = (uint16_t)song.numChannels;
    h.speed = song.speed;
    h.BPM = song.BPM;
    
    // count number of patterns
    int16_t i = MAX_PATTERNS;
    do {
        if (patternEmpty(i-1)) i--;
        else break;
    } while (i > 0);
    h.numPatterns = i;
    
    // count number of instruments
    i = 128;
    while (i > 0 && getUsedSamples(i) == 0 && song.instrName[i][0] == '\0') i--;
    h.numInstr = i;
    
    h.flags = audio.linearPeriodsFlag;
    memcpy(h.orders, song.orders, 256);
    
    DXM_SAVE_TRACE("[DXM-SAVE] Writing XM header data...\n");
    if (fwrite(&h, sizeof(h), 1, f) != 1) {
        return abortDxmSave(f, temporaryPath, "Error writing XM header!");
    }
    
    // Write patterns
    DXM_SAVE_TRACE("[DXM-SAVE] Writing %d patterns...\n", h.numPatterns);
    xmPatHdr_t ph;
    for (i = 0; i < h.numPatterns; i++) {
        if (patternEmpty(i)) {
            if (pattern[i] != NULL) {
                free(pattern[i]);
                pattern[i] = NULL;
            }
            patternNumRows[i] = 64;
        }
        
        ph.headerSize = sizeof(xmPatHdr_t);
        ph.numRows = patternNumRows[i];
        ph.type = 0;
        
        if (pattern[i] == NULL) {
            ph.dataSize = 0;
            if (fwrite(&ph, ph.headerSize, 1, f) != 1) {
                return abortDxmSave(f, temporaryPath, "Error writing pattern!");
            }
        } else {
            ph.dataSize = packPatt(packedPattData, (uint8_t *)pattern[i], patternNumRows[i]);
            if (fwrite(&ph, ph.headerSize, 1, f) != 1 ||
                fwrite(packedPattData, ph.dataSize, 1, f) != 1) {
                return abortDxmSave(f, temporaryPath, "Error writing pattern data!");
            }
        }
    }
    
    // Write instruments (without sample data - that goes in WAV chunk)
    DXM_SAVE_TRACE("[DXM-SAVE] Writing %d instruments...\n", h.numInstr);
    xmInsHdr_t ih;
    for (i = 1; i <= h.numInstr; i++) {
        memset(&ih, 0, sizeof(ih));
        
        int16_t j = (instr[i] == NULL) ? 0 : i;
        int16_t a = getUsedSamples(i);
        
        nameLength = (int32_t)strlen(song.instrName[i]);
        if (nameLength > 22) nameLength = 22;
        memset(ih.name, 0, 22);
        if (nameLength > 0) memcpy(ih.name, song.instrName[i], nameLength);
        
        ih.type = 0;
        ih.numSamples = a;
        ih.sampleSize = sizeof(xmSmpHdr_t);
        
        if (a > 0) {
            instr_t *ins = instr[j];
            memcpy(ih.note2SampleLUT, ins->note2SampleLUT, 96);
            memcpy(ih.volEnvPoints, ins->volEnvPoints, 12*2*sizeof(int16_t));
            memcpy(ih.panEnvPoints, ins->panEnvPoints, 12*2*sizeof(int16_t));
            ih.volEnvLength = ins->volEnvLength;
            ih.panEnvLength = ins->panEnvLength;
            ih.volEnvSustain = ins->volEnvSustain;
            ih.volEnvLoopStart = ins->volEnvLoopStart;
            ih.volEnvLoopEnd = ins->volEnvLoopEnd;
            ih.panEnvSustain = ins->panEnvSustain;
            ih.panEnvLoopStart = ins->panEnvLoopStart;
            ih.panEnvLoopEnd = ins->panEnvLoopEnd;
            ih.volEnvFlags = ins->volEnvFlags;
            ih.panEnvFlags = ins->panEnvFlags;
            ih.vibType = ins->autoVibType;
            ih.vibSweep = ins->autoVibSweep;
            ih.vibDepth = ins->autoVibDepth;
            ih.vibRate = ins->autoVibRate;
            ih.fadeout = ins->fadeout;
            ih.midiOn = ins->midiOn ? 1 : 0;
            ih.midiChannel = ins->midiChannel;
            ih.midiProgram = ins->midiProgram;
            ih.midiBend = ins->midiBend;
            ih.mute = ins->mute ? 1 : 0;
            ih.junk[0] = ins->useTF4 ? 1 : 0; // store useTF4 flag
            ih.instrSize = INSTR_HEADER_SIZE;
            
            // Write sample headers (without data)
            for (int16_t k = 0; k < a; k++) {
                sample_t *s = &instr[j]->smp[k];
                xmSmpHdr_t *dst = &ih.smp[k];
                
                bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
                
                dst->length = s->length;
                dst->loopStart = s->loopStart;
                dst->loopLength = s->loopLength;
                
                if (sample16Bit) {
                    dst->length <<= 1;
                    dst->loopStart <<= 1;
                    dst->loopLength <<= 1;
                }
                
                dst->volume = s->volume;
                dst->finetune = s->finetune;
                dst->flags = s->flags & ~SAMPLE_STEREO; // Remove stereo flag for XM compatibility
                dst->panning = s->panning;
                dst->relativeNote = s->relativeNote;
                
                nameLength = (int32_t)strlen(s->name);
                if (nameLength > 22) nameLength = 22;
                dst->nameLength = (uint8_t)nameLength;
                
                memset(dst->name, ' ', 22);
                if (nameLength > 0) memcpy(dst->name, s->name, nameLength);
                
                // For DXM, we don't include sample data in XM section
                dst->length = 0;
            }
        } else {
            ih.instrSize = 22 + 11;
        }
        
        if (fwrite(&ih, ih.instrSize + (a * sizeof(xmSmpHdr_t)), 1, f) != 1) {
            return abortDxmSave(f, temporaryPath, "Error writing instrument!");
        }
    }
    
    // Get XM data size
    const long xmEnd = ftell(f);
    if (xmEnd < 8 || (uint64_t)(xmEnd - 8) > UINT32_MAX ||
        !writeDxmU32At(f, xmSizePos, xmEnd, (uint32_t)(xmEnd - 8))) {
        return abortDxmSave(f, temporaryPath, "Error finalizing XM module data!");
    }
    // 5. Before writing any chunks that contain raw sample data, temporarily unfix all
    //    samples so that we write the *original* data without the interpolation tap
    //    padding added by fixSample(). Otherwise a subsequently loaded module would
    //    have its samples fixed *again*, leading to duplicated edge taps and sample
    //    corruption.
    DXM_SAVE_TRACE("[DXM-SAVE] Unfixing all samples before WAV chunk write...\n");
    setDxmSamplesFixed(false);
    // 5. Write DSP state chunk (real data)
    // Cache persistent mixer state before writing to DXM
    if (ui.mixerScreenShown)
        cacheMixerStateFromGUI();
    else
        cacheMixerStateFromData();
    DXM_SAVE_TRACE("[DXM-SAVE] Writing DSP state chunk...\n");
    const char dspChunkId[8] = {'D','X','M','D','S','P',0,0};
    fwrite(dspChunkId, 1, 8, f);
    uint32_t dspChunkLen = 0;
    long lenPos = ftell(f);
    fwrite(&dspChunkLen, 1, 4, f); // reserved length, backpatched below
    long dataStart = ftell(f);
    size_t dspBytes = writeDSPStateChunk(f);
    // write mixer strip fader and pan for backwards-compatible state saving
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
        fwrite(&stereoMixerCh[ch].fader, sizeof(float), 1, f);
        fwrite(&stereoMixerCh[ch].pan,   sizeof(float), 1, f);
    }
    // write master gain
    fwrite(&mixerMasterGain, sizeof(float), 1, f);
    long dataEnd = ftell(f);
    if (!finishDxmChunk(f, lenPos, dataStart, dataEnd, &dspChunkLen)) {
        setDxmSamplesFixed(true);
        return abortDxmSave(f, temporaryPath, "Error finalizing DXM DSP state!");
    }
    // 6. Write WAV data chunk (real sample data)
    DXM_SAVE_TRACE("[DXM-SAVE] Writing WAV data chunk...\n");
    const char wavChunkId[8] = {'D','X','M','W','A','V',0,0};
    fwrite(wavChunkId, 1, 8, f);
    uint32_t wavChunkLen = 0;
    long wavLenPos = ftell(f);
    fwrite(&wavChunkLen, 1, 4, f); // reserved length, backpatched below
    long wavDataStart = ftell(f);
    size_t wavBytes = 0;
    const bool wavOK = writeAllSamplesDXMWAV(f, &wavBytes);
    long wavDataEnd = ftell(f);
    const bool wavChunkOK = finishDxmChunk(f, wavLenPos, wavDataStart, wavDataEnd, &wavChunkLen);
    // 7. After we are done writing the WAV chunk, re-apply fixSample() on all
    //    samples so that playback continues with the expected "fixed" sample data.
    DXM_SAVE_TRACE("[DXM-SAVE] Re-fixing all samples after WAV chunk write...\n");
    setDxmSamplesFixed(true);
    if (!wavOK || !wavChunkOK) {
        return abortDxmSave(f, temporaryPath, "Error writing DXM sample data!");
    }
    // 7. Write TF4 state chunk (real TF4 data)
    DXM_SAVE_TRACE("[DXM-SAVE] Writing TF4 state chunk...\n");
    const char tf4ChunkId[8] = {'D','X','M','T','F','4',0,0};
    fwrite(tf4ChunkId, 1, 8, f);
    uint32_t tf4ChunkLen = 0;
    long tf4LenPos = ftell(f);
    fwrite(&tf4ChunkLen, 1, 4, f); // reserved length, backpatched below
    long tf4DataStart = ftell(f);
    size_t tf4Bytes = writeAllTF4StatesDXM(f);
    long tf4DataEnd = ftell(f);
    if (!finishDxmChunk(f, tf4LenPos, tf4DataStart, tf4DataEnd, &tf4ChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing Tunefish state!");
    // Enhanced debug: print all TF4 instrument indices, slots, and names
    DXM_SAVE_TRACE("[DXM-SAVE] TF4 chunk: %zu bytes written\n", tf4Bytes);
    DXM_SAVE_TRACE("[DXM-SAVE] TF4 instruments saved:\n");
    for (int instrIdx = 1; instrIdx <= 128; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (ins && ins->useTF4) {
            DXM_SAVE_TRACE("  [DXM-SAVE] instrIdx=%d, synthSlot=%d, name=\"%s\"\n", instrIdx, instrIdx, ins->smp[0].name);
        }
    }

    // Write Dexed state chunk
    DXM_SAVE_TRACE("[DXM-SAVE] Writing Dexed state chunk...\n");
    const char dexChunkId[8] = {'D','X','M','D','E','X',0,0};
    fwrite(dexChunkId, 1, 8, f);
    uint32_t dexChunkLen = 0;
    long dexLenPos = ftell(f);
    fwrite(&dexChunkLen, 1, 4, f); // reserved length, backpatched below
    long dexDataStart = ftell(f);
    writeAllDexedStatesDXM(f);
    long dexDataEnd = ftell(f);
    if (dexDataEnd < dexDataStart || (uint64_t)(dexDataEnd - dexDataStart) > UINT32_MAX)
        return abortDxmSave(f, temporaryPath, "Dexed state is too large to save!");
    dexChunkLen = (uint32_t)(dexDataEnd - dexDataStart);

    // Add padding to make the chunk size a multiple of 4
    size_t padding = (4 - (dexChunkLen % 4)) % 4;
    for (size_t i = 0; i < padding; i++) {
        fputc(0, f);
    }
    dexDataEnd += padding;
    dexChunkLen += padding;

    if (!writeDxmU32At(f, dexLenPos, dexDataEnd, dexChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing Dexed state!");
    DXM_SAVE_TRACE("[DXM-SAVE] Dexed chunk: %u bytes written\n", dexChunkLen);

    // 8. Write V2 state chunk
    DXM_SAVE_TRACE("[DXM-SAVE] Writing V2 state chunk...\n");
    const char v2ChunkId[8] = {'D','X','M','V','2','S',0,0};
    fwrite(v2ChunkId, 1, 8, f);
    uint32_t v2ChunkLen = 0;
    long v2LenPos = ftell(f);
    fwrite(&v2ChunkLen, 1, 4, f);
    long v2DataStart = ftell(f);
    size_t v2Bytes = 0;
    const bool v2OK = writeAllV2StatesDXM(f, &v2Bytes);
    long v2DataEnd = ftell(f);
    if (!v2OK)
        return abortDxmSave(f, temporaryPath, "Could not serialize V2 instrument state!");
    if (!finishDxmChunk(f, v2LenPos, v2DataStart, v2DataEnd, &v2ChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing V2 instrument state!");
    DXM_SAVE_TRACE("[DXM-SAVE] V2 chunk: %zu bytes written\n", v2Bytes);

    // 9. Write OsTIrus full state chunk
    DXM_SAVE_TRACE("[DXM-SAVE] Writing OsTIrus state chunk...\n");
    const char ostChunkId[8] = {'D','X','M','O','T','S',0,0};
    fwrite(ostChunkId, 1, 8, f);
    uint32_t ostChunkLen = 0;
    long ostLenPos = ftell(f);
    fwrite(&ostChunkLen, 1, 4, f);
    long ostDataStart = ftell(f);
    size_t ostBytes = 0;
    const bool ostOK = writeAllOsTirusStatesDXM(f, &ostBytes);
    long ostDataEnd = ftell(f);
    if (!ostOK)
        return abortDxmSave(f, temporaryPath, "Could not serialize OsTIrus instrument state!");
    if (!finishDxmChunk(f, ostLenPos, ostDataStart, ostDataEnd, &ostChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing OsTIrus instrument state!");
    DXM_SAVE_TRACE("[DXM-SAVE] OsTIrus state chunk: %zu bytes written\n", ostBytes);

    // 10. Write OsTIrus arp editor state chunk
    DXM_SAVE_TRACE("[DXM-SAVE] Writing OsTIrus arp state chunk...\n");
    const char arpChunkId[8] = {'D','X','M','A','R','P',0,0};
    fwrite(arpChunkId, 1, 8, f);
    uint32_t arpChunkLen = 0;
    long arpLenPos = ftell(f);
    fwrite(&arpChunkLen, 1, 4, f);
    long arpDataStart = ftell(f);
    size_t arpBytes = writeOsTirusArpStateChunk(f);
    long arpDataEnd = ftell(f);
    if (!finishDxmChunk(f, arpLenPos, arpDataStart, arpDataEnd, &arpChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing OsTIrus arpeggiator state!");
    DXM_SAVE_TRACE("[DXM-SAVE] OsTIrus arp chunk: %zu bytes written\n", arpBytes);

    // 11. Write DXM instrument metadata chunk (distinguishes XM vs DXM instruments)
        // 12. Write Macro Map chunk
    DXM_SAVE_TRACE("[DXM-SAVE] Writing Macro Map chunk...\n");
    const char mapChunkId[8] = {'D','X','M','M','A','P',0,0};
    fwrite(mapChunkId, 1, 8, f);
    uint32_t mapChunkLen = 0;
    long mapLenPos = ftell(f);
    fwrite(&mapChunkLen, 1, 4, f);
    long mapDataStart = ftell(f);
    // Write macro mappings for all instruments
    for (int instrIdx = 1; instrIdx <= MAX_INST; instrIdx++) {
        instr_t *ins = instr[instrIdx];
        if (!ins) {
            uint8_t zerosT[16] = {0};
            fwrite(zerosT, sizeof(uint8_t), 16, f);
            uint16_t zerosP[16] = {0};
            fwrite(zerosP, sizeof(uint16_t), 16, f);
            uint8_t zerosS[16] = {0};
            fwrite(zerosS, sizeof(uint8_t), 16, f);
        } else {
            ft2_macro_map_sanitize_instrument(ins);
            fwrite(ins->macroTargetType, sizeof(uint8_t), 16, f);
            fwrite(ins->macroParamID,     sizeof(uint16_t),16, f);
            fwrite(ins->macroScale,       sizeof(uint8_t), 16, f);
        }
    }
    long mapDataEnd = ftell(f);
    if (!finishDxmChunk(f, mapLenPos, mapDataStart, mapDataEnd, &mapChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing macro map state!");
    // 9. Write Macro Pattern chunk (per-track macro mode + per-pattern macro data)
    DXM_SAVE_TRACE("[DXM-SAVE] Writing Macro Pattern chunk...\n");
    const char macChunkId[8] = {'D','X','M','M','A','C',0,0};
    fwrite(macChunkId, 1, 8, f);
    uint32_t macChunkLen = 0;
    long macLenPos = ftell(f);
    fwrite(&macChunkLen, 1, 4, f);
    long macDataStart = ftell(f);

    uint16_t pattCount = h.numPatterns;
    uint16_t chCount = MAX_STEREO_PAIRS;
    uint16_t macroModeMask = 0;
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++)
        if (editor.macroMode[ch]) macroModeMask |= (1u << ch);

    fwrite(&pattCount, sizeof(uint16_t), 1, f);
    fwrite(&chCount, sizeof(uint16_t), 1, f);
    fwrite(&macroModeMask, sizeof(uint16_t), 1, f);

    for (uint16_t p = 0; p < pattCount; p++)
    {
        const uint16_t rows = patternNumRows[p];
        fwrite(&rows, sizeof(uint16_t), 1, f);
        for (uint16_t r = 0; r < rows; r++)
        {
            for (uint16_t ch = 0; ch < MAX_STEREO_PAIRS; ch++)
            {
                macroNote_t mn = {0xFF, 0xFF, 0x00};
                if (macroPattern[p] != NULL)
                    mn = macroPattern[p][(r * MAX_STEREO_PAIRS) + ch];
                fwrite(&mn.slot, 1, 1, f);
                fwrite(&mn.mode, 1, 1, f);
                fwrite(&mn.value, 1, 1, f);
            }
        }
    }

    long macDataEnd = ftell(f);
    if (!finishDxmChunk(f, macLenPos, macDataStart, macDataEnd, &macChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing macro pattern state!");
    DXM_SAVE_TRACE("[DXM-SAVE] Writing DXM metadata chunk...\n");
    const char metaChunkId[8] = {'D','X','M','M','E','T',0,0};
    fwrite(metaChunkId, 1, 8, f);
    uint32_t metaChunkLen = 0;
    long metaLenPos = ftell(f);
    fwrite(&metaChunkLen, 1, 4, f); // reserved length, backpatched below
    long metaDataStart = ftell(f);
    size_t metaBytes = writeDXMInstrumentMetadata(f);
    long metaDataEnd = ftell(f);
    if (!finishDxmChunk(f, metaLenPos, metaDataStart, metaDataEnd, &metaChunkLen))
        return abortDxmSave(f, temporaryPath, "Error finalizing instrument metadata!");
    DXM_SAVE_TRACE("[DXM-SAVE] Metadata chunk: %zu bytes written\n", metaBytes);
    const bool writeOK = fflush(f) == 0 && !ferror(f);
    const bool closeOK = fclose(f) == 0;
    if (!writeOK || !closeOK)
    {
        return abortDxmSave(NULL, temporaryPath, "I/O error while finalizing DXM file!");
    }

    if (!commitDxmTemporaryFile(temporaryPath, filenameU))
        return abortDxmSave(NULL, temporaryPath, "Could not replace the destination module file!");
    free(temporaryPath);

	return true;
}
