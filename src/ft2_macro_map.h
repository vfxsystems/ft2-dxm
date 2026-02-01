#ifndef FT2_MACRO_MAP_H
#define FT2_MACRO_MAP_H

/*
** Central list of Tunefish-4 parameter names exposed to the Macro-Map UI.
** The order must match the TF_* enum defined in tf4.hpp / tf4.cpp.
** Only Synth parameters are listed for now – DSP targets will be added later.
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

#ifdef __cplusplus
}
#endif

void showMacroMapEditor(void);
void hideMacroMapEditor(void);

// UI sync functions
void requestMacroUiSync(void);
void handleMacroUiSync(void);
void requestDspUiSync(void);
void handleDspUiSync(void);

#endif /* FT2_MACRO_MAP_H */
