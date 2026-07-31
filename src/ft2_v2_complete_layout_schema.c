#include <stddef.h>

#include "ft2_v2_complete_layout_schema.h"

#define PAGE_GLOBAL  FT2_UI_WIDGET_PAGE_BOTH
#define PAGE_VOICE   FT2_UI_WIDGET_PAGE_1
#define PAGE_FILTER  FT2_UI_WIDGET_PAGE_2
#define PAGE_LFOENV  FT2_UI_WIDGET_PAGE_3
#define PAGE_FX      FT2_UI_WIDGET_PAGE_4
#define PAGE_MASTER  FT2_UI_WIDGET_PAGE_5
#define PAGE_MOD     FT2_UI_WIDGET_PAGE_6

#define TF_BUTTON(name, x, y, w, h, page, text) \
    { 0, name, x, y, w, h, page, text }

#define TF_LABEL(name, x, y, w, h, page, text) \
    { 0, name, x, y, w, h, page, text }

#define TF_ROTARY(name, x, y, radius, page, text) \
    { 0, name, x, y, radius, page, -2.35f, 2.35f, text }

#define TF_LINEAR(name, x, y, w, h, page, vertical) \
    { 0, name, x, y, w, h, page, vertical }

#define TF_COMBO(name, x, y, w, h, page) \
    { 0, name, x, y, w, h, page, NULL, 0, 0 }

#define TF_METER(name, x, y, w, h, page, leds, peak) \
    { 0, name, x, y, w, h, page, leds, peak }

#define TF_GROUP(name, x, y, w, h, page, title) \
    { 0, name, x, y, w, h, page, title }

#define TF_ENV(name, x, y, w, h, page) \
    { 0, name, x, y, w, h, page }

#define TF_WAVE(name, x, y, w, h, page) \
    { 0, name, x, y, w, h, page }

static const ft2_ui_waveform_view_desc_t v2_waveform_views[] =
{
    TF_WAVE("waveform_view", 16, 248, 600, 132, PAGE_VOICE)
};

static const ft2_ui_tf_button_desc_t v2_tf_buttons[] =
{
    TF_BUTTON("page_voice_btn", 264, 32, 52, 18, PAGE_GLOBAL, "Voice"),
    TF_BUTTON("page_filter_btn", 320, 32, 52, 18, PAGE_GLOBAL, "Filter"),
    TF_BUTTON("page_lfo_env_btn", 376, 32, 64, 18, PAGE_GLOBAL, "Env/LFO"),
    TF_BUTTON("page_fx_btn", 444, 32, 42, 18, PAGE_GLOBAL, "FX"),
    TF_BUTTON("page_master_btn", 490, 32, 58, 18, PAGE_GLOBAL, "Master"),
    TF_BUTTON("page_mod_btn", 552, 32, 40, 18, PAGE_GLOBAL, "Mod"),
    TF_BUTTON("close_btn", 596, 8, 24, 18, PAGE_GLOBAL, "X")
};

