// Auto-generated FT2 GUI layout (schema-based)
#include <stdio.h>
#include "dx_complete_layout_schema.h"
#include "shared/ft2_ui_schema.h"

static const ft2_ui_bitmap_desc_t dx_complete_layout_bitmaps[DX_COMPLETE_LAYOUT_BMP_COUNT] = {
    { DX_COMPLETE_LAYOUT_BMP_BASE + 0, 408, 8, 180, 130, 1, 19, 0, 0, 0, 255 },
};

static const ft2_ui_waveform_view_desc_t dx_complete_layout_waveform_views[DX_COMPLETE_LAYOUT_WAVE_COUNT] = {
    { DX_COMPLETE_LAYOUT_WAVE_BASE + 0, "dx_env_display", 14, 260, 294, 128, 1 },
};

static const ft2_ui_tf_button_desc_t dx_complete_layout_tf_buttons[DX_COMPLETE_LAYOUT_TF_BUTTON_COUNT] = {
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE + 0, "dx_close_btn", 576, 8, 48, 22, 1, "Close" },
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE + 1, "dx_op1_select", 24, 70, 40, 20, 1, "OP1" },
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE + 2, "dx_op2_select", 72, 70, 40, 20, 1, "OP2" },
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE + 3, "dx_op3_select", 120, 70, 40, 20, 1, "OP3" },
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE + 4, "dx_op4_select", 168, 70, 40, 20, 1, "OP4" },
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE + 5, "dx_op5_select", 216, 70, 40, 20, 1, "OP5" },
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE + 6, "dx_op6_select", 264, 70, 40, 20, 1, "OP6" },
};

static const ft2_ui_tf_toggle_desc_t dx_complete_layout_tf_toggles[DX_COMPLETE_LAYOUT_TF_TOGGLE_COUNT] = {
    { DX_COMPLETE_LAYOUT_TF_TOGGLE_BASE + 0, "dx_op_p18", 576, 184, 36, 20, 1, "Mode", false },
    { DX_COMPLETE_LAYOUT_TF_TOGGLE_BASE + 1, "dx_lfo_sync", 544, 280, 60, 18, 1, "LFO Sync", false },
    { DX_COMPLETE_LAYOUT_TF_TOGGLE_BASE + 2, "dx_mono", 466, 340, 36, 18, 1, "Mono", false },
};

static const ft2_ui_tf_label_desc_t dx_complete_layout_tf_labels[DX_COMPLETE_LAYOUT_TF_LABEL_COUNT] = {
    { DX_COMPLETE_LAYOUT_TF_LABEL_BASE + 0, "dx_title_label", 20, 12, 180, 22, 1, "Dexed_DXM_Edition" },
};

static const ft2_ui_tf_rotary_slider_desc_t dx_complete_layout_tf_rotaries[DX_COMPLETE_LAYOUT_TF_ROTARY_COUNT] = {
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 0, "dx_op_p1", 18, 126, 18, 1, 0.000f, 2.350f, "EG-Rate-1" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 1, "dx_op_p2", 63, 126, 18, 1, 0.000f, 2.350f, "EG-Rate-2" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 2, "dx_op_p3", 108, 126, 18, 1, 0.000f, 2.350f, "EG-Rate-3" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 3, "dx_op_p4", 156, 126, 18, 1, 0.000f, 2.350f, "EG-Rate-4" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 4, "dx_op_p5", 18, 170, 18, 1, 0.000f, 2.350f, "EG-Level-1" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 5, "dx_op_p6", 63, 170, 18, 1, 0.000f, 2.350f, "EG-Level-2" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 6, "dx_op_p7", 108, 170, 18, 1, 0.000f, 2.350f, "EG-Level-3" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 7, "dx_op_p8", 156, 170, 18, 1, 0.000f, 2.350f, "EG-Level-4" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 8, "dx_op_p9", 220, 126, 18, 1, 0.000f, 2.350f, "Scl_BP" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 9, "dx_op_p10", 262, 126, 18, 1, 0.000f, 2.350f, "Scl_LD" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 10, "dx_op_p11", 305, 126, 18, 1, 0.000f, 2.350f, "Scl_RD" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 11, "dx_op_p12", 348, 126, 18, 1, 0.000f, 2.350f, "Scl_LC" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 12, "dx_op_p13", 390, 126, 18, 1, 0.000f, 2.350f, "Scl_RC" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 13, "dx_op_p14", 220, 170, 18, 1, 0.000f, 2.350f, "Rate_Scl" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 14, "dx_op_p15", 450, 126, 18, 1, 0.000f, 2.350f, "AMS" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 15, "dx_op_p16", 490, 126, 18, 1, 0.000f, 2.350f, "KVS" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 16, "dx_op_p17", 532, 126, 18, 1, 0.000f, 2.350f, "Out_Lvl" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 17, "dx_op_p19", 450, 168, 18, 1, 0.000f, 2.350f, "Crs_Freq" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 18, "dx_op_p20", 490, 168, 18, 1, 0.000f, 2.350f, "Fin_Freq" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 19, "dx_op_p21", 532, 168, 18, 1, 0.000f, 2.350f, "Detune" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 20, "dx_filter_cutoff", 324, 328, 18, 1, 0.000f, 2.350f, "Cutoff" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 21, "dx_filter_reso", 369, 328, 18, 1, 0.000f, 2.350f, "Reso" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 22, "dx_filter_gain", 413, 328, 18, 1, 0.000f, 2.350f, "Gain" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 23, "dx_lfo_rate", 334, 252, 18, 1, 0.000f, 2.350f, "Lfo-Rate" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 24, "dx_lfo_delay", 386, 252, 18, 1, 0.000f, 2.350f, "LFO-Decay" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 25, "dx_lfo_pitch_depth", 436, 252, 18, 1, 0.000f, 2.350f, "LFO-Pitch" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 26, "dx_lfo_amp_depth", 488, 252, 18, 1, 0.000f, 2.350f, "LFO-Amp" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 27, "dx_global_level", 542, 328, 18, 1, 0.000f, 2.350f, "Gain" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 28, "dx_feedback", 582, 328, 18, 1, 0.000f, 2.350f, "Feedback" },
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE + 29, "dx_porta_time", 503, 328, 18, 1, 0.000f, 2.350f, "Glide" },
};

