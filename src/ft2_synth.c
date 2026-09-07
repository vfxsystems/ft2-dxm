// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "scopes/ft2_scopes.h"
#include "ft2_video.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_wav_renderer.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "ft2_audioselector.h"
#include "mixer/ft2_mix.h"
#include "mixer/ft2_silence_mix.h"
#include "ft2_audio.h"
#include "ft2_mixer.h"
#include "ft2_dsp.h"
#include "ft2_synth.h"
#include <math.h>

// Debug flag for Tunefish4 synth
#ifndef DEBUG_TF4_SYNTH
#define DEBUG_TF4_SYNTH 0
#endif

#if DEBUG_TF4_SYNTH
#define TF4_DEBUG(fmt, ...) printf("[TF4_SYNTH] " fmt "\n", ##__VA_ARGS__)
#else
#define TF4_DEBUG(fmt, ...)
#endif

// Wrapper functions declared in ft2_synth.h

// Global synth state
static bool g_synthInitialized = false;
static int g_sampleRate = 44100;
static void* g_synth = NULL;
static void* g_instruments[MAX_INST] = {NULL}; // Per-instrument Tunefish4 synth instances (0-based TF4 slots)
static float* g_tempBufferL = NULL;
static float* g_tempBufferR = NULL;
static int g_maxBufferSize = 4096; // Dynamic buffer size to match FT2's allocation
static float* g_outputBuffers[2] = {NULL, NULL};
static bool g_skipPresetOnCreate = false;

// Track current preset index per instrument
static int g_current_preset[MAX_INST] = {0}; // Default to preset 0 for all (0-based TF4 slots)

// =============================================================================
// PER-INSTANCE PARAMETER PERSISTENCE SYSTEM
// =============================================================================

// Parameter constants
#define TF_PARAM_COUNT 128  // Total number of TF4 parameters
#define PERSISTENT_STORAGE_MAGIC 0x54463450  // "TF4P" magic number

// Per-instrument persistent parameter storage
typedef struct {
    bool hasState;                          // Whether this instrument has saved state
    float params[TF_PARAM_COUNT];          // Stored parameter values
    int currentPreset;                      // Current preset index (-1 = custom)
    char presetName[64];                    // Current preset name
    uint32_t magic;                         // Magic number for validation
} InstrumentParameterState;

static InstrumentParameterState g_persistentState[MAX_INST];
static bool g_persistentStorageInitialized = false;

static inline int tf4_index_from_ft2(int instrID)
{
    if (instrID < 1 || instrID > MAX_INST) return -1;
    return instrID - 1; // FT2 1..128 -> TF4 0..127
}

// Initialize persistent storage system
void ft2_synth_init_persistent_storage(void) {
    if (g_persistentStorageInitialized) return;
    
    // Clear all state
    for (int i = 0; i < MAX_INST; i++) {
        InstrumentParameterState* state = &g_persistentState[i];
        state->hasState = false;
        state->currentPreset = -1;
        state->presetName[0] = '\0';
        state->magic = PERSISTENT_STORAGE_MAGIC;
        
        // Initialize with default parameter values
        for (int p = 0; p < TF_PARAM_COUNT; p++) {
            state->params[p] = 0.0f;
        }
        
        // Set some sensible defaults for immediate playability
        state->params[5] = 0.7f;    // TF_GEN_VOLUME: 70% generator volume
        state->params[1] = 0.5f;    // TF_GEN_BANDWIDTH: 50% bandwidth
        state->params[2] = 0.3f;    // TF_GEN_NUMHARMONICS: ~30% harmonics
        state->params[12] = 0.5f;   // TF_GEN_POLYPHONY: 8 voices
        state->params[8] = 4.0f / 8.0f;  // TF_GEN_OCTAVE: middle octave
        state->params[26] = 0.01f;  // TF_ADSR1_ATTACK: quick attack
        state->params[27] = 0.3f;   // TF_ADSR1_DECAY: medium decay
        state->params[28] = 0.6f;   // TF_ADSR1_SUSTAIN: 60% sustain
        state->params[29] = 0.2f;   // TF_ADSR1_RELEASE: medium release
    }
    
    g_persistentStorageInitialized = true;
    TF4_DEBUG("Persistent parameter storage initialized for %d instruments", MAX_INST);
}