static const ft2_ui_tf_label_desc_t v2_tf_labels[] =
{
    TF_LABEL("title_label", 12, 8, 84, 18, PAGE_GLOBAL, "V2 Synth"),
    TF_LABEL("page_caption_label", 12, 34, 220, 14, PAGE_GLOBAL, "Voice / Osc"),

    TF_LABEL("lbl_p_58", 18, 78, 80, 12, PAGE_VOICE, "Key Sync"),
    TF_LABEL("lbl_p_0", 18, 118, 80, 12, PAGE_VOICE, "Panning"),
    TF_LABEL("lbl_p_1", 18, 158, 80, 12, PAGE_VOICE, "Transpose"),
    TF_LABEL("lbl_p_88", 96, 158, 36, 12, PAGE_VOICE, "Poly"),
    TF_LABEL("lbl_p_2", 162, 78, 64, 12, PAGE_VOICE, "Mode"),
    TF_LABEL("lbl_p_7", 258, 112, 48, 12, PAGE_VOICE, "Volume"),
    TF_LABEL("lbl_p_8", 322, 78, 64, 12, PAGE_VOICE, "Mode"),
    TF_LABEL("lbl_p_9", 424, 78, 40, 12, PAGE_VOICE, "Ring"),
    TF_LABEL("lbl_p_13", 422, 112, 40, 12, PAGE_VOICE, "Vol"),
    TF_LABEL("lbl_p_14", 482, 78, 64, 12, PAGE_VOICE, "Mode"),
    TF_LABEL("lbl_p_15", 584, 78, 40, 12, PAGE_VOICE, "Ring"),
    TF_LABEL("lbl_p_19", 582, 112, 40, 12, PAGE_VOICE, "Vol"),

    TF_LABEL("lbl_p_20", 20, 78, 64, 12, PAGE_FILTER, "Mode"),
    TF_LABEL("lbl_p_23", 224, 78, 64, 12, PAGE_FILTER, "Mode"),
    TF_LABEL("lbl_p_26", 428, 78, 72, 12, PAGE_FILTER, "Routing"),
    TF_LABEL("lbl_p_27", 428, 134, 72, 12, PAGE_FILTER, "Balance"),
    TF_LABEL("lbl_p_28", 20, 246, 64, 12, PAGE_FILTER, "Mode"),
    TF_LABEL("lbl_p_68", 332, 246, 64, 12, PAGE_FILTER, "Mode"),

    TF_LABEL("lbl_p_44", 18, 118, 60, 12, PAGE_LFOENV, "Mode"),
    TF_LABEL("lbl_p_45", 110, 118, 72, 12, PAGE_LFOENV, "KeySync"),
    TF_LABEL("lbl_p_46", 18, 144, 72, 12, PAGE_LFOENV, "EnvMode"),
    TF_LABEL("lbl_p_49", 110, 144, 72, 12, PAGE_LFOENV, "Polarity"),
    TF_LABEL("lbl_p_51", 330, 118, 60, 12, PAGE_LFOENV, "Mode"),
    TF_LABEL("lbl_p_52", 422, 118, 72, 12, PAGE_LFOENV, "KeySync"),
    TF_LABEL("lbl_p_53", 330, 144, 72, 12, PAGE_LFOENV, "EnvMode"),
    TF_LABEL("lbl_p_56", 422, 144, 72, 12, PAGE_LFOENV, "Polarity"),

    TF_LABEL("lbl_p_72", 436, 78, 84, 12, PAGE_FX, "Chorus/Flanger"),
    TF_LABEL("lbl_p_60", 20, 246, 64, 12, PAGE_FX, "Aux A In"),
    TF_LABEL("lbl_p_61", 110, 246, 64, 12, PAGE_FX, "Aux B In"),
    TF_LABEL("lbl_p_62", 200, 246, 64, 12, PAGE_FX, "Aux A Out"),
    TF_LABEL("lbl_p_63", 20, 320, 64, 12, PAGE_FX, "Aux B Out"),
    TF_LABEL("lbl_p_64", 110, 320, 64, 12, PAGE_FX, "Reverb"),
    TF_LABEL("lbl_p_65", 200, 320, 64, 12, PAGE_FX, "Delay"),
    TF_LABEL("lbl_p_66", 20, 282, 84, 12, PAGE_FX, "FX Route"),
    TF_LABEL("lbl_p_67", 200, 282, 48, 12, PAGE_FX, "Boost"),
    TF_LABEL("lbl_p_79", 332, 246, 60, 12, PAGE_FX, "Mode"),
    TF_LABEL("lbl_p_80", 430, 246, 64, 12, PAGE_FX, "Couple"),
    TF_LABEL("lbl_p_81", 528, 246, 72, 12, PAGE_FX, "Auto"),

    TF_LABEL("lbl_g_13", 20, 246, 60, 12, PAGE_MASTER, "Mode"),
    TF_LABEL("lbl_g_14", 118, 246, 64, 12, PAGE_MASTER, "Couple"),
    TF_LABEL("lbl_g_15", 216, 246, 72, 12, PAGE_MASTER, "Auto"),
    TF_LABEL("lbl_mod_bank", 476, 78, 36, 12, PAGE_MOD, "Bank"),
    TF_LABEL("m_lbl_0", 18, 112, 44, 12, PAGE_MOD, "Slot 1"),
    TF_LABEL("m_lbl_1", 18, 146, 44, 12, PAGE_MOD, "Slot 2"),
    TF_LABEL("m_lbl_2", 18, 180, 44, 12, PAGE_MOD, "Slot 3"),
    TF_LABEL("m_lbl_3", 18, 214, 44, 12, PAGE_MOD, "Slot 4"),
    TF_LABEL("m_lbl_4", 18, 248, 44, 12, PAGE_MOD, "Slot 5"),
    TF_LABEL("m_lbl_5", 18, 282, 44, 12, PAGE_MOD, "Slot 6"),
    TF_LABEL("m_lbl_6", 18, 316, 44, 12, PAGE_MOD, "Slot 7"),
    TF_LABEL("m_lbl_7", 18, 350, 44, 12, PAGE_MOD, "Slot 8")
};

