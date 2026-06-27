/* Fasttracker II DXM loader (Extended XM: XM + DSP + WAV + TF4)
**
** This loader handles the custom DXM format, which is based on XM but includes:
**  - Embedded DSP mixer/effect state
**  - Stereo WAV audio data
**  - Tunefish4 instrument state/presets
**
** See ft2_load_xm.c for standard XM loading logic.
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "../ft2_structs.h"
#include "../ft2_dsp.h"
#include "../ft2_sample_ed.h"
#include "../ft2_synth.h"
#include "../ft2_unified_synth.h"
#include "../ft2_v2.h"
#include "../ft2_audio.h"
#include "../ft2_checkboxes.h"
#include "../ft2_header.h"
#include "../ft2_mixer.h"
#include "../ft2_replayer.h"
#include <math.h>
#include "../ft2_scrollbars.h"
#define GAIN_SLIDER_END 200
#include <stdlib.h>
#include <string.h>
#include "../ft2_module_loader.h" // for loadXM prototype and instrTmp
#include "../ft2_sample_ed.h" // for allocateSmpData

// External flag to indicate if we're loading a DXM file
extern bool isDXMFormat;

// Externs for TF4 synth integration
extern void* createInstrumentInstance(int instrID);
extern void ft2_synth_set_all_persistent_params(int instrID, const float* params, int paramCount);
extern void setSkipPresetOnCreate(bool skip);

#define FT2_TF4_PARAM_PTR
#define DXM_TF4_PARAM_COUNT 128

#include "../ft2_dexed.h"
extern int dx_unpack_program_from_storage(const uint8_t* packed128, uint8_t* outUnpacked155);

// Forward declaration for XM loader
bool loadXM(FILE *f, uint32_t filesize);

static void normalizeSynthFlags(instr_t *ins)
{
    if (ins == NULL)
        return;

    if (ins->useV2) {
        ins->useDexed = false;
        ins->useTF4 = false;
    } else if (ins->useDexed) {
        ins->useTF4 = false;
    }
}

static void restoreAllDexedStatesDXM(FILE *f, uint32_t chunkLen)
{
    uint8_t numDexed = 0;
    fread(&numDexed, sizeof(uint8_t), 1, f);
    printf("[DXM-LOAD] Dexed chunk contains %d instruments\n", numDexed);

    if (!ft2_dx_is_initialized()) {
        int sr = (audio.freq > 0) ? audio.freq : 44100;
        ft2_dx_init(sr);
    }
    
    for (uint8_t i = 0; i < numDexed; i++) {
        uint8_t instrIdx = 0;
        fread(&instrIdx, sizeof(uint8_t), 1, f);

        uint8_t patchData[155];
        fread(patchData, sizeof(uint8_t), 155, f);

        if (instrIdx < 1 || instrIdx > 128) continue;
        instr_t *ins = instrTmp[instrIdx];
        if (!ins) {
            if (!allocateTmpInstr(instrIdx))
                continue;
            ins = instrTmp[instrIdx];
        }
        if (!ins) continue;

        // Set the Dexed flag
        ins->useDexed = true;
        normalizeSynthFlags(ins);
        ins->isDXMInstrument = true;
        
        // Copy patch data: sanitize feedback byte to avoid invalid shift counts in Dexed engine.
        // DX7 feedback byte is stored at index 135. Clamp to [0..8] (FEEDBACK_BITDEPTH)
        // before copying into the persistent instrument storage. This prevents
        // malformed or out-of-range patch bytes from causing negative shift
        // counts (which can trigger SIGFPE) inside the Dexed render path.
        if (patchData[135] > 8) {
            printf("[DXM-LOAD] Sanitizing Dexed patch feedback byte for instr %d (was=%u -> clamped=8)\n", instrIdx, (unsigned)patchData[135]);
            patchData[135] = 8;
        }

        {
            uint8_t tmp_unpacked[155];

            /* Heuristic detection: Many DXM writers will store a full 155-byte unpacked
             * program. However older/alternate storage might embed a 128-byte packed
             * program inside the 155-byte slot (with the remaining bytes zeroed or unused).
             *
             * Simple detection:
             * - If a relatively large number of bytes in the tail region [128..154]
             *   are zero, we treat the incoming blob as likely-packed storage and
             *   attempt to unpack via the plugin wrapper.
             * - Otherwise assume the blob is already the 155-byte unpacked program
             *   and copy directly.
             *
             * This avoids unnecessary unpack attempts and prevents accidental
             * reinterpretation of valid unpacked programs.
             */
            int zero_count = 0;
            for (int z = 128; z < 155; ++z) {
                if (patchData[z] == 0) ++zero_count;
            }

            const int ZERO_THRESHOLD = 10; /* if >= 10 of 27 bytes are zero, consider packed */
            if (zero_count >= ZERO_THRESHOLD) {
                /* Likely packed storage: try to unpack using the plugin wrapper.
                 * If the unpack wrapper fails for any reason, fall back to copying
                 * the provided blob directly to avoid losing data.
                 */
                if (dx_unpack_program_from_storage(patchData, tmp_unpacked)) {
                    memcpy(ins->dxParams, tmp_unpacked, 155);
                } else {
                    memcpy(ins->dxParams, patchData, 155);
                }
            } else {
                /* Likely already-unpacked 155-byte program — copy directly. */
                memcpy(ins->dxParams, patchData, 155);
            }
        }

        printf("[DXM-LOAD] Restored Dexed patch for instr %d\n", instrIdx);
    }

    // Ensure Dexed instances are created and patches are applied immediately
    for (int i = 1; i <= 128; i++) {
        if (instrTmp[i] && instrTmp[i]->useDexed) {
            ft2_dx_load_patch_for_instrument(i, instrTmp[i]->dxParams, 155);
        }
    }

}

