#include "ft2_tunefish_complete_layout.h"
#include "ft2_tunefish_widgets.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_structs.h"
#include "ft2_synth.h"
#include "ft2_header.h"
#include "ft2_radiobuttons.h"
#include "ft2_pushbuttons.h"
#include "ft2_scrollbars.h"
#include "ft2_checkboxes.h"
#include "ft2_waveform_view.h"
#include "ft2_inst_ed.h"
#include "ft2_bmp.h"
#include "shared/ft2_ui_schema.h"
#include "shared/ft2_ui_assets.h"
#ifdef TF_USE_SCHEMA_LAYOUT
#include "ft2_tunefish_complete_layout_schema.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef CLAMP
#define CLAMP(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))
#endif

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

// Redraw trigger stub
static void tf_request_redraw(void) { /* TODO: Implement redraw trigger */ }

// Widget management
static void tf_update_widget_visibility(TunefishCompleteLayout* layout);
static void tf_update_live_meters(TunefishCompleteLayout* layout);
static void tf_draw_adsr_envs(TunefishCompleteLayout* layout);
static bool tf_handle_adsr_env_mouse(TunefishCompleteLayout* layout, int mouseX, int mouseY, bool pressed);

// Widget creation helpers
static void create_global_widgets(TunefishCompleteLayout* layout, int* allIdx);
static void create_page1_widgets(TunefishCompleteLayout* layout, int* allIdx, int* p1Idx);
static void create_page2_widgets(TunefishCompleteLayout* layout, int* allIdx, int* p2Idx);

// Preset management
static void tf_preset_combo_selected(TunefishWidget* widget, int selectedIndex);
static void tf_populate_preset_combo(TunefishCompleteLayout* layout);
static void tf_load_preset_by_index(TunefishCompleteLayout* layout, int presetIndex);

// Callback helpers
static void tf_connect_widget_callbacks(TunefishCompleteLayout* layout);
static void tf_lfo1shape_button_value_handler(TunefishWidget* w, float v);
static void tf_lfo2shape_button_value_handler(TunefishWidget* w, float v);
static void tf_unisono_button_handler(TunefishWidget* w);
static void tf_octave_button_handler(TunefishWidget* w);
static void tf_formant_button_handler(TunefishWidget* w);
static void tf_draw_adsr_envs(TunefishCompleteLayout* layout);
static bool tf_handle_adsr_env_mouse(TunefishCompleteLayout* layout, int mouseX, int mouseY, bool pressed);

// Parameter synchronization
static void tf_refresh_shadow_parameters_from_synth(void);

// Ensure preset combo sync helper is visible before use
static void tf_sync_preset_combo_with_instrument(TunefishCompleteLayout* layout);

// =============================================================================
// EXTERNAL FUNCTION DECLARATIONS
// =============================================================================

// Synth parameter functions
extern void ft2_synth_send_midi_to_instrument(int instrID, uint8_t cmd, uint8_t data1, uint8_t data2);

// Preset functions from ft2_synth.c
extern int ft2_synth_get_preset_count(void);
extern const char* ft2_synth_get_preset_name(int index);
extern int ft2_synth_get_current_preset_for_instrument(int instrID);
extern int ft2_synth_load_preset_for_instrument(int instrID, int presetIndex);

// Exit button pending close (allows pressed visual before hiding)
static bool g_tf_exit_pending = false;

// Missing external variable definitions for linking
extern bool synthLFO1Sync;
extern bool synthLFO2Sync;
extern int synthPoly;
extern int synthPitchUp;

// Local implementation of getCurrentTF4InstrumentID (since the original is static in ft2_synth_ed.c)
static int getCurrentTF4InstrumentID(void) {
    extern struct editor_t editor; // from ft2_structs.h
    if (editor.curInstr >= 1 && editor.curInstr <= MAX_INST) {
        return editor.curInstr; // Use FT2 instrument number directly (1-based)
    }
    return -1; // Invalid
}

// =============================================================================
// PARAMETER BINDING SYSTEM
// =============================================================================

// Parameter binding structure
typedef struct {
    const char* widgetName;
    int paramId; // Direct Tunefish4 parameter ID
    int minValue;
    int maxValue;
    int* valuePtr;  // Pointer to the actual parameter value
} TunefishParameterBinding;

// Parameter value storage
extern int synthPoly, synthPitchUp, synthPitchDown, synthPatch;
extern int synthGenVolume, synthGenPanning, synthGenSpread, synthGenBandwidth, synthGenGlide;
extern int synthGenDamp, synthGenHarmonics, synthGenDrive, synthGenScale;
extern int synthGenModulation, synthGenNoise, synthGenNoiseFreq, synthGenNoiseBW;
extern int synthFilterLPFreq, synthFilterLPRes, synthFilterHPFreq, synthFilterHPRes;
extern int synthFilterBPFreq, synthFilterBPRes, synthFilterNTFreq, synthFilterNTRes;
extern int synthLFO1Rate, synthLFO1Depth, synthLFO1Shape, synthLFO2Rate, synthLFO2Depth, synthLFO2Shape;
extern bool synthLFO1Sync, synthLFO2Sync;
extern int adsr1A, adsr1D, adsr1S, adsr1R, adsr1Slope, adsr2A, adsr2D, adsr2S, adsr2R, adsr2Slope;
extern int synthUnisono, synthOctave;
extern int fxFlangerLFO, fxFlangerFreq, fxFlangerAmp, fxFlangerWet;
extern int fxReverbRoomSz, fxReverbDamp, fxReverbWet, fxReverbWidth;
extern int fxDelayLeft, fxDelayRight, fxDelayDecay;
extern int fxEQBass, fxEQMid, fxEQTreble;
extern int fxChorusFreq, fxChorusDepth, fxChorusGain;
extern int fxFormantWet, fxFormantA, fxFormantE, fxFormantI, fxFormantO, fxFormantU;
extern int fxDistortionAmount;

// Define the external variables that were referenced but not defined
int fxFormantWet = 0;
int fxFormantA = 0;
int fxFormantE = 0;
int fxFormantI = 0;
int fxFormantO = 0;
int fxFormantU = 0;
int fxDistortionAmount = 0;

// Additional synth variables that were referenced but not defined
int synthFilterHPRes = 0;
int synthFilterBPFreq = 0;
int synthFilterBPRes = 0;
int synthFilterNTFreq = 0;
int synthFilterNTRes = 0;
int fxChorusDepth = 0;
int fxChorusGain = 0;
int synthFilterLPFreq = 0;
int synthFilterLPRes = 0;
int synthFilterHPFreq = 0;

// Additional FX variables that were referenced but not defined
int fxDelayDecay = 0;
int fxEQBass = 0;
int fxEQMid = 0;
int fxEQTreble = 0;
int fxChorusFreq = 0;

// Additional reverb and delay variables that were referenced but not defined
int fxReverbDamp = 0;
int fxReverbWet = 0;
int fxReverbWidth = 0;
int fxDelayLeft = 0;
int fxDelayRight = 0;

// Additional flanger variables that were referenced but not defined
int fxFlangerLFO = 0;
int fxFlangerFreq = 0;
int fxFlangerAmp = 0;
int fxFlangerWet = 0;
int fxReverbRoomSz = 0;

// Additional ADSR envelope variables that were referenced but not defined
int adsr1R = 0;
int adsr2A = 0;
int adsr2D = 0;
int adsr2S = 0;
int adsr2R = 0;

// Additional LFO and ADSR variables that were referenced but not defined
int synthLFO2Rate = 0;
int synthLFO2Depth = 0;
int adsr1A = 0;
int adsr1D = 0;
int adsr1S = 0;

// Additional noise generator and LFO variables that were referenced but not defined
int synthGenNoise = 0;
int synthGenNoiseFreq = 0;
int synthGenNoiseBW = 0;
int synthLFO1Rate = 0;
int synthLFO1Depth = 0;

// Additional generator variables that were referenced but not defined
int synthGenDamp = 0;
int synthGenHarmonics = 0;
int synthGenDrive = 0;
int synthGenScale = 0;
int synthGenModulation = 0;

// Final missing variables that were referenced but not defined
int synthPitchDown = 0;
int synthGenVolume = 0;
int synthGenPanning = 0;
int synthGenSpread = 0;
int synthGenBandwidth = 0;
int synthGenGlide = 0;

// Convert bool to int for binding system
static int synthLFO1SyncInt = 0;
static int synthLFO2SyncInt = 0;

// Shadow storage for mod matrix and FX stack UI sync
static int mmSrc[8], mmDst[8], mmAmt[8];
static int fxSel[10], fxWet[10];

// Filter ON toggle state variables must be defined before binding table
static int synthFilterLPOn = 0;
static int synthFilterHPOn = 0;
static int synthFilterBPOn = 0;
static int synthFilterNTOn = 0;

#undef SYNTH_FILTER_ON_VARS

// ADSR mini-envelope drag state (per env)
static bool tf_env_dragging[2] = { false, false };
static int tf_env_drag_index[2] = { -1, -1 };
static int tf_env_selected_index[2] = { 0, 0 };
static int tf_env_last_mouse_x[2] = { 0, 0 };
static int tf_env_last_mouse_y[2] = { 0, 0 };
static int tf_env_save_mouse_x[2] = { 0, 0 };
static int tf_env_save_mouse_y[2] = { 0, 0 };

// Compile-time constants for item counts
#define POLY_MIN            1
#define POLY_MAX            16   // TF_MAXVOICES
#define PITCH_RANGE_MAX     12   // ±12 semitones

#define FX_TYPE_COUNT       8    // none + 7 FX sections
#define FX_STACK_COUNT      FX_TYPE_COUNT

// Full enumeration counts from Tunefish4 enum (tf4.hpp)
#define MOD_SRC_ENUM_COUNT  15   // eTfModMatrix::INPUT_COUNT
#define MOD_DST_ENUM_COUNT  40   // eTfModMatrix::OUTPUT_COUNT

// Tunefish4 Mod-Matrix parameter IDs (subset used here)
#define TF_MM1_SOURCE   44
#define TF_MM1_MOD      45
#define TF_MM1_TARGET   46
#define TF_MM2_SOURCE   47
#define TF_MM2_MOD      48
#define TF_MM2_TARGET   49
#define TF_MM3_SOURCE   50
#define TF_MM3_MOD      51
#define TF_MM3_TARGET   52
#define TF_MM4_SOURCE   53
#define TF_MM4_MOD      54
#define TF_MM4_TARGET   55
#define TF_MM5_SOURCE   56
#define TF_MM5_MOD      57
#define TF_MM5_TARGET   58
#define TF_MM6_SOURCE   59
#define TF_MM6_MOD      60
#define TF_MM6_TARGET   61
#define TF_MM7_SOURCE   62
#define TF_MM7_MOD      63
#define TF_MM7_TARGET   64
#define TF_MM8_SOURCE   65
#define TF_MM8_MOD      66
#define TF_MM8_TARGET   67
#define TF_GEN_GLIDE    9
#define TF_GEN_POLYPHONY 12
#define TF_GEN_UNISONO  14

// FX Stack parameter IDs (from tf4.hpp)
#define TF_EFFECT_1     74
#define TF_EFFECT_2     75
#define TF_EFFECT_3     76
#define TF_EFFECT_4     77
#define TF_EFFECT_5     78
#define TF_EFFECT_6     79
#define TF_EFFECT_7     80
#define TF_EFFECT_8     81
#define TF_EFFECT_9     82
#define TF_EFFECT_10    83

#define TF_LFO1_SHAPE   38
#define TF_LFO2_SHAPE   42
#define TF_ADSR1_ATTACK 26
#define TF_ADSR2_ATTACK 31

// Parameter binding table - connects widget names to tf4 instance parameter numbers and value storage
static TunefishParameterBinding g_paramBindings[] = {
    // Global parameters
    {"poly", TF_GEN_POLYPHONY, POLY_MIN, POLY_MAX, &synthPoly},
    {"pitch_up", 104, 0, PITCH_RANGE_MAX, &synthPitchUp},
    {"pitch_down", 105, 0, PITCH_RANGE_MAX, &synthPitchDown},

    // Generator parameters (expanded)
    {"gen_volume", 5, 0, 127, &synthGenVolume},
    {"gen_panning", 6, 0, 127, &synthGenPanning},
    {"gen_detune", 15, 0, 127, &synthGenSpread},
    {"gen_glide", TF_GEN_GLIDE, 0, 127, &synthGenGlide},
    {"gen_bandwidth", 1, 0, 127, &synthGenBandwidth},
    {"gen_damp", 3, 0, 127, &synthGenDamp},
    {"gen_harmonics", 2, 0, 127, &synthGenHarmonics},
    {"gen_drive", 13, 0, 127, &synthGenDrive},
    {"gen_scale", 16, 0, 127, &synthGenScale},
    {"gen_modulation", 4, 0, 127, &synthGenModulation},
    {"gen_noise", 17, 0, 127, &synthGenNoise},
    {"gen_noise_freq", 18, 0, 127, &synthGenNoiseFreq},
    {"gen_noise_bw", 19, 0, 127, &synthGenNoiseBW},

    // Filter parameters (expanded)
    {"filter_cutoff", 21, 0, 127, &synthFilterLPFreq},
    {"filter_resonance", 22, 0, 127, &synthFilterLPRes},
    {"filter_hp_freq", 24, 0, 127, &synthFilterHPFreq},
    {"filter_hp_res", 25, 0, 127, &synthFilterHPRes},
    {"filter_bp_freq", 107, 0, 127, &synthFilterBPFreq},
    {"filter_bp_res", 108, 0, 127, &synthFilterBPRes},
    {"filter_nt_freq", 110, 0, 127, &synthFilterNTFreq},
    {"filter_nt_res", 111, 0, 127, &synthFilterNTRes},

    // LFO parameters (expanded)
    {"lfo1_freq", 36, 0, 127, &synthLFO1Rate},
    {"lfo1_amp", 37, 0, 127, &synthLFO1Depth},
    {"lfo1_shape", 38, 0, 4, &synthLFO1Shape},
    {"lfo1_sync", 39, 0, 1, &synthLFO1SyncInt},
    {"lfo2_freq", 40, 0, 127, &synthLFO2Rate},
    {"lfo2_amp", 41, 0, 127, &synthLFO2Depth},
    {"lfo2_shape", 42, 0, 4, &synthLFO2Shape},
    {"lfo2_sync", 43, 0, 1, &synthLFO2SyncInt},

    // ADSR parameters
    {"adsr1_attack", 26, 0, 127, &adsr1A},
    {"adsr1_decay", 27, 0, 127, &adsr1D},
    {"adsr1_sustain", 28, 0, 127, &adsr1S},
    {"adsr1_release", 29, 0, 127, &adsr1R},
    {"adsr1_slope", 30, 0, 127, &adsr1Slope},
    {"adsr2_attack", 31, 0, 127, &adsr2A},
    {"adsr2_decay", 32, 0, 127, &adsr2D},
    {"adsr2_sustain", 33, 0, 127, &adsr2S},
    {"adsr2_release", 34, 0, 127, &adsr2R},
    {"adsr2_slope", 35, 0, 127, &adsr2Slope},

    // FX parameters (greatly expanded)
    {"flanger_lfo", 94, 0, 127, &fxFlangerLFO},
    {"flamger_freq", 95, 0, 127, &fxFlangerFreq},
    {"flanger_amp", 96, 0, 127, &fxFlangerAmp},
    {"flanger_wet", 97, 0, 127, &fxFlangerWet},
    {"reverb_room_sz", 90, 0, 127, &fxReverbRoomSz},
    {"reverb_damp", 91, 0, 127, &fxReverbDamp},
    {"reverb_wet", 92, 0, 127, &fxReverbWet},
    {"reverb_width", 93, 0, 127, &fxReverbWidth},
    {"delay_left", 87, 0, 127, &fxDelayLeft},
    {"delay_right", 88, 0, 127, &fxDelayRight},
    {"delay_decay", 89, 0, 127, &fxDelayDecay},
    {"eq_bass", 101, 0, 127, &fxEQBass},
    {"eq_mid", 102, 0, 127, &fxEQMid},
    {"eq_treble", 103, 0, 127, &fxEQTreble},
    {"chorus_freq", 85, 0, 127, &fxChorusFreq},
    {"chorus_depth", 86, 0, 127, &fxChorusDepth},
    {"chorus_gain", 98, 0, 127, &fxChorusGain},
    {"formant_wet", 100, 0, 127, &fxFormantWet},
    {"formant_a", 99, 0, 1, &fxFormantA},
    {"formant_e", 99, 0, 1, &fxFormantE},
    {"formant_i", 99, 0, 1, &fxFormantI},
    {"formant_o", 99, 0, 1, &fxFormantO},
    {"formant_u", 99, 0, 1, &fxFormantU},
    {"formant_amount", 100, 0, 127, &fxFormantWet},
    {"distortion_amount", 84, 0, 127, &fxDistortionAmount},

    // --- Aliases for multi-filter UI (filter1-4) ---
    {"filter1_cutoff", 21, 0, 127, &synthFilterLPFreq},
    {"filter1_res",    22, 0, 127, &synthFilterLPRes},
    {"filter1_on", 20, 0, 1, &synthFilterLPOn},   // TF_LP_FILTER_ON
    {"filter2_cutoff", 24, 0, 127, &synthFilterHPFreq},
    {"filter2_res",    25, 0, 127, &synthFilterHPRes},
    {"filter2_on", 23, 0, 1, &synthFilterHPOn},   // TF_HP_FILTER_ON
    {"filter3_cutoff", 107, 0, 127, &synthFilterBPFreq},
    {"filter3_res",    108, 0, 127, &synthFilterBPRes},
    {"filter3_on", 106, 0, 1, &synthFilterBPOn}, // TF_BP_FILTER_ON
    {"filter4_on", 109, 0, 1, &synthFilterNTOn}, // TF_NT_FILTER_ON
    {"filter4_cutoff", 110, 0, 127, &synthFilterNTFreq},
    {"filter4_res",    111, 0, 127, &synthFilterNTRes},

    // --- Aliases for Formant vowel radio buttons (numeric names in UI) ---
    {"formant_0", 99, 0, 1, &fxFormantA},
    {"formant_1", 99, 0, 1, &fxFormantE},
    {"formant_2", 99, 0, 1, &fxFormantI},
    {"formant_3", 99, 0, 1, &fxFormantO},
    {"formant_4", 99, 0, 1, &fxFormantU},

    // --- Modulation Matrix ---
    { "mod_src_0",  44, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[0] },
    { "mod_dst_0",  46, 0, MOD_DST_ENUM_COUNT-1, &mmDst[0] },
    { "mod_amt_0",  45, 0, 99,              &mmAmt[0] },
    { "mod_src_1",  47, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[1] },
    { "mod_dst_1",  49, 0, MOD_DST_ENUM_COUNT-1, &mmDst[1] },
    { "mod_amt_1",  48, 0, 99,              &mmAmt[1] },
    { "mod_src_2",  50, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[2] },
    { "mod_dst_2",  52, 0, MOD_DST_ENUM_COUNT-1, &mmDst[2] },
    { "mod_amt_2",  51, 0, 99,              &mmAmt[2] },
    { "mod_src_3",  53, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[3] },
    { "mod_dst_3",  55, 0, MOD_DST_ENUM_COUNT-1, &mmDst[3] },
    { "mod_amt_3",  54, 0, 99,              &mmAmt[3] },
    { "mod_src_4",  56, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[4] },
    { "mod_dst_4",  58, 0, MOD_DST_ENUM_COUNT-1, &mmDst[4] },
    { "mod_amt_4",  57, 0, 99,              &mmAmt[4] },
    { "mod_src_5",  59, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[5] },
    { "mod_dst_5",  61, 0, MOD_DST_ENUM_COUNT-1, &mmDst[5] },
    { "mod_amt_5",  60, 0, 99,              &mmAmt[5] },
    { "mod_src_6",  62, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[6] },
    { "mod_dst_6",  64, 0, MOD_DST_ENUM_COUNT-1, &mmDst[6] },
    { "mod_amt_6",  63, 0, 99,              &mmAmt[6] },
    { "mod_src_7",  65, 0, MOD_SRC_ENUM_COUNT-1, &mmSrc[7] },
    { "mod_dst_7",  67, 0, MOD_DST_ENUM_COUNT-1, &mmDst[7] },
    { "mod_amt_7",  66, 0, 99,              &mmAmt[7] },
    // --- FX Stack ---
    { "fxstk_sel_0", 74, 0, FX_STACK_COUNT-1, &fxSel[0] },
    { "fxstk_wet_0", -1, 0, 127,               &fxWet[0] },
    { "fxstk_sel_1", 75, 0, FX_STACK_COUNT-1, &fxSel[1] },
    { "fxstk_wet_1", -1, 0, 127,               &fxWet[1] },
    { "fxstk_sel_2", 76, 0, FX_STACK_COUNT-1, &fxSel[2] },
    { "fxstk_wet_2", -1, 0, 127,               &fxWet[2] },
    { "fxstk_sel_3", 77, 0, FX_STACK_COUNT-1, &fxSel[3] },
    { "fxstk_wet_3", -1, 0, 127,               &fxWet[3] },
    { "fxstk_sel_4", 78, 0, FX_STACK_COUNT-1, &fxSel[4] },
    { "fxstk_wet_4", -1, 0, 127,               &fxWet[4] },
    { "fxstk_sel_5", 79, 0, FX_STACK_COUNT-1, &fxSel[5] },
    { "fxstk_wet_5", -1, 0, 127,               &fxWet[5] },
    { "fxstk_sel_6", 80, 0, FX_STACK_COUNT-1, &fxSel[6] },
    { "fxstk_wet_6", -1, 0, 127,               &fxWet[6] },
    { "fxstk_sel_7", 81, 0, FX_STACK_COUNT-1, &fxSel[7] },
    { "fxstk_wet_7", -1, 0, 127,               &fxWet[7] },
    { "fxstk_sel_8", 82, 0, FX_STACK_COUNT-1, &fxSel[8] },
    { "fxstk_wet_8", -1, 0, 127,               &fxWet[8] },
    { "fxstk_sel_9", 83, 0, FX_STACK_COUNT-1, &fxSel[9] },
    { "fxstk_wet_9", -1, 0, 127,               &fxWet[9] },

};
static const int g_paramBindingCount = sizeof(g_paramBindings) / sizeof(g_paramBindings[0]);

