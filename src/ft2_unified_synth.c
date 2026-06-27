#include "ft2_unified_synth.h"
#include "ft2_dexed.h"
#include "ft2_v2.h"
#include "ft2_synth.h"
#include "ft2_replayer.h"
#include "ft2_header.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

//#define DEBUG_UNIFIED_SYNTH 1

#if DEBUG_UNIFIED_SYNTH
#define US_DEBUG(fmt, ...) printf("[UNIFIED_SYNTH] " fmt "\n", ##__VA_ARGS__)
#else
#define US_DEBUG(fmt, ...)
#endif

// Global state
static UnifiedSynthInterface g_engines[SYNTH_TYPE_COUNT] = {0};
static bool g_initialized = false;
static int g_currentSampleRate = 0;

// Helper macros
#define ENGINE_VALID(engine) ((engine) >= 0 && (engine) < SYNTH_TYPE_COUNT && g_engines[engine].engineName != NULL)

static inline float softLimitSample(float x, float threshold)
{
    float ax = fabsf(x);
    if (ax <= threshold) return x;
    float over = (ax - threshold) / (1.0f - threshold);
    float shaped = threshold + (1.0f - threshold) * tanhf(over);
    return copysignf(shaped, x);
}

static void applyTf4SoftLimiter(float *bufL, float *bufR, int nsamples)
{
    const float threshold = 0.95f;
    for (int i = 0; i < nsamples; i++) {
        bufL[i] = softLimitSample(bufL[i], threshold);
        bufR[i] = softLimitSample(bufR[i], threshold);
    }
}

// =============================================================================
// TUNEFISH4 ENGINE IMPLEMENTATION
// =============================================================================

static void tunefish4_init(int samplerate) {
    US_DEBUG("Initializing Tunefish4 engine at %d Hz", samplerate);
    US_DEBUG("[UNIFIED] Calling ft2_synth_init() from unified system");
    ft2_synth_init(samplerate);
}

static void tunefish4_shutdown(void) {
    US_DEBUG("Shutting down Tunefish4 engine");
    ft2_synth_shutdown();
}

static void tunefish4_render(float* bufL, float* bufR, int nsamples, int add) {
    ft2_synth_render_separate(bufL, bufR, nsamples, add);
}

static void tunefish4_render_for_channel(int instrID, float* bufL, float* bufR, int nsamples, int add) {
    ft2_synth_render_for_channel(instrID, bufL, bufR, nsamples, add);
}

static void tunefish4_send_midi(int instrID, const MidiMessage* message) {
    if (message) {
        ft2_synth_send_midi_to_instrument(instrID, message->status, message->data1, message->data2);
    }
}

static void tunefish4_panic(void) {
    // TODO: Implement panic for Tunefish4
}

static void tunefish4_set_param(int instrID, int paramId, float value) {
    ft2_synth_set_persistent_param(instrID, paramId, value);
}

static float tunefish4_get_param(int instrID, int paramId) {
    return ft2_synth_get_persistent_param(instrID, paramId);
}