// Helper to skip bytes in a file
static bool skip_bytes(FILE *f, size_t n)
{
    return fseek(f, (long)n, SEEK_CUR) == 0;
}

// Helper: Restore DSP state from buffer
static void restoreDSPStateDXM(FILE *f, uint32_t chunkLen)
{
    // Restore mixer channel DSP chains (32 channels)
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
        dspEffectInstance_t *chain = stereoMixerCh[ch].effects;
        for (int slot = 0; slot < DSP_MAX_SLOTS; slot++) {
            dspEffectInstance_t *e = &chain[slot];
            uint8_t type = 0, np = 0;
            fread(&type, 1, 1, f);
            fread(&np, 1, 1, f);
            e->type = type;
            if (type != DSP_TYPE_NONE) {
                dspInitEffect(e, (dspEffectType_t)type, audio.freq);
                dspResetEffectState(e);
                e->enabled = true;
            } else {
                dspFreeEffect(e);
            }
            float *p = (float *)&e->params;
            for (int i = 0; i < np; i++)
                fread(&p[i], sizeof(float), 1, f);
        }
    }
    // Restore master chain
    for (int slot = 0; slot < DSP_MAX_SLOTS; slot++) {
        dspEffectInstance_t *e = &masterEffects[slot];
        uint8_t type = 0, np = 0;
        fread(&type, 1, 1, f);
        fread(&np, 1, 1, f);
        e->type = type;
        if (type != DSP_TYPE_NONE) {
            dspInitEffect(e, (dspEffectType_t)type, audio.freq);
            dspResetEffectState(e);
            e->enabled = true;
        } else {
            dspFreeEffect(e);
        }
        float *p = (float *)&e->params;
        for (int i = 0; i < np; i++)
            fread(&p[i], sizeof(float), 1, f);
    }
    // Restore mixer fader and pan values
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
        float fader, pan;
        fread(&fader, sizeof(float), 1, f);
        stereoMixerCh[ch].fader = fader;
        fread(&pan, sizeof(float), 1, f);
        stereoMixerCh[ch].pan = pan;
    }
    // Restore master gain
    fread(&mixerMasterGain, sizeof(float), 1, f);
}