// Cleanup persistent storage system
void ft2_synth_cleanup_persistent_storage(void) {
    if (!g_persistentStorageInitialized) return;
    
    // Clear all state
    memset(g_persistentState, 0, sizeof(g_persistentState));
    g_persistentStorageInitialized = false;
    
    TF4_DEBUG("Persistent parameter storage cleaned up");
}

// Store current parameter state from TF4 instance
void ft2_synth_store_instrument_state(int instrID) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ERROR: store_instrument_state invalid instrID %d", instrID);
        return;
    }
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    void* instrument = g_instruments[tf4Idx];
    
    if (instrument) {
        // Store current parameters from TF4 instance
        for (int p = 0; p < TF_PARAM_COUNT; p++) {
            state->params[p] = tf_instrument_get_param(instrument, p);
        }
        state->currentPreset = g_current_preset[tf4Idx];
        TF4_DEBUG("Stored parameter state for instrument %d (preset %d)", instrID, state->currentPreset);
    }
    
    state->hasState = true;
    state->magic = PERSISTENT_STORAGE_MAGIC;
}

// Restore parameter state to TF4 instance
void ft2_synth_restore_instrument_state(int instrID) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ERROR: restore_instrument_state invalid instrID %d", instrID);
        return;
    }
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    
    // Validate state
    if (state->magic != PERSISTENT_STORAGE_MAGIC) {
        TF4_DEBUG("WARNING: Invalid magic number for instrument %d, skipping restore", instrID);
        return;
    }
    
    if (!state->hasState) {
        TF4_DEBUG("No stored state for instrument %d", instrID);
        return;
    }
    
    void* instrument = g_instruments[tf4Idx];
    if (!instrument) {
        TF4_DEBUG("No TF4 instance for instrument %d, cannot restore", instrID);
        return;
    }
    
    // Restore parameters to TF4 instance
    for (int p = 0; p < TF_PARAM_COUNT; p++) {
        tf_instrument_set_param(instrument, p, state->params[p]);
    }
    
    g_current_preset[tf4Idx] = state->currentPreset;
    TF4_DEBUG("Restored parameter state for instrument %d (preset %d)", instrID, state->currentPreset);
}

// Set parameter with automatic persistence
void ft2_synth_set_persistent_param(int instrID, int param, float value) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0 || param < 0 || param >= TF_PARAM_COUNT) {
        TF4_DEBUG("ERROR: set_persistent_param invalid instrID %d or param %d", instrID, param);
        return;
    }
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    
    // Update persistent storage
    state->params[param] = value;
    state->hasState = true;
    state->magic = PERSISTENT_STORAGE_MAGIC;
    
    // If TF4 instance exists, update it immediately
    void* instrument = g_instruments[tf4Idx];
    if (instrument) {
        tf_instrument_set_param(instrument, param, value);
    }
    
    // Mark as custom preset if parameter changed
    if (state->currentPreset >= 0) {
        state->currentPreset = -1; // Custom preset
        strncpy(state->presetName, "Custom", sizeof(state->presetName) - 1);
        g_current_preset[tf4Idx] = -1;
    }
}

// Get parameter from persistent storage
float ft2_synth_get_persistent_param(int instrID, int param) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0 || param < 0 || param >= TF_PARAM_COUNT) {
        TF4_DEBUG("ERROR: get_persistent_param invalid instrID %d or param %d", instrID, param);
        return 0.0f;
    }
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    
    // Validate state
    if (state->magic != PERSISTENT_STORAGE_MAGIC) {
        TF4_DEBUG("WARNING: Invalid magic number for instrument %d", instrID);
        return 0.0f;
    }
    
    return state->params[param];
}

