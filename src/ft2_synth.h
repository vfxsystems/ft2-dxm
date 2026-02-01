#ifndef FT2_SYNTH_H
#define FT2_SYNTH_H

// FT2 integration wrapper for Tunefish4 synth engine
#include <stdint.h>

// Initialize Tunefish4 synth. samplerate in Hz (e.g. 44100).
void ft2_synth_init(int samplerate);

// Shutdown and free Tunefish4 synth resources.
void ft2_synth_shutdown(void);

// Query Tunefish4 synth initialization state.
bool ft2_synth_is_initialized(void);

// Render interleaved stereo float samples: output buffer must have 2*nsamples floats.
// add==0: overwrite, add!=0: add to existing data.
void ft2_synth_render(float *buffer, int nsamples, int add);

// Render stereo float samples into separate left/right buffers.
void ft2_synth_render_separate(float *bufL, float *bufR, int nsamples, int add);

// Render Tunefish4 synth to specific stereo pair buffers.
void ft2_synth_render_to_pair(int pairIdx, float *bufL, float *bufR, int nsamples, int add);

// Render Tunefish4 synth to specific stereo pair buffers.
void ft2_synth_render_for_channel(int instrID, float *bufL, float *bufR, int nsamples, int add);

// Send a single MIDI message (status byte, data1, data2) to Tunefish4 synth.
void ft2_synth_send_midi(uint8_t status, uint8_t data1, uint8_t data2);

// Send a MIDI message to a specific instrument ID.
void ft2_synth_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2);

// Preset loading functions for FT2 integration
int ft2_synth_get_preset_count(void);
const char* ft2_synth_get_preset_name(int index);
int ft2_synth_load_preset_for_instrument(int instrID, int presetIndex);

// Current preset information functions for instrument display
int ft2_synth_get_current_preset_for_instrument(int instrID);
const char* ft2_synth_get_current_preset_name_for_instrument(int instrID);

// === Enumeration name helpers (FX types, Mod Matrix) ===
const char* const* ft2_synth_get_mod_source_items(void);
int ft2_synth_get_mod_source_count(void);
const char* const* ft2_synth_get_mod_dest_items(void);
int ft2_synth_get_mod_dest_count(void);
const char* const* ft2_synth_get_fx_type_items(void);
int ft2_synth_get_fx_type_count(void);

// =============================================================================
// PER-INSTANCE PARAMETER PERSISTENCE SYSTEM
// =============================================================================

// Store/restore parameter state for an instrument (survives instance creation/destruction)
void ft2_synth_store_instrument_state(int instrID);
void ft2_synth_restore_instrument_state(int instrID);

// Set/get individual parameters with automatic persistence
void ft2_synth_set_persistent_param(int instrID, int param, float value);
float ft2_synth_get_persistent_param(int instrID, int param);

// Bulk parameter operations for DXM loading/saving
void ft2_synth_set_all_persistent_params(int instrID, const float* params, int paramCount);
void ft2_synth_get_all_persistent_params(int instrID, float* params, int paramCount);

// Check if instrument has saved parameter state
bool ft2_synth_has_persistent_state(int instrID);

// Clear parameter state (reset to defaults)
void ft2_synth_clear_persistent_state(int instrID);

// Initialize/cleanup persistent storage system
void ft2_synth_init_persistent_storage(void);
void ft2_synth_cleanup_persistent_storage(void);

// =============================================================================

// C wrapper functions for Tunefish4 synth
extern void* tf_synth_create(void);
extern void tf_synth_destroy(void* synth);
extern void tf_synth_init(void* synth, unsigned int sampleRate);

extern void* tf_instrument_create(void* synth, int instrID);
extern void tf_instrument_destroy(void* instrument);
extern void tf_set_skip_preset_on_create(bool skip);
extern void tf_instrument_note_on(void* instrument, int note, int velocity);
extern void tf_instrument_note_off(void* instrument, int note);
extern void tf_instrument_set_param(void* instrument, int param, float value);
extern float tf_instrument_get_param(void* instrument, int param);
extern void tf_instrument_process(void* synth, void* instrument, float** outputs, unsigned int frameSize);
extern void tf_instrument_send_midi(void* instrument, unsigned char status, unsigned char data1, unsigned char data2);

// Preset loading functions
extern int tf_get_factory_preset_count(void);
extern const char* tf_get_factory_preset_name(int index);
extern int tf_instrument_load_factory_preset(void* instrument, int index);

// Query number of currently active voices for an instrument (0..TF_MAXVOICES)
int tf_instrument_get_active_voice_count(void* instrument);

// Voice data access functions for waveform view
void* tf_instrument_get_latest_voice(void* instrument);
float* tf_voice_get_freq_table(void* voice);
float* tf_voice_get_freq_mod_table(void* voice);
float* tf_voice_get_result_table(void* voice);
float tf_voice_get_modulation(void* voice);
void* tf_voice_get_mod_matrix(void* voice);
void* tf_voice_get_generator(void* voice);
float tf_instrument_get_drive_param(void* instrument);
int tf_voice_is_playing(void* voice);
float tf_voice_get_current_freq(void* voice);
int tf_voice_get_current_note(void* voice);
int tf_voice_get_current_velocity(void* voice);

// Kill all voices of all TF4 instruments
void ft2_synth_panic(void);
void tf_instrument_panic(void* instrument);
// Current preset information functions for instrument display

// Effect system wrapper functions
extern void* tf_effect_create(int effectType);
extern void tf_effect_delete(int effectType, void* fx);
extern void tf_effect_process(int effectType, void* fx, void* synth, void* instr, float** signal, unsigned int len);

// ADSR Slope parameter setters/getters
void ft2_synth_set_adsr1_slope(int instrID, int value);
int ft2_synth_get_adsr1_slope(int instrID);
void ft2_synth_set_adsr2_slope(int instrID, int value);
int ft2_synth_get_adsr2_slope(int instrID);

// Generic Tunefish4 parameter access helpers
void ft2_synth_set_param(int instrID, int param, float value);
float ft2_synth_get_param(int instrID, int param);

void ft2_synth_set_unisono(int instrID, int value);
void ft2_synth_set_octave(int instrID, int value);

// Active voice count for an instrument (0 if none or not initialized)
int ft2_synth_get_active_voice_count(int instrID);

#endif // FT2_SYNTH_H 
