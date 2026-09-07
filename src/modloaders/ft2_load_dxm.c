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
#include "../ft2_ostirus.h"
#include "../ft2_audio.h"
#include "../ft2_checkboxes.h"
#include "../ft2_header.h"
#include "../ft2_mixer.h"
#include "../ft2_replayer.h"
#include "../ft2_macro_map.h"
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
// Forward declaration for XM loader
bool loadXM(FILE *f, uint32_t filesize);

typedef struct pendingSynthBlob_t
{
    uint8_t *data;
    uint32_t size;
} pendingSynthBlob_t;

static pendingSynthBlob_t g_pendingV2States[MAX_INST + 1];
static pendingSynthBlob_t g_pendingOsTirusStates[MAX_INST + 1];

static void free_pending_blob(pendingSynthBlob_t *blob)
{
    if (blob == NULL)
        return;

    free(blob->data);
    blob->data = NULL;
    blob->size = 0;
}

static bool store_pending_blob(pendingSynthBlob_t *dst, const uint8_t *src, uint32_t size)
{
    if (dst == NULL || src == NULL || size == 0)
        return false;

    uint8_t *copy = (uint8_t *)malloc(size);
    if (copy == NULL)
        return false;

    memcpy(copy, src, size);
    free_pending_blob(dst);
    dst->data = copy;
    dst->size = size;
    return true;
}

static void normalizeSynthFlags(instr_t *ins)
{
    if (ins == NULL)
        return;

    if (ins->useOsTirus) {
        ins->useV2 = false;
        ins->useDexed = false;
        ins->useTF4 = false;
    } else if (ins->useV2) {
        ins->useDexed = false;
        ins->useTF4 = false;
    } else if (ins->useDexed) {
        ins->useTF4 = false;
    }
}

static bool skip_bytes(FILE *f, size_t n);
static bool read_chunk_data(FILE *f, void *dst, size_t size, size_t *remaining);

static bool restoreAllDexedStatesDXM(FILE *f, uint32_t chunkLen)
{
    size_t remaining = chunkLen;
    uint8_t numDexed = 0;
    if (!read_chunk_data(f, &numDexed, sizeof(numDexed), &remaining) ||
        (size_t)numDexed * 156 > remaining)
        return false;
    
    for (uint8_t i = 0; i < numDexed; i++) {
        uint8_t instrIdx = 0;
        uint8_t patchData[155];
        if (!read_chunk_data(f, &instrIdx, sizeof(instrIdx), &remaining) ||
            !read_chunk_data(f, patchData, sizeof(patchData), &remaining))
            return false;

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
        
        /* DXMDEX has always written the 155-byte unpacked representation. A
         * content heuristic here corrupted legitimate patches with sparse tails. */
        if (patchData[135] > 7) patchData[135] = 7;
        memcpy(ins->dxParams, patchData, sizeof(ins->dxParams));
    }

    return remaining == 0 || skip_bytes(f, remaining); /* writer padding */
}

// Helper to skip bytes in a file
static bool skip_bytes(FILE *f, size_t n)
{
    return fseek(f, (long)n, SEEK_CUR) == 0;
}

static bool read_chunk_data(FILE *f, void *dst, size_t size, size_t *remaining)
{
    if (size > *remaining || fread(dst, 1, size, f) != size)
        return false;
    *remaining -= size;
    return true;
}

static size_t dxm_sample_payload_size(uint32_t frames, bool sample16Bit, bool stereo)
{
    size_t bytes = (size_t)frames;
    bytes *= sample16Bit ? sizeof(int16_t) : sizeof(int8_t);
    if (stereo)
        bytes *= 2u;
    return bytes;
}

#define DXM_DSP_PARAM_CAPACITY (sizeof(((dspEffectInstance_t *)0)->params) / sizeof(float))
typedef struct dxmDiskEffect_t
{
    uint8_t type, numParams;
    float params[DXM_DSP_PARAM_CAPACITY];
} dxmDiskEffect_t;

