// Auto-generated FT2 GUI layout (schema-based)
#include <stdio.h>
#include "ft2_ostirus_complete_layout_schema.h"
#include "shared/ft2_ui_schema.h"

static const ft2_ui_waveform_view_desc_t _____waveform_views[_____WAVE_COUNT] = {
    { _____WAVE_BASE + 0, "waveform_view", 16, 248, 600, 132, 1 },
    { _____WAVE_BASE + 1, "arp_waveform_view", 228, 308, 388, 68, 6 },
};

static const ft2_ui_tf_button_desc_t _____tf_buttons[_____TF_BUTTON_COUNT] = {
    { _____TF_BUTTON_BASE + 0, "page_common_btn", 222, 32, 58, 18, 0, "Common" },
    { _____TF_BUTTON_BASE + 1, "page_filter_btn", 284, 32, 52, 18, 0, "Filter" },
    { _____TF_BUTTON_BASE + 2, "page_mod_matrix_btn", 340, 32, 58, 18, 0, "Matrix" },
    { _____TF_BUTTON_BASE + 3, "page_fx_btn", 402, 32, 40, 18, 0, "FX" },
    { _____TF_BUTTON_BASE + 4, "page_lfo_btn", 446, 32, 42, 18, 0, "LFO" },
    { _____TF_BUTTON_BASE + 5, "page_arp_btn", 492, 32, 42, 18, 0, "Arp" },
    { _____TF_BUTTON_BASE + 6, "page_browser_btn", 538, 32, 54, 18, 0, "Browser" },
    { _____TF_BUTTON_BASE + 7, "browser_load_btn", 540, 78, 64, 18, 7, "Load" },
    { _____TF_BUTTON_BASE + 8, "browser_default_btn", 520, 100, 84, 18, 7, "Default" },
    { _____TF_BUTTON_BASE + 9, "browser_prev_btn", 436, 78, 48, 18, 7, "Prev" },
    { _____TF_BUTTON_BASE + 10, "browser_next_btn", 488, 78, 48, 18, 7, "Next" },
    { _____TF_BUTTON_BASE + 11, "browser_clear_btn", 436, 100, 80, 18, 7, "Clear" },
    { _____TF_BUTTON_BASE + 12, "close_btn", 596, 8, 24, 18, 0, "X" },
};

static const ft2_ui_tf_toggle_desc_t _____tf_toggles[_____TF_TOGGLE_COUNT] = {
    { _____TF_TOGGLE_BASE + 0, "cc_64_hold", 112, 196, 66, 14, 1, "Hold", false },
    { _____TF_TOGGLE_BASE + 1, "cc_65_portamento", 112, 210, 84, 14, 1, "Portamento", false },
    { _____TF_TOGGLE_BASE + 2, "cc_66_sostenuto", 112, 224, 84, 14, 1, "Sostenuto", false },
};