static const ft2_ui_tf_combo_box_desc_t dx_complete_layout_tf_combos[DX_COMPLETE_LAYOUT_TF_COMBO_COUNT] = {
    { DX_COMPLETE_LAYOUT_TF_COMBO_BASE + 0, "dx_preset_combo", 280, 8, 130, 22, 1, NULL, 0, 0 },
    { DX_COMPLETE_LAYOUT_TF_COMBO_BASE + 1, "dx_alg_combo", 320, 72, 71, 16, 1, NULL, 0, 0 },
    { DX_COMPLETE_LAYOUT_TF_COMBO_BASE + 2, "dx_lfo_waveform", 536, 256, 80, 18, 1, NULL, 0, 0 },
};

static const ft2_ui_tf_level_meter_desc_t dx_complete_layout_tf_meters[DX_COMPLETE_LAYOUT_TF_METER_COUNT] = {
    { DX_COMPLETE_LAYOUT_TF_METER_BASE + 0, "dx_out_meter_L", 420, 10, 40, 18, 1, 12, true },
    { DX_COMPLETE_LAYOUT_TF_METER_BASE + 1, "dx_cpu_meter", 468, 10, 40, 18, 1, 12, true },
};

static const ft2_ui_tf_group_box_desc_t dx_complete_layout_tf_groups[DX_COMPLETE_LAYOUT_TF_GROUP_COUNT] = {
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 0, "dx_env_group", 10, 235, 302, 160, 1, "Op-Envelope" },
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 1, "dx_op_eg_group", 10, 116, 193, 112, 1, "Env-Generator" },
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 2, "dx_op_kbd_group", 207, 116, 226, 112, 1, "Keyboard-Scale" },
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 3, "dx_op_osc_group", 437, 115, 188, 113, 1, "Oscillator" },
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 4, "dx_filter_group", 318, 317, 135, 79, 1, "Filter" },
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 5, "dx_lfo_group", 318, 235, 307, 73, 1, "LFO" },
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 6, "dx_global_group", 458, 317, 167, 79, 1, "Global" },
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE + 7, "tf_group_55", 10, 56, 396, 50, 1, "Operators" },
};

const ft2_ui_layout_desc_t dx_complete_layout_layout = {
    "dx_complete_layout",
    FT2_UI_SCHEMA_VERSION,
    { DX_COMPLETE_LAYOUT_PB_BASE, DX_COMPLETE_LAYOUT_PB_COUNT },
#if DX_COMPLETE_LAYOUT_PB_COUNT > 0
    dx_complete_layout_pushbuttons,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_CB_BASE, DX_COMPLETE_LAYOUT_CB_COUNT },
#if DX_COMPLETE_LAYOUT_CB_COUNT > 0
    dx_complete_layout_checkboxes,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_RB_BASE, DX_COMPLETE_LAYOUT_RB_COUNT },
#if DX_COMPLETE_LAYOUT_RB_COUNT > 0
    dx_complete_layout_radiobuttons,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_SB_BASE, DX_COMPLETE_LAYOUT_SB_COUNT },
#if DX_COMPLETE_LAYOUT_SB_COUNT > 0
    dx_complete_layout_scrollbars,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TB_BASE, DX_COMPLETE_LAYOUT_TB_COUNT },
#if DX_COMPLETE_LAYOUT_TB_COUNT > 0
    dx_complete_layout_textboxes,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_FB_BASE, DX_COMPLETE_LAYOUT_FB_COUNT },
#if DX_COMPLETE_LAYOUT_FB_COUNT > 0
    dx_complete_layout_frameboxes,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_BMP_BASE, DX_COMPLETE_LAYOUT_BMP_COUNT },
#if DX_COMPLETE_LAYOUT_BMP_COUNT > 0
    dx_complete_layout_bitmaps,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_WAVE_BASE, DX_COMPLETE_LAYOUT_WAVE_COUNT },
