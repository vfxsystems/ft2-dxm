#ifndef FT2_DEXED_H
#define FT2_DEXED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global synth (engine factory) */
void* dx_synth_create(void);
void  dx_synth_destroy(void* synth);
void  dx_synth_init(void* synth, unsigned int sampleRate);

/* Per-instrument wrapper */
void* dx_instrument_create(void* synth, int instrID);
void  dx_instrument_destroy(void* inst);
void  dx_instrument_process(void* synth, void* inst, float** outputs, unsigned int frameSize);
void  dx_instrument_send_midi(void* inst, uint8_t status, uint8_t data1, uint8_t data2);

/* Parameter access */
void  dx_instrument_set_param(void* inst, int param, float value);
float dx_instrument_get_param(void* inst, int param);
int   dx_instrument_get_params(void* inst, float* out, int maxCount);

/* Factory preset access (lazy loaded) */
int         dx_get_factory_preset_count(void);
const char* dx_get_factory_preset_name(int index);
int         dx_instrument_load_factory_preset(void* inst, int index);

/* Unified Patch Loading API */
int  dx_instrument_load_patch(void* inst, const uint8_t* data, size_t size);
int  dx_instrument_load_packed_patch(void* inst, const uint8_t* data, size_t size);
int  dx_instrument_save_patch(void* inst, uint8_t* outBuffer, size_t outBufferSize);
int  dx_instrument_save_packed_patch(void* inst, uint8_t* outBuffer, size_t outBufferSize);

/* Patch format conversion helpers */
int dx_normalize_program(const uint8_t* unpacked155, uint8_t* outPacked128);
int dx_unpack_program_from_storage(const uint8_t* packed128, uint8_t* outUnpacked155);

/* State serialization */
size_t dx_instrument_serialize_state(void* inst, uint8_t* outBuf, size_t bufSize);
int    dx_instrument_deserialize_state(void* inst, const uint8_t* data, size_t size);

/* Voice utilities */
int   dx_instrument_get_active_voice_count(void* inst);
void  dx_instrument_panic(void* inst);
void* dx_instrument_get_latest_voice(void* inst);
int dx_instrument_get_current_preset_index(void* inst);

/* FT2-level helpers */
void ft2_dx_init(int samplerate);
void ft2_dx_shutdown(void);
void ft2_dx_render(float* bufL, float* bufR, int nsamples, int add);
void ft2_dx_render_for_channel(int instrID, float* bufL, float* bufR, int nsamples, int add);
void ft2_dx_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2);
int ft2_dx_load_packed_patch_for_instrument(int instrID, const uint8_t* data, size_t size);
void ft2_dx_set_param_for_instrument(int instrID, int paramId, float value);
float ft2_dx_get_param_for_instrument(int instrID, int paramId);
int   ft2_dx_get_params_for_instrument(int instrID, float* out, int maxCount);
int    ft2_dx_get_factory_preset_count(void);
const char* ft2_dx_get_factory_preset_name(int index);
int    ft2_dx_load_factory_preset_for_instrument(int instrID, int index);
int    ft2_dx_load_factory_preset_for_current_instrument(int index);
int    ft2_dx_get_current_preset_for_instrument(int instrID);
int    ft2_dx_get_active_voice_count(int instrID);
void ft2_dx_panic(void);
int ft2_dx_get_patch_data(int instrID, uint8_t *buffer, int32_t bufferSize);
int ft2_dx_get_packed_patch_data(int instrID, uint8_t *buffer, int32_t bufferSize);
int ft2_dx_load_patch_for_instrument(int instrID, const uint8_t *data, size_t size);
bool ft2_dx_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* FT2_DEXED_H */