static const ft2_ui_tf_rotary_slider_desc_t v2_tf_rotaries[] =
{
    TF_ROTARY("p_4", 170, 132, 18, PAGE_VOICE, "Txpose"),
    TF_ROTARY("p_5", 222, 132, 18, PAGE_VOICE, "Detune"),
    TF_ROTARY("p_6", 170, 172, 18, PAGE_VOICE, "Color"),
    TF_ROTARY("p_10", 330, 132, 18, PAGE_VOICE, "Txpose"),
    TF_ROTARY("p_11", 382, 132, 18, PAGE_VOICE, "Detune"),
    TF_ROTARY("p_12", 330, 172, 18, PAGE_VOICE, "Color"),
    TF_ROTARY("p_16", 490, 132, 18, PAGE_VOICE, "Txpose"),
    TF_ROTARY("p_17", 542, 132, 18, PAGE_VOICE, "Detune"),
    TF_ROTARY("p_18", 490, 172, 18, PAGE_VOICE, "Color"),

    TF_ROTARY("p_21", 56, 122, 22, PAGE_FILTER, "Cutoff"),
    TF_ROTARY("p_22", 126, 122, 22, PAGE_FILTER, "Reso"),
    TF_ROTARY("p_24", 260, 122, 22, PAGE_FILTER, "Cutoff"),
    TF_ROTARY("p_25", 330, 122, 22, PAGE_FILTER, "Reso"),
    TF_ROTARY("p_29", 64, 298, 20, PAGE_FILTER, "InGain"),
    TF_ROTARY("p_30", 138, 298, 20, PAGE_FILTER, "Param 1"),
    TF_ROTARY("p_31", 212, 298, 20, PAGE_FILTER, "Param 2"),
    TF_ROTARY("p_69", 376, 298, 20, PAGE_FILTER, "InGain"),
    TF_ROTARY("p_70", 450, 298, 20, PAGE_FILTER, "Param 1"),
    TF_ROTARY("p_71", 524, 298, 20, PAGE_FILTER, "Param 2"),

    TF_ROTARY("p_47", 24, 74, 18, PAGE_LFOENV, "Rate"),
    TF_ROTARY("p_48", 106, 74, 18, PAGE_LFOENV, "Phase"),
    TF_ROTARY("p_50", 188, 74, 18, PAGE_LFOENV, "Amount"),
    TF_ROTARY("p_54", 336, 74, 18, PAGE_LFOENV, "Rate"),
    TF_ROTARY("p_55", 418, 74, 18, PAGE_LFOENV, "Phase"),
    TF_ROTARY("p_57", 500, 74, 18, PAGE_LFOENV, "Amount"),
    TF_ROTARY("p_32", 28, 304, 18, PAGE_LFOENV, "Attack"),
    TF_ROTARY("p_33", 108, 304, 18, PAGE_LFOENV, "Decay"),
    TF_ROTARY("p_34", 188, 304, 18, PAGE_LFOENV, "Sustain"),
    TF_ROTARY("p_35", 28, 344, 18, PAGE_LFOENV, "SusTime"),
    TF_ROTARY("p_36", 108, 344, 18, PAGE_LFOENV, "Release"),
    TF_ROTARY("p_37", 188, 344, 18, PAGE_LFOENV, "Amplify"),
    TF_ROTARY("p_38", 340, 304, 18, PAGE_LFOENV, "Attack"),
    TF_ROTARY("p_39", 420, 304, 18, PAGE_LFOENV, "Decay"),
    TF_ROTARY("p_40", 500, 304, 18, PAGE_LFOENV, "Sustain"),
    TF_ROTARY("p_41", 340, 344, 18, PAGE_LFOENV, "SusTime"),
    TF_ROTARY("p_42", 420, 344, 18, PAGE_LFOENV, "Release"),
    TF_ROTARY("p_43", 500, 344, 18, PAGE_LFOENV, "Amplify"),

    TF_ROTARY("p_73", 436, 120, 18, PAGE_FX, "FeedBk"),
    TF_ROTARY("p_74", 488, 120, 18, PAGE_FX, "Delay L"),
    TF_ROTARY("p_75", 540, 120, 18, PAGE_FX, "Delay R"),
    TF_ROTARY("p_76", 436, 170, 18, PAGE_FX, "M.Rate"),
    TF_ROTARY("p_77", 488, 170, 18, PAGE_FX, "M.Depth"),
    TF_ROTARY("p_78", 540, 170, 18, PAGE_FX, "M.Phase"),
    TF_ROTARY("p_28", 24, 120, 18, PAGE_FX, "Mode"),
    TF_ROTARY("p_29", 76, 120, 18, PAGE_FX, "InGain"),
    TF_ROTARY("p_30", 128, 120, 18, PAGE_FX, "Param 1"),
    TF_ROTARY("p_31", 24, 170, 18, PAGE_FX, "Param 2"),
    TF_ROTARY("p_68", 232, 120, 18, PAGE_FX, "Mode"),
    TF_ROTARY("p_69", 284, 120, 18, PAGE_FX, "InGain"),
    TF_ROTARY("p_70", 336, 120, 18, PAGE_FX, "Param 1"),
    TF_ROTARY("p_71", 232, 170, 18, PAGE_FX, "Param 2"),
    TF_ROTARY("p_60", 24, 256, 18, PAGE_FX, "AuxA In"),
    TF_ROTARY("p_61", 114, 256, 18, PAGE_FX, "AuxB In"),
    TF_ROTARY("p_62", 204, 256, 18, PAGE_FX, "AuxA Out"),
    TF_ROTARY("p_63", 24, 330, 18, PAGE_FX, "AuxB Out"),
    TF_ROTARY("p_64", 114, 330, 18, PAGE_FX, "Reverb"),
    TF_ROTARY("p_65", 204, 330, 18, PAGE_FX, "Delay"),
    TF_ROTARY("p_67", 204, 292, 18, PAGE_FX, "Boost"),
    TF_ROTARY("p_82", 332, 286, 18, PAGE_FX, "LookAhd"),
    TF_ROTARY("p_83", 414, 286, 18, PAGE_FX, "Thresh"),
    TF_ROTARY("p_84", 496, 286, 18, PAGE_FX, "Ratio"),
    TF_ROTARY("p_85", 332, 336, 18, PAGE_FX, "Attack"),
    TF_ROTARY("p_86", 414, 336, 18, PAGE_FX, "Release"),
    TF_ROTARY("p_87", 496, 336, 18, PAGE_FX, "OutGain"),

    TF_ROTARY("g_0", 24, 90, 20, PAGE_MASTER, "Time"),
    TF_ROTARY("g_1", 92, 90, 20, PAGE_MASTER, "HighCut"),
    TF_ROTARY("g_2", 24, 150, 20, PAGE_MASTER, "LowCut"),
    TF_ROTARY("g_3", 92, 150, 20, PAGE_MASTER, "Volume"),
    TF_ROTARY("g_4", 228, 90, 18, PAGE_MASTER, "Volume"),
    TF_ROTARY("g_5", 280, 90, 18, PAGE_MASTER, "FeedBk"),
    TF_ROTARY("g_6", 332, 90, 18, PAGE_MASTER, "Delay L"),
    TF_ROTARY("g_7", 384, 90, 18, PAGE_MASTER, "Delay R"),
    TF_ROTARY("g_8", 228, 148, 18, PAGE_MASTER, "M.Rate"),
    TF_ROTARY("g_9", 280, 148, 18, PAGE_MASTER, "M.Depth"),
    TF_ROTARY("g_10", 332, 148, 18, PAGE_MASTER, "M.Phase"),
    TF_ROTARY("g_16", 24, 286, 18, PAGE_MASTER, "LookAhd"),
    TF_ROTARY("g_17", 106, 286, 18, PAGE_MASTER, "Thresh"),
    TF_ROTARY("g_18", 188, 286, 18, PAGE_MASTER, "Ratio"),
    TF_ROTARY("g_19", 270, 286, 18, PAGE_MASTER, "Attack"),
    TF_ROTARY("g_20", 352, 286, 18, PAGE_MASTER, "Release"),
    TF_ROTARY("g_21", 434, 286, 18, PAGE_MASTER, "OutGain")
};