// Validate the complete DSP chunk before changing live mixer state.
static bool restoreDSPStateDXM(FILE *f, uint32_t chunkLen)
{
    size_t remaining = chunkLen;
    dxmDiskEffect_t effects[MAX_STEREO_PAIRS + 1][DSP_MAX_SLOTS] = {0};
    float faders[MAX_STEREO_PAIRS], pans[MAX_STEREO_PAIRS], masterGain;

    for (int chain = 0; chain <= MAX_STEREO_PAIRS; ++chain)
    {
        for (int slot = 0; slot < DSP_MAX_SLOTS; ++slot)
        {
            dxmDiskEffect_t *disk = &effects[chain][slot];
            if (!read_chunk_data(f, &disk->type, 1, &remaining) ||
                !read_chunk_data(f, &disk->numParams, 1, &remaining) ||
                disk->type >= DSP_TYPE_COUNT)
                return false;

            int expectedParams = 0;
            dspGetParamInfo((dspEffectType_t)disk->type, &expectedParams);
            if (expectedParams < 0 || (size_t)expectedParams > DXM_DSP_PARAM_CAPACITY ||
                disk->numParams != expectedParams ||
                !read_chunk_data(f, disk->params, (size_t)expectedParams * sizeof(float), &remaining))
                return false;

            for (int param = 0; param < expectedParams; ++param)
                if (!isfinite(disk->params[param])) return false;
        }
    }

    for (int ch = 0; ch < MAX_STEREO_PAIRS; ++ch)
    {
        if (!read_chunk_data(f, &faders[ch], sizeof(float), &remaining) ||
            !read_chunk_data(f, &pans[ch], sizeof(float), &remaining) ||
            !isfinite(faders[ch]) || faders[ch] < 0.0f || faders[ch] > 2.0f ||
            !isfinite(pans[ch]) || pans[ch] < -1.0f || pans[ch] > 1.0f)
            return false;
    }
    if (!read_chunk_data(f, &masterGain, sizeof(float), &remaining) ||
        !isfinite(masterGain) || masterGain < 0.0f || masterGain > 2.0f ||
        (remaining != 0 && !skip_bytes(f, remaining)))
        return false;

    for (int chain = 0; chain <= MAX_STEREO_PAIRS; ++chain)
    {
        dspEffectInstance_t *target = chain == MAX_STEREO_PAIRS ? masterEffects : stereoMixerCh[chain].effects;
        for (int slot = 0; slot < DSP_MAX_SLOTS; ++slot)
        {
            const dxmDiskEffect_t *disk = &effects[chain][slot];
            dspEffectInstance_t *effect = &target[slot];
            if (disk->type == DSP_TYPE_NONE) dspFreeEffect(effect);
            else {
                dspInitEffect(effect, (dspEffectType_t)disk->type, audio.freq);
                memcpy(&effect->params, disk->params, (size_t)disk->numParams * sizeof(float));
                dspResetEffectState(effect);
                effect->enabled = true;
            }
        }
    }
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ++ch) {
        stereoMixerCh[ch].fader = faders[ch];
        stereoMixerCh[ch].pan = pans[ch];
    }
    mixerMasterGain = masterGain;
    return true;
}

