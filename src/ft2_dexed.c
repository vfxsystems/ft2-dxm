/* ft2_dexed.c - FT2 glue for Dexed FM synth
 *
 * Simplifications:
 *   - No factory preset scanning at startup
 *   - Each instrument loads its own patch on first access
 *   - Supports both packed (128-byte) and unpacked (155-byte) formats
 */

#include "ft2_dexed.h"
#include "ft2_header.h"
#include "ft2_structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if DEBUG_DX_SYNTH
#define DX_DEBUG(fmt, ...) do { printf("[DX_GLUE] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while (0)
#else
#define DX_DEBUG(fmt, ...)
#endif

#ifndef MAX_INST
#define MAX_INST 128
#endif

static bool  g_dxSynthInitialized = false;
static void* g_dxSynth = NULL;
static void* g_dxInst[MAX_INST] = {0};

static void* ensure_dx_inst(int instrID);

static inline int dx_index_from_ft2(int instrID)
{
    if (instrID < 1 || instrID > MAX_INST) return -1;
    return instrID - 1; // FT2 1..128 -> Dexed 0..127
}

/* =============================================================================
 * Initialization / Shutdown
 * ============================================================================= */

void ft2_dx_init(int samplerate)
{
    DX_DEBUG("ft2_dx_init(%d)", samplerate);

    if (g_dxSynthInitialized) {
        for (int i = 0; i < MAX_INST; ++i) {
            if (g_dxInst[i]) {
                dx_instrument_destroy(g_dxInst[i]);
                g_dxInst[i] = NULL;
            }
        }
        if (g_dxSynth) {
            dx_synth_destroy(g_dxSynth);
            g_dxSynth = NULL;
        }
    }

    g_dxSynth = dx_synth_create();
    if (!g_dxSynth) {
        DX_DEBUG("ERROR: dx_synth_create failed");
        return;
    }

    dx_synth_init(g_dxSynth, (unsigned int)samplerate);
    memset(g_dxInst, 0, sizeof(g_dxInst));
    g_dxSynthInitialized = true;
    DX_DEBUG("Dexed initialized (samplerate=%d)", samplerate);
}

void ft2_dx_shutdown(void)
{
    if (!g_dxSynthInitialized) return;

    for (int i = 0; i < MAX_INST; ++i) {
        if (g_dxInst[i]) {
            dx_instrument_destroy(g_dxInst[i]);
            g_dxInst[i] = NULL;
        }
    }

    if (g_dxSynth) {
        dx_synth_destroy(g_dxSynth);
        g_dxSynth = NULL;
    }

    g_dxSynthInitialized = false;
    DX_DEBUG("Dexed shutdown complete");
}

bool ft2_dx_is_initialized(void)
{
    return g_dxSynthInitialized;
}

/* =============================================================================
 * Per-Instrument Management
 * ============================================================================= */

static void* ensure_dx_inst(int instrID)
{
    int dxIdx = dx_index_from_ft2(instrID);
    if (dxIdx < 0) {
        DX_DEBUG("ensure_dx_inst: invalid instrID %d", instrID);
        return NULL;
    }

    if (!g_dxSynthInitialized || !g_dxSynth) {
        DX_DEBUG("ensure_dx_inst: synth not initialized");
        return NULL;
    }

    if (!g_dxInst[dxIdx]) {
        g_dxInst[dxIdx] = dx_instrument_create(g_dxSynth, dxIdx);

        if (!g_dxInst[dxIdx]) {
            DX_DEBUG("ensure_dx_inst: create failed for instr=%d", instrID);
            return NULL;
        }

        DX_DEBUG("Created Dexed instrument %d -> %p", instrID, g_dxInst[dxIdx]);

        /* Load patch from instrument struct if available */
        instr_t* ins = instr[instrID];
        if (ins && ins->useDexed) {
            bool hasPatch = false;

            for (int i = 0; i < 155; ++i) {
                if (ins->dxParams[i] != 0) {
                    hasPatch = true;
                    break;
                }
            }

            if (hasPatch) {
                /* Try loading as unpacked (155-byte) format first */
                if (dx_instrument_load_patch(g_dxInst[dxIdx], ins->dxParams, 155)) {
                    DX_DEBUG("Loaded unpacked patch for instrument %d", instrID);
                } else {
                    /* Fall back to packed format */
                    DX_DEBUG("Unpacked load failed, trying packed...");
                    if (dx_instrument_load_packed_patch(g_dxInst[dxIdx], ins->dxParams, 128)) {
                        DX_DEBUG("Loaded packed patch for instrument %d", instrID);
                    } else {
                        DX_DEBUG("Failed to load patch for instrument %d", instrID);
                    }
                }
            } else {
                /* No stored patch data yet, persist the engine defaults */
                dx_instrument_save_patch(g_dxInst[dxIdx], ins->dxParams, sizeof(ins->dxParams));
            }
        }
    }

    return g_dxInst[dxIdx];
}

/* =============================================================================
 * Patch Loading
 * ============================================================================= */

int ft2_dx_load_packed_patch_for_instrument(int instrID, const uint8_t* data, size_t size)
{
    if (dx_index_from_ft2(instrID) < 0) return 0;
    if (!g_dxSynthInitialized) return 0;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0;

    return dx_instrument_load_packed_patch(inst, data, size);
}

/* =============================================================================
 * Factory Preset API
 * ============================================================================= */