// Tunefish4 parameter ranges (based on tf4.hpp)
static const ParameterRange tunefish4_param_ranges[128] = {
    // Global parameters
    {0.0f, 1.0f, 0.5f}, // TF_GLOBAL_GAIN
    {0.0f, 1.0f, 0.5f}, // TF_GEN_VOLUME
    {0.0f, 1.0f, 0.5f}, // TF_GEN_PANNING
    {0.0f, 1.0f, 0.5f}, // TF_GEN_DETUNE
    {0.0f, 1.0f, 0.5f}, // TF_GEN_SPREAD
    {0.0f, 1.0f, 0.5f}, // TF_GEN_SCALE
    
    // Generator parameters
    {0.0f, 1.0f, 0.5f}, // TF_GEN_BANDWIDTH
    {0.0f, 1.0f, 0.5f}, // TF_GEN_NUMHARMONICS
    {0.0f, 1.0f, 0.5f}, // TF_GEN_DAMP
    {0.0f, 1.0f, 0.5f}, // TF_GEN_MODULATION
    {0.0f, 1.0f, 0.5f}, // TF_GEN_DRIVE
    
    // Filter parameters
    {0.0f, 1.0f, 0.5f}, // TF_LP_FILTER_CUTOFF
    {0.0f, 1.0f, 0.5f}, // TF_LP_FILTER_RESONANCE
    {0.0f, 1.0f, 0.5f}, // TF_HP_FILTER_CUTOFF
    {0.0f, 1.0f, 0.5f}, // TF_HP_FILTER_RESONANCE
    
    // LFO parameters
    {0.0f, 1.0f, 0.5f}, // TF_LFO1_RATE
    {0.0f, 1.0f, 0.5f}, // TF_LFO1_DEPTH
    {0.0f, 1.0f, 0.5f}, // TF_LFO2_RATE
    {0.0f, 1.0f, 0.5f}, // TF_LFO2_DEPTH
    
    // ADSR parameters
    {0.0f, 1.0f, 0.5f}, // TF_ADSR1_ATTACK
    {0.0f, 1.0f, 0.5f}, // TF_ADSR1_DECAY
    {0.0f, 1.0f, 0.5f}, // TF_ADSR1_SUSTAIN
    {0.0f, 1.0f, 0.5f}, // TF_ADSR1_RELEASE
    
    // FX parameters
    {0.0f, 1.0f, 0.5f}, // FX_FLANGER_FREQ
    {0.0f, 1.0f, 0.5f}, // FX_REVERB_ROOM_SZ
    {0.0f, 1.0f, 0.5f}, // FX_DELAY_LEFT
    {0.0f, 1.0f, 0.5f}, // FX_CHORUS_FREQ
    
    // ... more parameters would be defined here
};

static const ParameterRange* tunefish4_get_param_range(int paramId) {
    if (paramId >= 0 && paramId < 128) {
        return &tunefish4_param_ranges[paramId];
    }
    return NULL;
}

static int tunefish4_get_param_count(void) {
    return 128; // Tunefish4 has 128 parameters
}

static const char* tunefish4_get_param_name(int paramId) {
    static const char* param_names[] = {
        "Global Gain", "Volume", "Panning", "Detune", "Spread", "Scale",
        "Bandwidth", "Num Harmonics", "Damp", "Modulation", "Drive",
        "LP Freq", "LP Res", "HP Freq", "HP Res",
        "LFO1 Rate", "LFO1 Depth", "LFO2 Rate", "LFO2 Depth",
        "ADSR1 Attack", "ADSR1 Decay", "ADSR1 Sustain", "ADSR1 Release",
        "Flanger Freq", "Reverb Room", "Delay Left", "Chorus Freq"
        // ... more parameter names would go here
    };
    
    if (paramId >= 0 && paramId < 128) {
        return param_names[paramId];
    }
    return "Unknown";
}

static int tunefish4_load_patch(int instrID, const uint8_t* data, size_t size) {
    // TODO: Implement patch loading for Tunefish4
    return 0;
}

static int tunefish4_save_patch(int instrID, uint8_t* buffer, size_t bufferSize) {
    // TODO: Implement patch saving for Tunefish4
    return 0;
}

static int tunefish4_get_patch_size(int instrID) {
    // TODO: Return actual patch size for Tunefish4
    return 0;
}

static int tunefish4_get_preset_count(void) {
    return ft2_synth_get_preset_count();
}

static const char* tunefish4_get_preset_name(int presetIndex) {
    return ft2_synth_get_preset_name(presetIndex);
}

static int tunefish4_load_preset(int instrID, int presetIndex) {
    return ft2_synth_load_preset_for_instrument(instrID, presetIndex);
}

static void tunefish4_store_state(int instrID) {
    ft2_synth_store_instrument_state(instrID);
}

static void tunefish4_restore_state(int instrID) {
    ft2_synth_restore_instrument_state(instrID);
}

static bool tunefish4_has_state(int instrID) {
    return ft2_synth_has_persistent_state(instrID);
}

static void tunefish4_clear_state(int instrID) {
    ft2_synth_clear_persistent_state(instrID);
}

static int tunefish4_get_active_voices(int instrID) {
    return ft2_synth_get_active_voice_count(instrID);
}

// =============================================================================
// DEXED ENGINE IMPLEMENTATION
// =============================================================================