// Helper: Restore all sample data (including stereo) from buffer
static void restoreAllSamplesDXMWAV(FILE *f, uint32_t chunkLen)
{
    uint16_t numSamples = 0;
    fread(&numSamples, sizeof(uint16_t), 1, f);
    for (uint16_t sidx = 0; sidx < numSamples; sidx++) {
        uint8_t instrIdx = 0, smpIdx = 0, flags = 0;
        uint32_t frames = 0;
        fread(&instrIdx, sizeof(uint8_t), 1, f);
        fread(&smpIdx, sizeof(uint8_t), 1, f);
        fread(&flags, sizeof(uint8_t), 1, f);
        fread(&frames, sizeof(uint32_t), 1, f);
        bool sample16Bit = flags & 1;
        bool stereo = flags & 2;
        printf("[DXM-LOAD] instr=%d smp=%d flags=0x%02X frames=%u 16bit=%d stereo=%d\n", instrIdx, smpIdx, flags, frames, sample16Bit, stereo);
        if (instrIdx < 1 || instrIdx > 128 || smpIdx >= 16) {
            printf("[DXM-LOAD] Skipping sample: invalid instrument/sample index\n");
            fseek(f, (sample16Bit ? 2 : 1) * frames * (stereo ? 2 : 1), SEEK_CUR);
            continue;
        }
        instr_t *ins = instrTmp[instrIdx];
        if (!ins) {
            printf("[DXM-LOAD] Skipping sample: instrument %d not allocated\n", instrIdx);
            continue;
        }
        sample_t *s = &ins->smp[smpIdx];
        
        // For DXM format, trust the WAV chunk format info, not the XM header flags
        // The XM header doesn't have stereo info, it comes from DXM metadata
        printf("[DXM-LOAD] Processing sample: 16bit=%d stereo=%d frames=%u\n", 
               sample16Bit, stereo, frames);
        
        // Allocate sample data if not already allocated
        if (s->length == 0 || s->dataPtrL == NULL) {
            printf("[DXM-LOAD] Allocating sample data for instr=%d smp=%d (length=%d, 16bit=%d, stereo=%d)\n", 
                   instrIdx, smpIdx, frames, sample16Bit, stereo);
            
                    // Set the sample length and preserve existing flags, only add stereo flag
        s->length = frames;
        printf("[DXM-LOAD] Setting flags: old=0x%02X, wav_flags=0x%02X (16bit=%d, stereo=%d)\n", 
               s->flags, flags, (flags & 1) ? 1 : 0, (flags & 2) ? 1 : 0);
        
        // Preserve existing flags (especially 16-bit flag) and only add stereo flag
        uint8_t newFlags = s->flags;
        if (stereo) newFlags |= SAMPLE_STEREO;
        s->flags = newFlags;
        
        printf("[DXM-LOAD] Final flags: 0x%02X (16bit=%d, stereo=%d)\n", 
               s->flags, (s->flags & SAMPLE_16BIT) ? 1 : 0, (s->flags & SAMPLE_STEREO) ? 1 : 0);
            
            // Allocate the sample data
            if (!allocateSmpData(s, frames, sample16Bit, stereo)) {
                printf("[DXM-LOAD] Failed to allocate sample data\n");
                fseek(f, (sample16Bit ? 2 : 1) * frames * (stereo ? 2 : 1), SEEK_CUR);
                continue;
            }
            
            printf("[DXM-LOAD] Sample data allocated: L=%p, R=%p\n", 
                   (void*)s->dataPtrL, (void*)s->dataPtrR);
        }
        
        // For stereo samples, ensure R channel is allocated
        if (stereo && s->dataPtrR == NULL) {
            printf("[DXM-LOAD] Skipping stereo sample: R channel not allocated\n");
            fseek(f, (sample16Bit ? 2 : 1) * frames * 2, SEEK_CUR);
            continue;
        }
        // Read sample data: interleaved L+R if stereo, planar L only if mono
        if (stereo && s->dataPtrL && s->dataPtrR) {
            // Use the same approach as WAV loader - read interleaved data and convert to planar
            int32_t bytesPerSample = sample16Bit ? 2 : 1;
            int32_t totalBytes = frames * bytesPerSample;
            
            // Allocate temporary buffer for interleaved data
            void *tempBuffer = malloc(totalBytes * 2); // L+R interleaved
            if (tempBuffer) {
                fread(tempBuffer, 1, totalBytes * 2, f);
                
                // Deinterleave to planar format (L and R channels) - same as WAV loader
                if (sample16Bit) {
                    int16_t *temp16 = (int16_t *)tempBuffer;
                    int16_t *srcL = (int16_t *)s->dataPtrL;
                    int16_t *srcR = (int16_t *)s->dataPtrR;
                    for (int32_t i = 0; i < frames; i++) {
                        srcL[i] = temp16[i * 2];     // L channel
                        srcR[i] = temp16[i * 2 + 1]; // R channel
                    }
                } else {
                    int8_t *temp8 = (int8_t *)tempBuffer;
                    int8_t *srcL = (int8_t *)s->dataPtrL;
                    int8_t *srcR = (int8_t *)s->dataPtrR;
                    for (int32_t i = 0; i < frames; i++) {
                        srcL[i] = temp8[i * 2];     // L channel
                        srcR[i] = temp8[i * 2 + 1]; // R channel
                    }
                }
                
                free(tempBuffer);
            }
        } else {
            // Mono sample - read planar data
            if (sample16Bit)
                fread(s->dataPtrL, sizeof(int16_t), frames, f);
            else
                fread(s->dataPtrL, sizeof(int8_t), frames, f);
        }
        
        // Debug: Verify sample data after reading
        if (stereo && s->dataPtrL && s->dataPtrR) {
            printf("[DXM-LOAD] After reading: L[0]=%d, R[0]=%d\n", 
                   sample16Bit ? ((int16_t*)s->dataPtrL)[0] : s->dataPtrL[0],
                   sample16Bit ? ((int16_t*)s->dataPtrR)[0] : s->dataPtrR[0]);
            
            // Debug: Check if L and R are different (should be for proper stereo)
            if (sample16Bit) {
                int16_t *l = (int16_t *)s->dataPtrL;
                int16_t *r = (int16_t *)s->dataPtrR;
                bool different = false;
                for (int32_t i = 0; i < 10 && i < frames; i++) {
                    if (l[i] != r[i]) {
                        different = true;
                        break;
                    }
                }
                printf("[DXM-LOAD] L and R channels are %s\n", different ? "DIFFERENT (good)" : "IDENTICAL (bad)");
            }
        }
        
        // Debug output for loaded sample
        if (sample16Bit && stereo && s->dataPtrL && s->dataPtrR) {
            int16_t *l = (int16_t *)s->dataPtrL;
            int16_t *r = (int16_t *)s->dataPtrR;
            printf("[DXM-LOAD] First 4 L: %d %d %d %d\n", l[0], l[1], l[2], l[3]);
            printf("[DXM-LOAD] First 4 R: %d %d %d %d\n", r[0], r[1], r[2], r[3]);
        }
        
        // Debug: Check if sample data is properly allocated
        if (stereo && s->dataPtrL && s->dataPtrR) {
            printf("[DXM-LOAD] Sample allocated: L=%p, R=%p, length=%d\n", 
                   (void*)s->dataPtrL, (void*)s->dataPtrR, s->length);
        }
        
        // Note: No sign conversion needed - data is already in correct format
        // The WAV loader doesn't do sign conversion, so we don't either
        
        // Note: fixSample() is called later in setupLoadedModule() to avoid double-processing
    }
}

