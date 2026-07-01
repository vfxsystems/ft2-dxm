#ifndef FT2_OSTIRUS_H
#define FT2_OSTIRUS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FT2_OSTIRUS_MAX_SLOTS 16
#define FT2_OSTIRUS_MAX_CATEGORIES 23

void ft2_ostirus_init(int samplerate);
void ft2_ostirus_shutdown(void);
bool ft2_ostirus_is_initialized(void);

void ft2_ostirus_render(float *bufL, float *bufR, int nsamples, int add);
void ft2_ostirus_render_for_channel(int instrID, float *bufL, float *bufR, int nsamples, int add);
void ft2_ostirus_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2);
void ft2_ostirus_panic(void);

int ft2_ostirus_get_factory_preset_count(void);
const char *ft2_ostirus_get_factory_preset_name(int index);
const char *ft2_ostirus_get_factory_preset_plain_name(int index);
int ft2_ostirus_get_factory_preset_bank(int index);
int ft2_ostirus_get_factory_preset_program(int index);
int ft2_ostirus_get_factory_preset_category1(int index);
int ft2_ostirus_get_factory_preset_category2(int index);
const char *ft2_ostirus_get_category_name(int categoryIndex);
const char *ft2_ostirus_get_rom_model_name(void);
int ft2_ostirus_find_factory_preset(int bank, int program);
int ft2_ostirus_get_default_factory_preset_index(void);
int ft2_ostirus_load_factory_preset_for_instrument(int instrID, int index);
int ft2_ostirus_get_current_preset_for_instrument(int instrID);
const char *ft2_ostirus_get_current_preset_name_for_instrument(int instrID);

void ft2_ostirus_set_param_for_instrument(int instrID, int paramId, float value);
float ft2_ostirus_get_param_for_instrument(int instrID, int paramId);
int ft2_ostirus_get_active_voice_count(int instrID);

int ft2_ostirus_assign_slot_for_instrument(int instrID);
int ft2_ostirus_get_slot_for_instrument(int instrID);
void ft2_ostirus_release_instrument(int instrID);
size_t ft2_ostirus_serialize_state(int instrID, uint8_t *outBuf, size_t bufSize);
int ft2_ostirus_deserialize_state(int instrID, const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* FT2_OSTIRUS_H */