// Bulk parameter operations for DXM loading/saving
void ft2_synth_set_all_persistent_params(int instrID, const float* params, int paramCount) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0 || !params) {
        TF4_DEBUG("ERROR: set_all_persistent_params invalid instrID %d or null params", instrID);
        return;
    }
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    
    // Copy parameters to persistent storage
    int copyCount = (paramCount < TF_PARAM_COUNT) ? paramCount : TF_PARAM_COUNT;
    for (int p = 0; p < copyCount; p++) {
        state->params[p] = params[p];
    }
    
    state->hasState = true;
    state->currentPreset = -1; // Mark as custom
    strncpy(state->presetName, "Loaded from DXM", sizeof(state->presetName) - 1);
    state->magic = PERSISTENT_STORAGE_MAGIC;
    
    // If TF4 instance exists, update it immediately
    void* instrument = g_instruments[tf4Idx];
    if (instrument) {
        for (int p = 0; p < copyCount; p++) {
            tf_instrument_set_param(instrument, p, params[p]);
        }
    }
    
    g_current_preset[tf4Idx] = -1;
    TF4_DEBUG("Set all persistent parameters for instrument %d (%d params)", instrID, copyCount);
}

void ft2_synth_get_all_persistent_params(int instrID, float* params, int paramCount) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0 || !params) {
        TF4_DEBUG("ERROR: get_all_persistent_params invalid instrID %d or null params", instrID);
        return;
    }
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    
    // Validate state
    if (state->magic != PERSISTENT_STORAGE_MAGIC) {
        TF4_DEBUG("WARNING: Invalid magic number for instrument %d", instrID);
        // Return default values
        for (int p = 0; p < paramCount && p < TF_PARAM_COUNT; p++) {
            params[p] = 0.0f;
        }
        return;
    }
    
    // Copy parameters from persistent storage
    int copyCount = (paramCount < TF_PARAM_COUNT) ? paramCount : TF_PARAM_COUNT;
    for (int p = 0; p < copyCount; p++) {
        params[p] = state->params[p];
    }
    
    TF4_DEBUG("Retrieved all persistent parameters for instrument %d (%d params)", instrID, copyCount);
}

// Check if instrument has saved parameter state
bool ft2_synth_has_persistent_state(int instrID) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) return false;
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    return state->hasState && state->magic == PERSISTENT_STORAGE_MAGIC;
}

// Clear parameter state (reset to defaults)
void ft2_synth_clear_persistent_state(int instrID) {
    if (!g_persistentStorageInitialized) ft2_synth_init_persistent_storage();
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ERROR: clear_persistent_state invalid instrID %d", instrID);
        return;
    }
    
    InstrumentParameterState* state = &g_persistentState[tf4Idx];
    state->hasState = false;
    state->currentPreset = -1;
    state->presetName[0] = '\0';
    g_current_preset[tf4Idx] = 0;

    // Also tear down the live engine instance so a future instrument reassigned to
    // this same slot doesn't inherit stale voice/parameter state.
    if (g_instruments[tf4Idx]) {
        tf_instrument_destroy(g_instruments[tf4Idx]);
        g_instruments[tf4Idx] = NULL;
    }

    TF4_DEBUG("Cleared persistent state for instrument %d", instrID);
}

// =============================================================================

// Skip preset on create for DXM Loader
void setSkipPresetOnCreate(bool skip) { 
    g_skipPresetOnCreate = skip; 
    extern void tf_set_skip_preset_on_create(bool skip);
    tf_set_skip_preset_on_create(skip);
}