// Restore sample payloads only after their complete bytes have been read.
static bool restoreAllSamplesDXMWAV(FILE *f, uint32_t chunkLen)
{
    size_t remaining = chunkLen;
    uint16_t numSamples;
    if (!read_chunk_data(f, &numSamples, sizeof(numSamples), &remaining) || numSamples > MAX_INST * MAX_SMP_PER_INST)
        return false;

    for (uint16_t entry = 0; entry < numSamples; ++entry)
    {
        uint8_t instrIdx, smpIdx, flags;
        uint32_t frames;
        if (!read_chunk_data(f, &instrIdx, sizeof(instrIdx), &remaining) ||
            !read_chunk_data(f, &smpIdx, sizeof(smpIdx), &remaining) ||
            !read_chunk_data(f, &flags, sizeof(flags), &remaining) ||
            !read_chunk_data(f, &frames, sizeof(frames), &remaining) ||
            (flags & ~3u) != 0 || frames == 0 || frames > MAX_SAMPLE_LEN)
            return false;

        const bool sample16Bit = (flags & 1) != 0;
        const bool stereo = (flags & 2) != 0;
        const size_t payloadSize = dxm_sample_payload_size(frames, sample16Bit, stereo);
        if (payloadSize > remaining)
            return false;

        uint8_t *payload = (uint8_t *)malloc(payloadSize);
        if (payload == NULL || !read_chunk_data(f, payload, payloadSize, &remaining)) {
            free(payload);
            return false;
        }

        if (instrIdx < 1 || instrIdx > MAX_INST || smpIdx >= MAX_SMP_PER_INST || instrTmp[instrIdx] == NULL) {
            free(payload);
            return false;
        }

        sample_t *sample = &instrTmp[instrIdx]->smp[smpIdx];
        if (!allocateSmpData(sample, (int32_t)frames, sample16Bit, stereo)) {
            free(payload);
            return false;
        }
        sample->length = (int32_t)frames;
        sample->flags = (sample->flags & (LOOP_FWD | LOOP_BIDI)) |
            (sample16Bit ? SAMPLE_16BIT : 0) | (stereo ? SAMPLE_STEREO : 0);

        if (!stereo) {
            memcpy(sample->dataPtrL, payload, payloadSize);
        } else if (sample16Bit) {
            const int16_t *source = (const int16_t *)payload;
            int16_t *left = (int16_t *)sample->dataPtrL;
            int16_t *right = (int16_t *)sample->dataPtrR;
            for (uint32_t frame = 0; frame < frames; ++frame) {
                left[frame] = source[frame * 2];
                right[frame] = source[frame * 2 + 1];
            }
        } else {
            const int8_t *source = (const int8_t *)payload;
            for (uint32_t frame = 0; frame < frames; ++frame) {
                sample->dataPtrL[frame] = source[frame * 2];
                sample->dataPtrR[frame] = source[frame * 2 + 1];
            }
        }
        free(payload);
    }

    return remaining == 0 || skip_bytes(f, remaining);
}

// Helper: Restore all TF4 instrument states from buffer
static bool restoreAllTF4StatesDXM(FILE *f, uint32_t chunkLen)
{
    size_t remaining = chunkLen;
    uint8_t numTF4 = 0;
    if (!read_chunk_data(f, &numTF4, sizeof(numTF4), &remaining))
        return false;
    
    for (uint8_t i = 0; i < numTF4; i++) {
        uint8_t instrIdx = 0, nameLen = 0;
        if (!read_chunk_data(f, &instrIdx, sizeof(instrIdx), &remaining) ||
            !read_chunk_data(f, &nameLen, sizeof(nameLen), &remaining) || nameLen > 22)
            return false;
        char name[23] = {0};
        if (!read_chunk_data(f, name, nameLen, &remaining))
            return false;
        float params[DXM_TF4_PARAM_COUNT] = {0};
        if (!read_chunk_data(f, params, sizeof(params), &remaining))
            return false;
        for (int param = 0; param < DXM_TF4_PARAM_COUNT; ++param)
            if (!isfinite(params[param]) || params[param] < 0.0f || params[param] > 1.0f)
                return false;
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
        strncpy(ins->smp[0].name, name, 22);
        ins->smp[0].name[22] = '\0';

        // If you have a tf4Params pointer, copy params there:
        #ifdef FT2_TF4_PARAM_PTR
        memcpy(ins->tf4Params, params, DXM_TF4_PARAM_COUNT * sizeof(float));
        #endif
    }

    return remaining == 0 || skip_bytes(f, remaining);
}