static const ft2_ui_tf_linear_slider_desc_t v2_tf_linears[] =
{
    TF_LINEAR("p_0", 18, 132, 110, 18, PAGE_VOICE, false),
    TF_LINEAR("p_1", 18, 172, 110, 18, PAGE_VOICE, false),
    TF_LINEAR("p_88", 94, 172, 18, 36, PAGE_VOICE, true),
    TF_LINEAR("p_7", 260, 126, 18, 82, PAGE_VOICE, true),
    TF_LINEAR("p_13", 424, 126, 18, 82, PAGE_VOICE, true),
    TF_LINEAR("p_19", 584, 126, 18, 82, PAGE_VOICE, true),
    TF_LINEAR("p_27", 428, 148, 178, 18, PAGE_FILTER, false),
    TF_LINEAR("g_11", 468, 104, 140, 18, PAGE_MASTER, false),
    TF_LINEAR("g_12", 468, 150, 140, 18, PAGE_MASTER, false),
    TF_LINEAR("m_amt_0", 236, 108, 96, 18, PAGE_MOD, false),
    TF_LINEAR("m_amt_1", 236, 142, 96, 18, PAGE_MOD, false),
    TF_LINEAR("m_amt_2", 236, 176, 96, 18, PAGE_MOD, false),
    TF_LINEAR("m_amt_3", 236, 210, 96, 18, PAGE_MOD, false),
    TF_LINEAR("m_amt_4", 236, 244, 96, 18, PAGE_MOD, false),
    TF_LINEAR("m_amt_5", 236, 278, 96, 18, PAGE_MOD, false),
    TF_LINEAR("m_amt_6", 236, 312, 96, 18, PAGE_MOD, false),
    TF_LINEAR("m_amt_7", 236, 346, 96, 18, PAGE_MOD, false)
};