static const ft2_ui_tf_label_desc_t _____tf_labels[_____TF_LABEL_COUNT] = {
    { _____TF_LABEL_BASE + 0, "title_label", 12, 8, 84, 18, 0, "OsTIrus" },
    { _____TF_LABEL_BASE + 1, "page_caption_label", 12, 34, 128, 14, 0, "Common" },
    { _____TF_LABEL_BASE + 2, "preset_name_label", 360, 8, 116, 18, 0, "Init" },
    { _____TF_LABEL_BASE + 3, "slot_label", 150, 34, 64, 14, 0, "Part 01" },
    { _____TF_LABEL_BASE + 4, "mod_matrix_label", 16, 60, 112, 14, 3, "Mod Matrix" },
    { _____TF_LABEL_BASE + 5, "fx_label", 16, 60, 80, 14, 4, "FX" },
    { _____TF_LABEL_BASE + 6, "lfo_label", 16, 60, 80, 14, 5, "LFO" },
    { _____TF_LABEL_BASE + 7, "arp_label", 16, 60, 80, 14, 6, "Arp" },
    { _____TF_LABEL_BASE + 8, "mod_slot_header", 20, 190, 160, 14, 3, "Source / Destination" },
    { _____TF_LABEL_BASE + 9, "arp_pattern_label", 24, 166, 132, 14, 6, "Pattern Controls" },
    { _____TF_LABEL_BASE + 10, "arp_view_label", 228, 166, 124, 14, 6, "Pattern View" },
    { _____TF_LABEL_BASE + 11, "mod_matrix_bank_label", 548, 60, 56, 14, 3, "16 Slots" },
    { _____TF_LABEL_BASE + 12, "browser_label", 20, 60, 132, 14, 7, "Patch Browser" },
    { _____TF_LABEL_BASE + 13, "browser_bank_title", 20, 100, 56, 14, 7, "Search" },
    { _____TF_LABEL_BASE + 14, "browser_program_title", 436, 60, 68, 14, 7, "Actions" },
    { _____TF_LABEL_BASE + 15, "browser_status_label", 20, 300, 592, 14, 7, "ROM status" },
    { _____TF_LABEL_BASE + 16, "browser_selected_label", 20, 318, 592, 14, 7, "Selected" },
    { _____TF_LABEL_BASE + 17, "browser_current_label", 20, 336, 592, 14, 7, "Loaded" },
};