// Helper: Restore all V2 instrument states from buffer
static bool restorePendingSynthStatesDXM(FILE *f, uint32_t chunkLen,
    pendingSynthBlob_t states[MAX_INST + 1], bool osTirus)
{
    size_t remaining = chunkLen;
    uint8_t count = 0;
    bool seen[MAX_INST + 1] = { false };
    if (!read_chunk_data(f, &count, sizeof(count), &remaining) || count > MAX_INST || (size_t)count * 5u > remaining)
        return false;

    for (uint8_t i = 0; i < count; ++i) {
        uint8_t instrIdx = 0;
        uint32_t blobLen = 0;
        if (!read_chunk_data(f, &instrIdx, sizeof(instrIdx), &remaining) ||
            !read_chunk_data(f, &blobLen, sizeof(blobLen), &remaining) ||
            instrIdx < 1 || instrIdx > MAX_INST || seen[instrIdx] ||
            blobLen == 0 || blobLen > (16u * 1024u * 1024u) || blobLen > remaining)
            return false;
        seen[instrIdx] = true;

        uint8_t *blob = (uint8_t *)malloc(blobLen);
        if (blob == NULL || !read_chunk_data(f, blob, blobLen, &remaining)) {
            free(blob);
            return false;
        }

        instr_t *ins = instrTmp[instrIdx];
        if (ins == NULL && allocateTmpInstr(instrIdx))
            ins = instrTmp[instrIdx];
        if (ins == NULL || !store_pending_blob(&states[instrIdx], blob, blobLen)) {
            free(blob);
            return false;
        }

        if (osTirus) {
            ins->useOsTirus = true;
        } else {
            ins->useV2 = true;
        }
        normalizeSynthFlags(ins);
        ins->isDXMInstrument = true;
        free(blob);
    }

    return remaining == 0 || skip_bytes(f, remaining);
}

static bool restoreAllV2StatesDXM(FILE *f, uint32_t chunkLen)
{
    return restorePendingSynthStatesDXM(f, chunkLen, g_pendingV2States, false);
}

static bool restoreAllOsTirusStatesDXM(FILE *f, uint32_t chunkLen)
{
    return restorePendingSynthStatesDXM(f, chunkLen, g_pendingOsTirusStates, true);
}

// Helper: Restore DXM instrument metadata to distinguish from XM instruments
static bool restoreDXMInstrumentMetadata(FILE *f, uint32_t chunkLen)
{
    size_t remaining = chunkLen;
    uint8_t numDXMInstruments = 0;
    bool seen[MAX_INST + 1] = { false };
    if (!read_chunk_data(f, &numDXMInstruments, sizeof(numDXMInstruments), &remaining) ||
        numDXMInstruments > MAX_INST || (size_t)numDXMInstruments * 2u > remaining)
        return false;
    
    // Read DXM instrument indices and flags
    for (uint8_t i = 0; i < numDXMInstruments; i++) {
        uint8_t instrIdx = 0, flags = 0;
        if (!read_chunk_data(f, &instrIdx, sizeof(instrIdx), &remaining) ||
            !read_chunk_data(f, &flags, sizeof(flags), &remaining) ||
            instrIdx < 1 || instrIdx > MAX_INST || seen[instrIdx] || (flags & ~31u) != 0)
            return false;
        seen[instrIdx] = true;

        uint16_t osTirusPreset = 0xFFFF;
        uint8_t osTirusSlot = 0xFF;
        if ((flags & 16) != 0 &&
            (!read_chunk_data(f, &osTirusPreset, sizeof(osTirusPreset), &remaining) ||
             !read_chunk_data(f, &osTirusSlot, sizeof(osTirusSlot), &remaining)))
            return false;

        instr_t *ins = instrTmp[instrIdx];
        if (ins == NULL && allocateTmpInstr(instrIdx)) {
            ins = instrTmp[instrIdx];
        }
        if (ins == NULL)
            return false;
        
        // Mark this as a DXM instrument (not a regular XM instrument)
        // This will be used to determine how to handle stereo samples
        ins->isDXMInstrument = true;
        
        // Set TF4 flag if indicated
        if (flags & 1) ins->useTF4 = true;
        if (flags & 4) ins->useDexed = true;
        if (flags & 8) ins->useV2 = true;
        if (flags & 16) {
            ins->useOsTirus = true;
            ins->osTirusPreset = osTirusPreset;
            ins->osTirusSlot = osTirusSlot;
        } else {
            ins->osTirusPreset = 0xFFFF;
            ins->osTirusSlot = 0xFF;
        }
        normalizeSynthFlags(ins);
        
        // Note: Stereo sample flags are already set by the WAV chunk loader
        // This metadata just confirms which instruments are DXM vs XM
    }

    return remaining == 0 || skip_bytes(f, remaining);
}

