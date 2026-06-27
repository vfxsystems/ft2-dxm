#ifndef FT2_V2_H
#define FT2_V2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ft2_unified_synth.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FT2_V2_CTL_SKIP = 0,
    FT2_V2_CTL_SLIDER = 1,
    FT2_V2_CTL_MB = 2
} Ft2V2CtlType;

typedef struct {
    int version;
    const char* name;
    Ft2V2CtlType ctltype;
    int offset;
    int min;
    int max;
    int isdest;
    const char* ctlstr;
} Ft2V2ParamInfo;

typedef struct {
    int count;
    const char* name;
    const char* short_name;
} Ft2V2TopicInfo;

void ft2_v2_init(int samplerate);
void ft2_v2_shutdown(void);
bool ft2_v2_is_initialized(void);

int ft2_v2_get_patch_size(void);

int ft2_v2_get_param_count(void);
const char* ft2_v2_get_param_name(int paramId);
const Ft2V2ParamInfo* ft2_v2_get_param_info(int paramId);
const ParameterRange* ft2_v2_get_param_range(int paramId);

int ft2_v2_get_global_param_count(void);
const char* ft2_v2_get_global_param_name(int paramId);
const Ft2V2ParamInfo* ft2_v2_get_global_param_info(int paramId);
const ParameterRange* ft2_v2_get_global_param_range(int paramId);

int ft2_v2_get_topic_count(void);
const Ft2V2TopicInfo* ft2_v2_get_topic_info(int topicIndex);
int ft2_v2_get_topic_param_start(int topicIndex);
int ft2_v2_get_topic_param_count(int topicIndex);

int ft2_v2_get_global_topic_count(void);
const Ft2V2TopicInfo* ft2_v2_get_global_topic_info(int topicIndex);
int ft2_v2_get_global_topic_param_start(int topicIndex);
int ft2_v2_get_global_topic_param_count(int topicIndex);

int ft2_v2_get_mod_source_count(void);
const char* ft2_v2_get_mod_source_name(int index);

int ft2_v2_get_mod_dest_count(void);
const char* ft2_v2_get_mod_dest_name(int index);
int ft2_v2_get_mod_dest_param_index(int index);
int ft2_v2_find_mod_dest_list_index(int paramId);

int ft2_v2_get_factory_preset_count(void);
const char* ft2_v2_get_factory_preset_name(int index);
const char* ft2_v2_get_preset_name_for_instrument(int instrID, int index);
int ft2_v2_get_current_preset_for_instrument(int instrID);
const char* ft2_v2_get_current_preset_name_for_instrument(int instrID);
int ft2_v2_load_preset_for_instrument(int instrID, int index);
int ft2_v2_load_factory_preset_for_instrument(int instrID, int index);
int ft2_v2_load_factory_preset_for_current_instrument(int index);

int ft2_v2_load_patch_for_instrument(int instrID, const uint8_t* data, size_t size);
int ft2_v2_get_patch_data(int instrID, uint8_t* buffer, int32_t bufferSize);
size_t ft2_v2_serialize_state(int instrID, uint8_t* outBuf, size_t bufSize);
int ft2_v2_deserialize_state(int instrID, const uint8_t* data, size_t size);

void ft2_v2_set_param_for_instrument(int instrID, int paramId, float value);
float ft2_v2_get_param_for_instrument(int instrID, int paramId);
void ft2_v2_set_global_param_for_instrument(int instrID, int paramId, float value);
float ft2_v2_get_global_param_for_instrument(int instrID, int paramId);
int ft2_v2_get_mod_count_for_instrument(int instrID);
void ft2_v2_set_mod_count_for_instrument(int instrID, int count);
void ft2_v2_get_mod_slot_for_instrument(int instrID, int slot, int* source, int* amount, int* dest);
void ft2_v2_set_mod_slot_for_instrument(int instrID, int slot, int source, int amount, int dest);

int ft2_v2_get_active_voice_count(int instrID);
void ft2_v2_send_midi_to_instrument(int instrID, uint8_t status, uint8_t data1, uint8_t data2);
void ft2_v2_panic(void);

void ft2_v2_store_instrument_state(int instrID);
void ft2_v2_restore_instrument_state(int instrID);
bool ft2_v2_has_persistent_state(int instrID);
void ft2_v2_clear_persistent_state(int instrID);

void ft2_v2_render(float* bufL, float* bufR, int nsamples, int add);
void ft2_v2_render_for_channel(int instrID, float* bufL, float* bufR, int nsamples, int add);

#ifdef __cplusplus
}
#endif

#endif /* FT2_V2_H */