// =============================================================================
// PARAMETER BINDING HELPERS
// =============================================================================

// Helper function to find parameter binding by widget name
static TunefishParameterBinding* tf_find_param_binding(const char* widgetName) {
    for (int i = 0; i < g_paramBindingCount; i++) {
        if (strcmp(g_paramBindings[i].widgetName, widgetName) == 0) {
            return &g_paramBindings[i];
        }
    }
    return NULL;
}

// Helper function to send parameter update to TF4 engine
static void tf_send_parameter_cc(int paramId, int value) {
    if (paramId < 0) {
        // No direct TF4 param mapped (placeholder binding)
        return;
    }

    int instrID = getCurrentTF4InstrumentID();
    if (instrID < 0) return;

    float norm = (float)value / 127.0f;
    ft2_synth_set_param(instrID, paramId, norm);
    printf("[TF_PARAM] instr %d param %d <- %d (%0.3f)\n", instrID, paramId, value, norm);
}

// Sync bool variables with int wrappers
static void tf_sync_bool_parameters(void) {
    synthLFO1SyncInt = synthLFO1Sync ? 1 : 0;
    synthLFO2SyncInt = synthLFO2Sync ? 1 : 0;
}

static void tf_apply_bool_parameters(void) {
    synthLFO1Sync = (synthLFO1SyncInt != 0);
    synthLFO2Sync = (synthLFO2SyncInt != 0);
}

// Enhanced debug output for testing
static void tf_debug_parameter_change(const char* widgetName, int paramId, int oldValue, int newValue) {
    printf("🎛️  [WIDGET] %s: %d → %d (param %d)\n", widgetName, oldValue, newValue, paramId);
}

// Parameter change callback for widgets
static void tf_parameter_changed(TunefishWidget* widget, float normalizedValue) {
    if (!widget) return;

    TunefishParameterBinding* binding = tf_find_param_binding(widget->name);
    if (!binding) {
        // Special-case: unisono buttons named "unisono_X"
        if (strncmp(widget->name, "unisono_", 8) == 0) {
            int idx = atoi(widget->name + 8); // 1..10
            if (idx < 1 || idx > 10) return;
            int instrID = getCurrentTF4InstrumentID();
            if (instrID >= 0) {
                float norm = (float)(idx - 1) / 9.0f;
                ft2_synth_set_param(instrID, TF_GEN_UNISONO, norm);
                printf("[UNISONO PARAM] idx=%d norm=%0.3f param=14\n", idx-1, norm);
            }
            return;
        }
        // Special-case: LFO1 shape buttons named "lfo1shape_X"
        if (strncmp(widget->name, "lfo1_shape_", 4) == 0) {
            int idx = atoi(widget->name + 4);
            if (idx < 1 || idx > 5) return;
            int instrID = getCurrentTF4InstrumentID();
            if (instrID >= 0) {
                float norm = (float)(idx - 1) / 4.0f;
                ft2_synth_set_param(instrID, TF_LFO1_SHAPE, norm);
                printf("[LFO1 SHAPE PARAM] idx=%d norm=%0.3f param=38\n", idx-1, norm);
            }
            return;
        }
        // Special-case: LFO2 shape buttons named "lfo2_shape_X"
        if (strncmp(widget->name, "lfo2_shape_", 4) == 0) {
            int idx = atoi(widget->name + 4);
            if (idx < 1 || idx > 5) return;
            int instrID = getCurrentTF4InstrumentID();
            if (instrID >= 0) {
                float norm = (float)(idx - 1) / 4.0f;
                ft2_synth_set_param(instrID, TF_LFO2_SHAPE, norm);
                printf("[LFO2 SHAPE PARAM] idx=%d norm=%0.3f param=42\n", idx-1, norm);
            }
            return;
        }
        printf("❌ [TF_PARAM] No binding found for widget: %s\n", widget->name);
        return;
    }

    // Store old value for debug output
    int oldValue = *(binding->valuePtr);

    // Convert normalized value (0.0-1.0) to parameter range
    int paramValue;
    // For combo boxes we can rely on selectedIndex for accurate discrete value
    if (widget->type == TF_WIDGET_COMBO_BOX) {
        paramValue = widget->selectedIndex; // direct discrete index
    } else {
        paramValue = (int)(normalizedValue * (binding->maxValue - binding->minValue) + binding->minValue);
    }

    // ---------------------------------------------------------
    // Special scaling for Mod-Matrix SOURCE/TARGET parameters
    // ---------------------------------------------------------
    float norm;
    bool isSrc = (binding->paramId >= TF_MM1_SOURCE && binding->paramId <= TF_MM8_SOURCE && ((binding->paramId - TF_MM1_SOURCE) % 3) == 0);
    bool isDst = (binding->paramId >= TF_MM1_TARGET && binding->paramId <= TF_MM8_TARGET && ((binding->paramId - TF_MM1_TARGET) % 3) == 0);

    if (isSrc) {
        norm = (float)paramValue / (float)(MOD_SRC_ENUM_COUNT - 1); // divide by 14
    } else if (isDst) {
        norm = (float)paramValue / (float)(MOD_DST_ENUM_COUNT - 1); // divide by 39
    } else {
        // Special for FX stack selectors (TF_EFFECT_1..TF_EFFECT_10) use divisor 10
        bool isFxSel = (binding->paramId >= TF_EFFECT_1 && binding->paramId <= TF_EFFECT_1 + 9);
        if (isFxSel) {
            norm = (float)paramValue / 10.0f; // TF_MAXEFFECTS constant
        } else if (binding->maxValue == binding->minValue) {
            norm = 0.0f;
        } else {
            norm = (float)(paramValue - binding->minValue) / (float)(binding->maxValue - binding->minValue);
        }
    }

    if (binding->paramId >= 0) {
        int instrID = getCurrentTF4InstrumentID();
        if (instrID >= 0) {
            ft2_synth_set_param(instrID, binding->paramId, norm);
        }
    }

    printf("✅ [TF_PARAM] %s: %.2f → %d (norm %.3f param %d)\n",
           widget->name, normalizedValue, paramValue, norm, binding->paramId);
}

// Note: Widget event handling is now consolidated in tf_widget_handle_mouse_event

// Complete event handling for all page widgets
static bool tf_handle_page_widgets_event(TunefishCompleteLayout* layout, int mouseX, int mouseY, bool pressed) {
    if (!layout) return false;

    printf("📄 [PAGE] Checking page %d widgets for event at (%d, %d)\n", layout->current_page, mouseX, mouseY);

    if (layout->current_page == 0) {
        // Page 1 widgets
        TunefishWidget* page1Widgets[] = {
            layout->page1.poly_control,
            layout->page1.pitch_up_control,
            layout->page1.pitch_down_control,
            layout->page1.gen_glide_control,
            layout->page1.gen_volume_knob,
            layout->page1.gen_panning_knob,
            layout->page1.gen_detune_knob,
            layout->page1.lfo1_freq_knob,
            layout->page1.lfo1_depth_knob,
            layout->page1.lfo1_shape_buttons[5],
            layout->page1.lfo1_sync_toggle,
            layout->page1.lfo2_freq_knob,
            layout->page1.lfo2_depth_knob,
            layout->page1.lfo2_shape_buttons[5],
            layout->page1.lfo2_sync_toggle,
            layout->page1.filter_cutoff_knob,
            layout->page1.filter_resonance_knob,
            layout->page1.filter_type_combo,
            layout->page1.adsr1_env_view,
            layout->page1.adsr1_slope_slider,
            layout->page1.adsr2_env_view,
            layout->page1.adsr2_slope_slider,
            layout->page1.filter1_on_toggle,
            layout->page1.filter2_on_toggle,
            layout->page1.filter3_on_toggle,
            layout->page1.filter4_on_toggle
        };

        int page1WidgetCount = sizeof(page1Widgets) / sizeof(page1Widgets[0]);
        for (int i = 0; i < page1WidgetCount; i++) {
            TunefishWidget* widget = page1Widgets[i];
            if (!widget) continue;

            if (tf_widget_handle_mouse_event(widget, mouseX, mouseY, pressed)) {
                printf("✅ [PAGE1] Widget '%s' handled event successfully\n", widget->name);
                return true;
            }
        }

    } else if (layout->current_page == 1) {
        // Page 2 widgets
        TunefishWidget* page2Widgets[] = {
            layout->page2.fx1_type_combo,
            layout->page2.fx1_wet_knob,
            layout->page2.fx1_param1_knob,
            layout->page2.fx1_param2_knob,
            layout->page2.fx2_type_combo,
            layout->page2.fx2_wet_knob,
            layout->page2.fx2_param1_knob,
            layout->page2.fx2_param2_knob,
            layout->page2.formant_amount_knob
        };

        int page2WidgetCount = sizeof(page2Widgets) / sizeof(page2Widgets[0]);
        for (int i = 0; i < page2WidgetCount; i++) {
            TunefishWidget* widget = page2Widgets[i];
            if (!widget) continue;

            if (tf_widget_handle_mouse_event(widget, mouseX, mouseY, pressed)) {
                printf("✅ [PAGE2] Widget '%s' handled event successfully\n", widget->name);
                return true;
            }
        }



        // Handle modulation matrix widgets
        for (int i = 0; i < 8; i++) {
            if (layout->page2.mod_source_combos[i] &&
                tf_widget_handle_mouse_event(layout->page2.mod_source_combos[i], mouseX, mouseY, pressed)) {
                printf("🔗 [MOD] Source combo %d clicked\n", i);
                return true;
            }
            if (layout->page2.mod_dest_combos[i] &&
                tf_widget_handle_mouse_event(layout->page2.mod_dest_combos[i], mouseX, mouseY, pressed)) {
                printf("🔗 [MOD] Destination combo %d clicked\n", i);
                return true;
            }
            if (layout->page2.mod_amount_knobs[i] &&
                tf_widget_handle_mouse_event(layout->page2.mod_amount_knobs[i], mouseX, mouseY, pressed)) {
                printf("🔗 [MOD] Amount knob %d clicked\n", i);
                return true;
            }
        }
    }

    return false;
}

// Initialize widget values from current synth parameters
void tf_sync_widgets_with_parameters(TunefishCompleteLayout* layout) {
    if (!layout) return;

    for (int i = 0; i < g_paramBindingCount; i++) {
        TunefishParameterBinding* binding = &g_paramBindings[i];
        int curVal = *(binding->valuePtr);
        float norm = (float)(curVal - binding->minValue) / (float)(binding->maxValue - binding->minValue);

        TunefishWidget* w = tf_find_widget_by_name(layout, binding->widgetName);
        if (!w) continue;

        switch (w->type) {
            case TF_WIDGET_ROTARY_SLIDER:
            case TF_WIDGET_PARAMETER_CONTROL:
                w->minValue = (float)binding->minValue;
                w->maxValue = (float)binding->maxValue;
                w->value = norm;
                break;
            case TF_WIDGET_COMBO_BOX: {
                const bool isModSrc = (binding->paramId >= TF_MM1_SOURCE &&
                                       binding->paramId <= TF_MM8_SOURCE &&
                                       ((binding->paramId - TF_MM1_SOURCE) % 3) == 0);
                int idx = isModSrc ? curVal : (int)lroundf(norm * (w->comboItemCount - 1));
                if (idx < 0) idx = 0;
                if (idx >= w->comboItemCount) idx = w->comboItemCount - 1;
                w->selectedIndex = idx;
                w->value = (w->comboItemCount > 1) ? ((float)idx / (float)(w->comboItemCount - 1)) : 0.0f;
                break; }
            case TF_WIDGET_LINEAR_SLIDER:
                // linear slider uses value directly like rotary
                w->value = norm;
                break;
            case TF_WIDGET_TOGGLE_BUTTON:
                w->pressed = (curVal != 0);
                w->value = w->pressed ? 1.0f : 0.0f;
                break;
            default:
                break;
        }
    }

    // Sync LFO1 shape buttons
    extern int synthLFO1Shape;
    int lfo1shape_idx = synthLFO1Shape;
    for (int i = 0; i < 5; i++) {
        if (layout->page1.lfo1_shape_buttons[i])
            layout->page1.lfo1_shape_buttons[i]->pressed = (i == lfo1shape_idx);
    }
    // Sync LFO2 shape buttons
    extern int synthLFO2Shape;
    int lfo2shape_idx = synthLFO2Shape;
    for (int i = 0; i < 5; i++) {
        if (layout->page1.lfo2_shape_buttons[i])
            layout->page1.lfo2_shape_buttons[i]->pressed = (i == lfo2shape_idx);
    }
    // Sync unisono buttons
    extern int synthUnisono;
    int idx = synthUnisono - 1;
    for (int i = 0; i < 10; i++)
        if (layout->page1.unisono_buttons[i])
            layout->page1.unisono_buttons[i]->pressed = (i == idx);

    // Sync octave buttons
    extern int synthOctave;
    int oct_idx = synthOctave + 4;
    for (int i = 0; i < 9; i++)
        if (layout->page1.octave_buttons[i])
            layout->page1.octave_buttons[i]->pressed = (i == oct_idx);

    // Sync formant vowel radio buttons
    extern int synthFormant;
    for (int i = 0; i < 5; i++)
        if (layout->page2.formant_type_buttons[i])
            layout->page2.formant_type_buttons[i]->pressed = (i == synthFormant);

    tf_request_redraw();
}

// Update a specific widget from parameter value
void tf_update_widget_from_parameter(TunefishCompleteLayout* layout, const char* widgetName, int value) {
    if (!layout || !widgetName) return;

    TunefishParameterBinding* binding = tf_find_param_binding(widgetName);
    if (!binding) return;

    float normalizedValue = (float)(value - binding->minValue) / (float)(binding->maxValue - binding->minValue);

    // Update the parameter value
    *(binding->valuePtr) = value;

    // TODO: Find and update the actual widget object
    printf("[TF_PARAM] Updated widget %s to value %d\n", widgetName, value);
}

// Update all widgets from current synth state
void tf_update_all_widgets_from_synth(TunefishCompleteLayout* layout) {
    if (!layout) return;

    // Pull latest parameter values from synth into shadow arrays
    tf_refresh_shadow_parameters_from_synth();

    // Call the sync function and mark for visual refresh
    tf_sync_widgets_with_parameters(layout);

    printf("[TF_PARAM] All widgets updated from synth state\n");
}