static const ft2_ui_tf_rotary_slider_desc_t _____tf_rotaries[_____TF_ROTARY_COUNT] = {
    { _____TF_ROTARY_BASE + 0, "cc_07_channel_volume", 20, 84, 18, 1, 0.000f, 2.350f, "Ch Vol" },
    { _____TF_ROTARY_BASE + 1, "cc_08_balance", 64, 84, 18, 1, 0.000f, 2.350f, "Balance" },
    { _____TF_ROTARY_BASE + 2, "cc_10_panorama", 108, 84, 18, 1, 0.000f, 2.350f, "Pan" },
    { _____TF_ROTARY_BASE + 3, "cc_11_expression", 152, 84, 18, 1, 0.000f, 2.350f, "Expr" },
    { _____TF_ROTARY_BASE + 4, "cc_33_osc_balance", 20, 140, 18, 1, 0.000f, 2.350f, "OscBal" },
    { _____TF_ROTARY_BASE + 5, "cc_34_subosc_volume", 64, 196, 18, 1, 0.000f, 2.350f, "SubVol" },
    { _____TF_ROTARY_BASE + 6, "cc_36_osc_mainvolume", 552, 194, 18, 1, 0.000f, 2.350f, "OscVol" },
    { _____TF_ROTARY_BASE + 7, "cc_37_noise_volume", 108, 140, 18, 1, 0.000f, 2.350f, "Noise" },
    { _____TF_ROTARY_BASE + 8, "cc_39_noise_color", 152, 140, 18, 1, 0.000f, 2.350f, "Color" },
    { _____TF_ROTARY_BASE + 9, "cc_18_osc1_pulsewidth", 244, 150, 18, 1, 0.000f, 2.350f, "PW1" },
    { _____TF_ROTARY_BASE + 10, "cc_20_osc1_semitone", 300, 150, 18, 1, 0.000f, 2.350f, "Semi1" },
    { _____TF_ROTARY_BASE + 11, "cc_21_osc1_keyfollow", 356, 150, 18, 1, 0.000f, 2.350f, "Key1" },
    { _____TF_ROTARY_BASE + 12, "cc_23_osc2_pulsewidth", 440, 150, 18, 1, 0.000f, 2.350f, "PW2" },
    { _____TF_ROTARY_BASE + 13, "cc_26_osc2_detune", 496, 150, 18, 1, 0.000f, 2.350f, "Detune" },
    { _____TF_ROTARY_BASE + 14, "cc_27_osc2_fm_amount", 552, 150, 18, 1, 0.000f, 2.350f, "FM" },
    { _____TF_ROTARY_BASE + 15, "cc_28_osc2_sync", 440, 194, 18, 1, 0.000f, 2.350f, "Sync" },
    { _____TF_ROTARY_BASE + 16, "cc_31_osc2_keyfollow", 496, 194, 18, 1, 0.000f, 2.350f, "Key2" },
    { _____TF_ROTARY_BASE + 17, "cc_40_cutoff", 24, 126, 18, 2, 0.000f, 2.350f, "Cutoff" },
    { _____TF_ROTARY_BASE + 18, "cc_41_cutoff2", 232, 126, 18, 2, 0.000f, 2.350f, "Cut2" },
    { _____TF_ROTARY_BASE + 19, "cc_42_filter1_res", 80, 126, 18, 2, 0.000f, 2.350f, "Res1" },
    { _____TF_ROTARY_BASE + 20, "cc_43_filter2_res", 288, 126, 18, 2, 0.000f, 2.350f, "Res2" },
    { _____TF_ROTARY_BASE + 21, "cc_44_filter1_env_amt", 136, 126, 18, 2, 0.000f, 2.350f, "Env1" },
    { _____TF_ROTARY_BASE + 22, "cc_45_filter2_env_amt", 344, 126, 18, 2, 0.000f, 2.350f, "Env2" },
    { _____TF_ROTARY_BASE + 23, "cc_46_filter1_keyfollow", 80, 182, 18, 2, 0.000f, 2.350f, "Key1" },
    { _____TF_ROTARY_BASE + 24, "cc_47_filter2_keyfollow", 400, 126, 18, 2, 0.000f, 2.350f, "Key2" },
    { _____TF_ROTARY_BASE + 25, "cc_48_filter_balance", 472, 126, 18, 2, 0.000f, 2.350f, "Balance" },
    { _____TF_ROTARY_BASE + 26, "cc_49_saturation_curve", 528, 126, 18, 2, 0.000f, 2.350f, "Saturate" },
    { _____TF_ROTARY_BASE + 27, "cc_50_ringmod_volume", 584, 126, 18, 2, 0.000f, 2.350f, "Ring" },
    { _____TF_ROTARY_BASE + 28, "cc_54_filter_env_attack", 28, 344, 18, 2, 0.000f, 2.350f, "Atk" },
    { _____TF_ROTARY_BASE + 29, "cc_55_filter_env_decay", 84, 344, 18, 2, 0.000f, 2.350f, "Dec" },
    { _____TF_ROTARY_BASE + 30, "cc_56_filter_env_sustain", 140, 344, 18, 2, 0.000f, 2.350f, "Sus" },
    { _____TF_ROTARY_BASE + 31, "cc_57_filter_env_sustain_time", 196, 344, 18, 2, 0.000f, 2.350f, "SusT" },
    { _____TF_ROTARY_BASE + 32, "cc_58_filter_env_release", 252, 344, 18, 2, 0.000f, 2.350f, "Rel" },
    { _____TF_ROTARY_BASE + 33, "cc_59_amp_env_attack", 344, 344, 18, 2, 0.000f, 2.350f, "AtkA" },
    { _____TF_ROTARY_BASE + 34, "cc_60_amp_env_decay", 400, 344, 18, 2, 0.000f, 2.350f, "DecA" },
    { _____TF_ROTARY_BASE + 35, "cc_61_amp_env_sustain", 456, 344, 18, 2, 0.000f, 2.350f, "SusA" },
    { _____TF_ROTARY_BASE + 36, "cc_62_amp_env_sustain_time", 512, 344, 18, 2, 0.000f, 2.350f, "SusT" },
    { _____TF_ROTARY_BASE + 37, "cc_63_amp_env_release", 568, 344, 18, 2, 0.000f, 2.350f, "RelA" },
    { _____TF_ROTARY_BASE + 38, "cc_67_lfo1_rate", 36, 152, 18, 5, 0.000f, 2.350f, "Rate" },
    { _____TF_ROTARY_BASE + 39, "cc_71_lfo1_symmetry", 92, 152, 18, 5, 0.000f, 2.350f, "Sym" },
    { _____TF_ROTARY_BASE + 40, "cc_72_lfo1_keyfollow", 148, 152, 18, 5, 0.000f, 2.350f, "Key" },
    { _____TF_ROTARY_BASE + 41, "cc_74_lfo1_osc1_amt", 204, 152, 18, 5, 0.000f, 2.350f, "Osc1" },
    { _____TF_ROTARY_BASE + 42, "cc_75_lfo1_osc2_amt", 92, 208, 18, 5, 0.000f, 2.350f, "Osc2" },
    { _____TF_ROTARY_BASE + 43, "cc_78_lfo1_filtgain_amt", 148, 208, 18, 5, 0.000f, 2.350f, "Filt" },
    { _____TF_ROTARY_BASE + 44, "cc_79_lfo2_rate", 352, 152, 18, 5, 0.000f, 2.350f, "Rate2" },
    { _____TF_ROTARY_BASE + 45, "cc_83_lfo2_symmetry", 408, 152, 18, 5, 0.000f, 2.350f, "Sym2" },
    { _____TF_ROTARY_BASE + 46, "cc_84_lfo2_keyfollow", 464, 152, 18, 5, 0.000f, 2.350f, "Key2" },
    { _____TF_ROTARY_BASE + 47, "cc_86_lfo2_shape_amt", 520, 152, 18, 5, 0.000f, 2.350f, "Shape" },
    { _____TF_ROTARY_BASE + 48, "cc_87_lfo2_fm_amt", 380, 208, 18, 5, 0.000f, 2.350f, "FM" },
    { _____TF_ROTARY_BASE + 49, "cc_88_lfo2_cutoff1_amt", 436, 208, 18, 5, 0.000f, 2.350f, "Cut1" },
    { _____TF_ROTARY_BASE + 50, "cc_89_lfo2_cutoff2_amt", 492, 208, 18, 5, 0.000f, 2.350f, "Cut2" },
    { _____TF_ROTARY_BASE + 51, "cc_90_lfo2_pan_amt", 548, 208, 18, 5, 0.000f, 2.350f, "Pan" },
    { _____TF_ROTARY_BASE + 52, "cc_104_chorus_mix2", 32, 136, 18, 4, 0.000f, 2.350f, "Mix2" },
    { _____TF_ROTARY_BASE + 53, "cc_105_chorus_mix", 88, 136, 18, 4, 0.000f, 2.350f, "Mix" },
    { _____TF_ROTARY_BASE + 54, "cc_106_chorus_rate", 144, 136, 18, 4, 0.000f, 2.350f, "Rate" },
    { _____TF_ROTARY_BASE + 55, "cc_107_chorus_depth", 200, 136, 18, 4, 0.000f, 2.350f, "Depth" },
    { _____TF_ROTARY_BASE + 56, "cc_108_chorus_delay", 60, 192, 18, 4, 0.000f, 2.350f, "Delay" },
    { _____TF_ROTARY_BASE + 57, "cc_109_chorus_feedback", 116, 192, 18, 4, 0.000f, 2.350f, "Fdbk" },
    { _____TF_ROTARY_BASE + 58, "cc_113_delay_send", 344, 136, 18, 4, 0.000f, 2.350f, "Send" },
    { _____TF_ROTARY_BASE + 59, "cc_114_delay_time", 400, 136, 18, 4, 0.000f, 2.350f, "Time" },
    { _____TF_ROTARY_BASE + 60, "cc_115_delay_feedback", 456, 136, 18, 4, 0.000f, 2.350f, "Fdbk" },
    { _____TF_ROTARY_BASE + 61, "cc_116_delay_decay", 512, 136, 18, 4, 0.000f, 2.350f, "Decay" },
    { _____TF_ROTARY_BASE + 62, "cc_117_delay_depth", 400, 192, 18, 4, 0.000f, 2.350f, "Depth" },
    { _____TF_ROTARY_BASE + 63, "cc_119_delay_color", 456, 192, 18, 4, 0.000f, 2.350f, "Color" },
    { _____TF_ROTARY_BASE + 64, "cc_91_patch_volume", 64, 140, 18, 1, 0.000f, 2.350f, "Patch" },
    { _____TF_ROTARY_BASE + 65, "cc_93_transpose", 20, 196, 18, 1, 0.000f, 2.350f, "Transpose" },
    { _____TF_ROTARY_BASE + 66, "cc_103_arp_range", 80, 188, 18, 6, 0.000f, 2.350f, "Range" },
    { _____TF_ROTARY_BASE + 67, "cc_105_arp_tempo", 136, 188, 18, 6, 0.000f, 2.350f, "Tempo" },
    { _____TF_ROTARY_BASE + 68, "cc_111_arp_user_len", 24, 188, 18, 6, 0.000f, 2.350f, "UserLen" },
};