// Helper: Restore all TF4 instrument states from buffer
static void restoreAllTF4StatesDXM(FILE *f, uint32_t chunkLen)
{
    uint8_t numTF4 = 0;
    fread(&numTF4, sizeof(uint8_t), 1, f);
    printf("[DXM-LOAD] TF4 chunk contains %d instruments\n", numTF4);
    
    // CRITICAL: Skip preset loading during DXM restore to prevent overwriting loaded parameters
    setSkipPresetOnCreate(true);

    if (!ft2_synth_is_initialized()) {
        int sr = (audio.freq > 0) ? audio.freq : 44100;
        ft2_unified_synth_set_samplerate(sr);
    }
    
    for (uint8_t i = 0; i < numTF4; i++) {
        uint8_t instrIdx = 0, nameLen = 0;
        fread(&instrIdx, sizeof(uint8_t), 1, f);
        fread(&nameLen, sizeof(uint8_t), 1, f);
        char name[23] = {0};
        if (nameLen > 0 && nameLen < 23)
            fread(name, sizeof(char), nameLen, f);
        float params[DXM_TF4_PARAM_COUNT] = {0};
        fread(params, sizeof(float), DXM_TF4_PARAM_COUNT, f);
        printf("[DXM-LOAD] TF4 instr=%d name='%.*s'\n", instrIdx, nameLen, name);
        printf("[DXM-LOAD] TF4 params[0..3]: %.3f %.3f %.3f %.3f\n", params[0], params[1], params[2], params[3]);
        if (instrIdx < 1 || instrIdx > 128) continue;
        instr_t *ins = instrTmp[instrIdx];
        if (!ins) {
            if (!allocateTmpInstr(instrIdx))
                continue;
            ins = instrTmp[instrIdx];
        }
        if (!ins) continue;

        // Set the TF4 flag before any UI or synth refresh
        ins->useTF4 = true;
        normalizeSynthFlags(ins);
        printf("[DXM-LOAD] Set useTF4=TRUE for instr %d, addr=%p\n", instrIdx, (void*)ins);
        strncpy(ins->smp[0].name, name, 22);
        int tf4Idx = instrIdx;
        if (tf4Idx < 1 || tf4Idx > 128) continue;
        
        // CRITICAL: Set persistent parameters FIRST, then create the instance
        ft2_synth_set_all_persistent_params(tf4Idx, params, DXM_TF4_PARAM_COUNT);
        
        // Now create the TF4 instance - it will pick up the persistent parameters
        void* tf4inst = createInstrumentInstance(tf4Idx);
        // Debug print
        printf("[DXM] Created TF4 instance for instr %d (synthIdx=%d): %p\n", instrIdx, tf4Idx, tf4inst);
        // If you have a tf4Params pointer, copy params there:
        #ifdef FT2_TF4_PARAM_PTR
        memcpy(ins->tf4Params, params, DXM_TF4_PARAM_COUNT * sizeof(float));
        #endif
        printf("[DXM-LOAD] After param copy, useTF4 for instr %d = %d\n", instrIdx, ins->useTF4);
    }
    
    // Re-enable preset loading for future instruments without persistent state
    setSkipPresetOnCreate(false);
}