// Apply Tunefish styling and parameter connections
static void tf_connect_widget_callbacks(TunefishCompleteLayout* layout) {
    if (!layout) return;

    // Connect all widget value change callbacks to parameter system
    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        if (layout->global_widgets[i]) {
            layout->global_widgets[i]->onValueChange = tf_parameter_changed;
        }
    }

    // Connect Page 1 widgets
    if (layout->page1.poly_control) layout->page1.poly_control->onValueChange = tf_parameter_changed;
    if (layout->page1.gen_glide_control) layout->page1.gen_glide_control->onValueChange = tf_parameter_changed;
    if (layout->page1.pitch_up_control) layout->page1.pitch_up_control->onValueChange = tf_parameter_changed;
    if (layout->page1.pitch_down_control) layout->page1.pitch_down_control->onValueChange = tf_parameter_changed;
    if (layout->page1.gen_volume_knob) layout->page1.gen_volume_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.gen_panning_knob) layout->page1.gen_panning_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.gen_detune_knob) layout->page1.gen_detune_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.lfo1_freq_knob) layout->page1.lfo1_freq_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.lfo1_depth_knob) layout->page1.lfo1_depth_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.lfo1_sync_toggle) layout->page1.lfo1_sync_toggle->onValueChange = tf_parameter_changed;
    if (layout->page1.lfo2_freq_knob) layout->page1.lfo2_freq_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.lfo2_depth_knob) layout->page1.lfo2_depth_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.lfo2_sync_toggle) layout->page1.lfo2_sync_toggle->onValueChange = tf_parameter_changed;
    if (layout->page1.filter_cutoff_knob) layout->page1.filter_cutoff_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.filter_resonance_knob) layout->page1.filter_resonance_knob->onValueChange = tf_parameter_changed;
    if (layout->page1.filter_type_combo) layout->page1.filter_type_combo->onValueChange = tf_parameter_changed;
    if (layout->page1.adsr1_slope_slider) layout->page1.adsr1_slope_slider->onValueChange = tf_parameter_changed;
    if (layout->page1.adsr2_slope_slider) layout->page1.adsr2_slope_slider->onValueChange = tf_parameter_changed;
    if (layout->page1.filter1_on_toggle) layout->page1.filter1_on_toggle->onValueChange = tf_parameter_changed;
    if (layout->page1.filter2_on_toggle) layout->page1.filter2_on_toggle->onValueChange = tf_parameter_changed;
    if (layout->page1.filter3_on_toggle) layout->page1.filter3_on_toggle->onValueChange = tf_parameter_changed;
    if (layout->page1.filter4_on_toggle) layout->page1.filter4_on_toggle->onValueChange = tf_parameter_changed;

    // Connect Page 2 widgets
    if (layout->page2.fx1_type_combo) layout->page2.fx1_type_combo->onValueChange = tf_parameter_changed;
    if (layout->page2.fx1_wet_knob) layout->page2.fx1_wet_knob->onValueChange = tf_parameter_changed;
    if (layout->page2.fx1_param1_knob) layout->page2.fx1_param1_knob->onValueChange = tf_parameter_changed;
    if (layout->page2.fx1_param2_knob) layout->page2.fx1_param2_knob->onValueChange = tf_parameter_changed;
    if (layout->page2.fx2_type_combo) layout->page2.fx2_type_combo->onValueChange = tf_parameter_changed;
    if (layout->page2.fx2_wet_knob) layout->page2.fx2_wet_knob->onValueChange = tf_parameter_changed;
    if (layout->page2.fx2_param1_knob) layout->page2.fx2_param1_knob->onValueChange = tf_parameter_changed;
    if (layout->page2.fx2_param2_knob) layout->page2.fx2_param2_knob->onValueChange = tf_parameter_changed;
    if (layout->page2.formant_amount_knob) layout->page2.formant_amount_knob->onValueChange = tf_parameter_changed;

    // Connect modulation matrix
    for (int i = 0; i < 8; i++) {
        if (layout->page2.mod_source_combos[i]) {
            layout->page2.mod_source_combos[i]->onValueChange = tf_parameter_changed;
        }
        if (layout->page2.mod_dest_combos[i]) {
            layout->page2.mod_dest_combos[i]->onValueChange = tf_parameter_changed;
        }
        if (layout->page2.mod_amount_knobs[i]) {
            layout->page2.mod_amount_knobs[i]->onValueChange = tf_parameter_changed;
        }
    }

    // Connect FX grid widgets (wet/param1/param2/param3) and formant vowel buttons
    for (int i = 0; i < 7; i++) {
        if (layout->page2.fx_wet_knobs[i])      layout->page2.fx_wet_knobs[i]->onValueChange = tf_parameter_changed;
        if (layout->page2.fx_param1_knobs[i])   layout->page2.fx_param1_knobs[i]->onValueChange = tf_parameter_changed;
        if (layout->page2.fx_param2_knobs[i])   layout->page2.fx_param2_knobs[i]->onValueChange = tf_parameter_changed;
        if (layout->page2.fx_param3_knobs[i])   layout->page2.fx_param3_knobs[i]->onValueChange = tf_parameter_changed;
    }

    // Connect FX stack widgets (combo + wet knob)
    for (int i = 0; i < 10; i++) {
        if (layout->page2.fx_stack_combos[i])
            layout->page2.fx_stack_combos[i]->onValueChange = tf_parameter_changed;
        if (layout->page2.fx_stack_wet_knobs[i])
            layout->page2.fx_stack_wet_knobs[i]->onValueChange = tf_parameter_changed;
    }

    // Fallback: ensure every widget present in the layout has its callback set (avoids omissions)
    for (int i = 0; i < TF_TOTAL_WIDGETS; i++) {
        TunefishWidget* w = layout->all_widgets[i];
        if (w && !w->onValueChange) {
            // ADD THIS CHECK: Do not assign the generic handler to formant buttons,
            // as they have a custom 'onClick' handler.
            if (strncmp(w->name, "formant_", 8) != 0) {
                w->onValueChange = tf_parameter_changed;
            }
        }
    }

    printf("✅ [CALLBACKS] All widget value change callbacks connected to parameter system\n");
}

// Authentic Tunefish Layout Positions (based on original 640x400 layout)
static const TunefishLayoutPositions g_layoutPositions = {
    // Global area positions (persistent across pages) - Adjusted for 632px width
    .title_x = 20, .title_y = 10,
    .preset_x = 150, .preset_y = 8, .preset_w = 120, .preset_h = 16,
    .page_toggle_x = 280, .page_toggle_y = 8, .page_toggle_w = 60, .page_toggle_h = 16,
    .exit_x = 592, .exit_y = 8, .exit_w = 30, .exit_h = 16,  // Moved left by 8px
    .main_meter_x = 350, .main_meter_y = 8, .main_meter_w = 90, .main_meter_h = 12,  // Reduced width by 10px
    .cpu_meter_x = 450, .cpu_meter_y = 8, .cpu_meter_w = 80, .cpu_meter_h = 12,  // Moved left by 10px

    // Page 1 positions (Main Synth) - Adjusted for 632px width
    .page1 = {
        .global_group_x = 20, .global_group_y = 40, .global_group_w = 140, .global_group_h = 80,
        .generator_group_x = 180, .generator_group_y = 40, .generator_group_w = 140, .generator_group_h = 80,
        .lfo1_group_x = 340, .lfo1_group_y = 40, .lfo1_group_w = 120, .lfo1_group_h = 80,
        .lfo2_group_x = 470, .lfo2_group_y = 40, .lfo2_group_w = 120, .lfo2_group_h = 80,  // Moved left by 10px
        .filter_group_x = 20, .filter_group_y = 140, .filter_group_w = 200, .filter_group_h = 100,
        .adsr1_group_x = 240, .adsr1_group_y = 140, .adsr1_group_w = 175, .adsr1_group_h = 100,  // Reduced width by 5px
        .adsr2_group_x = 430, .adsr2_group_y = 140, .adsr2_group_w = 175, .adsr2_group_h = 100,  // Moved left by 10px, reduced width by 5px
    },

    // Page 2 positions (Effects) - Adjusted for 632px width
    .page2 = {
        .fx1_group_x = 20, .fx1_group_y = 40, .fx1_group_w = 180, .fx1_group_h = 100,
        .fx2_group_x = 220, .fx2_group_y = 40, .fx2_group_w = 180, .fx2_group_h = 100,
        .formant_group_x = 410, .formant_group_y = 40, .formant_group_w = 180, .formant_group_h = 100,  // Moved left by 10px
        .mod_group_x = 20, .mod_group_y = 160, .mod_group_w = 570, .mod_group_h = 200,  // Reduced width by 10px
    }
};

// Combo box item arrays
static const char* g_fxTypeItems[] = {
    "None", "Distortion", "Delay", "Chorus", "Flanger", "Reverb", "Formant", "EQ"
};
static const int g_fxTypeItemCount = 8;

static const char* g_filterTypeItems[] = {
    "LP 12dB", "LP 24dB", "HP 12dB", "HP 24dB", "BP 12dB", "BP 24dB", "Notch"
};
static const int g_filterTypeItemCount = 7;

// Modulation matrix source/dest item arrays
static const char* g_modSourceItems[] = {
    "None",      /* INPUT_NONE */
    "LFO1",      /* INPUT_LFO1 */
    "LFO2",      /* INPUT_LFO2 */
    "ADSR1",     /* INPUT_ADSR1 */
    "ADSR2",     /* INPUT_ADSR2 */
    "ModWheel"   /* INPUT_MODWHEEL */
};
static const int g_modSourceItemCount = 6;

static const char* g_modDestItems[] = {
    "None",          /* OUTPUT_NONE */
    "Bandwidth",     /* OUTPUT_BANDWIDTH */
    "Damp",          /* OUTPUT_DAMP */
    "Harmonics",     /* OUTPUT_NUMHARMONICS */
    "Scale",         /* OUTPUT_SCALE */
    "Volume",        /* OUTPUT_VOLUME */
    "Frequency",     /* OUTPUT_FREQ */
    "Panning",       /* OUTPUT_PAN */
    "Detune",        /* OUTPUT_DETUNE */
    "Spread",        /* OUTPUT_SPREAD */
    "Drive",         /* OUTPUT_DRIVE */
    "Noise",         /* OUTPUT_NOISE_AMOUNT */
    "LP Cutoff",     /* OUTPUT_LP_FILTER_CUTOFF */
    "LP Resonance",  /* OUTPUT_LP_FILTER_RESONANCE */
    "HP Cutoff",     /* OUTPUT_HP_FILTER_CUTOFF */
    "HP Resonance",  /* OUTPUT_HP_FILTER_RESONANCE */
    "BP Cutoff",     /* OUTPUT_BP_FILTER_CUTOFF */
    "BP Q",          /* OUTPUT_BP_FILTER_Q */
    "NT Cutoff",     /* OUTPUT_NT_FILTER_CUTOFF */
    "NT Q",          /* OUTPUT_NT_FILTER_Q */
    "ADSR1 Decay",   /* OUTPUT_ADSR1_DECAY */
    "ADSR2 Decay",   /* OUTPUT_ADSR2_DECAY */
    "Mod1",          /* OUTPUT_MOD1 */
    "Mod2",          /* OUTPUT_MOD2 */
    "Mod3",          /* OUTPUT_MOD3 */
    "Mod4",          /* OUTPUT_MOD4 */
    "Mod5",          /* OUTPUT_MOD5 */
    "Mod6",          /* OUTPUT_MOD6 */
    "Mod7",          /* OUTPUT_MOD7 */
    "Mod8",          /* OUTPUT_MOD8 */
    "LFO1 Depth",    /* OUTPUT_LFO1_DEPTH */
    "LFO2 Depth"     /* OUTPUT_LFO2_DEPTH */
};
static const int g_modDestItemCount = 32;

static const char* g_presetItems[] = {
    "Init", "Lead 1", "Lead 2", "Bass 1", "Bass 2", "Pad 1", "Pad 2", "Arp 1", "Arp 2", "FX 1"
};
static const int g_presetItemCount = 10;

// Formant type button labels
static const char* g_formantTypeItems[] = { "A", "E", "I", "O", "U" };

// FX stack items
static const char* fxStackItems[] = { "None", "Distortion", "Delay", "Chorus", "Flanger", "Reverb", "Formant", "EQ" };
static const int fxStackItemCount = sizeof(fxStackItems) / sizeof(fxStackItems[0]);

// Widget Creation Helpers
static TunefishWidget* tf_create_knob_with_label(const char* name, const char* label, int x, int y, int radius) {
    TunefishWidget* knob = tf_create_rotary_slider(name, x, y, radius, -2.35f, 2.35f);
    if (knob) {
        strncpy(knob->text, label, sizeof(knob->text) - 1);
        tf_apply_knob_styling(knob, 0.5f);
    }
    return knob;
}

static TunefishWidget* tf_create_mini_slider_with_label(const char* name, const char* label, int x, int y) {
    TunefishWidget* slider = tf_create_parameter_control(name, label, x, y, 40, 11);
    if (slider) {
        tf_apply_knob_styling(slider, 0.5f);
    }
    return slider;
}

// Add enum for widget page assignment
typedef enum { PAGE_BOTH = 0, PAGE1 = 0, PAGE2 = 1 } TunefishPage;

#ifdef TF_USE_SCHEMA_LAYOUT
static int tf_schema_page_value(ft2_ui_widget_page_t page)
{
    switch (page) {
        case FT2_UI_WIDGET_PAGE_2: return PAGE2;
        case FT2_UI_WIDGET_PAGE_1: return PAGE1;
        case FT2_UI_WIDGET_PAGE_BOTH: return PAGE_BOTH;
        default: return PAGE1;
    }
}

static void tf_schema_register_widget(TunefishCompleteLayout* layout, TunefishWidget* widget, ft2_ui_widget_page_t page)
{
    if (!layout || !widget) return;

    widget->page = tf_schema_page_value(page);

    for (int i = 0; i < TF_TOTAL_WIDGETS; i++) {
        if (layout->all_widgets[i] == NULL) {
            layout->all_widgets[i] = widget;
            break;
        }
    }

    if (page == FT2_UI_WIDGET_PAGE_BOTH) {
        for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
            if (layout->global_widgets[i] == NULL) {
                layout->global_widgets[i] = widget;
                break;
            }
        }
    } else if (page == FT2_UI_WIDGET_PAGE_1) {
        for (int i = 0; i < TF_PAGE1_WIDGETS; i++) {
            if (layout->page1_widgets[i] == NULL) {
                layout->page1_widgets[i] = widget;
                break;
            }
        }
    } else if (page == FT2_UI_WIDGET_PAGE_2) {
        for (int i = 0; i < TF_PAGE2_WIDGETS; i++) {
            if (layout->page2_widgets[i] == NULL) {
                layout->page2_widgets[i] = widget;
                break;
            }
        }
    }
}