static void dexed_init(int samplerate) {
    US_DEBUG("Initializing Dexed engine at %d Hz", samplerate);
    ft2_dx_init(samplerate);
}

static void dexed_shutdown(void) {
    US_DEBUG("Shutting down Dexed engine");
    ft2_dx_shutdown();
}

static void dexed_render(float* bufL, float* bufR, int nsamples, int add) {
    ft2_dx_render(bufL, bufR, nsamples, add);
}

static void dexed_render_for_channel(int instrID, float* bufL, float* bufR, int nsamples, int add) {
    ft2_dx_render_for_channel(instrID, bufL, bufR, nsamples, add);
}

static void dexed_send_midi(int instrID, const MidiMessage* message) {
    if (message) {
        ft2_dx_send_midi_to_instrument(instrID, message->status, message->data1, message->data2);
    }
}

static void dexed_panic(void) {
    ft2_dx_panic();
}

static void dexed_set_param(int instrID, int paramId, float value) {
    ft2_dx_set_param_for_instrument(instrID, paramId, value);
}

static float dexed_get_param(int instrID, int paramId) {
    return ft2_dx_get_param_for_instrument(instrID, paramId);
}

// Dexed parameter ranges (DX7 parameters normalized 0-1)
static const ParameterRange dexed_param_ranges[156] = {
    // Operator 1-4: frequency (0-99.99% -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // Operator 1-4: fine detune (0-99.99% -> 0.0-1.0)  
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // Operator 1-4: level (0-99.99% -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // Operator 1-4: attack rate (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // Operator 1-4: decay rate (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // Operator 1-4: release rate (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // Operator 1-4: sustain level (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // Algorithm (0-31 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // Pitch EG (attack/decay/release rates, sustain level - normalized)
    {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    // LFO rate (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // LFO depth (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // LFO delay (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // LFO sync (0-1 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // LFO waveform (0-3 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // Modulation sensitivity (0-7 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // Key tracking (0-7 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // Pitch bend range (0-12 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // Portamento (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // Brightness (0-99 -> 0.0-1.0)
    {0.0f, 1.0f, 0.0f},
    // Remaining empty slots (for compatibility)
    {0.0f, 1.0f, 0.0f}
};

static const ParameterRange* dexed_get_param_range(int paramId) {
    if (paramId >= 0 && paramId < 156) {
        return &dexed_param_ranges[paramId];
    }
    return NULL;
}

static int dexed_get_param_count(void) {
    return 156; // DX7 has 156 parameters
}

static const char* dexed_get_param_name(int paramId) {
    static const char* param_names[] = {
        "Op1 Freq", "Op1 Detune", "Op1 Level", "Op1 Attack", "Op1 Decay", "Op1 Release", "Op1 Sustain",
        "Op2 Freq", "Op2 Detune", "Op2 Level", "Op2 Attack", "Op2 Decay", "Op2 Release", "Op2 Sustain",
        "Op3 Freq", "Op3 Detune", "Op3 Level", "Op3 Attack", "Op3 Decay", "Op3 Release", "Op3 Sustain",
        "Op4 Freq", "Op4 Detune", "Op4 Level", "Op4 Attack", "Op4 Decay", "Op4 Release", "Op4 Sustain",
        "Algorithm", "Pitch EG Attack", "Pitch EG Decay", "Pitch EG Release", "Pitch EG Sustain",
        "LFO Rate", "LFO Depth", "LFO Delay", "LFO Sync", "LFO Wave", "Mod Sens", "Key Track", "Pitch Bend", "Portamento", "Brightness"
        // ... more parameter names would go here
    };
    static char param_buf[32];
    const int name_count = (int)(sizeof(param_names)/sizeof(param_names[0]));

    if (paramId >= 0 && paramId < name_count) {
        return param_names[paramId];
    }

    if (paramId >= 0 && paramId < 156) {
        snprintf(param_buf, sizeof(param_buf), "DX Param %d", paramId);
        return param_buf;
    }
    return "Unknown";
}

static int dexed_load_patch(int instrID, const uint8_t* data, size_t size) {
    if (size == 128) {
        return ft2_dx_load_packed_patch_for_instrument(instrID, data, size);
    } else if (size >= 155) {
        return ft2_dx_load_patch_for_instrument(instrID, data, size);
    }
    return 0;
}

static int dexed_save_patch(int instrID, uint8_t* buffer, size_t bufferSize) {
    return ft2_dx_get_packed_patch_data(instrID, buffer, bufferSize);
}

static int dexed_get_patch_size(int instrID) {
    return 128; // Standard DX7 packed format
}

static int dexed_get_preset_count(void) {
    return ft2_dx_get_factory_preset_count();
}

static const char* dexed_get_preset_name(int presetIndex) {
    return ft2_dx_get_factory_preset_name(presetIndex);
}

static int dexed_load_preset(int instrID, int presetIndex) {
    return ft2_dx_load_factory_preset_for_instrument(instrID, presetIndex);
}

static void dexed_store_state(int instrID) {
    // TODO: Implement state persistence for Dexed
    // For now, we'll just store the current parameter values
}

static void dexed_restore_state(int instrID) {
    // TODO: Implement state restoration for Dexed
}

static bool dexed_has_state(int instrID) {
    // TODO: Check if state exists for instrument
    return false;
}

static void dexed_clear_state(int instrID) {
    // TODO: Clear state for instrument
}

static int dexed_get_active_voices(int instrID) {
    // TODO: Implement voice counting for Dexed
    return 0;
}

// =============================================================================
// V2 ENGINE IMPLEMENTATION
// =============================================================================

static void v2_init(int samplerate) {
    US_DEBUG("Initializing V2 engine at %d Hz", samplerate);
    ft2_v2_init(samplerate);
}

static void v2_shutdown(void) {
    US_DEBUG("Shutting down V2 engine");
    ft2_v2_shutdown();
}

static void v2_render(float* bufL, float* bufR, int nsamples, int add) {
    ft2_v2_render(bufL, bufR, nsamples, add);
}

static void v2_render_for_channel(int instrID, float* bufL, float* bufR, int nsamples, int add) {
    ft2_v2_render_for_channel(instrID, bufL, bufR, nsamples, add);
}

static void v2_send_midi(int instrID, const MidiMessage* message) {
    if (message) {
        ft2_v2_send_midi_to_instrument(instrID, message->status, message->data1, message->data2);
    }
}

static void v2_panic(void) {
    ft2_v2_panic();
}

static void v2_set_param(int instrID, int paramId, float value) {
    ft2_v2_set_param_for_instrument(instrID, paramId, value);
}

static float v2_get_param(int instrID, int paramId) {
    return ft2_v2_get_param_for_instrument(instrID, paramId);
}

static const ParameterRange* v2_get_param_range(int paramId) {
    return ft2_v2_get_param_range(paramId);
}

static int v2_get_param_count(void) {
    return ft2_v2_get_param_count();
}

static const char* v2_get_param_name(int paramId) {
    return ft2_v2_get_param_name(paramId);
}

static int v2_load_patch(int instrID, const uint8_t* data, size_t size) {
    return ft2_v2_load_patch_for_instrument(instrID, data, size);
}

static int v2_save_patch(int instrID, uint8_t* buffer, size_t bufferSize) {
    return ft2_v2_get_patch_data(instrID, buffer, (int32_t)bufferSize);
}

static int v2_get_patch_size(int instrID) {
    (void)instrID;
    return ft2_v2_get_patch_size();
}

static int v2_get_preset_count(void) {
    return ft2_v2_get_factory_preset_count();
}

static const char* v2_get_preset_name(int presetIndex) {
    return ft2_v2_get_factory_preset_name(presetIndex);
}

static int v2_load_preset(int instrID, int presetIndex) {
    return ft2_v2_load_preset_for_instrument(instrID, presetIndex);
}

static void v2_store_state(int instrID) {
    ft2_v2_store_instrument_state(instrID);
}

static void v2_restore_state(int instrID) {
    ft2_v2_restore_instrument_state(instrID);
}

static bool v2_has_state(int instrID) {
    return ft2_v2_has_persistent_state(instrID);
}

static void v2_clear_state(int instrID) {
    ft2_v2_clear_persistent_state(instrID);
}

static int v2_get_active_voices(int instrID) {
    return ft2_v2_get_active_voice_count(instrID);
}

// =============================================================================
// GLOBAL SYNTH MANAGEMENT
// =============================================================================

void ft2_unified_synth_init(int samplerate) {
    if (g_initialized) {
        if (g_currentSampleRate == samplerate)
            return;
        ft2_unified_synth_shutdown();
    }
    
    US_DEBUG("Initializing unified synth system with sample rate %d", samplerate);
    
    // Clear all engines
    memset(g_engines, 0, sizeof(g_engines));
    
    // Register Tunefish4 engine
    US_DEBUG("Registering Tunefish4 engine");
    UnifiedSynthInterface tunefish4_engine = {
        .engineType = SYNTH_TYPE_TUNEFISH4,
        .engineName = "Tunefish4",
        .init = tunefish4_init,
        .shutdown = tunefish4_shutdown,
        .render = tunefish4_render,
        .render_for_channel = tunefish4_render_for_channel,
        .send_midi = tunefish4_send_midi,
        .panic = tunefish4_panic,
        .set_param = tunefish4_set_param,
        .get_param = tunefish4_get_param,
        .get_param_range = tunefish4_get_param_range,
        .get_param_count = tunefish4_get_param_count,
        .get_param_name = tunefish4_get_param_name,
        .load_patch = tunefish4_load_patch,
        .save_patch = tunefish4_save_patch,
        .get_patch_size = tunefish4_get_patch_size,
        .get_preset_count = tunefish4_get_preset_count,
        .get_preset_name = tunefish4_get_preset_name,
        .load_preset = tunefish4_load_preset,
        .store_state = tunefish4_store_state,
        .restore_state = tunefish4_restore_state,
        .has_state = tunefish4_has_state,
        .clear_state = tunefish4_clear_state,
        .get_active_voices = tunefish4_get_active_voices,
        .engineData = NULL
    };
    
    g_engines[SYNTH_TYPE_TUNEFISH4] = tunefish4_engine;
    
    // Initialize Tunefish4 engine
    US_DEBUG("Initializing Tunefish4 engine");
    if (tunefish4_engine.init) {
        tunefish4_engine.init(samplerate);
    }
    
    // Register Dexed engine
    US_DEBUG("Registering Dexed engine");
    UnifiedSynthInterface dexed_engine = {
        .engineType = SYNTH_TYPE_DEXED,
        .engineName = "Dexed",
        .init = dexed_init,
        .shutdown = dexed_shutdown,
        .render = dexed_render,
        .render_for_channel = dexed_render_for_channel,
        .send_midi = dexed_send_midi,
        .panic = dexed_panic,
        .set_param = dexed_set_param,
        .get_param = dexed_get_param,
        .get_param_range = dexed_get_param_range,
        .get_param_count = dexed_get_param_count,
        .get_param_name = dexed_get_param_name,
        .load_patch = dexed_load_patch,
        .save_patch = dexed_save_patch,
        .get_patch_size = dexed_get_patch_size,
        .get_preset_count = dexed_get_preset_count,
        .get_preset_name = dexed_get_preset_name,
        .load_preset = dexed_load_preset,
        .store_state = dexed_store_state,
        .restore_state = dexed_restore_state,
        .has_state = dexed_has_state,
        .clear_state = dexed_clear_state,
        .get_active_voices = dexed_get_active_voices,
        .engineData = NULL
    };
    
    g_engines[SYNTH_TYPE_DEXED] = dexed_engine;
    
    // Initialize Dexed engine
    US_DEBUG("Initializing Dexed engine");
    if (dexed_engine.init) {
        dexed_engine.init(samplerate);
    }

    // Register V2 engine
    US_DEBUG("Registering V2 engine");
    UnifiedSynthInterface v2_engine = {
        .engineType = SYNTH_TYPE_V2,
        .engineName = "V2",
        .init = v2_init,
        .shutdown = v2_shutdown,
        .render = v2_render,
        .render_for_channel = v2_render_for_channel,
        .send_midi = v2_send_midi,
        .panic = v2_panic,
        .set_param = v2_set_param,
        .get_param = v2_get_param,
        .get_param_range = v2_get_param_range,
        .get_param_count = v2_get_param_count,
        .get_param_name = v2_get_param_name,
        .load_patch = v2_load_patch,
        .save_patch = v2_save_patch,
        .get_patch_size = v2_get_patch_size,
        .get_preset_count = v2_get_preset_count,
        .get_preset_name = v2_get_preset_name,
        .load_preset = v2_load_preset,
        .store_state = v2_store_state,
        .restore_state = v2_restore_state,
        .has_state = v2_has_state,
        .clear_state = v2_clear_state,
        .get_active_voices = v2_get_active_voices,
        .engineData = NULL
    };

    g_engines[SYNTH_TYPE_V2] = v2_engine;

    // Initialize V2 engine
    US_DEBUG("Initializing V2 engine");
    if (v2_engine.init) {
        v2_engine.init(samplerate);
    }
    
    g_currentSampleRate = samplerate;
    g_initialized = true;
    US_DEBUG("Unified synth system initialized with %d engines", SYNTH_TYPE_COUNT);
}

void ft2_unified_synth_shutdown(void) {
    if (!g_initialized) return;
    
    US_DEBUG("Shutting down unified synth system");
    
    // Shutdown all engines
    for (int i = 0; i < SYNTH_TYPE_COUNT; i++) {
        if (g_engines[i].shutdown) {
            g_engines[i].shutdown();
        }
    }
    
    g_currentSampleRate = 0;
    g_initialized = false;
}

void ft2_unified_synth_set_samplerate(int samplerate) {
    ft2_unified_synth_init(samplerate);
}

void ft2_unified_synth_register_engine(const UnifiedSynthInterface* engine) {
    if (!engine || !engine->engineName) return;
    
    US_DEBUG("Registering engine: %s", engine->engineName);
    g_engines[engine->engineType] = *engine;
}

void ft2_unified_synth_unregister_engine(SynthEngineType engineType) {
    if (!ENGINE_VALID(engineType)) return;
    
    US_DEBUG("Unregistering engine: %s", g_engines[engineType].engineName);
    
    // Shutdown the engine first
    if (g_engines[engineType].shutdown) {
        g_engines[engineType].shutdown();
    }
    
    // Clear the engine
    memset(&g_engines[engineType], 0, sizeof(UnifiedSynthInterface));
}

const UnifiedSynthInterface* ft2_unified_synth_get_engine(SynthEngineType engineType) {
    if (!ENGINE_VALID(engineType)) return NULL;
    return &g_engines[engineType];
}

SynthEngineType ft2_unified_synth_get_active_engine(int instrID) {
    extern struct editor_t editor;
    
    // Determine which synth engine to use for this instrument
    if (instrID >= 1 && instrID <= MAX_INST) {
        extern instr_t *instr[128+4]; // from ft2_replayer.h
        instr_t *ins = instr[instrID];
        
        if (ins) {
            US_DEBUG("Engine detection for instr %d: useTF4=%d, useDexed=%d, useV2=%d", 
                    instrID, ins->useTF4 ? 1 : 0, ins->useDexed ? 1 : 0, ins->useV2 ? 1 : 0);
            if (ins->useV2) {
                return SYNTH_TYPE_V2;
            }
            if (ins->useDexed) {
                return SYNTH_TYPE_DEXED;
            }
            if (ins->useTF4) {
                return SYNTH_TYPE_TUNEFISH4;
            }
            return SYNTH_TYPE_COUNT;
        }
    }
    
    return SYNTH_TYPE_COUNT;
}

void ft2_unified_synth_render_all(float* bufL, float* bufR, int nsamples, int add) {
    if (!g_initialized) return;
    
    // Render each active engine
    for (int i = 0; i < SYNTH_TYPE_COUNT; i++) {
        if (g_engines[i].render) {
            g_engines[i].render(bufL, bufR, nsamples, add);
        }
    }
}

void ft2_unified_synth_render_channel(int instrID, float* bufL, float* bufR, int nsamples, int add) {
    if (!g_initialized) return;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->render_for_channel) {
        engine->render_for_channel(instrID, bufL, bufR, nsamples, add);
        if (engineType == SYNTH_TYPE_TUNEFISH4) {
            applyTf4SoftLimiter(bufL, bufR, nsamples);
        }
    }
}