// Helper: Restore all V2 instrument states from buffer
static void restoreAllV2StatesDXM(FILE *f, uint32_t chunkLen)
{
    (void)chunkLen;
    uint8_t numV2 = 0;
    fread(&numV2, sizeof(uint8_t), 1, f);
    printf("[DXM-LOAD] V2 chunk contains %d instruments\n", numV2);

    for (uint8_t i = 0; i < numV2; i++) {
        uint8_t instrIdx = 0;
        uint32_t blobLen = 0;
        fread(&instrIdx, sizeof(uint8_t), 1, f);
        fread(&blobLen, sizeof(uint32_t), 1, f);

        if (instrIdx < 1 || instrIdx > 128 || blobLen == 0) {
            if (blobLen > 0)
                skip_bytes(f, blobLen);
            continue;
        }

        if (blobLen > chunkLen || blobLen > (16u * 1024u * 1024u)) {
            printf("[DXM-LOAD] WARNING: Invalid V2 blob length %u for instr %u\n", blobLen, instrIdx);
            return;
        }

        uint8_t *blob = (uint8_t *)malloc(blobLen);
        if (!blob) {
            skip_bytes(f, blobLen);
            continue;
        }

        if (fread(blob, 1, blobLen, f) != blobLen) {
            free(blob);
            return;
        }

        instr_t *ins = instrTmp[instrIdx];
        if (!ins) {
            if (!allocateTmpInstr(instrIdx)) {
                free(blob);
                continue;
            }
            ins = instrTmp[instrIdx];
        }
        if (!ins) {
            free(blob);
            continue;
        }

        if (ft2_v2_deserialize_state(instrIdx, blob, blobLen)) {
            ins->useV2 = true;
            normalizeSynthFlags(ins);
            ins->isDXMInstrument = true;
            printf("[DXM-LOAD] Restored V2 state for instr %d\n", instrIdx);
        } else {
            printf("[DXM-LOAD] WARNING: Failed to restore V2 state for instr %d\n", instrIdx);
        }

        free(blob);
    }
}