static const ft2_ui_tf_linear_slider_desc_t _____tf_linears[_____TF_LINEAR_COUNT] = {
    { _____TF_LINEAR_BASE + 0, "arp_step_01", 228, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 1, "arp_step_02", 250, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 2, "arp_step_03", 272, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 3, "arp_step_04", 294, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 4, "arp_step_05", 316, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 5, "arp_step_06", 338, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 6, "arp_step_07", 360, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 7, "arp_step_08", 382, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 8, "arp_step_09", 404, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 9, "arp_step_10", 426, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 10, "arp_step_11", 448, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 11, "arp_step_12", 470, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 12, "arp_step_13", 492, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 13, "arp_step_14", 514, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 14, "arp_step_15", 536, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 15, "arp_step_16", 558, 188, 18, 108, 6, true },
    { _____TF_LINEAR_BASE + 16, "cc_193_slot1_amount", 36, 80, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 17, "cc_196_slot2_amount", 168, 80, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 18, "cc_199_slot3_amount", 36, 104, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 19, "cc_202_slot4_amount", 168, 104, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 20, "cc_205_slot5_amount", 36, 128, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 21, "cc_208_slot6_amount", 168, 128, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 22, "cc_211_slot7_amount", 36, 152, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 23, "cc_214_slot8_amount", 168, 152, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 24, "cc_217_slot9_amount", 348, 80, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 25, "cc_220_slot10_amount", 480, 80, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 26, "cc_223_slot11_amount", 348, 104, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 27, "cc_226_slot12_amount", 480, 104, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 28, "cc_229_slot13_amount", 348, 128, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 29, "cc_232_slot14_amount", 480, 128, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 30, "cc_235_slot15_amount", 348, 152, 96, 18, 3, false },
    { _____TF_LINEAR_BASE + 31, "cc_238_slot16_amount", 480, 152, 96, 18, 3, false },
};

