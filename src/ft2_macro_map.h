#ifndef FT2_MACRO_MAP_H
#define FT2_MACRO_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ft2_unified_synth.h"

typedef struct instr_t instr_t;

#define FT2_MACRO_MAP_NUM_SLOTS 16
#define FT2_MACRO_MAP_PUSHBUTTON_COUNT 86

/*
** Central list of Tunefish-4 parameter names exposed to the Macro-Map UI.
** The order must match the TF_* enum defined in tf4.hpp / tf4.cpp.
** TF4 is one of several synth targets handled by the shared Macro Map helpers.
*/

#ifdef __cplusplus
extern "C" {
#endif

extern const char *const tf4_param_names[];
extern const int tf4_param_name_count; /* equals TF_PARAM_COUNT */

/* Convenience helpers (bounds-checked) */
static inline const char *tf4_param_name(int id)
{
    extern const char *const tf4_param_names[];
    extern const int tf4_param_name_count;
    return (id >= 0 && id < tf4_param_name_count) ? tf4_param_names[id] : "?";
}

static inline int tf4_param_count(void)
{
    extern const int tf4_param_name_count;
    return tf4_param_name_count;
}

void showMacroMapEditor(void);
void hideMacroMapEditor(void);

// UI sync functions
void requestMacroUiSync(void);
void handleMacroUiSync(void);
void requestDspUiSync(void);
void handleDspUiSync(void);

bool ft2_macro_map_target_to_engine(uint8_t target, SynthEngineType *out);
bool ft2_macro_map_target_matches_instrument(uint8_t target, const instr_t *ins);
int ft2_macro_map_target_param_count(uint8_t target);
const char *ft2_macro_map_target_param_name(uint8_t target, uint16_t paramId);
void ft2_macro_map_sanitize_slot(instr_t *ins, int slot);
void ft2_macro_map_sanitize_instrument(instr_t *ins);
bool ft2_macro_map_self_test(char *errBuf, size_t errBufSize);

#ifdef __cplusplus
}
#endif

#endif /* FT2_MACRO_MAP_H */