#if DX_COMPLETE_LAYOUT_WAVE_COUNT > 0
    dx_complete_layout_waveform_views,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_BUTTON_BASE, DX_COMPLETE_LAYOUT_TF_BUTTON_COUNT },
#if DX_COMPLETE_LAYOUT_TF_BUTTON_COUNT > 0
    dx_complete_layout_tf_buttons,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_TOGGLE_BASE, DX_COMPLETE_LAYOUT_TF_TOGGLE_COUNT },
#if DX_COMPLETE_LAYOUT_TF_TOGGLE_COUNT > 0
    dx_complete_layout_tf_toggles,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_LABEL_BASE, DX_COMPLETE_LAYOUT_TF_LABEL_COUNT },
#if DX_COMPLETE_LAYOUT_TF_LABEL_COUNT > 0
    dx_complete_layout_tf_labels,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_ROTARY_BASE, DX_COMPLETE_LAYOUT_TF_ROTARY_COUNT },
#if DX_COMPLETE_LAYOUT_TF_ROTARY_COUNT > 0
    dx_complete_layout_tf_rotaries,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_LINEAR_BASE, DX_COMPLETE_LAYOUT_TF_LINEAR_COUNT },
#if DX_COMPLETE_LAYOUT_TF_LINEAR_COUNT > 0
    dx_complete_layout_tf_linears,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_COMBO_BASE, DX_COMPLETE_LAYOUT_TF_COMBO_COUNT },
#if DX_COMPLETE_LAYOUT_TF_COMBO_COUNT > 0
    dx_complete_layout_tf_combos,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_METER_BASE, DX_COMPLETE_LAYOUT_TF_METER_COUNT },
#if DX_COMPLETE_LAYOUT_TF_METER_COUNT > 0
    dx_complete_layout_tf_meters,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_PARAM_BASE, DX_COMPLETE_LAYOUT_TF_PARAM_COUNT },
#if DX_COMPLETE_LAYOUT_TF_PARAM_COUNT > 0
    dx_complete_layout_tf_params,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_ENV_BASE, DX_COMPLETE_LAYOUT_TF_ENV_COUNT },
#if DX_COMPLETE_LAYOUT_TF_ENV_COUNT > 0
    dx_complete_layout_tf_envs,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_TF_GROUP_BASE, DX_COMPLETE_LAYOUT_TF_GROUP_COUNT },
#if DX_COMPLETE_LAYOUT_TF_GROUP_COUNT > 0
    dx_complete_layout_tf_groups,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_MIXER_STRIP_BASE, DX_COMPLETE_LAYOUT_MIXER_STRIP_COUNT },
#if DX_COMPLETE_LAYOUT_MIXER_STRIP_COUNT > 0
    dx_complete_layout_mixer_strips,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_MIXER_GAIN_BASE, DX_COMPLETE_LAYOUT_MIXER_GAIN_COUNT },
#if DX_COMPLETE_LAYOUT_MIXER_GAIN_COUNT > 0
    dx_complete_layout_mixer_gains,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_MIXER_PAN_BASE, DX_COMPLETE_LAYOUT_MIXER_PAN_COUNT },
#if DX_COMPLETE_LAYOUT_MIXER_PAN_COUNT > 0
    dx_complete_layout_mixer_pans,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_MIXER_MUTE_BASE, DX_COMPLETE_LAYOUT_MIXER_MUTE_COUNT },
#if DX_COMPLETE_LAYOUT_MIXER_MUTE_COUNT > 0
    dx_complete_layout_mixer_mutes,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_MIXER_SCOPE_BASE, DX_COMPLETE_LAYOUT_MIXER_SCOPE_COUNT },
#if DX_COMPLETE_LAYOUT_MIXER_SCOPE_COUNT > 0
    dx_complete_layout_mixer_scopes,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_MIXER_MASTER_BASE, DX_COMPLETE_LAYOUT_MIXER_MASTER_COUNT },
#if DX_COMPLETE_LAYOUT_MIXER_MASTER_COUNT > 0
    dx_complete_layout_mixer_masters,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_DSP_WINDOW_BASE, DX_COMPLETE_LAYOUT_DSP_WINDOW_COUNT },
#if DX_COMPLETE_LAYOUT_DSP_WINDOW_COUNT > 0
    dx_complete_layout_dsp_windows,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_DSP_SLOT_BASE, DX_COMPLETE_LAYOUT_DSP_SLOT_COUNT },
#if DX_COMPLETE_LAYOUT_DSP_SLOT_COUNT > 0
    dx_complete_layout_dsp_slots,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_DSP_MENU_BASE, DX_COMPLETE_LAYOUT_DSP_MENU_COUNT },
#if DX_COMPLETE_LAYOUT_DSP_MENU_COUNT > 0
    dx_complete_layout_dsp_menus,
#else
    NULL,
#endif
    { DX_COMPLETE_LAYOUT_DSP_PARAM_BASE, DX_COMPLETE_LAYOUT_DSP_PARAM_COUNT },
#if DX_COMPLETE_LAYOUT_DSP_PARAM_COUNT > 0
    dx_complete_layout_dsp_params,
#else
    NULL
#endif
};