static const ft2_ui_tf_combo_box_desc_t _____tf_combos[_____TF_COMBO_COUNT] = {
    { _____TF_COMBO_BASE + 0, "cc_17_osc1_shape", 228, 112, 92, 18, 1, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 1, "cc_19_osc1_wavetable", 328, 112, 92, 18, 1, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 2, "cc_22_osc2_shape", 424, 112, 92, 18, 1, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 3, "cc_24_osc2_wave_select", 524, 112, 92, 18, 1, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 4, "cc_35_subosc_shape", 228, 194, 92, 18, 1, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 5, "cc_51_filter1_mode", 20, 84, 160, 18, 2, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 6, "cc_52_filter2_mode", 224, 84, 140, 18, 2, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 7, "cc_53_filter_routing", 384, 84, 224, 18, 2, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 8, "cc_192_slot1_source", 20, 204, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 9, "cc_194_slot1_destination", 20, 222, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 10, "cc_195_slot2_source", 172, 204, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 11, "cc_197_slot2_destination", 172, 222, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 12, "cc_198_slot3_source", 324, 204, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 13, "cc_200_slot3_destination", 324, 222, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 14, "cc_201_slot4_source", 476, 204, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 15, "cc_203_slot4_destination", 476, 222, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 16, "cc_204_slot5_source", 20, 248, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 17, "cc_206_slot5_destination", 20, 266, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 18, "cc_207_slot6_source", 172, 248, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 19, "cc_209_slot6_destination", 172, 266, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 20, "cc_210_slot7_source", 324, 248, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 21, "cc_212_slot7_destination", 324, 266, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 22, "cc_213_slot8_source", 476, 248, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 23, "cc_215_slot8_destination", 476, 266, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 24, "cc_216_slot9_source", 20, 292, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 25, "cc_218_slot9_destination", 20, 310, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 26, "cc_219_slot10_source", 172, 292, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 27, "cc_221_slot10_destination", 172, 310, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 28, "cc_222_slot11_source", 324, 292, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 29, "cc_224_slot11_destination", 324, 310, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 30, "cc_225_slot12_source", 476, 292, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 31, "cc_227_slot12_destination", 476, 310, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 32, "cc_228_slot13_source", 20, 336, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 33, "cc_230_slot13_destination", 20, 354, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 34, "cc_231_slot14_source", 172, 336, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 35, "cc_233_slot14_destination", 172, 354, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 36, "cc_234_slot15_source", 324, 336, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 37, "cc_236_slot15_destination", 324, 354, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 38, "cc_237_slot16_source", 476, 336, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 39, "cc_239_slot16_destination", 476, 354, 136, 18, 3, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 40, "cc_68_lfo1_shape", 24, 84, 84, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 41, "cc_69_lfo1_env_mode", 114, 84, 84, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 42, "cc_70_lfo1_mode", 204, 84, 84, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 43, "cc_73_lfo1_keytrigger", 24, 110, 120, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 44, "cc_80_lfo2_shape", 336, 84, 84, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 45, "cc_81_lfo2_env_mode", 426, 84, 84, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 46, "cc_82_lfo2_mode", 516, 84, 84, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 47, "cc_85_lfo2_keytrigger", 336, 110, 120, 18, 5, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 48, "cc_103_chorus_type", 24, 84, 92, 18, 4, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 49, "cc_110_chorus_lfo_shape", 124, 84, 92, 18, 4, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 50, "cc_112_delay_mode", 336, 84, 92, 18, 4, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 51, "cc_118_delay_lfo_shape", 436, 84, 92, 18, 4, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 52, "cc_94_key_mode", 524, 80, 92, 18, 1, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 53, "cc_32_bank_select", 228, 80, 92, 18, 1, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 54, "cc_102_arp_mode", 24, 110, 92, 18, 6, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 55, "cc_104_arp_clock", 124, 84, 92, 18, 6, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 56, "cc_106_arp_direction", 24, 84, 92, 18, 6, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 57, "cc_107_arp_pattern", 124, 110, 92, 18, 6, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 58, "cc_108_arp_note_order", 24, 136, 92, 18, 6, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 59, "cc_109_arp_velocity", 124, 136, 92, 18, 6, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 60, "preset_combo", 104, 8, 248, 18, 0, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 61, "slot_display", 0, 0, 0, 18, 0, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 62, "browser_bank_combo", 20, 356, 156, 18, 7, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 63, "browser_program_combo", 188, 356, 260, 18, 7, NULL, 0, 0 },
    { _____TF_COMBO_BASE + 64, "unused_combo_0", 0, 0, 0, 18, 0, NULL, 0, 0 },
};

