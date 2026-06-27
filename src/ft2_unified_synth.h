#ifndef FT2_UNIFIED_SYNTH_H
#define FT2_UNIFIED_SYNTH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Synth engine types
typedef enum {
    SYNTH_TYPE_TUNEFISH4,
    SYNTH_TYPE_DEXED,
    SYNTH_TYPE_V2,
    SYNTH_TYPE_SAMPLES,
    SYNTH_TYPE_COUNT
} SynthEngineType;

// MIDI message structure
typedef struct {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
} MidiMessage;

// Parameter range structure
typedef struct {
    float min;
    float max;
    float def; // Changed from 'default' to avoid keyword conflict
} ParameterRange;

// Unified synth interface
typedef struct {
    // Engine identification
    SynthEngineType engineType;
    const char* engineName;
    
    // Lifecycle management
    void (*init)(int samplerate);
    void (*shutdown)(void);
    
    // Rendering
    void (*render)(float* bufL, float* bufR, int nsamples, int add);
    void (*render_for_channel)(int instrID, float* bufL, float* bufR, int nsamples, int add);
    
    // MIDI handling
    void (*send_midi)(int instrID, const MidiMessage* message);
    void (*panic)(void);
    
    // Parameter management
    void (*set_param)(int instrID, int paramId, float value);
    float (*get_param)(int instrID, int paramId);
    const ParameterRange* (*get_param_range)(int paramId);
    int (*get_param_count)(void);
    const char* (*get_param_name)(int paramId);
    
    // Patch management
    int (*load_patch)(int instrID, const uint8_t* data, size_t size);
    int (*save_patch)(int instrID, uint8_t* buffer, size_t bufferSize);
    int (*get_patch_size)(int instrID);
    
    // Factory presets
    int (*get_preset_count)(void);
    const char* (*get_preset_name)(int presetIndex);
    int (*load_preset)(int instrID, int presetIndex);
    
    // State management
    void (*store_state)(int instrID);
    void (*restore_state)(int instrID);
    bool (*has_state)(int instrID);
    void (*clear_state)(int instrID);
    
    // Voice monitoring
    int (*get_active_voices)(int instrID);
    
    // Engine-specific data
    void* engineData;
} UnifiedSynthInterface;

// Global synth management
void ft2_unified_synth_init(int samplerate);
void ft2_unified_synth_shutdown(void);
void ft2_unified_synth_set_samplerate(int samplerate);

// Engine registration
void ft2_unified_synth_register_engine(const UnifiedSynthInterface* engine);
void ft2_unified_synth_unregister_engine(SynthEngineType engineType);

// Engine access
const UnifiedSynthInterface* ft2_unified_synth_get_engine(SynthEngineType engineType);
SynthEngineType ft2_unified_synth_get_active_engine(int instrID);

// Global rendering (all active engines)
void ft2_unified_synth_render_all(float* bufL, float* bufR, int nsamples, int add);

// Channel-specific rendering
void ft2_unified_synth_render_channel(int instrID, float* bufL, float* bufR, int nsamples, int add);

// MIDI dispatch
void ft2_unified_synth_send_midi(int instrID, const MidiMessage* message);

// Parameter management (delegated to appropriate engine)
void ft2_unified_synth_set_param(int instrID, int paramId, float value);
float ft2_unified_synth_get_param(int instrID, int paramId);

// Patch management
int ft2_unified_synth_load_patch(int instrID, const uint8_t* data, size_t size, bool isPacked);
int ft2_unified_synth_save_patch(int instrID, uint8_t* buffer, size_t bufferSize, bool* isPacked);

// Factory presets
int ft2_unified_synth_get_preset_count(int instrID);
const char* ft2_unified_synth_get_preset_name(int instrID, int presetIndex);
int ft2_unified_synth_load_preset(int instrID, int presetIndex);

// State management
void ft2_unified_synth_store_state(int instrID);
void ft2_unified_synth_restore_state(int instrID);
bool ft2_unified_synth_has_state(int instrID);
void ft2_unified_synth_clear_state(int instrID);

// Utility functions
const char* ft2_unified_synth_get_engine_name(SynthEngineType engineType);
bool ft2_unified_synth_is_engine_supported(SynthEngineType engineType);

// Legacy compatibility - redirect old APIs to unified system
// These allow existing code to work without immediate refactoring
void ft2_synth_legacy_render(float *buffer, int nsamples, int add);
void ft2_dx_legacy_render(float *bufL, float *bufR, int nsamples, int add);

#ifdef __cplusplus
}
#endif

#endif /* FT2_UNIFIED_SYNTH_H */