// Create instrument instance if it doesn't exist for this slot
void* createInstrumentInstance(int instrID) {
    TF4_DEBUG("createInstrumentInstance() called for instrument %d", instrID);
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ERROR: Invalid instrument ID %d", instrID);
        return NULL;
    }
    
    if (!g_synth) {
        TF4_DEBUG("ERROR: Tunefish synth not initialized");
        return NULL;
    }
    
    if (!g_instruments[tf4Idx]) {
        g_instruments[tf4Idx] = tf_instrument_create(g_synth, instrID);
        if (g_instruments[tf4Idx]) {
            // Persistent parameters are now handled in the wrapper
            if (!g_skipPresetOnCreate && !ft2_synth_has_persistent_state(instrID)) {
                // Only load factory preset if not skipping and no persistent parameters
                tf_instrument_load_factory_preset(g_instruments[tf4Idx], 0);
                g_current_preset[tf4Idx] = 0;
            }
        }
    }
    
    return g_instruments[tf4Idx];
}

// Get existing instrument instance
void* getInstrumentInstance(int instrID) {
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        return NULL;
    }
    return g_instruments[tf4Idx];
}

// Kill all voices on all TF4 instruments
void ft2_synth_panic(void)
{
    if (!g_synthInitialized) return;
    for (int i = 0; i < MAX_INST; i++)
    {
        if (g_instruments[i])
        {
            tf_instrument_panic(g_instruments[i]);
        }
    }
}

void ft2_synth_init(int samplerate) 
{
    TF4_DEBUG("ft2_synth_init() called with samplerate=%d", samplerate);
    
    if (g_synthInitialized) {
        TF4_DEBUG("Synth already initialized, shutting down first");
        ft2_synth_shutdown();
    }
    
    g_sampleRate = samplerate;
    
    // Initialize persistent parameter storage system
    ft2_synth_init_persistent_storage();
    
    // Create Tunefish synth
    TF4_DEBUG("Creating Tunefish synth engine...");
    g_synth = tf_synth_create();
    if (!g_synth) {
        TF4_DEBUG("ERROR: Failed to create Tunefish synth!");
        return;
    }
    TF4_DEBUG("Tunefish synth created successfully");
    
    // Initialize Tunefish synth engine
    TF4_DEBUG("Initializing Tunefish synth with samplerate %d...", samplerate);
    tf_synth_init(g_synth, samplerate);
    TF4_DEBUG("Tunefish synth initialized");
    
    // Initialize per-instrument array
    for (int i = 0; i < MAX_INST; i++) {
        g_instruments[i] = NULL;
    }
    TF4_DEBUG("Cleared %d instrument slots", MAX_INST);
    
    // Calculate max buffer size based on FT2's allocation logic
    // This matches setupAudioBuffers() in ft2_audio.c
    const int32_t maxAudioFreq = MAX(MAX_AUDIO_FREQ, MAX_WAV_RENDER_FREQ);
    g_maxBufferSize = (int32_t)ceil(maxAudioFreq / (MIN_BPM / 2.5)) + 1;
    
    // Allocate audio buffers to match FT2's maximum size
    g_tempBufferL = (float*)calloc(g_maxBufferSize, sizeof(float));
    g_tempBufferR = (float*)calloc(g_maxBufferSize, sizeof(float));
    
    if (!g_tempBufferL || !g_tempBufferR) {
        TF4_DEBUG("ERROR: Failed to allocate audio buffers!");
        ft2_synth_shutdown();
        return;
    }
    TF4_DEBUG("Allocated audio buffers: %d samples (max)", g_maxBufferSize);
    
    // Set up output buffer pointers
    g_outputBuffers[0] = g_tempBufferL;
    g_outputBuffers[1] = g_tempBufferR;
    
    g_synthInitialized = true;
    TF4_DEBUG("Tunefish4 synth initialization complete!");

    // Rehydrate TF4 instruments after init (e.g., cold boot load)
    for (int i = 1; i <= MAX_INST; i++) {
        if (instr[i] && instr[i]->useTF4 && ft2_synth_has_persistent_state(i)) {
            createInstrumentInstance(i);
        }
    }
}

bool ft2_synth_is_initialized(void)
{
    return g_synthInitialized;
}