static const ft2_ui_tf_level_meter_desc_t _____tf_meters[_____TF_METER_COUNT] = {
    { _____TF_METER_BASE + 0, "voice_meter", 540, 10, 44, 12, 0, 12, true },
};

static const ft2_ui_tf_envelope_display_desc_t _____tf_envs[_____TF_ENV_COUNT] = {
    { _____TF_ENV_BASE + 0, "filter_env_display", 18, 262, 280, 72, 2 },
    { _____TF_ENV_BASE + 1, "amp_env_display", 334, 262, 280, 72, 2 },
};

static const ft2_ui_tf_group_box_desc_t _____tf_groups[_____TF_GROUP_COUNT] = {
    { _____TF_GROUP_BASE + 0, "page0_osc_group", 8, 56, 196, 184, 1, "Performance" },
    { _____TF_GROUP_BASE + 1, "page0_osc2_group", 212, 56, 412, 184, 1, "Oscillators" },
    { _____TF_GROUP_BASE + 2, "page1_filter_a_group", 8, 56, 196, 184, 2, "Filter 1" },
    { _____TF_GROUP_BASE + 3, "page1_filter_b_group", 212, 56, 412, 184, 2, "Filter 2 / Routing" },
    { _____TF_GROUP_BASE + 4, "page1_env_group", 8, 248, 616, 144, 2, "Envelopes" },
    { _____TF_GROUP_BASE + 5, "page2_lfo1_group", 8, 56, 304, 184, 5, "LFO 1" },
    { _____TF_GROUP_BASE + 6, "page2_lfo2_group", 320, 56, 304, 184, 5, "LFO 2" },
    { _____TF_GROUP_BASE + 7, "page2_chorus_group", 8, 56, 304, 184, 4, "Chorus / Flanger" },
    { _____TF_GROUP_BASE + 8, "page2_delay_group", 320, 56, 304, 184, 4, "Delay / Aux" },
    { _____TF_GROUP_BASE + 9, "page3_utility_group", 8, 56, 304, 122, 3, "Slots 1-8" },
    { _____TF_GROUP_BASE + 10, "page3_performance_group", 320, 56, 304, 122, 3, "Slots 9-16" },
    { _____TF_GROUP_BASE + 11, "page3_routing_group", 8, 184, 616, 208, 3, "Mod Matrix" },
    { _____TF_GROUP_BASE + 12, "page5_arp_group", 8, 56, 616, 336, 6, "Arp / Pattern" },
    { _____TF_GROUP_BASE + 13, "page6_browser_group", 8, 56, 616, 336, 7, "Patch Browser" },
};