static const ft2_ui_tf_combo_box_desc_t v2_tf_combos[] =
{
    TF_COMBO("preset_combo", 104, 8, 248, 18, PAGE_GLOBAL),
    TF_COMBO("p_58", 18, 92, 110, 18, PAGE_VOICE),
    TF_COMBO("p_2", 160, 92, 96, 18, PAGE_VOICE),
    TF_COMBO("p_8", 320, 92, 96, 18, PAGE_VOICE),
    TF_COMBO("p_9", 420, 92, 42, 18, PAGE_VOICE),
    TF_COMBO("p_14", 480, 92, 96, 18, PAGE_VOICE),
    TF_COMBO("p_15", 580, 92, 42, 18, PAGE_VOICE),

    TF_COMBO("p_20", 18, 92, 140, 18, PAGE_FILTER),
    TF_COMBO("p_23", 222, 92, 140, 18, PAGE_FILTER),
    TF_COMBO("p_26", 426, 92, 180, 18, PAGE_FILTER),
    TF_COMBO("p_28", 18, 260, 140, 18, PAGE_FILTER),
    TF_COMBO("p_68", 330, 260, 140, 18, PAGE_FILTER),

    TF_COMBO("p_44", 18, 130, 84, 18, PAGE_LFOENV),
    TF_COMBO("p_45", 110, 130, 84, 18, PAGE_LFOENV),
    TF_COMBO("p_46", 18, 156, 84, 18, PAGE_LFOENV),
    TF_COMBO("p_49", 110, 156, 84, 18, PAGE_LFOENV),
    TF_COMBO("p_51", 330, 130, 84, 18, PAGE_LFOENV),
    TF_COMBO("p_52", 422, 130, 84, 18, PAGE_LFOENV),
    TF_COMBO("p_53", 330, 156, 84, 18, PAGE_LFOENV),
    TF_COMBO("p_56", 422, 156, 84, 18, PAGE_LFOENV),

    TF_COMBO("p_66", 18, 294, 140, 18, PAGE_FX),
    TF_COMBO("p_79", 330, 260, 84, 18, PAGE_FX),
    TF_COMBO("p_80", 428, 260, 84, 18, PAGE_FX),
    TF_COMBO("p_81", 526, 260, 84, 18, PAGE_FX),

    TF_COMBO("g_13", 18, 260, 84, 18, PAGE_MASTER),
    TF_COMBO("g_14", 116, 260, 84, 18, PAGE_MASTER),
    TF_COMBO("g_15", 214, 260, 84, 18, PAGE_MASTER),

    TF_COMBO("mod_bank_combo", 516, 74, 96, 18, PAGE_MOD),
    TF_COMBO("m_src_0", 72, 108, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_0", 340, 108, 268, 18, PAGE_MOD),
    TF_COMBO("m_src_1", 72, 142, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_1", 340, 142, 268, 18, PAGE_MOD),
    TF_COMBO("m_src_2", 72, 176, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_2", 340, 176, 268, 18, PAGE_MOD),
    TF_COMBO("m_src_3", 72, 210, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_3", 340, 210, 268, 18, PAGE_MOD),
    TF_COMBO("m_src_4", 72, 244, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_4", 340, 244, 268, 18, PAGE_MOD),
    TF_COMBO("m_src_5", 72, 278, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_5", 340, 278, 268, 18, PAGE_MOD),
    TF_COMBO("m_src_6", 72, 312, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_6", 340, 312, 268, 18, PAGE_MOD),
    TF_COMBO("m_src_7", 72, 346, 156, 18, PAGE_MOD),
    TF_COMBO("m_dst_7", 340, 346, 268, 18, PAGE_MOD)
};

static const ft2_ui_tf_level_meter_desc_t v2_tf_meters[] =
{
    TF_METER("voice_meter", 540, 10, 44, 12, PAGE_GLOBAL, 12, true)
};

static const ft2_ui_tf_envelope_display_desc_t v2_tf_envs[] =
{
    TF_ENV("amp_env_display", 18, 222, 280, 72, PAGE_LFOENV),
    TF_ENV("eg2_env_display", 330, 222, 280, 72, PAGE_LFOENV)
};

static const ft2_ui_tf_group_box_desc_t v2_tf_groups[] =
{
    TF_GROUP("voice_group", 8, 56, 136, 160, PAGE_VOICE, "Voice"),
    TF_GROUP("osc1_group", 152, 56, 152, 160, PAGE_VOICE, "Osc 1"),
    TF_GROUP("osc2_group", 312, 56, 152, 160, PAGE_VOICE, "Osc 2"),
    TF_GROUP("osc3_group", 472, 56, 152, 160, PAGE_VOICE, "Osc 3"),
    TF_GROUP("waveform_group", 8, 224, 616, 168, PAGE_VOICE, "Waveform"),

    TF_GROUP("vcf1_group", 8, 56, 196, 160, PAGE_FILTER, "VCF 1"),
    TF_GROUP("vcf2_group", 212, 56, 196, 160, PAGE_FILTER, "VCF 2"),
    TF_GROUP("routing_group", 416, 56, 208, 160, PAGE_FILTER, "Routing"),
    TF_GROUP("voice_dist_group", 8, 224, 304, 168, PAGE_FILTER, "Voice Dist"),
    TF_GROUP("channel_dist_group", 320, 224, 304, 168, PAGE_FILTER, "Channel Dist"),

    TF_GROUP("lfo1_group", 8, 56, 300, 152, PAGE_LFOENV, "LFO 1"),
    TF_GROUP("lfo2_group", 320, 56, 304, 152, PAGE_LFOENV, "LFO 2"),
    TF_GROUP("amp_env_group", 8, 216, 300, 176, PAGE_LFOENV, "Amp EG"),
    TF_GROUP("eg2_group", 320, 216, 304, 176, PAGE_LFOENV, "EG 2"),

    TF_GROUP("fx_voice_dist_group", 8, 56, 200, 156, PAGE_FX, "Voice Dist"),
    TF_GROUP("fx_channel_dist_group", 216, 56, 200, 156, PAGE_FX, "Channel Dist"),
    TF_GROUP("chorus_group", 424, 56, 200, 156, PAGE_FX, "Chorus / Flanger"),
    TF_GROUP("send_group", 8, 224, 304, 168, PAGE_FX, "Patch FX Sends"),
    TF_GROUP("patch_comp_group", 320, 224, 304, 168, PAGE_FX, "Patch Compressor"),

    TF_GROUP("master_reverb_group", 8, 56, 196, 156, PAGE_MASTER, "Reverb"),
    TF_GROUP("master_delay_group", 212, 56, 236, 156, PAGE_MASTER, "Stereo Delay"),
    TF_GROUP("master_filter_group", 456, 56, 168, 156, PAGE_MASTER, "Post Filters"),
    TF_GROUP("master_comp_group", 8, 224, 616, 168, PAGE_MASTER, "Sum Compressor"),

    TF_GROUP("mod_group", 8, 56, 616, 336, PAGE_MOD, "Mod Matrix")
};

const ft2_ui_layout_desc_t ft2_v2_complete_layout_layout =
{
    "ft2_v2_complete_layout",
    FT2_UI_SCHEMA_VERSION,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, (uint16_t)(sizeof(v2_waveform_views) / sizeof(v2_waveform_views[0])) }, v2_waveform_views,
    { 0, (uint16_t)(sizeof(v2_tf_buttons) / sizeof(v2_tf_buttons[0])) }, v2_tf_buttons,
    { 0, 0 }, NULL,
    { 0, (uint16_t)(sizeof(v2_tf_labels) / sizeof(v2_tf_labels[0])) }, v2_tf_labels,
    { 0, (uint16_t)(sizeof(v2_tf_rotaries) / sizeof(v2_tf_rotaries[0])) }, v2_tf_rotaries,
    { 0, (uint16_t)(sizeof(v2_tf_linears) / sizeof(v2_tf_linears[0])) }, v2_tf_linears,
    { 0, (uint16_t)(sizeof(v2_tf_combos) / sizeof(v2_tf_combos[0])) }, v2_tf_combos,
    { 0, (uint16_t)(sizeof(v2_tf_meters) / sizeof(v2_tf_meters[0])) }, v2_tf_meters,
    { 0, 0 }, NULL,
    { 0, (uint16_t)(sizeof(v2_tf_envs) / sizeof(v2_tf_envs[0])) }, v2_tf_envs,
    { 0, (uint16_t)(sizeof(v2_tf_groups) / sizeof(v2_tf_groups[0])) }, v2_tf_groups,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL,
    { 0, 0 }, NULL
};