int ft2_dx_get_factory_preset_count(void)
{
    return dx_get_factory_preset_count();
}

const char* ft2_dx_get_factory_preset_name(int index)
{
    return dx_get_factory_preset_name(index);
}

int ft2_dx_load_factory_preset_for_instrument(int instrID, int index)
{
    if (index < 0) return 0;
    if (dx_index_from_ft2(instrID) < 0) return 0;
    if (!g_dxSynthInitialized) return 0;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0;

    int ok = dx_instrument_load_factory_preset(inst, index);
    if (ok) {
        instr_t* ins = instr[instrID];
        if (ins && ins->useDexed) {
            dx_instrument_save_patch(inst, ins->dxParams, sizeof(ins->dxParams));
        }
    }
    return ok;
}

int ft2_dx_load_factory_preset_for_current_instrument(int index)
{
    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (instrID <= 0) return 0;
    return ft2_dx_load_factory_preset_for_instrument(instrID, index);
}

int ft2_dx_get_current_preset_for_instrument(int instrID)
{
    if (dx_index_from_ft2(instrID) < 0) return -1;
    if (!g_dxSynthInitialized) return -1;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return -1;

    return dx_instrument_get_current_preset_index(inst);
}

int ft2_dx_get_active_voice_count(int instrID)
{
    if (dx_index_from_ft2(instrID) < 0) return 0;
    if (!g_dxSynthInitialized) return 0;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0;

    return dx_instrument_get_active_voice_count(inst);
}

/* =============================================================================
 * Rendering
 * ============================================================================= */

void ft2_dx_render_for_channel(int instrID, float* bufL, float* bufR, int nsamples, int add)
{
    if (!g_dxSynthInitialized || !bufL || !bufR || nsamples <= 0) return;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) {
        if (!add) {
            memset(bufL, 0, nsamples * sizeof(float));
            memset(bufR, 0, nsamples * sizeof(float));
        }
        return;
    }

    if (!add) {
        memset(bufL, 0, nsamples * sizeof(float));
        memset(bufR, 0, nsamples * sizeof(float));
    }

    float* outs[2] = { bufL, bufR };
    dx_instrument_process(g_dxSynth, inst, outs, (unsigned int)nsamples);
}

void ft2_dx_render(float* bufL, float* bufR, int nsamples, int add)
{
    if (!g_dxSynthInitialized || !bufL || !bufR) return;

    if (!add) {
        memset(bufL, 0, nsamples * sizeof(float));
        memset(bufR, 0, nsamples * sizeof(float));
    }

    for (int i = 0; i < MAX_INST; ++i) {
        if (!g_dxInst[i]) continue;
        float* outs[2] = { bufL, bufR };
        dx_instrument_process(g_dxSynth, g_dxInst[i], outs, (unsigned int)nsamples);
    }
}

/* =============================================================================
 * MIDI
 * ============================================================================= */

void ft2_dx_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2)
{
    if (!g_dxSynthInitialized) return;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return;

    dx_instrument_send_midi(inst, status, data1, data2);
}

void ft2_dx_panic(void)
{
    for (int i = 0; i < MAX_INST; ++i) {
        if (g_dxInst[i]) {
            dx_instrument_panic(g_dxInst[i]);
        }
    }
}

/* =============================================================================
 * Parameter Access
 * ============================================================================= */

void ft2_dx_set_param_for_instrument(int instrID, int paramId, float value)
{
    if (dx_index_from_ft2(instrID) < 0) return;
    if (!g_dxSynthInitialized) return;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return;

    dx_instrument_set_param(inst, paramId, value);
}

float ft2_dx_get_param_for_instrument(int instrID, int paramId)
{
    if (dx_index_from_ft2(instrID) < 0) return 0.0f;
    if (!g_dxSynthInitialized) return 0.0f;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0.0f;

    float v = dx_instrument_get_param(inst, paramId);
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

int ft2_dx_get_params_for_instrument(int instrID, float* out, int maxCount)
{
    if (dx_index_from_ft2(instrID) < 0) return 0;
    if (!g_dxSynthInitialized) return 0;
    if (!out || maxCount <= 0) return 0;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0;

    return dx_instrument_get_params(inst, out, maxCount);
}

/* =============================================================================
 * Patch Data I/O
 * ============================================================================= */

int ft2_dx_get_patch_data(int instrID, uint8_t* buffer, int32_t bufferSize)
{
    if (dx_index_from_ft2(instrID) < 0 || buffer == NULL) return 0;
    if (!g_dxSynthInitialized) return 0;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0;

    return dx_instrument_save_patch(inst, buffer, bufferSize);
}

int ft2_dx_get_packed_patch_data(int instrID, uint8_t* buffer, int32_t bufferSize)
{
    if (dx_index_from_ft2(instrID) < 0 || buffer == NULL) return 0;
    if (!g_dxSynthInitialized) return 0;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0;

    return dx_instrument_save_packed_patch(inst, buffer, bufferSize);
}

int ft2_dx_load_patch_for_instrument(int instrID, const uint8_t* data, size_t size)
{
    if (dx_index_from_ft2(instrID) < 0 || !data) return 0;
    if (!g_dxSynthInitialized) return 0;

    void* inst = ensure_dx_inst(instrID);
    if (!inst) return 0;

    return dx_instrument_load_patch(inst, data, size);
}