const ft2_ui_layout_desc_t _____layout = {
    "ft2_ostirus_complete_layout",
    FT2_UI_SCHEMA_VERSION,
    { _____PB_BASE, _____PB_COUNT },
#if _____PB_COUNT > 0
    _____pushbuttons,
#else
    NULL,
#endif
    { _____CB_BASE, _____CB_COUNT },
#if _____CB_COUNT > 0
    _____checkboxes,
#else
    NULL,
#endif
    { _____RB_BASE, _____RB_COUNT },
#if _____RB_COUNT > 0
    _____radiobuttons,
#else
    NULL,
#endif
    { _____SB_BASE, _____SB_COUNT },
#if _____SB_COUNT > 0
    _____scrollbars,
#else
    NULL,
#endif
    { _____TB_BASE, _____TB_COUNT },
#if _____TB_COUNT > 0
    _____textboxes,
#else
    NULL,
#endif
    { _____FB_BASE, _____FB_COUNT },
#if _____FB_COUNT > 0
    _____frameboxes,
#else
    NULL,
#endif
    { _____BMP_BASE, _____BMP_COUNT },
#if _____BMP_COUNT > 0
    _____bitmaps,
#else
    NULL,
#endif
    { _____WAVE_BASE, _____WAVE_COUNT },
#if _____WAVE_COUNT > 0
    _____waveform_views,
#else
    NULL,
#endif
    { _____TF_BUTTON_BASE, _____TF_BUTTON_COUNT },
#if _____TF_BUTTON_COUNT > 0
    _____tf_buttons,
#else
    NULL,
#endif
    { _____TF_TOGGLE_BASE, _____TF_TOGGLE_COUNT },
#if _____TF_TOGGLE_COUNT > 0
    _____tf_toggles,
#else
    NULL,
#endif
    { _____TF_LABEL_BASE, _____TF_LABEL_COUNT },
#if _____TF_LABEL_COUNT > 0
    _____tf_labels,
#else
    NULL,
#endif
    { _____TF_ROTARY_BASE, _____TF_ROTARY_COUNT },
#if _____TF_ROTARY_COUNT > 0
    _____tf_rotaries,
#else
    NULL,
#endif
    { _____TF_LINEAR_BASE, _____TF_LINEAR_COUNT },
#if _____TF_LINEAR_COUNT > 0
    _____tf_linears,
#else
    NULL,
#endif
    { _____TF_COMBO_BASE, _____TF_COMBO_COUNT },
#if _____TF_COMBO_COUNT > 0
    _____tf_combos,
#else
    NULL,
#endif
    { _____TF_METER_BASE, _____TF_METER_COUNT },
#if _____TF_METER_COUNT > 0
    _____tf_meters,
#else
    NULL,
#endif
    { _____TF_PARAM_BASE, _____TF_PARAM_COUNT },
#if _____TF_PARAM_COUNT > 0
    _____tf_params,
#else
    NULL,
#endif
    { _____TF_ENV_BASE, _____TF_ENV_COUNT },
#if _____TF_ENV_COUNT > 0
    _____tf_envs,
#else
    NULL,
#endif
    { _____TF_GROUP_BASE, _____TF_GROUP_COUNT },
#if _____TF_GROUP_COUNT > 0
    _____tf_groups,
#else
    NULL,
#endif
    { _____MIXER_STRIP_BASE, _____MIXER_STRIP_COUNT },
#if _____MIXER_STRIP_COUNT > 0
    _____mixer_strips,
#else
    NULL,
#endif
    { _____MIXER_GAIN_BASE, _____MIXER_GAIN_COUNT },
#if _____MIXER_GAIN_COUNT > 0
    _____mixer_gains,
#else
    NULL,
#endif
    { _____MIXER_PAN_BASE, _____MIXER_PAN_COUNT },
#if _____MIXER_PAN_COUNT > 0
    _____mixer_pans,
#else
    NULL,
#endif
    { _____MIXER_MUTE_BASE, _____MIXER_MUTE_COUNT },
#if _____MIXER_MUTE_COUNT > 0
    _____mixer_mutes,
#else
    NULL,
#endif
    { _____MIXER_SCOPE_BASE, _____MIXER_SCOPE_COUNT },
#if _____MIXER_SCOPE_COUNT > 0
    _____mixer_scopes,
#else
    NULL,
#endif
    { _____MIXER_MASTER_BASE, _____MIXER_MASTER_COUNT },
#if _____MIXER_MASTER_COUNT > 0
    _____mixer_masters,
#else
    NULL,
#endif
    { _____DSP_WINDOW_BASE, _____DSP_WINDOW_COUNT },
#if _____DSP_WINDOW_COUNT > 0
    _____dsp_windows,
#else
    NULL,
#endif
    { _____DSP_SLOT_BASE, _____DSP_SLOT_COUNT },
#if _____DSP_SLOT_COUNT > 0
    _____dsp_slots,
#else
    NULL,
#endif
    { _____DSP_MENU_BASE, _____DSP_MENU_COUNT },
#if _____DSP_MENU_COUNT > 0
    _____dsp_menus,
#else
    NULL,
#endif
    { _____DSP_PARAM_BASE, _____DSP_PARAM_COUNT },
#if _____DSP_PARAM_COUNT > 0
    _____dsp_params,
#else
    NULL
#endif
};