// Helper: Restore DXM instrument metadata to distinguish from XM instruments
static void restoreDXMInstrumentMetadata(FILE *f, uint32_t chunkLen)
{
    uint8_t numDXMInstruments = 0;
    fread(&numDXMInstruments, sizeof(uint8_t), 1, f);
    
    printf("[DXM-LOAD] Found %d DXM instruments\n", numDXMInstruments);
    
    // Read DXM instrument indices and flags
    for (uint8_t i = 0; i < numDXMInstruments; i++) {
        uint8_t instrIdx = 0, flags = 0;
        fread(&instrIdx, sizeof(uint8_t), 1, f);
        fread(&flags, sizeof(uint8_t), 1, f);
        
        if (instrIdx < 1 || instrIdx > 128) continue;
        instr_t *ins = instrTmp[instrIdx];
        if (!ins) {
            if (!allocateTmpInstr(instrIdx))
                continue;
            ins = instrTmp[instrIdx];
        }
        if (!ins) continue;
        
        printf("[DXM-LOAD] DXM instr %d: flags=0x%02X (TF4=%d, stereo=%d, Dexed=%d, V2=%d)\n", 
               instrIdx, flags, (flags & 1) ? 1 : 0, (flags & 2) ? 1 : 0, (flags & 4) ? 1 : 0, (flags & 8) ? 1 : 0);
        
        // Mark this as a DXM instrument (not a regular XM instrument)
        // This will be used to determine how to handle stereo samples
        ins->isDXMInstrument = true;
        
        // Set TF4 flag if indicated
        if (flags & 1) {
            ins->useTF4 = true;
            printf("[DXM-LOAD] Set TF4 flag for instrument %d from metadata\n", instrIdx);
        }
        if (flags & 4) {
            ins->useDexed = true;
            printf("[DXM-LOAD] Set Dexed flag for instrument %d from metadata\n", instrIdx);
        }
        if (flags & 8) {
            ins->useV2 = true;
            printf("[DXM-LOAD] Set V2 flag for instrument %d from metadata\n", instrIdx);
        }
        normalizeSynthFlags(ins);
        
        // Note: Stereo sample flags are already set by the WAV chunk loader
        // This metadata just confirms which instruments are DXM vs XM
    }
}

// Helper: Restore Macro Map chunk
static void restoreMacroMapDXM(FILE *f, uint32_t chunkLen)
{
    uint32_t expectedLen = MAX_INST * (16 * sizeof(uint8_t) + 16 * sizeof(uint16_t) + 16 * sizeof(uint8_t));
    if (chunkLen < expectedLen) {
        // not enough data for macro maps, skip entire chunk
        skip_bytes(f, chunkLen);
        return;
    }
    for (int instrIdx = 1; instrIdx <= MAX_INST; instrIdx++) {
        instr_t *ins = instrTmp[instrIdx];
        if (!ins) {
            if (allocateTmpInstr(instrIdx))
                ins = instrTmp[instrIdx];
        }
        // Read target types
        for (int i = 0; i < 16; i++) {
            uint8_t t;
            fread(&t, sizeof(uint8_t), 1, f);
            if (ins) ins->macroTargetType[i] = t;
        }
        // Read parameter IDs
        for (int i = 0; i < 16; i++) {
            uint16_t pid;
            fread(&pid, sizeof(uint16_t), 1, f);
            if (ins) ins->macroParamID[i] = pid;
        }
        // Read scales
        for (int i = 0; i < 16; i++) {
            uint8_t s;
            fread(&s, sizeof(uint8_t), 1, f);
            if (ins) ins->macroScale[i] = s;
        }
    }
    // Skip any extra bytes beyond expected
    if (chunkLen > expectedLen) skip_bytes(f, chunkLen - expectedLen);
}