void ft2_unified_synth_send_midi(int instrID, const MidiMessage* message) {
    if (!g_initialized || !message) return;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->send_midi) {
        engine->send_midi(instrID, message);
    }
}

void ft2_unified_synth_set_param(int instrID, int paramId, float value) {
    if (!g_initialized) return;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->set_param) {
        engine->set_param(instrID, paramId, value);
    }
}

float ft2_unified_synth_get_param(int instrID, int paramId) {
    if (!g_initialized) return 0.0f;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return 0.0f;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->get_param) {
        return engine->get_param(instrID, paramId);
    }
    
    return 0.0f;
}

int ft2_unified_synth_load_patch(int instrID, const uint8_t* data, size_t size, bool isPacked) {
    if (!g_initialized || !data) return 0;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return 0;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->load_patch) {
        return engine->load_patch(instrID, data, size);
    }
    
    return 0;
}

int ft2_unified_synth_save_patch(int instrID, uint8_t* buffer, size_t bufferSize, bool* isPacked) {
    if (!g_initialized || !buffer) return 0;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return 0;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->save_patch) {
        int result = engine->save_patch(instrID, buffer, bufferSize);
        if (result > 0 && isPacked) {
            *isPacked = (engineType == SYNTH_TYPE_DEXED); // Dexed uses packed format
        }
        return result;
    }
    
    return 0;
}