void ft2_synth_shutdown(void) 
{
    TF4_DEBUG("ft2_synth_shutdown() called");
    
    if (g_synthInitialized) {
        // Free buffers
        if (g_tempBufferL) {
            free(g_tempBufferL);
            g_tempBufferL = NULL;
            TF4_DEBUG("Freed left audio buffer");
        }
        if (g_tempBufferR) {
            free(g_tempBufferR);
            g_tempBufferR = NULL;
            TF4_DEBUG("Freed right audio buffer");
        }
        
        // Free all instrument instances
        int activeInstruments = 0;
        for (int i = 0; i < MAX_INST; i++) {
            if (g_instruments[i]) {
                tf_instrument_destroy(g_instruments[i]);
                g_instruments[i] = NULL;
                activeInstruments++;
            }
        }
        TF4_DEBUG("Freed %d active instrument instances", activeInstruments);
        
        // Free synth
        if (g_synth) {
            tf_synth_destroy(g_synth);
            g_synth = NULL;
            TF4_DEBUG("Destroyed Tunefish synth engine");
        }
        
        g_synthInitialized = false;
        TF4_DEBUG("Tunefish4 synth shutdown complete");
    } else {
        TF4_DEBUG("Synth was not initialized, nothing to shutdown");
    }
}

// Render Tunefish4 synth to buffers - called during channel mixing
void ft2_synth_render_separate(float *bufL, float *bufR, int nsamples, int add)
{
    if (!g_synthInitialized || !bufL || !bufR || !g_synth) {
        return;
    }
    
    // Validate buffer size
    if (nsamples > g_maxBufferSize) {
        TF4_DEBUG("WARNING: Buffer size %d exceeds max %d, clamping", nsamples, g_maxBufferSize);
        nsamples = g_maxBufferSize;
    }
    
    // Clear temp buffers
    memset(g_tempBufferL, 0, nsamples * sizeof(float));
    memset(g_tempBufferR, 0, nsamples * sizeof(float));
    
    const float preFaderGain = 0.5f;
    // Process all active V2 instruments and mix them together
    bool hasActiveInstruments = false;
    for (int i = 0; i < MAX_INST; i++) {
        if (g_instruments[i]) {
            hasActiveInstruments = true;
            
            // Clear per-instrument temp buffers
            static float instrL[4096], instrR[4096];
            if (nsamples <= 4096) {
                memset(instrL, 0, nsamples * sizeof(float));
                memset(instrR, 0, nsamples * sizeof(float));
                
                // Set up instrument output buffers
                float* instrOutputs[2] = {instrL, instrR};
                
                // Process this instrument
                tf_instrument_process(g_synth, g_instruments[i], instrOutputs, nsamples);
                
                // Mix into main buffers (apply pre-fader gain)
                for (int j = 0; j < nsamples; j++) {
                    g_tempBufferL[j] += instrL[j] * preFaderGain;
                    g_tempBufferR[j] += instrR[j] * preFaderGain;
                }
            }
        }
    }
    
    // Mix Tunefish4 synth output into the target buffers
    if (hasActiveInstruments) {
        for (int i = 0; i < nsamples; i++) {
            if (add) {
                bufL[i] += g_tempBufferL[i];
                bufR[i] += g_tempBufferR[i];
            } else {
                bufL[i] = g_tempBufferL[i];
                bufR[i] = g_tempBufferR[i];
            }
        }
    }
}