// Helper: Restore Macro Map chunk
static bool restoreMacroMapDXM(FILE *f, uint32_t chunkLen)
{
    const size_t recordLen = 16 * sizeof(uint8_t) + 16 * sizeof(uint16_t) + 16 * sizeof(uint8_t);
    const size_t expectedLen = MAX_INST * recordLen;
    size_t remaining = chunkLen;
    if (chunkLen < expectedLen) {
        return false;
    }
    for (int instrIdx = 1; instrIdx <= MAX_INST; instrIdx++) {
        uint8_t targets[16], scales[16];
        uint16_t paramIDs[16];
        if (!read_chunk_data(f, targets, sizeof(targets), &remaining) ||
            !read_chunk_data(f, paramIDs, sizeof(paramIDs), &remaining) ||
            !read_chunk_data(f, scales, sizeof(scales), &remaining))
            return false;

        instr_t *ins = instrTmp[instrIdx];
        bool hasMapping = false;
        for (int i = 0; i < 16; ++i)
            hasMapping |= targets[i] != 0 || paramIDs[i] != 0 || scales[i] != 0;
        if (ins == NULL && hasMapping && allocateTmpInstr(instrIdx))
                ins = instrTmp[instrIdx];
        if (ins != NULL) {
            memcpy(ins->macroTargetType, targets, sizeof(targets));
            memcpy(ins->macroParamID, paramIDs, sizeof(paramIDs));
            memcpy(ins->macroScale, scales, sizeof(scales));
            ft2_macro_map_sanitize_instrument(ins);
        } else if (hasMapping) {
            return false;
        }
    }
    return remaining == 0 || skip_bytes(f, remaining);
}

// Restore macro cells without ever reading beyond the declared chunk.
static bool restoreMacroPatternDXM(FILE *f, uint32_t chunkLen)
{
    size_t remaining = chunkLen;
    uint16_t pattCount, chCount, macroModeMask;
    if (!read_chunk_data(f, &pattCount, sizeof(pattCount), &remaining) ||
        !read_chunk_data(f, &chCount, sizeof(chCount), &remaining) ||
        !read_chunk_data(f, &macroModeMask, sizeof(macroModeMask), &remaining))
        return false;

    if (chCount != MAX_STEREO_PAIRS || pattCount > MAX_PATTERNS)
        return false;

    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++)
        editor.macroMode[ch] = (macroModeMask >> ch) & 1;

    for (uint16_t p = 0; p < pattCount; p++)
    {
        uint16_t rows;
        if (!read_chunk_data(f, &rows, sizeof(rows), &remaining) || rows > MAX_PATT_LEN)
            return false;

        const size_t payload = (size_t)rows * MAX_STEREO_PAIRS * 3;
        if (payload > remaining)
            return false;

        if (rows > 0 && macroPatternTmp[p] == NULL)
        {
            macroPatternTmp[p] = (macroNote_t *)malloc(MAX_PATT_LEN * MAX_STEREO_PAIRS * sizeof (macroNote_t));
            if (macroPatternTmp[p] == NULL)
                return false;
            memset(macroPatternTmp[p], 0xFF, MAX_PATT_LEN * MAX_STEREO_PAIRS * sizeof (macroNote_t));
        }

        for (uint16_t r = 0; r < rows; r++)
        {
            for (uint16_t ch = 0; ch < MAX_STEREO_PAIRS; ch++)
            {
                macroNote_t *mn = &macroPatternTmp[p][(r * MAX_STEREO_PAIRS) + ch];
                if (!read_chunk_data(f, &mn->slot, 1, &remaining) ||
                    !read_chunk_data(f, &mn->mode, 1, &remaining) ||
                    !read_chunk_data(f, &mn->value, 1, &remaining))
                    return false;
            }
        }
    }

    return remaining == 0 || skip_bytes(f, remaining);
}