static void tf_schema_assign_widget(TunefishCompleteLayout* layout, TunefishWidget* widget)
{
    if (!layout || !widget) return;

    const char* name = widget->name;
    if (!name) return;

    if (strcmp(name, "title_label") == 0) { layout->title_label = widget; return; }
    if (strcmp(name, "preset_dropdown") == 0) { layout->preset_combo = widget; return; }
    if (strcmp(name, "page_toggle_btn") == 0) { layout->page_toggle_button = widget; return; }
    if (strcmp(name, "exit_btn") == 0) { layout->exit_button = widget; return; }
    if (strcmp(name, "out_meter_L") == 0) { layout->main_level_meter = widget; return; }
    if (strcmp(name, "out_meter_R") == 0) { layout->cpu_meter = widget; return; }
    if (strcmp(name, "waveform_view") == 0) { layout->waveform_view = widget; layout->page1.gen_waveform_view = widget; return; }

    if (strcmp(name, "gen_group") == 0) { layout->page1.gen_group = widget; return; }
    if (strcmp(name, "lfo1_group") == 0) { layout->page1.lfo1_group = widget; return; }
    if (strcmp(name, "lfo2_group") == 0) { layout->page1.lfo2_group = widget; return; }
    if (strcmp(name, "adsr1_group") == 0) { layout->page1.adsr1_group = widget; return; }
    if (strcmp(name, "adsr2_group") == 0) { layout->page1.adsr2_group = widget; return; }

    if (strcmp(name, "gen_volume") == 0) { layout->page1.gen_volume_knob = widget; return; }
    if (strcmp(name, "gen_panning") == 0) { layout->page1.gen_panning_knob = widget; return; }
    if (strcmp(name, "gen_detune") == 0) { layout->page1.gen_detune_knob = widget; return; }
    if (strcmp(name, "gen_bandwidth") == 0) { layout->page1.gen_bandwidth_knob = widget; return; }
    if (strcmp(name, "gen_damp") == 0) { layout->page1.gen_damp_knob = widget; return; }
    if (strcmp(name, "gen_harmonics") == 0) { layout->page1.gen_harmonics_knob = widget; return; }
    if (strcmp(name, "gen_drive") == 0) { layout->page1.gen_drive_knob = widget; return; }
    if (strcmp(name, "gen_scale") == 0) { layout->page1.gen_scale_knob = widget; return; }
    if (strcmp(name, "gen_modulation") == 0) { layout->page1.gen_modulation_knob = widget; return; }
    if (strcmp(name, "gen_noise") == 0) { layout->page1.gen_noise_knob = widget; return; }
    if (strcmp(name, "gen_noise_freq") == 0) { layout->page1.gen_noise_freq_knob = widget; return; }
    if (strcmp(name, "gen_noise_bw") == 0) { layout->page1.gen_noise_bw_knob = widget; return; }
    if (strcmp(name, "poly") == 0) { layout->page1.poly_control = widget; return; }
    if (strcmp(name, "gen_glide") == 0) { layout->page1.gen_glide_control = widget; return; }

    if (strcmp(name, "lfo1_freq") == 0) { layout->page1.lfo1_freq_knob = widget; return; }
    if (strcmp(name, "lfo1_amp") == 0) { layout->page1.lfo1_depth_knob = widget; return; }
    if (strcmp(name, "lfo1_sync") == 0) { layout->page1.lfo1_sync_toggle = widget; return; }
    if (strcmp(name, "lfo2_freq") == 0) { layout->page1.lfo2_freq_knob = widget; return; }
    if (strcmp(name, "lfo2_amp") == 0) { layout->page1.lfo2_depth_knob = widget; return; }
    if (strcmp(name, "lfo2_sync") == 0) { layout->page1.lfo2_sync_toggle = widget; return; }

    if (strcmp(name, "adsr1_env") == 0) { layout->page1.adsr1_env_view = widget; return; }
    if (strcmp(name, "adsr1_slope") == 0) { layout->page1.adsr1_slope_slider = widget; return; }
    if (strcmp(name, "adsr2_env") == 0) { layout->page1.adsr2_env_view = widget; return; }
    if (strcmp(name, "adsr2_slope") == 0) { layout->page1.adsr2_slope_slider = widget; return; }

    if (strcmp(name, "filter1_group") == 0) { layout->page1.filter_group = widget; return; }
    if (strcmp(name, "filter2_group") == 0) { layout->page1.filter2_group = widget; return; }
    if (strcmp(name, "filter3_group") == 0) { layout->page1.filter3_group = widget; return; }
    if (strcmp(name, "filter4_group") == 0) { layout->page1.filter4_group = widget; return; }

    if (strcmp(name, "filter1_cutoff") == 0) { layout->page1.filter_cutoff_knob = widget; return; }
    if (strcmp(name, "filter1_res") == 0) { layout->page1.filter_resonance_knob = widget; return; }
    if (strcmp(name, "filter1_on") == 0) { layout->page1.filter1_on_toggle = widget; return; }
    if (strcmp(name, "filter2_cutoff") == 0) { layout->page1.filter2_cutoff_knob = widget; return; }
    if (strcmp(name, "filter2_res") == 0) { layout->page1.filter2_resonance_knob = widget; return; }
    if (strcmp(name, "filter2_on") == 0) { layout->page1.filter2_on_toggle = widget; return; }
    if (strcmp(name, "filter3_cutoff") == 0) { layout->page1.filter3_cutoff_knob = widget; return; }
    if (strcmp(name, "filter3_res") == 0) { layout->page1.filter3_resonance_knob = widget; return; }
    if (strcmp(name, "filter3_on") == 0) { layout->page1.filter3_on_toggle = widget; return; }
    if (strcmp(name, "filter4_cutoff") == 0) { layout->page1.filter4_cutoff_knob = widget; return; }
    if (strcmp(name, "filter4_res") == 0) { layout->page1.filter4_resonance_knob = widget; return; }
    if (strcmp(name, "filter4_on") == 0) { layout->page1.filter4_on_toggle = widget; return; }

    int idx = -1;
    if (sscanf(name, "lfo1_shape_%d", &idx) == 1 && idx >= 1 && idx <= 5) {
        layout->page1.lfo1_shape_buttons[idx - 1] = widget;
        return;
    }
    if (sscanf(name, "lfo2_shape_%d", &idx) == 1 && idx >= 1 && idx <= 5) {
        layout->page1.lfo2_shape_buttons[idx - 1] = widget;
        return;
    }
    if (sscanf(name, "unisono_%d", &idx) == 1 && idx >= 1 && idx <= 10) {
        layout->page1.unisono_buttons[idx - 1] = widget;
        return;
    }
    if (sscanf(name, "octave_%d", &idx) == 1 || sscanf(name, "octave__%d", &idx) == 1) {
        if (name[7] == '_') idx = -idx; // handle "octave__4" => -4 from schema names
        int octave_idx = idx + 4;
        if (octave_idx >= 0 && octave_idx < 9) {
            layout->page1.octave_buttons[octave_idx] = widget;
            return;
        }
    }

    if (sscanf(name, "fxgrp_%d", &idx) == 1 && idx >= 0 && idx < 7) {
        layout->page2.fx_groups[idx] = widget;
        if (idx == 5) layout->page2.formant_group = widget;
        return;
    }
    if (strcmp(name, "mod_group") == 0) { layout->page2.modulation_group = widget; return; }
    if (strcmp(name, "fx_stack_group") == 0) { layout->page2.fx_stack_group = widget; return; }

    if (sscanf(name, "mod_src_%d", &idx) == 1 && idx >= 0 && idx < 8) {
        layout->page2.mod_source_combos[idx] = widget;
        return;
    }
    if (sscanf(name, "mod_dst_%d", &idx) == 1 && idx >= 0 && idx < 8) {
        layout->page2.mod_dest_combos[idx] = widget;
        return;
    }
    if (sscanf(name, "mod_amt_%d", &idx) == 1 && idx >= 0 && idx < 8) {
        layout->page2.mod_amount_knobs[idx] = widget;
        return;
    }
    if (sscanf(name, "fxstk_sel_%d", &idx) == 1 && idx >= 0 && idx < 10) {
        layout->page2.fx_stack_combos[idx] = widget;
        return;
    }
    if (sscanf(name, "formant_%d", &idx) == 1 && idx >= 0 && idx < 5) {
        layout->page2.formant_type_buttons[idx] = widget;
        return;
    }

    if (strncmp(name, "flanger_", 8) == 0) {
        layout->page2.fx_wet_knobs[0] = (strcmp(name, "flanger_wet") == 0) ? widget : layout->page2.fx_wet_knobs[0];
        layout->page2.fx_param1_knobs[0] = (strcmp(name, "flanger_lfo") == 0) ? widget : layout->page2.fx_param1_knobs[0];
        layout->page2.fx_param2_knobs[0] = (strcmp(name, "flanger_freq") == 0) ? widget : layout->page2.fx_param2_knobs[0];
        layout->page2.fx_param3_knobs[0] = (strcmp(name, "flanger_amp") == 0) ? widget : layout->page2.fx_param3_knobs[0];
        return;
    }
    if (strncmp(name, "reverb_", 7) == 0) {
        layout->page2.fx_wet_knobs[1] = (strcmp(name, "reverb_wet") == 0) ? widget : layout->page2.fx_wet_knobs[1];
        layout->page2.fx_param1_knobs[1] = (strcmp(name, "reverb_room_sz") == 0) ? widget : layout->page2.fx_param1_knobs[1];
        layout->page2.fx_param2_knobs[1] = (strcmp(name, "reverb_damp") == 0) ? widget : layout->page2.fx_param2_knobs[1];
        layout->page2.fx_param3_knobs[1] = (strcmp(name, "reverb_width") == 0) ? widget : layout->page2.fx_param3_knobs[1];
        return;
    }
    if (strncmp(name, "delay_", 6) == 0) {
        layout->page2.fx_param1_knobs[2] = (strcmp(name, "delay_left") == 0) ? widget : layout->page2.fx_param1_knobs[2];
        layout->page2.fx_param2_knobs[2] = (strcmp(name, "delay_right") == 0) ? widget : layout->page2.fx_param2_knobs[2];
        layout->page2.fx_param3_knobs[2] = (strcmp(name, "delay_decay") == 0) ? widget : layout->page2.fx_param3_knobs[2];
        return;
    }
    if (strncmp(name, "eq_", 3) == 0) {
        layout->page2.fx_param1_knobs[3] = (strcmp(name, "eq_bass") == 0) ? widget : layout->page2.fx_param1_knobs[3];
        layout->page2.fx_param2_knobs[3] = (strcmp(name, "eq_mid") == 0) ? widget : layout->page2.fx_param2_knobs[3];
        layout->page2.fx_param3_knobs[3] = (strcmp(name, "eq_treble") == 0) ? widget : layout->page2.fx_param3_knobs[3];
        return;
    }
    if (strncmp(name, "chorus_", 7) == 0) {
        layout->page2.fx_wet_knobs[4] = (strcmp(name, "chorus_gain") == 0) ? widget : layout->page2.fx_wet_knobs[4];
        layout->page2.fx_param1_knobs[4] = (strcmp(name, "chorus_freq") == 0) ? widget : layout->page2.fx_param1_knobs[4];
        layout->page2.fx_param2_knobs[4] = (strcmp(name, "chorus_depth") == 0) ? widget : layout->page2.fx_param2_knobs[4];
        return;
    }
    if (strcmp(name, "formant_wet") == 0) {
        layout->page2.fx_wet_knobs[5] = widget;
        layout->page2.formant_amount_knob = widget;
        return;
    }
    if (strcmp(name, "distortion_amount") == 0) {
        layout->page2.fx_wet_knobs[6] = widget;
        return;
    }
}

static void tf_schema_apply_special_callbacks(TunefishWidget* widget)
{
    if (!widget) return;

    int idx = -1;
    if (sscanf(widget->name, "lfo1_shape_%d", &idx) == 1 && idx >= 1 && idx <= 5) {
        widget->onValueChange = tf_lfo1shape_button_value_handler;
        return;
    }
    if (sscanf(widget->name, "lfo2_shape_%d", &idx) == 1 && idx >= 1 && idx <= 5) {
        widget->onValueChange = tf_lfo2shape_button_value_handler;
        return;
    }
    if (strncmp(widget->name, "unisono_", 8) == 0) {
        widget->onClick = tf_unisono_button_handler;
        return;
    }
    if (strncmp(widget->name, "octave_", 7) == 0) {
        widget->onClick = tf_octave_button_handler;
        return;
    }
    if (sscanf(widget->name, "formant_%d", &idx) == 1 && idx >= 0 && idx < 5) {
        widget->onClick = tf_formant_button_handler;
        return;
    }
}

static const ft2_ui_bitmap_asset_t *tf_find_bitmap_asset(uint16_t id)
{
    for (uint16_t i = 0; i < ft2_ui_assets.bitmap_count; i++)
    {
        if (ft2_ui_assets.bitmaps[i].id == id)
            return &ft2_ui_assets.bitmaps[i];
    }
    return NULL;
}