int ft2_unified_synth_get_preset_count(int instrID) {
    if (!g_initialized) return 0;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return 0;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->get_preset_count) {
        return engine->get_preset_count();
    }
    
    return 0;
}

const char* ft2_unified_synth_get_preset_name(int instrID, int presetIndex) {
    if (!g_initialized) return NULL;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return NULL;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->get_preset_name) {
        return engine->get_preset_name(presetIndex);
    }
    
    return NULL;
}

int ft2_unified_synth_load_preset(int instrID, int presetIndex) {
    if (!g_initialized) return 0;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return 0;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->load_preset) {
        return engine->load_preset(instrID, presetIndex);
    }
    
    return 0;
}

void ft2_unified_synth_store_state(int instrID) {
    if (!g_initialized) return;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->store_state) {
        engine->store_state(instrID);
    }
}

void ft2_unified_synth_restore_state(int instrID) {
    if (!g_initialized) return;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->restore_state) {
        engine->restore_state(instrID);
    }
}

bool ft2_unified_synth_has_state(int instrID) {
    if (!g_initialized) return false;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return false;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->has_state) {
        return engine->has_state(instrID);
    }
    
    return false;
}

void ft2_unified_synth_clear_state(int instrID) {
    if (!g_initialized) return;
    
    SynthEngineType engineType = ft2_unified_synth_get_active_engine(instrID);
    if (engineType == SYNTH_TYPE_COUNT) return;
    
    const UnifiedSynthInterface* engine = ft2_unified_synth_get_engine(engineType);
    if (engine && engine->clear_state) {
        engine->clear_state(instrID);
    }
}

const char* ft2_unified_synth_get_engine_name(SynthEngineType engineType) {
    if (!ENGINE_VALID(engineType)) return NULL;
    return g_engines[engineType].engineName;
}

bool ft2_unified_synth_is_engine_supported(SynthEngineType engineType) {
    return ENGINE_VALID(engineType);
}

// =============================================================================
// LEGACY COMPATIBILITY FUNCTIONS
// =============================================================================

void ft2_synth_legacy_render(float *buffer, int nsamples, int add) {
    // Redirect to Tunefish4 engine
    SynthEngineType engineType = SYNTH_TYPE_TUNEFISH4;
    if (g_engines[engineType].render) {
        g_engines[engineType].render(buffer, buffer, nsamples, add);
    }
}

void ft2_dx_legacy_render(float *bufL, float *bufR, int nsamples, int add) {
    // Redirect to Dexed engine
    SynthEngineType engineType = SYNTH_TYPE_DEXED;
    if (g_engines[engineType].render) {
        g_engines[engineType].render(bufL, bufR, nsamples, add);
    }
}