// Render Tunefish4 synth to specific stereo pair buffers
void ft2_synth_render_to_pair(int pairIdx, float *bufL, float *bufR, int nsamples, int add)
{
    if (!g_synthInitialized || !bufL || !bufR || !g_synth || pairIdx < 0 || pairIdx >= MAX_STEREO_PAIRS) {
        return;
    }
    
    // Validate buffer size
    if (nsamples > g_maxBufferSize) {
        TF4_DEBUG("WARNING: Buffer size %d exceeds max %d, clamping", nsamples, g_maxBufferSize);
        nsamples = g_maxBufferSize;
    }
    
    // Clear temp buffers
    memset(g_tempBufferL, 0, nsamples * sizeof(float));
    memset(g_tempBufferR, 0, nsamples * sizeof(float));
    
    const float preFaderGain = 0.5f;
    // Process all active V2 instruments and mix them together
    bool hasActiveInstruments = false;
    for (int i = 0; i < MAX_INST; i++) {
        if (g_instruments[i]) {
            hasActiveInstruments = true;
            
            // Clear per-instrument temp buffers
            static float instrL[4096], instrR[4096];
            if (nsamples <= 4096) {
                memset(instrL, 0, nsamples * sizeof(float));
                memset(instrR, 0, nsamples * sizeof(float));
                
                // Set up instrument output buffers
                float* instrOutputs[2] = {instrL, instrR};
                
                // Process this instrument
                tf_instrument_process(g_synth, g_instruments[i], instrOutputs, nsamples);
                
                // Mix into main buffers (apply pre-fader gain)
                for (int j = 0; j < nsamples; j++) {
                    g_tempBufferL[j] += instrL[j] * preFaderGain;
                    g_tempBufferR[j] += instrR[j] * preFaderGain;
                }
            }
        }
    }
    
    // Mix Tunefish4 synth output into the target stereo pair buffers
    if (hasActiveInstruments) {
        for (int i = 0; i < nsamples; i++) {
            if (add) {
                bufL[i] += g_tempBufferL[i];
                bufR[i] += g_tempBufferR[i];
            } else {
                bufL[i] = g_tempBufferL[i];
                bufR[i] = g_tempBufferR[i];
            }
        }
    }
}

// Render Tunefish4 synth for a specific instrument (tracker channel)
void ft2_synth_render_for_channel(int instrID, float *bufL, float *bufR, int nsamples, int add)
{
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (!g_synthInitialized || !bufL || !bufR || !g_synth || tf4Idx < 0) {
        return;
    }
    void* instrument = g_instruments[tf4Idx];
    if (!instrument) {
        // No synth instance for this instrument
        if (!add) {
            memset(bufL, 0, nsamples * sizeof(float));
            memset(bufR, 0, nsamples * sizeof(float));
        }
        return;
    }
    if (nsamples > g_maxBufferSize) nsamples = g_maxBufferSize;
    memset(g_tempBufferL, 0, nsamples * sizeof(float));
    memset(g_tempBufferR, 0, nsamples * sizeof(float));
    float* instrOutputs[2] = {g_tempBufferL, g_tempBufferR};
    tf_instrument_process(g_synth, instrument, instrOutputs, nsamples);
    const float preFaderGain = 0.5f;
    // Apply pre-fader gain before mixing
    for (int j = 0; j < nsamples; j++) {
        float l = g_tempBufferL[j] * preFaderGain;
        float r = g_tempBufferR[j] * preFaderGain;
        if (add) {
            bufL[j] += l;
            bufR[j] += r;
        } else {
            bufL[j] = l;
            bufR[j] = r;
        }
    }
}

// Legacy function for backward compatibility
void ft2_synth_render(float *buffer, int nsamples, int add)
{
    if (!g_synthInitialized || !buffer) {
        return;
    }
    
    // Render stereo separated
    ft2_synth_render_separate(g_tempBufferL, g_tempBufferR, nsamples, 0);
    
    // Interleave stereo output
    for (int i = 0; i < nsamples; i++) {
        float sampleL = g_tempBufferL[i];
        float sampleR = g_tempBufferR[i];
        
        if (add) {
            buffer[i * 2 + 0] += sampleL;
            buffer[i * 2 + 1] += sampleR;
        } else {
            buffer[i * 2 + 0] = sampleL;
            buffer[i * 2 + 1] = sampleR;
        }
    }
}

// Send MIDI message to current instrument (for UI use)
void ft2_synth_send_midi(uint8_t status, uint8_t data1, uint8_t data2) {
    TF4_DEBUG("ft2_synth_send_midi() called: status=0x%02X, data1=%d, data2=%d", 
             status, data1, data2);
    
    extern struct editor_t editor; // from ft2_structs.h
    int currentInstr = editor.curInstr;
    if (currentInstr >= 1 && currentInstr <= MAX_INST) {
        ft2_synth_send_midi_to_instrument(currentInstr, status, data1, data2);
    } else {
        TF4_DEBUG("ERROR: Invalid current instrument %d", currentInstr);
    }
}