static bool restoreOsTirusArpStateDXM(FILE *f, uint32_t chunkLen)
{
    const size_t perInstrumentBytes = sizeof(uint8_t) + (16 * sizeof(uint8_t)) + (16 * sizeof(uint8_t)) + (16 * sizeof(uint8_t));
    size_t remaining = chunkLen;
    uint8_t numArp = 0;
    bool seen[MAX_INST + 1] = { false };
    if (!read_chunk_data(f, &numArp, sizeof(numArp), &remaining) ||
        numArp > MAX_INST || (size_t)numArp * perInstrumentBytes > remaining)
        return false;

    for (uint8_t i = 0; i < numArp; ++i)
    {
        uint8_t instrIdx = 0;
        uint8_t gate[16], velocity[16], length[16];
        if (!read_chunk_data(f, &instrIdx, sizeof(instrIdx), &remaining) ||
            !read_chunk_data(f, gate, sizeof(gate), &remaining) ||
            !read_chunk_data(f, velocity, sizeof(velocity), &remaining) ||
            !read_chunk_data(f, length, sizeof(length), &remaining) ||
            instrIdx < 1 || instrIdx > MAX_INST || seen[instrIdx])
            return false;
        seen[instrIdx] = true;

        instr_t *ins = instrTmp[instrIdx];
        if (ins == NULL && allocateTmpInstr(instrIdx))
            ins = instrTmp[instrIdx];
        if (ins == NULL)
            return false;

        memcpy(ins->osTirusArpStepGate, gate, sizeof(gate));
        memcpy(ins->osTirusArpStepVelocity, velocity, sizeof(velocity));
        memcpy(ins->osTirusArpStepLength, length, sizeof(length));
        ins->useOsTirus = true;
        ins->isDXMInstrument = true;
        normalizeSynthFlags(ins);
    }

    return remaining == 0 || skip_bytes(f, remaining);
}

void clearPendingDXMSynthLoadState(void)
{
    for (int i = 1; i <= MAX_INST; ++i)
    {
        free_pending_blob(&g_pendingV2States[i]);
        free_pending_blob(&g_pendingOsTirusStates[i]);
    }
}

void finalizeDXMSynthLoadState(void)
{
    const int sr = (audio.freq > 0) ? audio.freq : 44100;

    if (!ft2_synth_is_initialized() || !ft2_dx_is_initialized() ||
        !ft2_v2_is_initialized() || !ft2_ostirus_is_initialized())
    {
        ft2_unified_synth_shutdown();
        ft2_unified_synth_set_samplerate(sr);
    }

    setSkipPresetOnCreate(true);

    for (int i = 1; i <= MAX_INST; ++i)
    {
        instr_t *ins = instr[i];
        if (ins == NULL)
            continue;

        if (ins->useTF4)
        {
            ft2_synth_set_all_persistent_params(i, ins->tf4Params, DXM_TF4_PARAM_COUNT);
            createInstrumentInstance(i);
        }

        if (ins->useDexed)
            ft2_dx_load_patch_for_instrument(i, ins->dxParams, 155);

        if (g_pendingV2States[i].data != NULL && g_pendingV2States[i].size > 0)
        {
            if (!ft2_v2_deserialize_state(i, g_pendingV2States[i].data, g_pendingV2States[i].size))
                printf("[DXM-LOAD] WARNING: Failed to finalize V2 state for instr %d\n", i);
        }

        if (ins->useOsTirus)
        {
            if (g_pendingOsTirusStates[i].data != NULL && g_pendingOsTirusStates[i].size > 0)
            {
                if (!ft2_ostirus_deserialize_state(i, g_pendingOsTirusStates[i].data, g_pendingOsTirusStates[i].size))
                    printf("[DXM-LOAD] WARNING: Failed to finalize OsTIrus state for instr %d\n", i);
            }
            else if (ins->osTirusPreset != 0xFFFF)
            {
                if (!ft2_ostirus_load_factory_preset_for_instrument(i, (int)ins->osTirusPreset))
                    printf("[DXM-LOAD] WARNING: Failed to restore OsTIrus preset %u for instr %d\n", (unsigned)ins->osTirusPreset, i);
            }
            else
            {
                (void)ft2_ostirus_assign_slot_for_instrument(i);
            }
        }
    }

    setSkipPresetOnCreate(false);
    clearPendingDXMSynthLoadState();
}