static TunefishCompleteLayout* tf_create_complete_layout_from_schema(const ft2_ui_layout_desc_t* desc)
{
    if (!desc || desc->version != FT2_UI_SCHEMA_VERSION) return NULL;

    TunefishCompleteLayout* layout = (TunefishCompleteLayout*)calloc(1, sizeof(TunefishCompleteLayout));
    if (!layout) return NULL;

    layout->current_page = 0;
    layout->initialized = false;
    layout->visible = false;

    if (desc->bitmaps.count > 0 && desc->bitmap_desc) {
        for (uint16_t i = 0; i < desc->bitmaps.count; i++) {
            const ft2_ui_bitmap_desc_t* d = &desc->bitmap_desc[i];
            const ft2_ui_bitmap_asset_t *asset = tf_find_bitmap_asset(d->bitmap_id);
            if (!asset || !asset->bmp || asset->fmt != FT2_UI_BMP_FMT_RLE4)
                continue;

            int32_t bmp_w = 0, bmp_h = 0;
            uint8_t *pixels = ft2_bmp_decode_rle4_to_pal(asset->bmp, &bmp_w, &bmp_h);
            if (!pixels) continue;

            TunefishWidget* w = tf_create_bitmap("bitmap", d->x, d->y, d->w, d->h, pixels, bmp_w, bmp_h, true);
            tf_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->waveform_views.count > 0 && desc->waveform_view_desc) {
        for (uint16_t i = 0; i < desc->waveform_views.count; i++) {
            const ft2_ui_waveform_view_desc_t* d = &desc->waveform_view_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "waveform_view";
            TunefishWidget* w = tf_create_waveform_view(name, d->x, d->y, d->w, d->h);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_buttons.count > 0 && desc->tf_button_desc) {
        for (uint16_t i = 0; i < desc->tf_buttons.count; i++) {
            const ft2_ui_tf_button_desc_t* d = &desc->tf_button_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "button";
            const char* text = d->text ? d->text : "";
            TunefishWidget* w = (d->page == FT2_UI_WIDGET_PAGE_BOTH)
                ? tf_create_button(name, text, d->x, d->y, d->w, d->h)
                : tf_create_pushbutton(name, text, d->x, d->y, d->w, d->h, text);
            if (w && d->page != FT2_UI_WIDGET_PAGE_BOTH) {
                tf_apply_button_styling(w);
            }
            tf_schema_apply_special_callbacks(w);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_toggles.count > 0 && desc->tf_toggle_desc) {
        for (uint16_t i = 0; i < desc->tf_toggles.count; i++) {
            const ft2_ui_tf_toggle_desc_t* d = &desc->tf_toggle_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "toggle";
            const char* text = d->text ? d->text : "";
            TunefishWidget* w = tf_create_toggle_button(name, text, d->x, d->y, d->w, d->h);
            if (w) {
                w->pressed = d->default_pressed;
                w->value = w->pressed ? 1.0f : 0.0f;
            }
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_labels.count > 0 && desc->tf_label_desc) {
        for (uint16_t i = 0; i < desc->tf_labels.count; i++) {
            const ft2_ui_tf_label_desc_t* d = &desc->tf_label_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "label";
            const char* text = d->text ? d->text : "";
            TunefishWidget* w = tf_create_label(name, text, d->x, d->y, d->w, d->h);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_rotary_sliders.count > 0 && desc->tf_rotary_slider_desc) {
        for (uint16_t i = 0; i < desc->tf_rotary_sliders.count; i++) {
            const ft2_ui_tf_rotary_slider_desc_t* d = &desc->tf_rotary_slider_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "rotary";
            int center_x = d->x + (int)d->radius;
            int center_y = d->y + (int)d->radius;
            TunefishWidget* w = tf_create_rotary_slider(name, center_x, center_y, d->radius, d->start_angle, d->end_angle);
            if (w && d->label) {
                tf_widget_set_label(w, d->label);
            }
            tf_apply_knob_styling(w, 0.5f);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_linear_sliders.count > 0 && desc->tf_linear_slider_desc) {
        for (uint16_t i = 0; i < desc->tf_linear_sliders.count; i++) {
            const ft2_ui_tf_linear_slider_desc_t* d = &desc->tf_linear_slider_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "linear";
            TunefishWidget* w = tf_create_linear_slider(name, d->x, d->y, d->w, d->h, d->vertical);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_combo_boxes.count > 0 && desc->tf_combo_box_desc) {
        for (uint16_t i = 0; i < desc->tf_combo_boxes.count; i++) {
            const ft2_ui_tf_combo_box_desc_t* d = &desc->tf_combo_box_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "combo";
            const char* const* items = d->items;
            uint16_t item_count = d->item_count;

            if ((!items || item_count == 0) && strncmp(name, "mod_src_", 8) == 0) {
                items = ft2_synth_get_mod_source_items();
                item_count = (uint16_t)ft2_synth_get_mod_source_count();
            } else if ((!items || item_count == 0) && strncmp(name, "mod_dst_", 8) == 0) {
                items = ft2_synth_get_mod_dest_items();
                item_count = (uint16_t)ft2_synth_get_mod_dest_count();
            } else if ((!items || item_count == 0) && strncmp(name, "fxstk_sel_", 10) == 0) {
                items = fxStackItems;
                item_count = (uint16_t)fxStackItemCount;
            }

            TunefishWidget* w = tf_create_combo_box(name, d->x, d->y, d->w, d->h, items, item_count);
            if (w && item_count > 0) {
                w->selectedIndex = d->selected_index;
            }
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_level_meters.count > 0 && desc->tf_level_meter_desc) {
        for (uint16_t i = 0; i < desc->tf_level_meters.count; i++) {
            const ft2_ui_tf_level_meter_desc_t* d = &desc->tf_level_meter_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "meter";
            int num_leds = d->num_leds > 0 ? d->num_leds : 12;
            TunefishWidget* w = tf_create_level_meter(name, d->x, d->y, d->w, d->h, num_leds, d->show_peak);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_parameter_controls.count > 0 && desc->tf_parameter_control_desc) {
        for (uint16_t i = 0; i < desc->tf_parameter_controls.count; i++) {
            const ft2_ui_tf_parameter_control_desc_t* d = &desc->tf_parameter_control_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "param";
            const char* label = d->label ? d->label : "";
            TunefishWidget* w = tf_create_parameter_control(name, label, d->x, d->y, d->w, d->h);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    if (desc->tf_group_boxes.count > 0 && desc->tf_group_box_desc) {
        for (uint16_t i = 0; i < desc->tf_group_boxes.count; i++) {
            const ft2_ui_tf_group_box_desc_t* d = &desc->tf_group_box_desc[i];
            const char* name = (d->name && d->name[0]) ? d->name : "group";
            const char* title = d->title ? d->title : "";
            TunefishWidget* w = tf_create_group_box(name, title, d->x, d->y, d->w, d->h);
            tf_schema_register_widget(layout, w, d->page);
            tf_schema_assign_widget(layout, w);
        }
    }

    tf_style_all_widgets_authentic(layout);
    tf_connect_widget_callbacks(layout);
    tf_populate_preset_combo(layout);

    layout->initialized = true;
    return layout;
}
#endif



// =============================================================================
// PUBLIC API FUNCTIONS
// =============================================================================

// Layout Creation Function
TunefishCompleteLayout* tf_create_complete_layout(void) {
#ifdef TF_USE_SCHEMA_LAYOUT
    TunefishCompleteLayout* schema_layout = tf_create_complete_layout_from_schema(&ft2_tunefish_complete_layout_layout);
    if (schema_layout) return schema_layout;
#endif

    TunefishCompleteLayout* layout = (TunefishCompleteLayout*)calloc(1, sizeof(TunefishCompleteLayout));
    if (!layout) return NULL;

    int widgetIndex = 0;      // master all_widgets index
    int page1Index  = 0;
    int page2Index  = 0;

    // Build widgets in three focused helper calls
    create_global_widgets(layout, &widgetIndex);
    create_page1_widgets(layout, &widgetIndex, &page1Index);
    create_page2_widgets(layout, &widgetIndex, &page2Index);

    // Ensure unused array slots are NULL to help debugging
    for (int i = widgetIndex; i < TF_TOTAL_WIDGETS; i++) layout->all_widgets[i] = NULL;
    for (int i = page1Index; i < TF_PAGE1_WIDGETS; i++) layout->page1_widgets[i] = NULL;
    for (int i = page2Index; i < TF_PAGE2_WIDGETS; i++) layout->page2_widgets[i] = NULL;

    // The old inlined creation blocks have been moved to helper functions above

    // --- Finalize ---
    layout->current_page = 0;
    layout->initialized = true;
    layout->visible = false;
    tf_style_all_widgets_authentic(layout);
    tf_connect_widget_callbacks(layout);
    printf("Tunefish Complete Layout created with %d total widgets\n", widgetIndex);
    return layout;
}

// Page Management
void tf_switch_to_page(TunefishCompleteLayout* layout, int page) {
    if (!layout || page < 0 || page >= TF_PAGE_COUNT) return;
    layout->current_page = page;
    // Update page toggle button text
    if (layout->page_toggle_button) {
        snprintf(layout->page_toggle_button->text, sizeof(layout->page_toggle_button->text),
                 "page %d", page + 1);
    }
    tf_update_widget_visibility(layout); // Ensure correct widget visibility
    printf("Switched to Tunefish page %d\n", page + 1);
}

void tf_show_layout(TunefishCompleteLayout* layout) {
    if (!layout) return;

    // Forward declaration for global active layout pointer
    extern TunefishCompleteLayout* g_active_tunefish_layout;
    // Make this layout globally accessible for event handlers
    g_active_tunefish_layout = layout;

    /* Ensure we pull the latest persistent parameters from the currently
     * selected Tunefish4 instrument *before* the editor becomes visible.
     * This guarantees that all widget states are up-to-date the very first
     * frame we draw the GUI (no more stale values from a previously edited
     * instrument).
     */
    tf_update_all_widgets_from_synth(layout); // refresh shadow arrays + widget values
    tf_sync_bool_parameters();               // keep bool/int wrappers in sync
    tf_sync_preset_combo_with_instrument(layout); // update preset name/selection

    layout->visible = true;
    tf_update_widget_visibility(layout);     // show widgets on the active page
    printf("Tunefish Complete Layout shown\n");
}

void tf_hide_layout(TunefishCompleteLayout* layout) {
    if (!layout) return;

    // Forward declaration for global active layout pointer
    extern TunefishCompleteLayout* g_active_tunefish_layout;
    // Only clear the global pointer if this was the active layout
    if (g_active_tunefish_layout == layout) {
        g_active_tunefish_layout = NULL;
    }

    layout->visible = false;
    printf("Tunefish Complete Layout hidden\n");
}

// Rendering Functions
void tf_render_complete_layout(TunefishCompleteLayout* layout) {
    if (!layout || !layout->visible) return;

    // Keep UI controls in sync with live synth state while visible
    tf_update_all_widgets_from_synth(layout);

    // Pull latest live meter values from audio engine
    tf_update_live_meters(layout);
    // Clear background with FT2 desktop color
    fillRect(0, 0, SCREEN_W, SCREEN_H, PAL_DESKTOP);
    
    // Draw global bitmap widgets first (background)
    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        if (layout->global_widgets[i] && layout->global_widgets[i]->visible && layout->global_widgets[i]->page == PAGE_BOTH &&
            layout->global_widgets[i]->type == TF_WIDGET_BITMAP)
            tf_draw_widget(layout->global_widgets[i]);
    }

    // Draw remaining global widgets
    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        if (layout->global_widgets[i] && layout->global_widgets[i]->visible && layout->global_widgets[i]->page == PAGE_BOTH &&
            layout->global_widgets[i]->type != TF_WIDGET_BITMAP)
            tf_draw_widget(layout->global_widgets[i]);
    }
    
    // Draw current page widgets in correct order
    TunefishWidget** pageWidgets = (layout->current_page == 0) ? layout->page1_widgets : layout->page2_widgets;
    int pageWidgetCount = (layout->current_page == 0) ? TF_PAGE1_WIDGETS : TF_PAGE2_WIDGETS;
    
    // First pass: Draw bitmap widgets (background elements)
    for (int i = 0; i < pageWidgetCount; i++) {
        if (pageWidgets[i] && pageWidgets[i]->visible && pageWidgets[i]->page == layout->current_page &&
            pageWidgets[i]->type == TF_WIDGET_BITMAP) {
            tf_draw_widget(pageWidgets[i]);
        }
    }

    // Second pass: Draw group boxes (frames)
    for (int i = 0; i < pageWidgetCount; i++) {
        if (pageWidgets[i] && pageWidgets[i]->visible && pageWidgets[i]->page == layout->current_page && 
            pageWidgets[i]->type == TF_WIDGET_GROUP_BOX) {
            tf_draw_widget(pageWidgets[i]);
        }
    }
    
    // Third pass: Draw all other widgets (foreground elements)
    for (int i = 0; i < pageWidgetCount; i++) {
        if (pageWidgets[i] && pageWidgets[i]->visible && pageWidgets[i]->page == layout->current_page && 
            pageWidgets[i]->type != TF_WIDGET_GROUP_BOX && pageWidgets[i]->type != TF_WIDGET_BITMAP) {
            tf_draw_widget(pageWidgets[i]);
        }
    }

    // Custom ADSR mini envelopes (draw last)
    if (layout->current_page == 0) {
        tf_draw_adsr_envs(layout);
    }
}

// Enhanced event handling with debug output
bool tf_handle_layout_mouse_event(TunefishCompleteLayout* layout, int mouseX, int mouseY, bool pressed) {
    if (!layout || !layout->visible) return false;

    printf("🖱️  [MOUSE] Event at (%d, %d), pressed=%s\n", mouseX, mouseY, pressed ? "YES" : "NO");

    if (!pressed && g_tf_exit_pending) {
        g_tf_exit_pending = false;
        tf_hide_layout(layout);
        ft2_close_synth_editor();
        return true;
    }

    // Check global widgets first
    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        if (layout->global_widgets[i] && tf_widget_handle_mouse_event(layout->global_widgets[i], mouseX, mouseY, pressed)) {
            printf("🎯 [WIDGET] Global widget clicked: %s\n", layout->global_widgets[i]->name);

            // Handle special global widget actions
            if (layout->global_widgets[i] == layout->page_toggle_button && pressed) {
                tf_switch_to_page(layout, (layout->current_page + 1) % TF_PAGE_COUNT);
                printf("📄 [PAGE] Switched to page %d\n", layout->current_page);
            } else if (layout->global_widgets[i] == layout->exit_button && pressed) {
                // Arm close on mouse-up to show pressed state
                printf("🚪 [EXIT] Closing Tunefish synth editor\n");
                g_tf_exit_pending = true;
            } else if (layout->global_widgets[i] == layout->preset_combo && pressed) {
                // Handle preset selection
                // The combo box widget will handle up/down button clicks internally
                // and call its onComboSelect callback when selection changes
                return true;
            }
            return true;
        }
    }

    if (layout->current_page == 0) {
        if (tf_handle_adsr_env_mouse(layout, mouseX, mouseY, pressed))
            return true;
    }

    // Iterate widgets on current page
    TunefishWidget** pageArray = (layout->current_page == 0) ? layout->page1_widgets : layout->page2_widgets;
    int pageCount = (layout->current_page == 0) ? TF_PAGE1_WIDGETS : TF_PAGE2_WIDGETS;
    for (int i = 0; i < pageCount; i++) {
        TunefishWidget* w = pageArray[i];
        if (!w) continue;
        if (tf_widget_handle_mouse_event(w, mouseX, mouseY, pressed)) {
            printf("🎯 [WIDGET] Page %d widget clicked: %s\n", layout->current_page+1, w->name);
            return true;
        }
    }
    // If editor is visible, always consume the event to prevent click-through
    return true;
}

// Keyboard test function for easier widget testing (uses safe, non-conflicting keys)
bool tf_handle_layout_keyboard_test(TunefishCompleteLayout* layout, int key) {
    if (!layout || !layout->visible) return false;

    printf("⌨️  [KEYBOARD] Test key pressed: %d\n", key);

    switch (key) {
        // Use function keys that don't conflict with FT2's note playing
        case SDLK_F9:  // F9 - Test preset change
            if (layout->preset_combo && layout->preset_combo->comboItems && layout->preset_combo->comboItemCount > 0) {
                int oldIndex = layout->preset_combo->selectedIndex;
                layout->preset_combo->selectedIndex = (layout->preset_combo->selectedIndex + 1) % layout->preset_combo->comboItemCount;
                printf("🎵 [TEST] Preset: '%s' → '%s'\n",
                       layout->preset_combo->comboItems[oldIndex],
                       layout->preset_combo->comboItems[layout->preset_combo->selectedIndex]);
            }
            return true;

        case '\t':  // Tab key - switch pages (safe, commonly used for UI navigation)
            tf_switch_to_page(layout, (layout->current_page + 1) % TF_PAGE_COUNT);
            printf("📄 [TEST] Switched to page %d\n", layout->current_page);
            return true;

        case SDLK_F5:  // F5 - Test first rotary knob on current page
            if (layout->current_page == 0 && layout->page1.gen_volume_knob) {
                tf_parameter_changed(layout->page1.gen_volume_knob, 0.5f);
                printf("🎛️  [TEST] Volume knob set to 50%%\n");
            } else if (layout->current_page == 1 && layout->page2.fx1_wet_knob) {
                tf_parameter_changed(layout->page2.fx1_wet_knob, 0.75f);
                printf("🎛️  [TEST] FX1 wet knob set to 75%%\n");
            }
            return true;

        case SDLK_F6:  // F6 - Test second rotary knob on current page
            if (layout->current_page == 0 && layout->page1.gen_panning_knob) {
                tf_parameter_changed(layout->page1.gen_panning_knob, 0.25f);
                printf("🎛️  [TEST] Panning knob set to 25%%\n");
            } else if (layout->current_page == 1 && layout->page2.fx2_wet_knob) {
                tf_parameter_changed(layout->page2.fx2_wet_knob, 0.60f);
                printf("🎛️  [TEST] FX2 wet knob set to 60%%\n");
            }
            return true;

        case SDLK_F7:  // F7 - Test filter knob
            if (layout->current_page == 0 && layout->page1.filter_cutoff_knob) {
                tf_parameter_changed(layout->page1.filter_cutoff_knob, 0.80f);
                printf("🎛️  [TEST] Filter cutoff set to 80%%\n");
            }
            return true;

        case SDLK_F8:  // F8 - Test LFO
            if (layout->current_page == 0 && layout->page1.lfo1_freq_knob) {
                tf_parameter_changed(layout->page1.lfo1_freq_knob, 0.65f);
                printf("🎛️  [TEST] LFO1 frequency set to 65%%\n");
            }
            return true;



        default:
            // Don't handle other keys - let FT2 handle them for note playing etc.
            return false;
    }
}

// Widget Styling Functions
void tf_style_all_widgets_authentic(TunefishCompleteLayout* layout) {
    if (!layout) return;

    // Style global widgets
    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        if (layout->global_widgets[i]) {
            tf_apply_tunefish_styling(layout->global_widgets[i]);
        }
    }

    // Apply specific styling based on widget type
    if (layout->title_label) {
        layout->title_label->textColor = TF_COL_TEXT_NORMAL;
    }

    if (layout->main_level_meter) {
        tf_apply_meter_styling(layout->main_level_meter);
    }

    if (layout->cpu_meter) {
        tf_apply_meter_styling(layout->cpu_meter);
    }
}

void tf_apply_knob_styling(TunefishWidget* knob, float defaultValue) {
    if (!knob) return;
    tf_style_as_parameter_knob(knob);
    knob->defaultValue = defaultValue;
    knob->value = defaultValue;
}

void tf_apply_button_styling(TunefishWidget* button) {
    if (!button) return;
    tf_style_as_main_button(button);
}

void tf_apply_combo_styling(TunefishWidget* combo) {
    if (!combo) return;
    combo->bgColor = TF_COL_COMBO_BG;
    combo->textColor = TF_COL_COMBO_TEXT;
}

void tf_apply_meter_styling(TunefishWidget* meter) {
    if (!meter) return;
    tf_style_as_level_indicator(meter);
}

// Utility Functions
const TunefishLayoutPositions* tf_get_layout_positions(void) {
    return &g_layoutPositions;
}

void tf_destroy_complete_layout(TunefishCompleteLayout* layout) {
    if (!layout) return;

    // Destroy all widgets
    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        if (layout->global_widgets[i]) {
            tf_widget_destroy(layout->global_widgets[i]);
        }
    }

    // TODO: Destroy page widgets properly

    free(layout);
    printf("Tunefish Complete Layout destroyed\n");
}

// Note: Callback functions removed - event handling is now done directly in tf_handle_layout_mouse_event

// When switching pages, update widget visibility for all widgets
void tf_update_widget_visibility(TunefishCompleteLayout* layout) {
    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        if (layout->global_widgets[i]) layout->global_widgets[i]->visible = (layout->global_widgets[i]->page == PAGE_BOTH);
    }
    for (int i = 0; i < TF_PAGE1_WIDGETS; i++) {
        if (layout->page1_widgets[i]) layout->page1_widgets[i]->visible = (layout->page1_widgets[i]->page == PAGE1 && layout->current_page == 0);
    }
    for (int i = 0; i < TF_PAGE2_WIDGETS; i++) {
        if (layout->page2_widgets[i]) layout->page2_widgets[i]->visible = (layout->page2_widgets[i]->page == PAGE2 && layout->current_page == 1);
    }
}

// =============================================================================
// WIDGET CREATION HELPERS
// =============================================================================

void tf_add_widget_to_layout(TunefishCompleteLayout* layout, TunefishWidget* widget, int page)
{
    if (!layout || !widget) return;

    widget->page = page;

    for (int i = 0; i < TF_TOTAL_WIDGETS; i++) {
        if (layout->all_widgets[i] == NULL) {
            layout->all_widgets[i] = widget;
            break;
        }
    }

    if (page == 1) { // PAGE1
        for (int i = 0; i < TF_PAGE1_WIDGETS; i++) {
            if (layout->page1_widgets[i] == NULL) {
                layout->page1_widgets[i] = widget;
                break;
            }
        }
    } else if (page == 2) { // PAGE2
        for (int i = 0; i < TF_PAGE2_WIDGETS; i++) {
            if (layout->page2_widgets[i] == NULL) {
                layout->page2_widgets[i] = widget;
                break;
            }
        }
    } else if (page == 0) { // PAGE_BOTH
        for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
            if (layout->global_widgets[i] == NULL) {
                layout->global_widgets[i] = widget;
                break;
            }
        }
    }
}

static void create_global_widgets(TunefishCompleteLayout* layout, int* allIdx) {
    const int gsp = 8;
    const int btnW = 60, btnH = 22;
    const int dropW = 130, dropH = 22;
    int gy = 8;

    // Title and top row widgets
    const int titleW = 180, titleH = 22;
    int titleX = (TF_LAYOUT_WIDTH - titleW) - 440;
    const int meterW = 40, meterH = 18;
    const int pageBtnW = 48, exitBtnW = 48;

    layout->title_label = tf_create_label("title_label", "TF4 DXM Edition", titleX, gy, titleW, titleH);
    layout->preset_combo = tf_create_combo_box("preset_dropdown", titleX + 260 + gsp, gy, dropW, dropH, NULL, 0);
    layout->page_toggle_button = tf_create_button("page_toggle_btn", "Page", TF_LAYOUT_WIDTH - pageBtnW - exitBtnW - gsp*2, gy, pageBtnW, btnH);
    layout->exit_button = tf_create_button("exit_btn", "Exit", TF_LAYOUT_WIDTH - exitBtnW - gsp, gy, exitBtnW, btnH);
    int meterBaseX = TF_LAYOUT_WIDTH - meterW*2 - gsp*4 - 100;
    layout->main_level_meter = tf_create_level_meter("out_meter_L", meterBaseX, gy + 2, meterW, meterH, 12, true);
    layout->cpu_meter = tf_create_level_meter("out_meter_R", meterBaseX + meterW + gsp, gy + 2, meterW, meterH, 12, true);

    TunefishWidget* globals[TF_GLOBAL_WIDGETS] = {
        layout->title_label, layout->preset_combo, layout->page_toggle_button,
        layout->exit_button, layout->main_level_meter, layout->cpu_meter
    };

    for (int i = 0; i < TF_GLOBAL_WIDGETS; i++) {
        layout->global_widgets[i] = globals[i];
        globals[i]->page = PAGE_BOTH;
        globals[i]->visible = true;
        layout->all_widgets[(*allIdx)++] = globals[i];
    }

    // After global widgets array fill, populate preset combo
    tf_populate_preset_combo(layout);
}

// LFO1 Shape button handler
static void tf_lfo1shape_button_handler(TunefishWidget* w) {
    if (!w) return;
    extern TunefishCompleteLayout* g_active_tunefish_layout;
    TunefishCompleteLayout* layout = g_active_tunefish_layout;
    if (!layout) return;
    
    // Extract index from widget name "lfo1_shape_X" where X is 1-5
    int idx = atoi(w->name + 11) - 1; // Convert "1" to 0, "2" to 1, etc.
    if (idx < 0 || idx > 4) return;
    
    for (int i = 0; i < 5; i++)
        if (layout->page1.lfo1_shape_buttons[i])
            layout->page1.lfo1_shape_buttons[i]->pressed = (i == idx);

    // Update local shadow variable
    extern int synthLFO1Shape; synthLFO1Shape = idx + 1; // 1..5

    float norm = (float)idx / 4.0f;
    printf("[LFO1SHAPE_HANDLER] button=%d norm=%0.3f\n", idx+1, norm);

    // Send to synth directly
    int instrID = getCurrentTF4InstrumentID();
    if (instrID >= 0)
    {
        ft2_synth_set_param(instrID, TF_LFO1_SHAPE, norm);
        printf("[LFO1SHAPE PARAM] idx=%d norm=%0.3f param=%d\n", idx, norm, TF_LFO1_SHAPE);
    } else {
        printf("[LFO1SHAPE WARN] No active instrument to send param.\n");
    }
    tf_request_redraw();
}

// Wrapper allowing assignment to onValueChange callbacks if needed
static void tf_lfo1shape_button_value_handler(TunefishWidget* w, float v){ tf_lfo1shape_button_handler(w); }

// LFO2 Shape button handler
static void tf_lfo2shape_button_handler(TunefishWidget* w) {
    if (!w) return;
    extern TunefishCompleteLayout* g_active_tunefish_layout;
    TunefishCompleteLayout* layout = g_active_tunefish_layout;
    if (!layout) return;
    
    // Extract index from widget name "lfo2_shape_X" where X is 1-5
    int idx = atoi(w->name + 11) - 1; // Convert "1" to 0, "2" to 1, etc.
    if (idx < 0 || idx > 4) return;
    
    for (int i = 0; i < 5; i++)
        if (layout->page1.lfo2_shape_buttons[i])
            layout->page1.lfo2_shape_buttons[i]->pressed = (i == idx);

    // Update local shadow variable
    extern int synthLFO2Shape; synthLFO2Shape = idx + 1; // 1..5

    float norm = (float)idx / 4.0f;
    printf("[LFO2SHAPE_HANDLER] button=%d norm=%0.3f\n", idx+1, norm);

    // Send to synth directly
    int instrID = getCurrentTF4InstrumentID();
    if (instrID >= 0)
    {
        ft2_synth_set_param(instrID, TF_LFO2_SHAPE, norm);
        printf("[LFO2SHAPE PARAM] idx=%d norm=%0.3f param=%d\n", idx, norm, TF_LFO2_SHAPE);
    } else {
        printf("[LFO2SHAPE WARN] No active instrument to send param.\n");
    }
    tf_request_redraw();
}

// Wrapper allowing assignment to onValueChange callbacks if needed
static void tf_lfo2shape_button_value_handler(TunefishWidget* w, float v){ tf_lfo2shape_button_handler(w); }

// Unisono button handler
static void tf_unisono_button_handler(TunefishWidget* w) {
    if (!w) return;
    extern TunefishCompleteLayout* g_active_tunefish_layout;
    TunefishCompleteLayout* layout = g_active_tunefish_layout;
    if (!layout) return;
    int idx = atoi(w->text) - 1; // button "1" is index 0
    if (idx < 0 || idx > 9) return;
    for (int i = 0; i < 10; i++)
        if (layout->page1.unisono_buttons[i])
            layout->page1.unisono_buttons[i]->pressed = (i == idx);

    // Update local shadow variable
    extern int synthUnisono; synthUnisono = idx + 1; // 1..10

    float norm = (float)idx / 9.0f;
    printf("[UNISONO HANDLER] button=%d norm=%0.3f\n", idx+1, norm);

    // Send to synth directly
    int instrID = getCurrentTF4InstrumentID();
    if (instrID >= 0)
    {
        ft2_synth_set_param(instrID, TF_GEN_UNISONO, norm);
        printf("[UNISONO PARAM] idx=%d norm=%0.3f param=%d\n", idx, norm, TF_GEN_UNISONO);
    } else {
        printf("[UNISONO WARN] No active instrument to send param.\n");
    }
    tf_request_redraw();
}

// Wrapper allowing assignment to onValueChange callbacks if needed
static void tf_unisono_button_value_handler(TunefishWidget* w, float v){ tf_unisono_button_handler(w); }

// Octave button handler
static void tf_octave_button_handler(TunefishWidget* w) {
    if (!w) return;
    extern TunefishCompleteLayout* g_active_tunefish_layout;
    TunefishCompleteLayout* layout = g_active_tunefish_layout;
    if (!layout) return;
    int octave = atoi(w->text); // -4..+4
    int idx = octave + 4;       // 0..8
    if (idx < 0 || idx > 8) return;
    for (int i = 0; i < 9; i++)
        if (layout->page1.octave_buttons[i])
            layout->page1.octave_buttons[i]->pressed = (i == idx);

    extern int synthOctave; synthOctave = octave;

    int instrID = getCurrentTF4InstrumentID();
    if (instrID >= 0)
    {
        /* Tunefish expects octave index 8 => -4  and 0 => +4 (reverse order).
         * Therefore invert the index before normalising. */
        float norm = (float)(8 - idx) / 8.0f; // 0..1, where idx=0 (-4) → 1.0, idx=8 (+4) → 0.0
        ft2_synth_set_param(instrID, 8, norm); // TF_GEN_OCTAVE = 8
    }
    tf_request_redraw();
}

// Formant button handler (radio behaviour handled externally)
static void tf_formant_button_handler(TunefishWidget* w) {
    if (!w) return;

    // Get the active layout to access the full button array
    extern TunefishCompleteLayout* g_active_tunefish_layout;
    TunefishCompleteLayout* layout = g_active_tunefish_layout;
    if (!layout) return;

    int idx = atoi(&w->name[8]); // Get index from "formant_X"
    if (idx < 0 || idx > 4) return;

    // Enforce radio button behavior: Set this button to pressed
    // and all others in the group to not pressed.
    for (int i = 0; i < 5; i++) {
        if (layout->page2.formant_type_buttons[i]) {
            layout->page2.formant_type_buttons[i]->pressed = (i == idx);
        }
    }

    int instrID = getCurrentTF4InstrumentID();
    if (instrID >= 0) {
        float norm = (float)idx / 4.0f;
        ft2_synth_set_param(instrID, 99, norm); // TF_FORMANT_MODE
    }

    // Update the shadow variable and request a redraw
    extern int synthFormant;
    synthFormant = idx;
    tf_request_redraw();
}


static void create_page1_widgets(TunefishCompleteLayout* l, int* allIdx, int* p1Idx) {
    if (!l) return;

    const int knobR = 18;
    const int xMargin = 8;
    int y = 38; // initial Y offset after globals

    // ----------------------------------------------------
    // 1. Global controls (Poly / Pitch)
    // ----------------------------------------------------
    //    l->page1.global_group = tf_create_group_box("global_group", "Global", xMargin, y, 160, 36);
    //    l->page1.poly_control       = tf_create_parameter_control("poly", "Poly", xMargin + 10, y + 18, 40, 11);
    //    l->page1.pitch_up_control   = tf_create_parameter_control("pitch_up", "Up",   xMargin + 60, y + 18, 40, 11);
    //    l->page1.pitch_down_control = tf_create_parameter_control("pitch_down", "Down", xMargin + 110, y + 18, 40, 11);

    //    TunefishWidget* globalSet[] = { l->page1.global_group, l->page1.poly_control, l->page1.pitch_up_control, l->page1.pitch_down_control };
    //    for (size_t i=0;i<sizeof(globalSet)/sizeof(globalSet[0]);++i){ if(!globalSet[i]) continue; globalSet[i]->page = PAGE1; l->page1_widgets[(*p1Idx)++] = globalSet[i]; l->all_widgets[(*allIdx)++] = globalSet[i]; }

    // ----------------------------------------------------
    // 2. Generator Row (12 knobs)
    // ----------------------------------------------------
    y -= 2; // move below global group
    const int genGroupH = 195; // increased to fit unisono + octave rows
    l->page1.gen_group = tf_create_group_box("gen_group", "Generator", xMargin, y, TF_LAYOUT_WIDTH - xMargin*2, genGroupH);

    const char* genNames[12]  = { "gen_volume", "gen_panning", "gen_detune", "gen_bandwidth", "gen_damp", "gen_harmonics", "gen_drive", "gen_scale", "gen_modulation", "gen_noise", "gen_noise_freq", "gen_noise_bw" };
    const char* genLabels[12] = { "Vol", "Pan", "Det", "BW", "Dmp", "Hrm", "Drv", "Scl", "Mod", "Nse", "NFq", "NBw" };
    TunefishWidget** genPtrArr[12] = {
        &l->page1.gen_volume_knob,&l->page1.gen_panning_knob,&l->page1.gen_detune_knob,&l->page1.gen_bandwidth_knob,&l->page1.gen_damp_knob,&l->page1.gen_harmonics_knob,
        &l->page1.gen_drive_knob,&l->page1.gen_scale_knob,&l->page1.gen_modulation_knob,&l->page1.gen_noise_knob,&l->page1.gen_noise_freq_knob,&l->page1.gen_noise_bw_knob };

    int knobSpacing = 52;
    int gxStart = xMargin + 22; // nudge
    for (int i=0;i<12;i++) {
        int kx = gxStart + i*knobSpacing;
        *(genPtrArr[i]) = tf_create_knob_with_label(genNames[i], genLabels[i], kx, y + 28, knobR); // nudged
    }

    // collect generator widgets
    TunefishWidget* genSet[13];
    genSet[0] = l->page1.gen_group;
    for (int i=0;i<12;i++) genSet[i+1] = *(genPtrArr[i]);
    for (int i=0;i<13;i++){ if(!genSet[i]) continue; genSet[i]->page = PAGE1; l->page1_widgets[(*p1Idx)++] = genSet[i]; l->all_widgets[(*allIdx)++] = genSet[i]; }

    // ----------------------------------------------------
    // 3. Filter Row (4 filters)
    // ----------------------------------------------------
    y += genGroupH + 8; // 84 + 8 = 92
    const int filterGroupW = 155;
    for (int i=0;i<4;i++) {
        int fx = xMargin + i*filterGroupW;
        const char* grpTitles[4] = { "Filter LP", "Filter HP", "Filter BP", "Filter NT" };
        char grpName[32]; snprintf(grpName,sizeof(grpName),"filter%d_group", i+1);
        TunefishWidget* grp = tf_create_group_box(grpName, grpTitles[i], fx, y, filterGroupW-4, 60);

        char cName[32]; snprintf(cName,sizeof(cName),"filter%d_cutoff", i+1);
        char rName[32]; snprintf(rName,sizeof(rName),"filter%d_res", i+1);
        char tName[32]; snprintf(tName,sizeof(tName),"filter%d_type", i+1);
        char oName[32]; snprintf(oName,sizeof(oName),"filter%d_on", i+1);

        int cutX = fx + 22;
        int resX = fx + 64;
        int ctrlY = y + 28;
        int labelX = fx + 22;
        int labelY = y + 10;
        int toggleX = fx + 100;

        TunefishWidget* cut = tf_create_knob_with_label(cName, "Cut", cutX, ctrlY, knobR);
        TunefishWidget* res = tf_create_knob_with_label(rName, "Res", resX, ctrlY, knobR);
        TunefishWidget* typ = NULL;
        TunefishWidget* onT = tf_create_toggle_button(oName, "On", toggleX, ctrlY, 36, 14);

        // store into struct (first filter keeps legacy names)
        switch(i) {
            case 0: l->page1.filter_group = grp; l->page1.filter_cutoff_knob = cut; l->page1.filter_resonance_knob = res; l->page1.filter_type_combo = typ; l->page1.filter1_on_toggle = onT; break;
            case 1: l->page1.filter2_group = grp; l->page1.filter2_cutoff_knob = cut; l->page1.filter2_resonance_knob = res; l->page1.filter2_type_label = typ; l->page1.filter2_on_toggle = onT; break;
            case 2: l->page1.filter3_group = grp; l->page1.filter3_cutoff_knob = cut; l->page1.filter3_resonance_knob = res; l->page1.filter3_type_label = typ; l->page1.filter3_on_toggle = onT; break;
            case 3: l->page1.filter4_group = grp; l->page1.filter4_cutoff_knob = cut; l->page1.filter4_resonance_knob = res; l->page1.filter4_type_label = typ; l->page1.filter4_on_toggle = onT; break;
        }

        TunefishWidget* filtSet[] = { grp, cut, res, onT };
        for (size_t j=0;j<sizeof(filtSet)/sizeof(filtSet[0]); ++j) {
            if(!filtSet[j]) continue; filtSet[j]->page = PAGE1; l->page1_widgets[(*p1Idx)++] = filtSet[j]; l->all_widgets[(*allIdx)++] = filtSet[j];
        }
    }

    // ----------------------------------------------------
    // 4. LFO1, LFO2, ADSR1, ADSR2 row
    // ----------------------------------------------------
    y += 68;
    const int colW = (TF_LAYOUT_WIDTH - xMargin*2) / 4;

    // LFO1
    int colX = xMargin;
    l->page1.lfo1_group = tf_create_group_box("lfo1_group", "LFO1", colX, y, colW-3, 84);
    l->page1.lfo1_freq_knob   = tf_create_knob_with_label("lfo1_freq", "Rate",  colX + 22, y + 28, knobR);
    l->page1.lfo1_depth_knob  = tf_create_knob_with_label("lfo1_amp",  "Depth", colX + 64, y + 28, knobR);
    l->page1.lfo1_sync_toggle = tf_create_toggle_button("lfo1_sync", "Sync", colX + 90, y + 28, 40, 14);
    // Add LFO1 shape pushbuttons
    int lfo1Shape_x = colX + 10;
    int lfo1Shape_y = y + 62;
    const char* lfo1ShapeLabels[5] = {"", "", "", "", ""}; // Empty labels since we draw waveforms
    for (int i = 0; i < 5; i++) {
       int shape = i + 1;
       char name[16];
      snprintf(name, sizeof(name), "lfo1_shape_%d", shape);
      l->page1.lfo1_shape_buttons[i] = tf_create_pushbutton(name, lfo1ShapeLabels[i], (lfo1Shape_x + i*18), lfo1Shape_y, 18, 14, lfo1ShapeLabels[i]);
      l->page1.lfo1_shape_buttons[i]->onValueChange = tf_lfo1shape_button_value_handler;
      l->page1.lfo1_shape_buttons[i]->page = PAGE1;
      l->page1_widgets[(*p1Idx)++] = l->page1.lfo1_shape_buttons[i];
      l->all_widgets[(*allIdx)++] = l->page1.lfo1_shape_buttons[i];
    }
    TunefishWidget* lfo1Set[] = { l->page1.lfo1_group, l->page1.lfo1_freq_knob, l->page1.lfo1_depth_knob, l->page1.lfo1_sync_toggle };
    for (size_t i=0;i<sizeof(lfo1Set)/sizeof(lfo1Set[0]); ++i){ if(!lfo1Set[i]) continue; lfo1Set[i]->page = PAGE1; l->page1_widgets[(*p1Idx)++] = lfo1Set[i]; l->all_widgets[(*allIdx)++] = lfo1Set[i]; }

    // LFO2
    colX += colW + 1;
    l->page1.lfo2_group = tf_create_group_box("lfo2_group", "LFO2", colX, y, colW-3, 84);
    l->page1.lfo2_freq_knob   = tf_create_knob_with_label("lfo2_freq", "Rate",  colX + 22, y + 28, knobR);
    l->page1.lfo2_depth_knob  = tf_create_knob_with_label("lfo2_amp",  "Depth", colX + 64, y + 28, knobR);
    l->page1.lfo2_sync_toggle = tf_create_toggle_button("lfo2_sync", "Sync", colX + 90, y + 28, 40, 14);
    // Add LFO2 shape pushbuttons
    int lfo2Shape_x = colX + 10;
    int lfo2Shape_y = y + 62;
    const char* lfo2ShapeLabels[5] = {"", "", "", "", ""}; // Empty labels since we draw waveforms
    for (int i = 0; i < 5; i++) {
       int shape = i + 1;
       char name[16];
      snprintf(name, sizeof(name), "lfo2_shape_%d", shape);
      l->page1.lfo2_shape_buttons[i] = tf_create_pushbutton(name, lfo2ShapeLabels[i], (lfo2Shape_x + i*18), lfo2Shape_y, 18, 14, lfo2ShapeLabels[i]);
      l->page1.lfo2_shape_buttons[i]->onValueChange = tf_lfo2shape_button_value_handler;
      l->page1.lfo2_shape_buttons[i]->page = PAGE1;
      l->page1_widgets[(*p1Idx)++] = l->page1.lfo2_shape_buttons[i];
      l->all_widgets[(*allIdx)++] = l->page1.lfo2_shape_buttons[i];
    }
    TunefishWidget* lfo2Set[] = { l->page1.lfo2_group, l->page1.lfo2_freq_knob, l->page1.lfo2_depth_knob, l->page1.lfo2_sync_toggle };
    for (size_t i=0;i<sizeof(lfo2Set)/sizeof(lfo2Set[0]); ++i){ if(!lfo2Set[i]) continue; lfo2Set[i]->page = PAGE1; l->page1_widgets[(*p1Idx)++] = lfo2Set[i]; l->all_widgets[(*allIdx)++] = lfo2Set[i]; }

    // ADSR1
    colX += colW + 1;
    l->page1.adsr1_group = tf_create_group_box("adsr1_group", "ADSR1", colX, y, colW-3, 84);
    l->page1.adsr1_slope_slider   = tf_create_linear_slider("adsr1_slope",  colX + 114, y + 18, 20, 52, true);
    l->page1.adsr1_env_view = tf_create_waveform_view("adsr1_env", colX + 8, y + 18, 118, 46);
    TunefishWidget* a1Set[] = { l->page1.adsr1_group, l->page1.adsr1_env_view, l->page1.adsr1_slope_slider };
    for (size_t i=0;i<sizeof(a1Set)/sizeof(a1Set[0]); ++i){ if(!a1Set[i]) continue; a1Set[i]->page = PAGE1; l->page1_widgets[(*p1Idx)++] = a1Set[i]; l->all_widgets[(*allIdx)++] = a1Set[i]; }

    // ADSR2
    colX += colW + 1;
    l->page1.adsr2_group = tf_create_group_box("adsr2_group", "ADSR2", colX, y, colW-3, 84);
    l->page1.adsr2_slope_slider   = tf_create_linear_slider("adsr2_slope",  colX + 114, y + 18, 20, 52, true);
    l->page1.adsr2_env_view = tf_create_waveform_view("adsr2_env", colX + 8, y + 18, 118, 46);
    TunefishWidget* a2Set[] = { l->page1.adsr2_group, l->page1.adsr2_env_view, l->page1.adsr2_slope_slider };
    for (size_t i=0;i<sizeof(a2Set)/sizeof(a2Set[0]); ++i){ if(!a2Set[i]) continue; a2Set[i]->page = PAGE1; l->page1_widgets[(*p1Idx)++] = a2Set[i]; l->all_widgets[(*allIdx)++] = a2Set[i]; }

    // After generator knobs, add Unisono pushbuttons
    int unisono_y = l->page1.gen_group->y + 72; // 40px below group top
    int unisono_x = xMargin + 10; // align with generator group
    for (int i = 0; i < 10; i++) {
        char name[16], label[4];
        snprintf(name, sizeof(name), "unisono_%d", i+1);
        snprintf(label, sizeof(label), "%d", i+1);
        l->page1.unisono_buttons[i] = tf_create_pushbutton(name, label, (unisono_x + i*18), unisono_y, 18, 14, label);
        l->page1.unisono_buttons[i]->onClick = tf_unisono_button_handler;
        l->page1.unisono_buttons[i]->page = PAGE1;
        l->page1_widgets[(*p1Idx)++] = l->page1.unisono_buttons[i];
        l->all_widgets[(*allIdx)++] = l->page1.unisono_buttons[i];
    }
    // Add Octave pushbuttons (-4 to +4) underneath unisono
    int octave_y = unisono_y + 22;
    int octave_x = unisono_x;
    for (int i = 0; i < 9; i++) {
        int octave = i - 4;
        char name[16], label[5];
        snprintf(name, sizeof(name), "octave_%d", octave);
        snprintf(label, sizeof(label), "%d", octave);
        l->page1.octave_buttons[i] = tf_create_pushbutton(name, label, (octave_x + i*18), octave_y, 18, 14, label);
        l->page1.octave_buttons[i]->onClick = tf_octave_button_handler;
        l->page1.octave_buttons[i]->page = PAGE1;
        l->page1_widgets[(*p1Idx)++] = l->page1.octave_buttons[i];
        l->all_widgets[(*allIdx)++] = l->page1.octave_buttons[i];
    }

    // Polyphony + Glide parameter controls under the generator area
    const int param_y = l->page1.gen_group->y + 104;
    const int poly_x = xMargin + 10;
    l->page1.poly_control = tf_create_parameter_control("poly", "Poly", poly_x, param_y, 40, 11);
    l->page1.gen_glide_control = tf_create_parameter_control("gen_glide", "Glide", poly_x + 52, param_y, 40, 11);
    TunefishWidget* paramSet[] = { l->page1.poly_control, l->page1.gen_glide_control };
    for (size_t i = 0; i < sizeof(paramSet)/sizeof(paramSet[0]); i++) {
        if (!paramSet[i]) continue;
        paramSet[i]->page = PAGE1;
        l->page1_widgets[(*p1Idx)++] = paramSet[i];
        l->all_widgets[(*allIdx)++] = paramSet[i];
    }
    
    // Add waveform view widget at the end (highest Z-order) - positioned within generator group area
    l->waveform_view = tf_create_waveform_view("waveform_view", 210, 100, 400, 120);
    printf("🌊 [LAYOUT] Created waveform view widget: %p\n", l->waveform_view);
    l->waveform_view->page = PAGE1;
    l->page1_widgets[(*p1Idx)++] = l->waveform_view;
    l->all_widgets[(*allIdx)++] = l->waveform_view;
}

static void create_page2_widgets(TunefishCompleteLayout* l, int* allIdx, int* p2Idx) {
    if (!l) return;

    // Retrieve dynamic enumeration names from synth engine
    const char* const* modSrcItems = ft2_synth_get_mod_source_items();
    const int modSrcItemCount = ft2_synth_get_mod_source_count();
    const char* const* modDstItems = ft2_synth_get_mod_dest_items();
    const int modDstItemCount = ft2_synth_get_mod_dest_count();

    const int startX = 8;
    const int startY = 36;
    const int groupW = 155;
    const int groupH = 60;
    const int knobR  = 18; // Match generator knob size

    // --- FX GRID: 7 Effect groups (2x4 grid, last slot spare) ---
    for (int i=0;i<7;i++) {
        int col = i % 4;
        int row = i / 4;
        int gx = startX + col*groupW;
        int gy = startY + row*(groupH+10);

        char grpName[32]; snprintf(grpName,sizeof(grpName),"fxgrp_%d", i);
        static const char* fxTitles[7] = { "Flanger", "Reverb", "Delay", "EQ", "Chorus", "Formant", "Distortion" };
        const char* title = fxTitles[i];
        TunefishWidget* grp = tf_create_group_box(grpName, title, gx, gy, groupW-4, groupH);

        // Build parameter knob set per FX type
        TunefishWidget* wet=NULL; TunefishWidget* p1=NULL; TunefishWidget* p2=NULL; TunefishWidget* p3=NULL;
        int knobX = gx+20;
        #define MAKE_KNOB(name,label) tf_create_knob_with_label(name,label, knobX, gy+28, knobR); knobX += 37;

        switch(i){
            case 0: // Flanger
                wet = MAKE_KNOB("flanger_wet","Wet");
                p1  = MAKE_KNOB("flanger_lfo","LFO");
                p2  = MAKE_KNOB("flanger_freq","Frq");
                p3  = MAKE_KNOB("flanger_amp","Amp");
                break;
            case 1: // Reverb
                p1 = MAKE_KNOB("reverb_room_sz","Size");
                p2 = MAKE_KNOB("reverb_damp","Dmp");
                p3 = MAKE_KNOB("reverb_width","Wid");
                wet= MAKE_KNOB("reverb_wet","Wet");
                break;
            case 2: // Delay
                p1 = MAKE_KNOB("delay_left","Left");
                p2 = MAKE_KNOB("delay_right","Right");
                p3 = MAKE_KNOB("delay_decay","Dcy");
                break;
            case 3: // EQ
                p1 = MAKE_KNOB("eq_bass","Bass");
                p2 = MAKE_KNOB("eq_mid","Mid");
                p3 = MAKE_KNOB("eq_treble","Hi");
                break;
            case 4: // Chorus
                wet= MAKE_KNOB("chorus_gain","Wet");
                p1 = MAKE_KNOB("chorus_freq","Freq");
                p2 = MAKE_KNOB("chorus_depth","Dpth");
                break;
            case 5: // Formant
                wet = MAKE_KNOB("formant_wet","Wet");
                break;
            case 6: // Distortion
                wet = MAKE_KNOB("distortion_amount","Amt");
                break;
        }

        l->page2.fx_groups[i]=grp;
        l->page2.fx_wet_knobs[i]=wet;
        l->page2.fx_param1_knobs[i]=p1;
        l->page2.fx_param2_knobs[i]=p2;
        l->page2.fx_param3_knobs[i]=p3;

        TunefishWidget* arr[]={grp,wet,p1,p2,p3};
        for(size_t k=0;k<sizeof(arr)/sizeof(arr[0]); ++k){ if(!arr[k]) continue; arr[k]->page = PAGE2; l->page2_widgets[(*p2Idx)++] = arr[k]; l->all_widgets[(*allIdx)++] = arr[k]; }

        if(i==5) {
            static const char* vowels[5] = { "A","E","I","O","U" };
            int rbX = gx + 44;
            int rbY = gy + 24;
            for(int v=0; v<5; v++) {
                char btnN[32]; snprintf(btnN,sizeof(btnN),"formant_%d",v);
                l->page2.formant_type_buttons[v] = tf_create_pushbutton(btnN, vowels[v], rbX + v*20, rbY, 18, 12, vowels[v]);
                l->page2.formant_type_buttons[v]->onClick = tf_formant_button_handler;
                TunefishWidget* b = l->page2.formant_type_buttons[v];
                /* pushbutton inherits global styling */
            }
            l->page2.formant_amount_knob = wet; // reuse wet knob pointer
        }
    }

    int y = startY + 2*(groupH+10); // Pos for next rows (~ after grid)

    // Modulation matrix (2 rows ×4 per row => 8 slots)
    l->page2.modulation_group = tf_create_group_box("mod_group", "Mod Matrix", 8, y - 2, 616, 120);
    for (int i=0;i<8;i++) {
        int col = i % 4;
        int row = i / 4;
        int mx = 22 + col*150; // start x inside group (tighter spacing)
        int my = y + 18 + row*50;
        char n[32];
        snprintf(n,sizeof(n),"mod_src_%d",i);
        l->page2.mod_source_combos[i] = tf_create_combo_box(n, mx, my, 110,20, modSrcItems, modSrcItemCount);
        snprintf(n,sizeof(n),"mod_dst_%d",i);
        l->page2.mod_dest_combos[i] = tf_create_combo_box(n, mx, my+24, 110,20, modDstItems, modDstItemCount);
        snprintf(n,sizeof(n),"mod_amt_%d",i);
        l->page2.mod_amount_knobs[i] = tf_create_knob_with_label(n, "Amt", mx+129, my+11, 18);
    }

    // Collect widgets in arrays
    TunefishWidget* fxSet[] = {
        l->page2.fx_groups[0], l->page2.fx_wet_knobs[0], l->page2.fx_param1_knobs[0], l->page2.fx_param2_knobs[0],
        l->page2.fx_groups[1], l->page2.fx_wet_knobs[1], l->page2.fx_param1_knobs[1], l->page2.fx_param2_knobs[1],
        l->page2.fx_groups[2], l->page2.fx_wet_knobs[2], l->page2.fx_param1_knobs[2], l->page2.fx_param2_knobs[2],
        l->page2.fx_groups[3], l->page2.fx_wet_knobs[3], l->page2.fx_param1_knobs[3], l->page2.fx_param2_knobs[3],
        l->page2.fx_groups[4], l->page2.fx_wet_knobs[4], l->page2.fx_param1_knobs[4], l->page2.fx_param2_knobs[4],
        l->page2.fx_groups[5], l->page2.fx_wet_knobs[5], l->page2.fx_param1_knobs[5], l->page2.fx_param2_knobs[5],
        l->page2.fx_groups[6], l->page2.fx_wet_knobs[6], l->page2.fx_param1_knobs[6], l->page2.fx_param2_knobs[6],
        l->page2.modulation_group
    };
    for(size_t i=0;i<sizeof(fxSet)/sizeof(fxSet[0]);++i){ if(!fxSet[i]) continue; fxSet[i]->page = PAGE2; l->page2_widgets[(*p2Idx)++] = fxSet[i]; l->all_widgets[(*allIdx)++] = fxSet[i]; }
    // Formant buttons
    for(int i=0;i<5;i++){ l->page2.formant_type_buttons[i]->page=PAGE2; l->page2_widgets[(*p2Idx)++] = l->page2.formant_type_buttons[i]; l->all_widgets[(*allIdx)++] = l->page2.formant_type_buttons[i]; }
    // Mod matrix
    for(int i=0;i<8;i++){
        TunefishWidget* s=l->page2.mod_source_combos[i]; TunefishWidget* d=l->page2.mod_dest_combos[i]; TunefishWidget* a=l->page2.mod_amount_knobs[i];
        s->page=d->page=a->page=PAGE2;
        l->page2_widgets[(*p2Idx)++] = s; l->page2_widgets[(*p2Idx)++] = d; l->page2_widgets[(*p2Idx)++] = a;
        l->all_widgets[(*allIdx)++] = s; l->all_widgets[(*allIdx)++] = d; l->all_widgets[(*allIdx)++] = a;
    }

    // Push remaining top-level widgets (formant group and amount knob)
    TunefishWidget* extraSet[] = { l->page2.modulation_group };
    for(size_t i=0;i<sizeof(extraSet)/sizeof(extraSet[0]); ++i){ if(!extraSet[i]) continue; extraSet[i]->page = PAGE2; l->page2_widgets[(*p2Idx)++] = extraSet[i]; l->all_widgets[(*allIdx)++] = extraSet[i]; }
    #undef MAKE_KNOB

    // ----------------------------------------------------
    // FX STACK GROUP (2 rows ×5 per row => 10 slots)
    // ----------------------------------------------------
    int stackY = y + 120 + 6; // small gap after mod matrix
    l->page2.fx_stack_group = tf_create_group_box("fx_stack_group", "FX Stack", 8, stackY, 616, 90);

    for (int i=0;i<10;i++) {
        int col = i % 5;
        int row = i / 5;
        int sx = 22 + col*120; // 5 columns fit in 570 (110*5 =550)
        int sy = stackY + 16 + row*40;
        char n[32];

        // Combo box
        snprintf(n,sizeof(n),"fxstk_sel_%d",i);
        l->page2.fx_stack_combos[i] = tf_create_combo_box(n, sx, sy, 107, 20, fxStackItems, fxStackItemCount);

        // Wet knob (placed where up/down buttons were)

    }

    // Collect FX stack widgets
    for(int i=0;i<10;i++){
        TunefishWidget* s = l->page2.fx_stack_combos[i];
        TunefishWidget* w = l->page2.fx_stack_wet_knobs[i];
        TunefishWidget* arr[] = { s, w };
        for(size_t k=0;k<2;k++){
            if(!arr[k]) continue;
            arr[k]->page = PAGE2;
            l->page2_widgets[(*p2Idx)++] = arr[k];
            l->all_widgets[(*allIdx)++] = arr[k];
        }
    }

    // Add the group box itself last
    l->page2.fx_stack_group->page = PAGE2;
    l->page2_widgets[(*p2Idx)++] = l->page2.fx_stack_group;
    l->all_widgets[(*allIdx)++] = l->page2.fx_stack_group;
}

// --- Preset Combo Helpers ---
static void tf_preset_combo_selected(TunefishWidget* widget, int selectedIndex) {
    if (!widget || !widget->comboItems || selectedIndex < 0) return;

    // Find the layout from the widget (assuming it's stored as user data or globally accessible)
    // For now, we'll need to pass the layout through the widget creation
    TunefishCompleteLayout* layout = (TunefishCompleteLayout*)widget->comboItems[widget->comboItemCount]; // Hack: store layout pointer at end
    if (!layout) return;

    printf("🎵 [PRESET] Selected preset %d: '%s'\n", selectedIndex, widget->comboItems[selectedIndex]);
    tf_load_preset_by_index(layout, selectedIndex);
}

static void tf_populate_preset_combo(TunefishCompleteLayout* layout) {
    if (!layout || !layout->preset_combo) return;
    int count = ft2_synth_get_preset_count();
    if (count <= 0) return;

    // Free any existing items first
    if (layout->preset_combo->comboItems) {
        for (int i = 0; i < layout->preset_combo->comboItemCount; i++) {
            free(layout->preset_combo->comboItems[i]);
        }
        free(layout->preset_combo->comboItems);
        layout->preset_combo->comboItems = NULL;
        layout->preset_combo->comboItemCount = 0;
    }

    // Allocate one extra slot to store layout pointer (hack)
    layout->preset_combo->comboItems = (char**)calloc(count + 1, sizeof(char*));
    layout->preset_combo->comboItemCount = count;
    for (int i = 0; i < count; i++) {
        const char* name = ft2_synth_get_preset_name(i);
        if (!name) name = "(Unknown)";
        size_t len = strlen(name)+1;
        layout->preset_combo->comboItems[i] = (char*)malloc(len);
        memcpy(layout->preset_combo->comboItems[i], name, len);
    }

    // Store layout pointer in extra slot (hack to pass context)
    layout->preset_combo->comboItems[count] = (char*)layout;

    // Set the combo select callback
    layout->preset_combo->onComboSelect = tf_preset_combo_selected;

    // Sync to current preset of instrument
    int instrID = getCurrentTF4InstrumentID();
    int curPreset = (instrID >= 0) ? ft2_synth_get_current_preset_for_instrument(instrID) : 0;
    if (curPreset < 0 || curPreset >= count) curPreset = 0;
    layout->preset_combo->selectedIndex = curPreset;
}

static void tf_load_preset_by_index(TunefishCompleteLayout* layout, int presetIndex) {
    if (!layout) return;
    int instrID = getCurrentTF4InstrumentID();
    if (instrID < 0) return;
    if (!ft2_synth_load_preset_for_instrument(instrID, presetIndex)) return;

    // Update all UI parameters to reflect new preset values
    tf_update_all_widgets_from_synth(layout);

    // Update instrument text boxes on main FT2 gui
    extern void updateInstrumentTextBoxNames(void);
    updateInstrumentTextBoxNames();

    printf("🎵 [PRESET] Loaded factory preset %d into instrument %d\n", presetIndex, instrID);
}

// Find widget by name across all widgets arrays
TunefishWidget* tf_find_widget_by_name(TunefishCompleteLayout* l, const char* name)
{
    if (!l || !name) return NULL;
    for (int i = 0; i < TF_TOTAL_WIDGETS; i++) {
        TunefishWidget* w = l->all_widgets[i];
        if (w && strcmp(w->name, name) == 0) return w;
    }
    return NULL;
}

// Forward declaration from ft2_synth.c (not in header)
extern void* getInstrumentInstance(int instrID);

// Constants mapping to Tunefish TF_PARAM enum indices (keep in sync with tf4.hpp)
#define TF_MM1_SOURCE   44
#define TF_MM1_MOD      45
#define TF_MM1_TARGET   46
#define TF_EFFECT_1     74
#define TF_DISTORT_AMOUNT 84
#define TF_DELAY_DECAY   89
#define TF_REVERB_WET    92
#define TF_FLANGER_WET   97
#define TF_CHORUS_GAIN   98
#define TF_FORMANT_WET   100
#define TF_MM8_SOURCE   65
#define TF_MM8_TARGET   67

// Refresh shadow parameter arrays (mmSrc/mmDst/mmAmt/fxSel/fxWet) from current instrument state
static void tf_refresh_shadow_parameters_from_synth(void)
{
    int instrID = getCurrentTF4InstrumentID();
    if (instrID < 0) return;

    void* instr = getInstrumentInstance(instrID);
    if (!instr) return;

    // --------------------------------------------------
    // 1) Generic bindings – loop over the binding table
    // --------------------------------------------------
    for (int i = 0; i < g_paramBindingCount; i++) {
        TunefishParameterBinding* b = &g_paramBindings[i];
        if (b->paramId < 0) continue;        // placeholder / wet knobs etc.

        float norm = tf_instrument_get_param(instr, b->paramId);
        int intVal = (int)lroundf(norm * (b->maxValue - b->minValue) + b->minValue);
        if (intVal < b->minValue) intVal = b->minValue;
        if (intVal > b->maxValue) intVal = b->maxValue;
        *(b->valuePtr) = intVal;
    }

    // --------------------------------------------------
    // 2) Specialised arrays (Mod Matrix & FX Stack)
    // --------------------------------------------------

    for (int i = 0; i < 8; i++) {
        int base = TF_MM1_SOURCE + i * 3; // SOURCE, MOD, TARGET consecutive

        float srcNorm = tf_instrument_get_param(instr, base);
        float modNorm = tf_instrument_get_param(instr, base + 1);
        float dstNorm = tf_instrument_get_param(instr, base + 2);

        /* Convert normalized [0..1] back to enumeration values, then map to UI subset */
        int srcEnum = (int)lroundf(srcNorm * (MOD_SRC_ENUM_COUNT - 1));
        if (srcEnum < 0 || srcEnum >= MOD_SRC_ENUM_COUNT) srcEnum = 0; // clamp to visible range
        mmSrc[i] = srcEnum;

        int dstEnum = (int)lroundf(dstNorm * (MOD_DST_ENUM_COUNT - 1));
        if (dstEnum < 0 || dstEnum >= MOD_DST_ENUM_COUNT) dstEnum = 0;
        mmDst[i] = dstEnum;

        mmAmt[i] = (int)lroundf(modNorm * 127.0f);
        if (mmAmt[i] < 0) mmAmt[i] = 0; if (mmAmt[i] > 127) mmAmt[i] = 127;
    }

    // FX Stack selections (10 slots) -------------------
    for (int i = 0; i < 10; i++) {
        float selNorm = tf_instrument_get_param(instr, TF_EFFECT_1 + i);
        // TF_MAXEFFECTS in original code is 10 – indices sent as idx/10
        const int TF_MAXEFFECTS = 10;
        int idx = (int)lroundf(selNorm * (float)TF_MAXEFFECTS);
        if (idx < 0) idx = 0;
        if (idx >= FX_STACK_COUNT) idx = FX_STACK_COUNT - 1;
        fxSel[i] = idx;

        // Derive per-slot wet levels from the global FX parameters
        int wetParamId = -1;
        switch (idx) {
            case 1: /* Distortion */ wetParamId = TF_DISTORT_AMOUNT; break;
            case 2: /* Delay      */ wetParamId = TF_DELAY_DECAY;   break; // delay has no dedicated wet – use decay as proxy
            case 3: /* Chorus     */ wetParamId = TF_CHORUS_GAIN;   break;
            case 4: /* Flanger    */ wetParamId = TF_FLANGER_WET;   break;
            case 5: /* Reverb     */ wetParamId = TF_REVERB_WET;    break;
            case 6: /* Formant    */ wetParamId = TF_FORMANT_WET;   break;
            default: wetParamId = -1; break; // none / EQ / unknown
        }

        if (wetParamId >= 0) {
            float wetNorm = tf_instrument_get_param(instr, wetParamId);
            int wetVal = (int)lroundf(wetNorm * 127.0f);
            if (wetVal < 0) wetVal = 0; if (wetVal > 127) wetVal = 127;
            fxWet[i] = wetVal;
        } else {
            fxWet[i] = 0;
        }
    }

    // --------------------------------------------------
    // 3) Boolean helpers (sync int<->bool)
    // --------------------------------------------------
    tf_apply_bool_parameters();

    // --------------------------------------------------
    // 4) Unisono and Octave (radio-button rows)
    // --------------------------------------------------
    {
        extern int synthFormant;
        /* UNISONO (param 14) is stored as idx/9  – where idx 0..9 corresponds to 1..10 voices. */
        float unisonoNorm = tf_instrument_get_param(instr, TF_GEN_UNISONO);
        int uIdx = (int)lroundf(unisonoNorm * 9.0f); // 0..9
        if (uIdx < 0) uIdx = 0; if (uIdx > 9) uIdx = 9;
        synthUnisono = uIdx + 1; // store 1..10

        /* OCTAVE (param 8) is stored in reverse order: norm 0 => +4  and 1 => -4.
         * Derive index 0..8 then convert to signed octave shift (-4..+4). */
        float octaveNorm = tf_instrument_get_param(instr, 8 /* TF_GEN_OCTAVE */);
        int oIdx = (int)lroundf(octaveNorm * 8.0f); // 0..8 (0 lowest (+4) logically)
        if (oIdx < 0) oIdx = 0; if (oIdx > 8) oIdx = 8;
        int realIdx = 8 - oIdx;          // invert mapping back to UI order (-4..+4)
        synthOctave = realIdx - 4;       // store -4..+4

        /* FORMANT MODE (param 99): index 0..4 for vowels A,E,I,O,U */
        float formantNorm = tf_instrument_get_param(instr, 99 /* TF_FORMANT_MODE */);
        int fIdx = (int)lroundf(formantNorm * 4.0f); // 0..4
        if (fIdx < 0) fIdx = 0; if (fIdx > 4) fIdx = 4;
        synthFormant = fIdx;
    }
}

// Live meter extern for output peak
extern volatile float g_audioOutPeak;

// Wrapper to query active voice count from TF instrument
extern int tf_instrument_get_active_voice_count(void* instrument);

// Update CPU/output meters just before rendering
static void tf_update_live_meters(TunefishCompleteLayout* layout)
{
    if (!layout) return;

    if (layout->main_level_meter)
    {
        layout->main_level_meter->value = CLAMP(g_audioOutPeak, 0.0f, 1.0f);
        layout->main_level_meter->peakLevel = layout->main_level_meter->value;
    }

    if (layout->cpu_meter)
    {
        int instrID = getCurrentTF4InstrumentID();
        int voices = 0;
        if (instrID >= 0)
        {
            void* instr = getInstrumentInstance(instrID);
            if (instr)
                voices = tf_instrument_get_active_voice_count(instr);
        }
        if (voices < 0) voices = 0;
        if (voices > 16) voices = 16;
        layout->cpu_meter->value = (float)voices / 16.0f;
    }
}

// --- ADSR mini envelope rendering + interaction (FT2-style) ---
static void tf_adsr_env_pixel(int x, int y, uint8_t pal)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    video.frameBuffer[(y * SCREEN_W) + x] = video.palette[pal];
}

typedef struct {
    int x;
    int y;
} tf_env_point_t;

static void tf_adsr_env_get_points(int envIndex, const TunefishWidget* w, tf_env_point_t points[4])
{
    const int pad = 4;
    int left = w->x + pad;
    int top = w->y + pad;
    int right = w->x + w->w - pad - 1;
    int bottom = w->y + w->h - pad - 1;
    int width = right - left;
    int height = bottom - top;
    if (width < 4 || height < 4) {
        for (int i = 0; i < 4; i++) { points[i].x = left; points[i].y = bottom; }
        return;
    }

    const float WA = 0.35f;
    const float WD = 0.25f;
    const float WR = 0.35f;

    int a = (envIndex == 0) ? adsr1A : adsr2A;
    int d = (envIndex == 0) ? adsr1D : adsr2D;
    int s = (envIndex == 0) ? adsr1S : adsr2S;
    int r = (envIndex == 0) ? adsr1R : adsr2R;

    float aN = (float)a / 127.0f;
    float dN = (float)d / 127.0f;
    float sN = (float)s / 127.0f;
    float rN = (float)r / 127.0f;

    int x0 = left + (int)lroundf(aN * WA * (float)width);
    int x1 = x0 + (int)lroundf(dN * WD * (float)width);
    int x2 = right - (int)lroundf(rN * WR * (float)width);

    const int minSpacing = 4;
    if (x0 < left) x0 = left;
    if (x1 < x0 + minSpacing) x1 = x0 + minSpacing;
    if (x2 < x1 + minSpacing) x2 = x1 + minSpacing;
    if (x2 > right - minSpacing) x2 = right - minSpacing;

    int yS = top + (int)lroundf((1.0f - sN) * (float)height);
    if (yS < top) yS = top;
    if (yS > bottom) yS = bottom;

    points[0].x = x0;     points[0].y = top;
    points[1].x = x1;     points[1].y = yS;
    points[2].x = x2;     points[2].y = yS;
    points[3].x = right;  points[3].y = bottom;
}

static void tf_draw_adsr_env(const TunefishWidget* w, int envIndex)
{
    if (!w || !w->visible) return;

    const int pad = 4;
    int left = w->x + pad;
    int top = w->y + pad;
    int right = w->x + w->w - pad - 1;
    int bottom = w->y + w->h - pad - 1;
    int width = right - left;
    int height = bottom - top;
    if (width <= 0 || height <= 0) return;

    fillRect(w->x + 1, w->y + 1, w->w - 2, w->h - 2, PAL_BUTTON2);
    hLine(w->x, w->y, w->w, PAL_BCKGRND);
    hLine(w->x, w->y + w->h - 1, w->w, PAL_BCKGRND);
    vLine(w->x, w->y, w->h, PAL_BCKGRND);
    vLine(w->x + w->w - 1, w->y, w->h, PAL_BCKGRND);

    tf_env_point_t pts[4];
    tf_adsr_env_get_points(envIndex, w, pts);

    // Fill under curve
    int prevX = left;
    int prevY = bottom;
    for (int i = 0; i < 4; i++) {
        int x1 = pts[i].x;
        int y1 = pts[i].y;
        if (x1 < prevX) x1 = prevX;
        if (x1 == prevX) {
            int y = prevY;
            if (y1 < y) y = y1;
            if (y < top) y = top;
            if (y > bottom) y = bottom;
            fillRect(prevX, y, 1, bottom - y + 1, PAL_BLCKMRK);
        } else {
            for (int x = prevX; x <= x1; x++) {
                float t = (float)(x - prevX) / (float)(x1 - prevX);
                int y = prevY + (int)lroundf(t * (float)(y1 - prevY));
                if (y < top) y = top;
                if (y > bottom) y = bottom;
                fillRect(x, y, 1, bottom - y + 1, PAL_BLCKMRK);
            }
        }
        prevX = x1;
        prevY = y1;
    }

    // Dotted margins (FT2-style)
    for (int i = 0; i <= height / 2; i++) tf_adsr_env_pixel(left - 1, top + 1 + i * 2, PAL_PATTEXT);
    for (int i = 0; i <= height / 8; i++) tf_adsr_env_pixel(left - 2, top + 1 + i * 8, PAL_PATTEXT);
    for (int i = 0; i <= width / 2; i++) tf_adsr_env_pixel(left + 1 + i * 2, bottom + 1, PAL_PATTEXT);
    for (int i = 0; i <= width / 50; i++) tf_adsr_env_pixel(left + 1 + i * 50, bottom + 2, PAL_PATTEXT);

    // Envelope line
    int x0 = left;
    int y0 = bottom;
    for (int i = 0; i < 4; i++) {
        line((int16_t)x0, (int16_t)pts[i].x, (int16_t)y0, (int16_t)pts[i].y, PAL_FORGRND);
        x0 = pts[i].x;
        y0 = pts[i].y;
    }

    // Points + selection markers
    int sel = tf_env_selected_index[envIndex];
    for (int i = 0; i < 3; i++) { // last point fixed
        int px = pts[i].x;
        int py = pts[i].y;
        fillRect(px - 1, py - 1, 3, 3, PAL_BLCKTXT);
        if (i == sel) {
            line((int16_t)(px - 3), (int16_t)(px - 3), (int16_t)(py - 3), (int16_t)(py + 3), PAL_BLCKTXT);
            line((int16_t)(px + 3), (int16_t)(px + 3), (int16_t)(py - 3), (int16_t)(py + 3), PAL_BLCKTXT);
            tf_adsr_env_pixel(left - 1, py, PAL_BLCKTXT);
            tf_adsr_env_pixel(px, bottom + 1, PAL_BLCKTXT);
        }
    }
}

static void tf_draw_adsr_envs(TunefishCompleteLayout* layout)
{
    if (!layout) return;
    tf_draw_adsr_env(layout->page1.adsr1_env_view, 0);
    tf_draw_adsr_env(layout->page1.adsr2_env_view, 1);
}

static void tf_adsr_set_params(int envIndex, float aN, float dN, float sN, float rN)
{
    if (aN < 0.0f) aN = 0.0f; if (aN > 1.0f) aN = 1.0f;
    if (dN < 0.0f) dN = 0.0f; if (dN > 1.0f) dN = 1.0f;
    if (sN < 0.0f) sN = 0.0f; if (sN > 1.0f) sN = 1.0f;
    if (rN < 0.0f) rN = 0.0f; if (rN > 1.0f) rN = 1.0f;

    int aVal = (int)lroundf(aN * 127.0f);
    int dVal = (int)lroundf(dN * 127.0f);
    int sVal = (int)lroundf(sN * 127.0f);
    int rVal = (int)lroundf(rN * 127.0f);

    if (envIndex == 0) {
        adsr1A = aVal; adsr1D = dVal; adsr1S = sVal; adsr1R = rVal;
    } else {
        adsr2A = aVal; adsr2D = dVal; adsr2S = sVal; adsr2R = rVal;
    }

    int instrID = getCurrentTF4InstrumentID();
    if (instrID >= 0) {
        int base = (envIndex == 0) ? TF_ADSR1_ATTACK : TF_ADSR2_ATTACK;
        ft2_synth_set_param(instrID, base + 0, aN);
        ft2_synth_set_param(instrID, base + 1, dN);
        ft2_synth_set_param(instrID, base + 2, sN);
        ft2_synth_set_param(instrID, base + 3, rN);
    }
}

static bool tf_handle_adsr_env_mouse(TunefishCompleteLayout* layout, int mouseX, int mouseY, bool pressed)
{
    if (!layout) return false;
    TunefishWidget* envs[2] = { layout->page1.adsr1_env_view, layout->page1.adsr2_env_view };

    for (int envIndex = 0; envIndex < 2; envIndex++) {
        TunefishWidget* w = envs[envIndex];
        if (!w || !w->visible) continue;

        const int pad = 4;
        int left = w->x + pad;
        int top = w->y + pad;
        int right = w->x + w->w - pad - 1;
        int bottom = w->y + w->h - pad - 1;

        const bool inEnv = (mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom);

        if (pressed && !tf_env_dragging[envIndex] && inEnv) {
            tf_env_point_t pts[4];
            tf_adsr_env_get_points(envIndex, w, pts);
            tf_env_last_mouse_x[envIndex] = mouseX;
            tf_env_last_mouse_y[envIndex] = mouseY;

            bool hit = false;
            for (int i = 0; i < 3; i++) {
                int dx = mouseX - pts[i].x;
                int dy = mouseY - pts[i].y;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx <= 3 && dy <= 3) {
                    tf_env_dragging[envIndex] = true;
                    tf_env_drag_index[envIndex] = i;
                    tf_env_selected_index[envIndex] = i;
                    tf_env_save_mouse_x[envIndex] = left + (tf_env_last_mouse_x[envIndex] - pts[i].x);
                    tf_env_save_mouse_y[envIndex] = top + (tf_env_last_mouse_y[envIndex] - pts[i].y);
                    hit = true;
                    break;
                }
            }
            if (!hit && tf_env_selected_index[envIndex] >= 0 && tf_env_selected_index[envIndex] < 3) {
                int i = tf_env_selected_index[envIndex];
                tf_env_dragging[envIndex] = true;
                tf_env_drag_index[envIndex] = i;
                tf_env_save_mouse_x[envIndex] = left + (tf_env_last_mouse_x[envIndex] - pts[i].x);
                tf_env_save_mouse_y[envIndex] = top + (tf_env_last_mouse_y[envIndex] - pts[i].y);
            }
            return true;
        }

        if (tf_env_dragging[envIndex]) {
            if (pressed) {
                tf_env_point_t pts[4];
                tf_adsr_env_get_points(envIndex, w, pts);
                int idx = tf_env_drag_index[envIndex];

                const float WA = 0.35f;
                const float WD = 0.25f;
                const float WR = 0.35f;

                int width = right - left;
                int height = bottom - top;
                if (width < 4 || height < 4) return true;

                int minX = left;
                int maxX = right;
                if (idx > 0) minX = pts[idx - 1].x + 4;
                if (idx < 2) maxX = pts[idx + 1].x - 4;
                if (minX < left) minX = left;
                if (maxX > right) maxX = right;

                int clampedX = CLAMP(mouseX, minX, maxX);
                int clampedY = CLAMP(mouseY, top, bottom);

                float aN = (float)adsr1A / 127.0f;
                float dN = (float)adsr1D / 127.0f;
                float sN = (float)adsr1S / 127.0f;
                float rN = (float)adsr1R / 127.0f;
                if (envIndex == 1) { aN = (float)adsr2A / 127.0f; dN = (float)adsr2D / 127.0f; sN = (float)adsr2S / 127.0f; rN = (float)adsr2R / 127.0f; }

                if (idx == 0) {
                    aN = (float)(clampedX - left) / (WA * (float)width);
                } else if (idx == 1) {
                    dN = (float)(clampedX - pts[0].x) / (WD * (float)width);
                    sN = 1.0f - ((float)(clampedY - top) / (float)height);
                } else if (idx == 2) {
                    rN = (float)(right - clampedX) / (WR * (float)width);
                    sN = 1.0f - ((float)(clampedY - top) / (float)height);
                }

                tf_adsr_set_params(envIndex, aN, dN, sN, rN);
                return true;
            } else {
                tf_env_dragging[envIndex] = false;
                tf_env_drag_index[envIndex] = -1;
                return true;
            }
        }
    }

    return false;
}

bool tf_handle_layout_mouse_drag(TunefishCompleteLayout* layout, int mouseX, int mouseY)
{
    if (!layout || !layout->visible) return false;
    if (layout->current_page != 0) return false;

    for (int envIndex = 0; envIndex < 2; envIndex++) {
        if (tf_env_dragging[envIndex]) {
            tf_handle_adsr_env_mouse(layout, mouseX, mouseY, true);
            return true;
        }
    }

    return false;
}

// Global variable definitions for linker
int synthLFO1Shape = 0;
int synthLFO2Shape = 0;
int synthUnisono = 1;
int synthOctave = 0;
int adsr1Slope = 0;
int adsr2Slope = 0;
int synthFormant = 0; // 0=A..4=U current vowel
TunefishCompleteLayout* g_active_tunefish_layout = NULL;

// Forward declaration from ft2_synth.c (not in header)
extern float tf_instrument_get_param(void* instrument, int param);

// Filter ON state storage (0=off,1=on) used by binding table below
//#if 0
// static int synthFilterLPOn = 0;
// static int synthFilterHPOn = 0;
// static int synthFilterBPOn = 0;
// static int synthFilterNTOn = 0;
//#endif

// -----------------------------------------------------------------------------
// Helper: align preset combo selection with current TF4 instrument preset
// -----------------------------------------------------------------------------
static void tf_sync_preset_combo_with_instrument(TunefishCompleteLayout* layout)
{
    if (!layout || !layout->preset_combo) return;

    int instrID = getCurrentTF4InstrumentID();
    if (instrID < 0) return;

    int curPreset = ft2_synth_get_current_preset_for_instrument(instrID);
    if (curPreset < 0) curPreset = 0;
    if (curPreset >= layout->preset_combo->comboItemCount) curPreset = 0;

    layout->preset_combo->selectedIndex = curPreset;
}

// =============================================================================
// MISSING FUNCTION IMPLEMENTATIONS (GUISAN COMPATIBILITY STUBS)
// =============================================================================

// Legacy compatibility functions
void ui_sync_from_instrument(void) {
    // Stub implementation - UI sync from instrument
    // This was used to sync UI elements when instrument parameters changed
    printf("[UI] Sync from instrument (stub)\n");
}

bool ft2_guisan_is_enabled(void) {
    // Stub implementation - always return false since Guisan is disabled
    return false;
}

bool ft2_guisan_is_synth_editor_shown(void) {
    // Stub implementation - always return false since Guisan is disabled
    return false;
}

void ft2_guisan_hide_synth_editor(void) {
    // Stub implementation - hide synth editor
    printf("[GUI] Hide synth editor (stub)\n");
}

// Missing external variable definitions
bool synthLFO1Sync = false;
bool synthLFO2Sync = false;
int synthPoly = 1; // Default to monophonic
int synthPitchUp = 0; // Default pitch up amount