// Send MIDI message to a specific instrument
void ft2_synth_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2) {
    TF4_DEBUG("ft2_synth_send_midi_to_instrument() called: instrID=%d, status=0x%02X, data1=%d, data2=%d", 
              instrID, status, data1, data2);
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ERROR: Invalid instrument ID %d", instrID);
        return;
    }
    
    void* instrument = g_instruments[tf4Idx];
    if (!instrument) {
        // Auto-create instrument on first MIDI message
        instrument = createInstrumentInstance(instrID);
        if (!instrument) {
            TF4_DEBUG("ERROR: Could not create/get instrument %d", instrID);
            return;
        }
    }
    
    tf_instrument_send_midi(instrument, status, data1, data2);
    TF4_DEBUG("Sent MIDI to instrument %d: 0x%02X %d %d", instrID, status, data1, data2);
}

// Preset loading functions for FT2 integration
int ft2_synth_get_preset_count(void) {
    return tf_get_factory_preset_count();
}

const char* ft2_synth_get_preset_name(int index) {
    return tf_get_factory_preset_name(index);
}

int ft2_synth_load_preset_for_instrument(int instrID, int presetIndex) {
    TF4_DEBUG("ft2_synth_load_preset_for_instrument() called: instrID=%d, presetIndex=%d", instrID, presetIndex);
    
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ERROR: Invalid instrument ID %d", instrID);
        return 0;
    }
    
    if (!g_synthInitialized || presetIndex < 0) return 0;
    void* instrument = createInstrumentInstance(instrID);
    if (!instrument) {
        TF4_DEBUG("ERROR: No Tunefish4 instrument in slot %d", instrID);
        return 0;
    }
    
    int result = tf_instrument_load_factory_preset(instrument, presetIndex);
    if (result) {
        // Track the current preset index
        g_current_preset[tf4Idx] = presetIndex;
        
        const char* presetName = tf_get_factory_preset_name(presetIndex);
        TF4_DEBUG("Successfully loaded preset %d (\"%s\") into instrument %d", 
                  presetIndex, presetName ? presetName : "Unknown", instrID);
    } else {
        TF4_DEBUG("ERROR: Failed to load preset %d into instrument %d", presetIndex, instrID);
    }
    
    return result;
}

// Get current preset index for an instrument
int ft2_synth_get_current_preset_for_instrument(int instrID) {
    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        return -1; // Invalid instrument ID
    }
    
    if (!g_instruments[tf4Idx]) {
        return -1; // No TF4 instrument in this slot
    }
    
    return g_current_preset[tf4Idx];
}

// Get current preset name for an instrument
const char* ft2_synth_get_current_preset_name_for_instrument(int instrID) {
    int presetIndex = ft2_synth_get_current_preset_for_instrument(instrID);
    if (presetIndex < 0) {
        return NULL; // No valid preset
    }
    
    return ft2_synth_get_preset_name(presetIndex);
} 

// ------------------------------------------------------------
// Generic parameter access helpers (direct TF4 param read/write)
// ------------------------------------------------------------

float ft2_synth_get_param(int instrID, int param)
{
    if (!g_synthInitialized) {
        TF4_DEBUG("ft2_synth_get_param() called while synth not initialized");
        return 0.0f;
    }

    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ft2_synth_get_param() invalid instrID %d", instrID);
        return 0.0f;
    }

    if (param < 0 /*|| param >= TF_PARAM_COUNT*/) {
        // We cannot include tf4.hpp (C++ header) from C file, so just check >=0
        TF4_DEBUG("ft2_synth_get_param() invalid param %d", param);
        return 0.0f;
    }

    void* instrument = g_instruments[tf4Idx];
    if (!instrument) {
        // No instrument yet, treat as default value 0.0
        return 0.0f;
    }

    return tf_instrument_get_param(instrument, param);
}