// Helper: Restore Macro Pattern chunk (per-track macro mode + per-pattern macro data)
static void restoreMacroPatternDXM(FILE *f, uint32_t chunkLen)
{
    if (chunkLen < sizeof(uint16_t) * 3)
    {
        skip_bytes(f, chunkLen);
        return;
    }

    uint16_t pattCount = 0;
    uint16_t chCount = 0;
    uint16_t macroModeMask = 0;
    fread(&pattCount, sizeof(uint16_t), 1, f);
    fread(&chCount, sizeof(uint16_t), 1, f);
    fread(&macroModeMask, sizeof(uint16_t), 1, f);

    if (chCount != MAX_STEREO_PAIRS)
    {
        // Unsupported channel count, skip remainder
        skip_bytes(f, chunkLen - sizeof(uint16_t) * 3);
        return;
    }

    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++)
        editor.macroMode[ch] = (macroModeMask >> ch) & 1;

    if (pattCount > MAX_PATTERNS)
        pattCount = MAX_PATTERNS;

    for (uint16_t p = 0; p < pattCount; p++)
    {
        uint16_t rows = 0;
        fread(&rows, sizeof(uint16_t), 1, f);
        if (rows > MAX_PATT_LEN)
            rows = MAX_PATT_LEN;

        if (macroPattern[p] == NULL)
        {
            macroPattern[p] = (macroNote_t *)malloc(MAX_PATT_LEN * MAX_STEREO_PAIRS * sizeof (macroNote_t));
            if (macroPattern[p] == NULL)
            {
                // skip remaining data if allocation failed
                skip_bytes(f, (chunkLen - sizeof(uint16_t) * 3) - sizeof(uint16_t));
                return;
            }
            memset(macroPattern[p], 0xFF, MAX_PATT_LEN * MAX_STEREO_PAIRS * sizeof (macroNote_t));
        }

        for (uint16_t r = 0; r < rows; r++)
        {
            for (uint16_t ch = 0; ch < MAX_STEREO_PAIRS; ch++)
            {
                macroNote_t *mn = &macroPattern[p][(r * MAX_STEREO_PAIRS) + ch];
                fread(&mn->slot, 1, 1, f);
                fread(&mn->mode, 1, 1, f);
                fread(&mn->value, 1, 1, f);
            }
        }
    }
}