// DXM loader: verify header, scan chunks, and restore state
bool loadDXM(FILE *f, uint32_t filesize)
{
    clearPendingDXMSynthLoadState();

    if (f == NULL || filesize < 8)
        return false;

    // 1. Read and verify DXM magic header
    char dxmMagic[4];
    if (fread(dxmMagic, 1, 4, f) != 4 || dxmMagic[0]!='D' || dxmMagic[1]!='X' || dxmMagic[2]!='M' || dxmMagic[3]!='0')
        return false;

    // 2. Read XM data size (uint32_t)
    uint32_t xmSize = 0;
    if (fread(&xmSize, 1, 4, f) != 4 || xmSize == 0 || xmSize > filesize - 8u)
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
    
    // 4. Read chunks (scan until EOF)
    char chunkId[8];
    bool chunksOK = true;
    for (;;) {
        const long chunkHeaderStart = ftell(f);
        if (chunkHeaderStart < 0) { chunksOK = false; break; }
        if (chunkHeaderStart == (long)filesize) break;
        if (chunkHeaderStart > (long)filesize || (long)filesize - chunkHeaderStart < 12) {
            chunksOK = false;
            break;
        }
        if (fread(chunkId, 1, 8, f) != 8) { chunksOK = false; break; }
        uint32_t chunkLen = 0;
        if (fread(&chunkLen, 1, 4, f) != 4) { chunksOK = false; break; }
        if (chunkLen == 0) continue;
        const long chunkDataStart = ftell(f);
        const long chunkDataEnd = chunkDataStart + (long)chunkLen;
        if (chunkDataEnd < chunkDataStart || chunkDataEnd > (long)filesize) {
            chunksOK = false;
            break;
        }

        if (memcmp(chunkId, "DXMDSP", 6) == 0) {
            chunksOK = restoreDSPStateDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMWAV", 6) == 0) {
            chunksOK = restoreAllSamplesDXMWAV(f, chunkLen);
        } else if (memcmp(chunkId, "DXMTF4", 6) == 0) {
            chunksOK = restoreAllTF4StatesDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMDEX", 6) == 0) {
            chunksOK = restoreAllDexedStatesDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMV2S", 6) == 0) {
            chunksOK = restoreAllV2StatesDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMOTS", 6) == 0) {
            chunksOK = restoreAllOsTirusStatesDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMARP", 6) == 0) {
            chunksOK = restoreOsTirusArpStateDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMMAP", 6) == 0) {
            chunksOK = restoreMacroMapDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMMAC", 6) == 0) {
            chunksOK = restoreMacroPatternDXM(f, chunkLen);
        } else if (memcmp(chunkId, "DXMMET", 6) == 0) {
            chunksOK = restoreDXMInstrumentMetadata(f, chunkLen);
        } else {
            // Unknown chunk, skip
            chunksOK = skip_bytes(f, chunkLen);
        }

        if (!chunksOK) break;
        if (ftell(f) != chunkDataEnd && fseek(f, chunkDataEnd, SEEK_SET) != 0) {
            chunksOK = false;
            break;
        }
    }

    if (!chunksOK || ferror(f)) {
        clearPendingDXMSynthLoadState();
        return false;
    }

    // Cache persistent mixer/DSP state after loading DXM
    cacheMixerStateFromData();

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

    return true;
}

#ifdef FT2_STABILITY_TESTS
#include "../../tests/dxm_tests.inc"
#endif