int ft2_synth_get_active_voice_count(int instrID)
{
    if (!g_synthInitialized)
        return 0;

    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0)
        return 0;

    void* instrument = g_instruments[tf4Idx];
    if (!instrument)
        return 0;

    return tf_instrument_get_active_voice_count(instrument);
}

void ft2_synth_set_param(int instrID, int param, float value)
{
    if (!g_synthInitialized) {
        TF4_DEBUG("ft2_synth_set_param() called while synth not initialized");
        return;
    }

    int tf4Idx = tf4_index_from_ft2(instrID);
    if (tf4Idx < 0) {
        TF4_DEBUG("ft2_synth_set_param() invalid instrID %d", instrID);
        return;
    }

    if (param < 0 /*|| param >= TF_PARAM_COUNT*/) {
        TF4_DEBUG("ft2_synth_set_param() invalid param %d", param);
        return;
    }

    void* instrument = g_instruments[tf4Idx];
    if (!instrument) {
        instrument = createInstrumentInstance(instrID);
        if (!instrument) {
            TF4_DEBUG("ft2_synth_set_param() failed to create instrument %d", instrID);
            return;
        }
    }

    tf_instrument_set_param(instrument, param, value);
} 

// =============================================================================
// Enumeration name helpers (FX types, Mod Matrix)
// =============================================================================

// Modulation Matrix Sources (taken from PluginEditor MOD_SOURCES string)
static const char* const g_modSourceNames[] = {
    "none", "LFO1", "LFO2", "ADSR1", "ADSR2", "ModWheel"
};
static const int g_modSourceCount = sizeof(g_modSourceNames) / sizeof(g_modSourceNames[0]);

// Modulation Matrix Destinations (taken from PluginEditor MOD_TARGETS string)
static const char* const g_modDestNames[] = {
    "none", "Bandwidth", "Damp", "Harmonics", "Scale", "Volume", "Frequency", "Panning",
    "Detune", "Spread", "Drive", "Noise", "LP Cutoff", "LP Resonance", "HP Cutoff",
    "HP Resonance", "BP Cutoff", "BP Q", "NT Cutoff", "NT Q", "ADSR1 Decay", "ADSR2 Decay",
    "Mod1", "Mod2", "Mod3", "Mod4", "Mod5", "Mod6", "Mod7", "Mod8", "LFO1 Depth", "LFO2 Depth",
    // --- Extended FX parameter targets (reserved slots 32-39) ---
    "Distortion Amt",  // 32
    "Delay Left",      // 33
    "Delay Right",     // 34
    "Delay Decay",     // 35
    "Reverb Wet",      // 36
    "Flanger Wet",     // 37
    "Chorus Gain",     // 38
    "Formant Wet"      // 39
};
static const int g_modDestCount = sizeof(g_modDestNames) / sizeof(g_modDestNames[0]);

// FX section names (taken from PluginEditor FX_SECTIONS string)
static const char* const g_fxTypeNames[] = {
    "none", "Distortion", "Delay", "Chorus", "Flanger", "Reverb", "Formant", "EQ"
};
static const int g_fxTypeCount = sizeof(g_fxTypeNames) / sizeof(g_fxTypeNames[0]);

// Accessor wrappers used by GUI ---------------------------------------------
const char* const* ft2_synth_get_mod_source_items(void) { return g_modSourceNames; }
int ft2_synth_get_mod_source_count(void) { return g_modSourceCount; }

const char* const* ft2_synth_get_mod_dest_items(void) { return g_modDestNames; }
int ft2_synth_get_mod_dest_count(void) { return g_modDestCount; }

const char* const* ft2_synth_get_fx_type_items(void) { return g_fxTypeNames; }
int ft2_synth_get_fx_type_count(void) { return g_fxTypeCount; }
// ============================================================================= 