// DXM loader: verify header, scan chunks, and restore state
bool loadDXM(FILE *f, uint32_t filesize)
{
    // 1. Read and verify DXM magic header
    char dxmMagic[4];
    if (fread(dxmMagic, 1, 4, f) != 4 || dxmMagic[0]!='D' || dxmMagic[1]!='X' || dxmMagic[2]!='M' || dxmMagic[3]!='0')
        return false;

    // 2. Read XM data size (uint32_t)
    uint32_t xmSize = 0;
    if (fread(&xmSize, 1, 4, f) != 4)
        return false;

    // 3. Read XM data into a buffer
    char *xmBuf = (char *)malloc(xmSize);
    if (!xmBuf || fread(xmBuf, 1, xmSize, f) != xmSize) { free(xmBuf); return false; }

FILE *xmFile = NULL;
#ifdef _GNU_SOURCE
    xmFile = fmemopen(xmBuf, xmSize, "rb");
#else
    xmFile = tmpfile();
    if (xmFile == NULL) { free(xmBuf); return false; }
    if (fwrite(xmBuf, 1, xmSize, xmFile) != xmSize) {
        fclose(xmFile);
        free(xmBuf);
        return false;
    }
    rewind(xmFile);
#endif
    if (!xmFile) { free(xmBuf); return false; }
    
    // Set DXM format flag before calling loadXM so stereo samples are preserved
    isDXMFormat = true;
    bool xmOk = loadXM(xmFile, xmSize);
    fclose(xmFile);
    free(xmBuf);
    if (!xmOk) return false;
    
    // Ensure all instruments are properly allocated before restoring sample data
    printf("[DXM-LOAD] XM loading completed, checking instrument allocation...\n");
    for (int i = 1; i <= 128; i++) {
        if (instrTmp[i] != NULL) {
            printf("[DXM-LOAD] Instrument %d allocated: %p\n", i, (void*)instrTmp[i]);
        }
    }

    // 4. Read chunks (scan until EOF)
    char chunkId[8];
    while (ftell(f) < (long)filesize) {
        if (fread(chunkId, 1, 8, f) != 8) break;
        uint32_t chunkLen = 0;
        if (fread(&chunkLen, 1, 4, f) != 4) break;
        if (chunkLen == 0) continue;
        printf("[DXM-LOAD] Found chunk: %.6s, length=%d\n", chunkId, chunkLen);
        if (memcmp(chunkId, "DXMDSP", 6) == 0) {
            restoreDSPStateDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMWAV", 6) == 0) {
            // Process WAV chunk - this contains the actual stereo sample data
            restoreAllSamplesDXMWAV(f, chunkLen);
        } else if (memcmp(chunkId, "DXMTF4", 6) == 0) {
            printf("[DXM-LOAD] Found TF4 chunk, length=%d\n", chunkLen);
            restoreAllTF4StatesDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMDEX", 6) == 0) {
            printf("[DXM-LOAD] Found Dexed chunk, length=%d\n", chunkLen);
            restoreAllDexedStatesDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMV2S", 6) == 0) {
            printf("[DXM-LOAD] Found V2 chunk, length=%d\n", chunkLen);
            restoreAllV2StatesDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMMAP", 6) == 0) {
            printf("[DXM-LOAD] Found Macro Map chunk, length=%u\n", chunkLen);
            restoreMacroMapDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMMAC", 6) == 0) {
            printf("[DXM-LOAD] Found Macro Pattern chunk, length=%u\n", chunkLen);
            restoreMacroPatternDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMMET", 6) == 0) {
            restoreDXMInstrumentMetadata(f, chunkLen);
        } else {
            // Unknown chunk, skip
            fseek(f, chunkLen, SEEK_CUR);
        }
    }

        // Cache persistent mixer/DSP state after loading DXM
        cacheMixerStateFromData();

    // Final post-load synth init pass (cold boot safety)
    if (!ft2_synth_is_initialized()) {
        int sr = (audio.freq > 0) ? audio.freq : 44100;
        ft2_unified_synth_set_samplerate(sr);
    }
    if (!ft2_dx_is_initialized()) {
        int sr = (audio.freq > 0) ? audio.freq : 44100;
        ft2_dx_init(sr);
    }

    for (int i = 1; i <= 128; i++) {
        if (instrTmp[i] && instrTmp[i]->useTF4) {
            createInstrumentInstance(i);
        }
        if (instrTmp[i] && instrTmp[i]->useDexed) {
            ft2_dx_load_patch_for_instrument(i, instrTmp[i]->dxParams, 155);
        }
    }

        // Sync mixer GUI scrollbars if screen visible
    if (ui.mixerScreenShown) {
        for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
            // Gain
            int gainUnits = (int)lrintf(mixerCh[ch].fader * 100.0f);
            int gainPos = GAIN_SLIDER_END - gainUnits;
            setScrollBarPos(SB_MIX_GAIN_0 + ch, gainPos, false);
            // Pan
            int panUnits = (int)lrintf((mixerCh[ch].pan + 1.0f) * 100.0f);
            setScrollBarPos(SB_MIX_PAN_0 + ch, panUnits, false);
        }
        // Master gain
        int masterUnits = (int)lrintf(mixerMasterGain * 100.0f);
        int masterPos = GAIN_SLIDER_END - masterUnits;
        setScrollBarPos(SB_MIX_MASTER_GAIN, masterPos, false);
    }

    // If we got here, the structure is valid
    return true;
}
