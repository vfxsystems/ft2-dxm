#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_bmp.h"
#include "ft2_inst_ed.h"
#include "ft2_structs.h"
#include "ft2_ostirus_complete_layout.h"
#include "ft2_ostirus_complete_layout_schema.h"
#include "shared/ft2_ui_assets.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef CLAMP
#define CLAMP(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))
#endif

extern struct editor_t editor;
extern instr_t *instr[128 + 4];

OsTirusCompleteLayout* g_active_ostirus_layout = NULL;

typedef struct
{
    bool dragging;
    int dragIndex;
    int selectedIndex;
} OsTirusEnvDragState;

static OsTirusEnvDragState g_env_state[2];

void osti_update_all_widgets_from_synth(OsTirusCompleteLayout *layout);
static int osti_current_instr_id(const OsTirusCompleteLayout *layout);
static inline float osti_clampf(float v, float lo, float hi);
static void osti_sync_browser_selection_to_current(OsTirusCompleteLayout *layout);
static void osti_refresh_browser_controls(OsTirusCompleteLayout *layout);
static void osti_browser_reset_filters(OsTirusCompleteLayout *layout);

static const char *const k_page_names[OSTI_PAGE_COUNT] =
{
    "Common",
    "Filter",
    "Mod Matrix",
    "FX",
    "LFO",
    "Arp",
    "Browser"
};

static const char *const k_filter1_mode_items[] =
{
    "Low Pass", "High Pass", "Band Pass", "Band Stop", "Analog 1 Pole", "Analog 2 Pole", "Analog 3 Pole", "Analog 4 Pole"
};

static const char *const k_filter2_mode_items[] =
{
    "Low Pass", "High Pass", "Band Pass", "Band Stop"
};

static const char *const k_filter_routing_items[] =
{
    "Serial 4", "Serial 6", "Parallel 4", "Split Mode"
};

static const char *const k_subosc_shape_items[] =
{
    "Square", "Triangle"
};

static const char *const k_key_mode_items[] =
{
    "Poly", "Mono 1", "Mono 2", "Mono 3", "Mono 4", "Hold"
};

static const char *const k_lfo_mode_items[] =
{
    "Poly", "Mono"
};

static const char *const k_bool_items[] =
{
    "Off", "On"
};

static const char *const k_chorus_type_items[] =
{
    "Off", "Classic", "Vintage", "Hyper Chorus", "Air Chorus", "Vibrato", "Rotary Speaker"
};

static const char *const k_delay_lfo_shape_items[] =
{
    "Sine", "Triangle", "Saw", "Square", "S&H", "S&G"
};

static const char *const k_delay_mode_items[] =
{
    "Off", "Simple Delay", "Ping Pong 2:1", "Ping Pong 4:3", "Ping Pong 4:1", "Ping Pong 8:7",
    "Pattern 1+1", "Pattern 2+1", "Pattern 3+1", "Pattern 4+1", "Pattern 5+1", "Pattern 2+3",
    "Pattern 2+5", "Pattern 3+2", "Pattern 3+3", "Pattern 3+4", "Pattern 3+5", "Pattern 4+3",
    "Pattern 4+5", "Pattern 5+2", "Pattern 5+3", "Pattern 5+4", "Pattern 5+5"
};

static const char *const k_mod_source_items[] =
{
    "Off", "LFO 1", "LFO 1 * MW", "LFO 1 * AT", "LFO 2", "Filter Env", "Amp Env", "Wave Env",
    "Free Env", "Key Follow", "Keytrack", "Velocity", "Release Vel", "Aftertouch", "Poly Pressure",
    "Pitch Bend", "Modwheel", "Sustain Pedal", "Foot Control", "Breath Control", "Control W",
    "Control X", "Control Y", "Control Z", "Control Delay", "Modifier #1", "Modifier #2",
    "Modifier #3", "Modifier #4", "MIDI Clock", "Min", "Max"
};

static const char *const k_mod_dest_items[] =
{
    "Pitch", "Osc 1 Pitch", "Osc 2 Pitch", "Wave 1 Startwave", "Wave 2 Startwave", "Mix Wave 1",
    "Mix Wave 2", "Mix Ring Mod", "Mix Noise", "Filter 1 Cutoff", "Filter 1 Resonance",
    "Filter 2 Cutoff", "Volume", "Pan", "Filter Env Attack", "Filter Env Decay",
    "Filter Env Sustain", "Filter Env Release", "Amp Env Attack", "Amp Env Decay",
    "Amp Env Sustain", "Amp Env Release", "Wave Env Times", "Wave Env Levels", "Free Env Times",
    "Free Env Levels", "LFO 1 Rate", "LFO 1 Level", "LFO 2 Rate", "LFO 2 Level",
    "Mod #1 Amount", "Mod #2 Amount", "Mod #3 Amount", "Mod #4 Amount", "FM Amount",
    "Filter 1 Extra"
};

static const char *const k_arp_mode_items[] =
{
    "Off", "On", "Hold", "Sound"
};

static const char *const k_arp_clock_items[] =
{
    "1/1", "1/2 D", "1/2 T", "1/2", "1/4 D", "1/4 T", "1/4", "1/8 D",
    "1/8 T", "1/8", "1/16 D", "1/16 T", "1/16", "1/32 D", "1/32 T", "1/32"
};

static const char *const k_arp_pattern_items[] =
{
    "Off", "User", "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "11", "12", "13", "14", "15"
};

static const char *const k_arp_direction_items[] =
{
    "Up", "Down", "Up/Down", "Random"
};

static const char *const k_arp_note_order_items[] =
{
    "Played", "Low", "High", "Chord"
};

static const char *const k_arp_velocity_items[] =
{
    "Track", "Fixed"
};

static const char *const k_lfo_shape_prefix[] =
{
    "Sine", "Triangle", "Saw", "Square", "S&H", "S&G"
};

static bool osti_is_arp_step_widget(const TunefishWidget *widget)
{
    return widget && strncmp(widget->name, "arp_step_", 9) == 0;
}

static int osti_arp_step_index_from_name(const char *name)
{
    if (!name)
        return -1;

    int idx = -1;
    if (sscanf(name, "arp_step_%d", &idx) != 1)
        return -1;

    if (idx < 1 || idx > OSTI_ARP_STEP_COUNT)
        return -1;

    return idx - 1;
}

static int osti_arp_group_for_step(int stepIndex)
{
    if (stepIndex < 0)
        return -1;
    return stepIndex / 4;
}

static instr_t *osti_current_instrument_ptr(const OsTirusCompleteLayout *layout)
{
    const int instrID = osti_current_instr_id(layout);
    if (instrID < 1 || instrID > MAX_INST)
        return NULL;
    return instr[instrID];
}

static uint8_t osti_arp_pack_group_bits(const instr_t *ins, int group)
{
    if (!ins || group < 0 || group >= 4)
        return 0;

    uint8_t packed = 0;
    const int baseStep = group * 4;
    for (int i = 0; i < 4; ++i)
    {
        const int step = baseStep + i;
        if (step < 0 || step >= OSTI_ARP_STEP_COUNT)
            continue;
        if (ins->osTirusArpStepGate[step] != 0)
            packed |= (uint8_t)(1u << (3 - i));
    }

    return packed;
}

static void osti_arp_write_group_to_synth(OsTirusCompleteLayout *layout, int group)
{
    if (!layout || group < 0 || group >= 4)
        return;

    const int instrID = osti_current_instr_id(layout);
    if (instrID < 1 || instrID > MAX_INST)
        return;

    instr_t *ins = instr[instrID];
    if (!ins || !ins->useOsTirus)
        return;

    const uint8_t packed = osti_arp_pack_group_bits(ins, group);
    ft2_ostirus_set_param_for_instrument(instrID, 240 + group, (float)packed / 15.0f);
}

static void osti_sync_arp_widgets_from_state(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    instr_t *ins = osti_current_instrument_ptr(layout);
    for (int i = 0; i < OSTI_ARP_STEP_COUNT; ++i)
    {
        TunefishWidget *widget = layout->arp_step_widgets[i];
        if (!widget)
            continue;

        if (!ins || !ins->useOsTirus)
        {
            widget->pressed = false;
            widget->value = 0.0f;
            widget->modValue = 0.0f;
            continue;
        }

        widget->pressed = (ins->osTirusArpStepGate[i] != 0);
        widget->value = osti_clampf((float)ins->osTirusArpStepVelocity[i] / 127.0f, 0.0f, 1.0f);
        widget->modValue = osti_clampf((float)ins->osTirusArpStepLength[i] / 127.0f, 0.0f, 1.0f);
    }
}

static bool osti_update_arp_step_from_mouse(OsTirusCompleteLayout *layout, TunefishWidget *widget, int mouseX, int mouseY, bool pressed)
{
    if (!layout || !widget || !osti_is_arp_step_widget(widget))
        return false;

    const int stepIndex = osti_arp_step_index_from_name(widget->name);
    if (stepIndex < 0 || stepIndex >= OSTI_ARP_STEP_COUNT)
        return false;

    if (!pressed)
    {
        if (layout->arp_drag_step_index == stepIndex)
            layout->arp_drag_step_index = -1;
        return true;
    }

    const int relX = mouseX - widget->x;
    const int relY = mouseY - widget->y;
    const float velocity = osti_clampf(1.0f - ((float)relY / (float)widget->h), 0.0f, 1.0f);
    const float noteLength = osti_clampf((float)relX / (float)widget->w, 0.0f, 1.0f);

    widget->value = velocity;
    widget->modValue = noteLength;
    widget->pressed = true;
    layout->arp_drag_step_index = stepIndex;

    instr_t *ins = osti_current_instrument_ptr(layout);
    if (ins && ins->useOsTirus)
    {
        ins->osTirusArpStepVelocity[stepIndex] = (uint8_t)lroundf(widget->value * 127.0f);
        ins->osTirusArpStepLength[stepIndex] = (uint8_t)lroundf(widget->modValue * 127.0f);
        ins->osTirusArpStepGate[stepIndex] = 1;

        const int group = osti_arp_group_for_step(stepIndex);
        osti_arp_write_group_to_synth(layout, group);
    }

    osti_update_all_widgets_from_synth(layout);
    return true;
}

static const int k_env_param_ids[2][4] =
{
    { 54, 55, 56, 58 }, /* filter env attack/decay/sustain/release */
    { 59, 60, 61, 63 }  /* amp env attack/decay/sustain/release */
};

static inline int osti_clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float osti_clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int osti_current_instr_id(const OsTirusCompleteLayout *layout)
{
    if (layout && layout->active_instrument_id >= 1 && layout->active_instrument_id <= MAX_INST)
        return layout->active_instrument_id;

    if (editor.curInstr >= 1 && editor.curInstr <= MAX_INST)
        return editor.curInstr;

    return 0;
}

static void osti_format_bank_name(int bank, char *out, size_t outSize)
{
    if (!out || outSize == 0)
        return;

    if (bank < 0)
    {
        snprintf(out, outSize, "--");
    }
    else
    {
        char suffix[16];
        int suffixLen = 0;
        int value = bank;

        do
        {
            suffix[suffixLen++] = (char)('A' + (value % 26));
            value = (value / 26) - 1;
        }
        while (value >= 0 && suffixLen < (int)(sizeof(suffix) - 1));

        for (int i = 0; i < suffixLen / 2; ++i)
        {
            const char tmp = suffix[i];
            suffix[i] = suffix[suffixLen - 1 - i];
            suffix[suffixLen - 1 - i] = tmp;
        }
        suffix[suffixLen] = '\0';
        snprintf(out, outSize, "ROM %s", suffix);
    }
}

static void osti_populate_bank_select_combo(TunefishWidget *combo)
{
    if (!combo)
        return;

    tf_widget_clear_combo_items(combo);

    int maxBank = -1;
    const int presetCount = ft2_ostirus_get_factory_preset_count();
    for (int i = 0; i < presetCount; ++i)
    {
        const int bank = ft2_ostirus_get_factory_preset_bank(i);
        if (bank > maxBank)
            maxBank = bank;
    }

    for (int bank = 0; bank <= maxBank; ++bank)
    {
        char bankName[32];
        osti_format_bank_name(bank, bankName, sizeof(bankName));
        tf_widget_add_combo_item(combo, bankName);
    }
}

typedef struct
{
    int x, y, w, h;
} osti_rect_t;

static osti_rect_t osti_browser_search_rect(void)
{
    const osti_rect_t r = { 20, 78, 240, 18 };
    return r;
}

static osti_rect_t osti_browser_column_rect(OsTirusBrowserColumn column)
{
    switch (column)
    {
        default:
        case OSTI_BROWSER_COLUMN_ROM:      { const osti_rect_t r = { 20, 126, 96, 160 }; return r; }
        case OSTI_BROWSER_COLUMN_BANK:     { const osti_rect_t r = { 124, 126, 96, 160 }; return r; }
        case OSTI_BROWSER_COLUMN_CATEGORY: { const osti_rect_t r = { 228, 126, 160, 160 }; return r; }
        case OSTI_BROWSER_COLUMN_PATCH:    { const osti_rect_t r = { 396, 126, 212, 160 }; return r; }
    }
}

static bool osti_rect_contains(const osti_rect_t *r, int x, int y)
{
    return r && x >= r->x && x < (r->x + r->w) && y >= r->y && y < (r->y + r->h);
}

static int osti_browser_visible_rows(const osti_rect_t *r)
{
    const int headerH = 14;
    const int itemH = 11;
    return (r && r->h > headerH) ? ((r->h - headerH - 4) / itemH) : 0;
}

static void osti_browser_clamp_scrolls(OsTirusCompleteLayout *layout);
static bool osti_browser_preset_matches(const OsTirusCompleteLayout *layout, int presetIndex);
static void osti_browser_rebuild_filtered_presets(OsTirusCompleteLayout *layout);
static void osti_browser_rebuild_bank_list(OsTirusCompleteLayout *layout);
static void osti_browser_rebuild_category_list(OsTirusCompleteLayout *layout);

static bool osti_browser_has_category_filters(const OsTirusCompleteLayout *layout)
{
    if (!layout)
        return false;

    for (int i = 0; i < FT2_OSTIRUS_MAX_CATEGORIES; ++i)
    {
        if (layout->browser_category_selected[i])
            return true;
    }

    return false;
}

static bool osti_browser_search_match(const char *text, const char *needle)
{
    if (!needle || !needle[0])
        return true;
    if (!text || !text[0])
        return false;

    char textBuf[160];
    char needleBuf[48];
    size_t i;

    for (i = 0; i < sizeof(textBuf) - 1 && text[i]; ++i)
        textBuf[i] = (char)tolower((unsigned char)text[i]);
    textBuf[i] = '\0';

    for (i = 0; i < sizeof(needleBuf) - 1 && needle[i]; ++i)
        needleBuf[i] = (char)tolower((unsigned char)needle[i]);
    needleBuf[i] = '\0';

    return strstr(textBuf, needleBuf) != NULL;
}

static bool osti_browser_preset_matches_category_filter(const OsTirusCompleteLayout *layout, int presetIndex)
{
    if (!layout || !osti_browser_has_category_filters(layout))
        return true;

    const int c1 = ft2_ostirus_get_factory_preset_category1(presetIndex);
    const int c2 = ft2_ostirus_get_factory_preset_category2(presetIndex);

    if (c1 >= 0 && c1 < FT2_OSTIRUS_MAX_CATEGORIES && layout->browser_category_selected[c1])
        return true;
    if (c2 >= 0 && c2 < FT2_OSTIRUS_MAX_CATEGORIES && layout->browser_category_selected[c2])
        return true;

    return false;
}

static bool osti_browser_preset_matches(const OsTirusCompleteLayout *layout, int presetIndex)
{
    if (!layout || presetIndex < 0)
        return false;

    if (layout->browser_selected_bank >= 0 &&
        ft2_ostirus_get_factory_preset_bank(presetIndex) != layout->browser_selected_bank)
    {
        return false;
    }

    if (!osti_browser_preset_matches_category_filter(layout, presetIndex))
        return false;

    if (layout->browser_search_text[0] != '\0')
    {
        const char *plainName = ft2_ostirus_get_factory_preset_plain_name(presetIndex);
        const char *displayName = ft2_ostirus_get_factory_preset_name(presetIndex);
        const char *category1 = ft2_ostirus_get_category_name(ft2_ostirus_get_factory_preset_category1(presetIndex));
        const char *category2 = ft2_ostirus_get_category_name(ft2_ostirus_get_factory_preset_category2(presetIndex));

        if (!osti_browser_search_match(plainName, layout->browser_search_text) &&
            !osti_browser_search_match(displayName, layout->browser_search_text) &&
            !osti_browser_search_match(category1, layout->browser_search_text) &&
            !osti_browser_search_match(category2, layout->browser_search_text))
        {
            return false;
        }
    }

    return true;
}

static void osti_browser_rebuild_bank_list(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    layout->browser_bank_count = 0;

    const int presetCount = ft2_ostirus_get_factory_preset_count();
    for (int i = 0; i < presetCount; ++i)
    {
        const int bank = ft2_ostirus_get_factory_preset_bank(i);
        bool exists = false;

        for (int j = 0; j < layout->browser_bank_count; ++j)
        {
            if (layout->browser_bank_values[j] == bank)
            {
                exists = true;
                break;
            }
        }

        if (!exists && layout->browser_bank_count < OSTI_MAX_BROWSER_BANKS)
            layout->browser_bank_values[layout->browser_bank_count++] = bank;
    }

    if (layout->browser_selected_bank >= 0)
    {
        bool found = false;
        for (int i = 0; i < layout->browser_bank_count; ++i)
        {
            if (layout->browser_bank_values[i] == layout->browser_selected_bank)
            {
                found = true;
                break;
            }
        }
        if (!found)
            layout->browser_selected_bank = -1;
    }
}

static void osti_browser_rebuild_category_list(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    bool used[FT2_OSTIRUS_MAX_CATEGORIES] = { false };
    layout->browser_category_count = 0;

    const int presetCount = ft2_ostirus_get_factory_preset_count();
    for (int i = 0; i < presetCount; ++i)
    {
        if (layout->browser_selected_bank >= 0 &&
            ft2_ostirus_get_factory_preset_bank(i) != layout->browser_selected_bank)
        {
            continue;
        }

        const int categories[2] =
        {
            ft2_ostirus_get_factory_preset_category1(i),
            ft2_ostirus_get_factory_preset_category2(i)
        };

        for (int c = 0; c < 2; ++c)
        {
            const int value = categories[c];
            if (value <= 0 || value >= FT2_OSTIRUS_MAX_CATEGORIES || used[value])
                continue;
            used[value] = true;
            layout->browser_category_values[layout->browser_category_count++] = value;
        }
    }

    for (int i = 0; i < FT2_OSTIRUS_MAX_CATEGORIES; ++i)
    {
        if (!used[i])
            layout->browser_category_selected[i] = false;
    }
}

static void osti_browser_rebuild_filtered_presets(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    layout->browser_filtered_preset_count = 0;

    const int presetCount = ft2_ostirus_get_factory_preset_count();
    for (int i = 0; i < presetCount; ++i)
    {
        if (!osti_browser_preset_matches(layout, i))
            continue;
        if (layout->browser_filtered_preset_count >= OSTI_MAX_BROWSER_PRESETS)
            break;
        layout->browser_filtered_preset_indices[layout->browser_filtered_preset_count++] = i;
    }

    if (layout->browser_selected_preset_index >= 0)
    {
        bool found = false;
        for (int i = 0; i < layout->browser_filtered_preset_count; ++i)
        {
            if (layout->browser_filtered_preset_indices[i] == layout->browser_selected_preset_index)
            {
                found = true;
                break;
            }
        }
        if (!found)
            layout->browser_selected_preset_index = (layout->browser_filtered_preset_count > 0) ?
                layout->browser_filtered_preset_indices[0] : -1;
    }
    else if (layout->browser_filtered_preset_count > 0)
    {
        layout->browser_selected_preset_index = layout->browser_filtered_preset_indices[0];
    }
}

static void osti_browser_clamp_scrolls(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    int counts[OSTI_BROWSER_COLUMN_COUNT];
    counts[OSTI_BROWSER_COLUMN_ROM] = 1;
    counts[OSTI_BROWSER_COLUMN_BANK] = layout->browser_bank_count;
    counts[OSTI_BROWSER_COLUMN_CATEGORY] = layout->browser_category_count;
    counts[OSTI_BROWSER_COLUMN_PATCH] = layout->browser_filtered_preset_count;

    for (int i = 0; i < OSTI_BROWSER_COLUMN_COUNT; ++i)
    {
        const osti_rect_t rect = osti_browser_column_rect((OsTirusBrowserColumn)i);
        const int visible = osti_browser_visible_rows(&rect);
        const int maxScroll = (counts[i] > visible) ? (counts[i] - visible) : 0;
        if (layout->browser_scroll_offsets[i] < 0)
            layout->browser_scroll_offsets[i] = 0;
        if (layout->browser_scroll_offsets[i] > maxScroll)
            layout->browser_scroll_offsets[i] = maxScroll;
    }
}

static void osti_update_browser_labels(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    const int presetCount = ft2_ostirus_get_factory_preset_count();
    int bankCount = layout->browser_bank_count;

    if (layout->browser_status_label)
    {
        char statusText[128];
        if (!ft2_ostirus_is_initialized())
        {
            snprintf(statusText, sizeof(statusText), "ROM status: No ROM loaded");
        }
        else
        {
            snprintf(statusText, sizeof(statusText), "ROM status: Ready | Banks %d | Presets %d", bankCount, presetCount);
        }
        tf_widget_set_label(layout->browser_status_label, statusText);
    }

    if (layout->browser_selected_label)
    {
        const int selectedIndex = layout->browser_selected_preset_index;
        const char *name = (selectedIndex >= 0) ? ft2_ostirus_get_factory_preset_plain_name(selectedIndex) : NULL;
        char bankName[32];
        char selectedText[192];
        char programText[8];
        const int bank = (selectedIndex >= 0) ? ft2_ostirus_get_factory_preset_bank(selectedIndex) : -1;
        const int program = (selectedIndex >= 0) ? ft2_ostirus_get_factory_preset_program(selectedIndex) : -1;
        osti_format_bank_name(bank, bankName, sizeof(bankName));
        if (program >= 0)
            snprintf(programText, sizeof(programText), "%02d", program + 1);
        else
            snprintf(programText, sizeof(programText), "--");
        snprintf(selectedText, sizeof(selectedText), "Selected: %s %s  %s",
                 bankName, programText,
                 (name && *name) ? name : "--");
        tf_widget_set_label(layout->browser_selected_label, selectedText);
    }

    if (layout->browser_current_label)
    {
        const int instrID = osti_current_instr_id(layout);
        const int currentIndex = (instrID > 0) ? ft2_ostirus_get_current_preset_for_instrument(instrID) : -1;
        const char *name = (currentIndex >= 0) ? ft2_ostirus_get_factory_preset_plain_name(currentIndex) : NULL;
        const int bank = (currentIndex >= 0) ? ft2_ostirus_get_factory_preset_bank(currentIndex) : -1;
        const int program = (currentIndex >= 0) ? ft2_ostirus_get_factory_preset_program(currentIndex) : -1;
        char bankName[32];
        char currentText[192];
        char programText[8];
        osti_format_bank_name(bank, bankName, sizeof(bankName));
        if (program >= 0)
            snprintf(programText, sizeof(programText), "%02d", program + 1);
        else
            snprintf(programText, sizeof(programText), "--");
        snprintf(currentText, sizeof(currentText), "Loaded: %s %s  %s",
                 bankName, programText,
                 (name && *name) ? name : (ft2_ostirus_is_initialized() ? "Init" : "No ROM"));
        tf_widget_set_label(layout->browser_current_label, currentText);
    }
}

static void osti_refresh_browser_controls(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    if (layout->browser_selected_preset_index < 0)
        osti_sync_browser_selection_to_current(layout);

    osti_browser_rebuild_bank_list(layout);
    osti_browser_rebuild_category_list(layout);
    osti_browser_rebuild_filtered_presets(layout);
    osti_browser_clamp_scrolls(layout);
    osti_update_browser_labels(layout);
}

static void osti_sync_browser_selection_to_current(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    const int instrID = osti_current_instr_id(layout);
    int presetIndex = (instrID > 0) ? ft2_ostirus_get_current_preset_for_instrument(instrID) : -1;
    if (presetIndex < 0)
        presetIndex = ft2_ostirus_get_default_factory_preset_index();

    layout->browser_selected_preset_index = presetIndex;
    layout->browser_selected_bank = (presetIndex >= 0) ? ft2_ostirus_get_factory_preset_bank(presetIndex) : -1;
}

static bool osti_load_browser_selection(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return false;

    const int instrID = osti_current_instr_id(layout);
    if (instrID <= 0)
        return false;

    const int presetIndex = layout->browser_selected_preset_index;
    if (presetIndex < 0)
        return false;

    if (!ft2_ostirus_load_factory_preset_for_instrument(instrID, presetIndex))
        return false;

    osti_sync_browser_selection_to_current(layout);
    osti_update_all_widgets_from_synth(layout);
    return true;
}

static bool osti_step_browser_preset(OsTirusCompleteLayout *layout, int delta)
{
    if (!layout)
        return false;

    const int count = ft2_ostirus_get_factory_preset_count();
    if (count <= 0)
        return false;

    int presetIndex = layout->browser_selected_preset_index;
    if (presetIndex < 0)
    {
        const int instrID = osti_current_instr_id(layout);
        presetIndex = (instrID > 0) ? ft2_ostirus_get_current_preset_for_instrument(instrID) : -1;
    }
    if (presetIndex < 0)
        presetIndex = ft2_ostirus_get_default_factory_preset_index();
    if (presetIndex < 0)
        return false;

    presetIndex = (presetIndex + delta + count) % count;
    layout->browser_selected_preset_index = presetIndex;
    layout->browser_selected_bank = ft2_ostirus_get_factory_preset_bank(presetIndex);
    return osti_load_browser_selection(layout);
}

static bool osti_is_placeholder_widget(const TunefishWidget *widget)
{
    if (!widget) return true;
    if (widget->name[0] == '\0') return true;
    if (widget->w == 0 || widget->h == 0) return true;
    if (strcmp(widget->name, "slot_display") == 0) return true;
    if (strncmp(widget->name, "unused_", 7) == 0) return true;
    return false;
}

static bool osti_widget_visible_on_page(const TunefishWidget *widget, int currentPage)
{
    if (!widget) return false;
    if (widget->page == FT2_UI_WIDGET_PAGE_BOTH) return true;
    return widget->page == (currentPage + 1);
}

static void osti_register_widget(OsTirusCompleteLayout *layout, TunefishWidget *widget, ft2_ui_widget_page_t page)
{
    if (!layout || !widget)
    {
        tf_widget_destroy(widget);
        return;
    }

    if (osti_is_placeholder_widget(widget))
    {
        tf_widget_destroy(widget);
        return;
    }

    widget->page = (int)page;
    widget->visible = osti_widget_visible_on_page(widget, layout->current_page);

    if (layout->all_widget_count < OSTI_MAX_WIDGETS)
        layout->all_widgets[layout->all_widget_count++] = widget;
    else
        tf_widget_destroy(widget);
}

static void osti_fill_combo_items_from_array(TunefishWidget *combo, const char *const *items, int count)
{
    if (!combo || !items || count <= 0) return;
    for (int i = 0; i < count; ++i)
        tf_widget_add_combo_item(combo, items[i]);
}

static void osti_fill_osc_shape_items(TunefishWidget *combo)
{
    if (!combo) return;

    tf_widget_add_combo_item(combo, "Spectral Wave");
    for (int i = 1; i <= 63; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Wave>Saw %d %%", i);
        tf_widget_add_combo_item(combo, buf);
    }

    tf_widget_add_combo_item(combo, "Sawtooth");
    for (int i = 1; i <= 62; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Saw>Pulse %d %%", i);
        tf_widget_add_combo_item(combo, buf);
    }

    tf_widget_add_combo_item(combo, "Pulse");
}

static void osti_fill_osc_wave_items(TunefishWidget *combo)
{
    if (!combo) return;
    tf_widget_add_combo_item(combo, "Sine");
    tf_widget_add_combo_item(combo, "Triangle");
    for (int i = 3; i <= 128; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Wave %d", i);
        tf_widget_add_combo_item(combo, buf);
    }
}

static void osti_fill_lfo_shape_items(TunefishWidget *combo)
{
    if (!combo) return;

    tf_widget_add_combo_item(combo, "Sine");
    tf_widget_add_combo_item(combo, "Triangle");
    tf_widget_add_combo_item(combo, "Saw");
    tf_widget_add_combo_item(combo, "Square");
    tf_widget_add_combo_item(combo, "S&H");
    tf_widget_add_combo_item(combo, "S&G");
    for (int i = 3; i <= 64; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Wave %d", i);
        tf_widget_add_combo_item(combo, buf);
    }
}

static int osti_binding_index_for_widget(const OsTirusCompleteLayout *layout, const TunefishWidget *widget)
{
    if (!layout || !widget) return -1;
    for (int i = 0; i < layout->param_binding_count; ++i)
    {
        if (layout->param_bindings[i].widget == widget)
            return i;
    }
    return -1;
}

static void osti_apply_page_button_state(OsTirusCompleteLayout *layout)
{
    if (!layout) return;

    if (layout->page_common_button)     layout->page_common_button->pressed     = (layout->current_page == OSTI_PAGE_COMMON);
    if (layout->page_filter_button)      layout->page_filter_button->pressed      = (layout->current_page == OSTI_PAGE_FILTER);
    if (layout->page_mod_matrix_button)  layout->page_mod_matrix_button->pressed  = (layout->current_page == OSTI_PAGE_MOD_MATRIX);
    if (layout->page_fx_button)          layout->page_fx_button->pressed          = (layout->current_page == OSTI_PAGE_FX);
    if (layout->page_lfo_button)         layout->page_lfo_button->pressed         = (layout->current_page == OSTI_PAGE_LFO);
    if (layout->page_arp_button)         layout->page_arp_button->pressed         = (layout->current_page == OSTI_PAGE_ARP);
    if (layout->page_browser_button)     layout->page_browser_button->pressed     = (layout->current_page == OSTI_PAGE_BROWSER);
}

static void osti_update_widget_visibility(OsTirusCompleteLayout *layout)
{
    if (!layout) return;
    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (!w) continue;
        w->visible = osti_widget_visible_on_page(w, layout->current_page);
    }

    /* Global widgets are visible on every page. */
    if (layout->title_label) layout->title_label->visible = true;
    if (layout->page_caption_label) layout->page_caption_label->visible = true;
    if (layout->preset_combo) layout->preset_combo->visible = true;
    if (layout->preset_name_label) layout->preset_name_label->visible = true;
    if (layout->slot_label) layout->slot_label->visible = true;
    if (layout->voice_meter) layout->voice_meter->visible = true;
    if (layout->close_button) layout->close_button->visible = true;
    if (layout->page_common_button) layout->page_common_button->visible = true;
    if (layout->page_filter_button) layout->page_filter_button->visible = true;
    if (layout->page_mod_matrix_button) layout->page_mod_matrix_button->visible = true;
    if (layout->page_fx_button) layout->page_fx_button->visible = true;
    if (layout->page_lfo_button) layout->page_lfo_button->visible = true;
    if (layout->page_arp_button) layout->page_arp_button->visible = true;
    if (layout->page_browser_button) layout->page_browser_button->visible = true;
    if (layout->browser_bank_combo) layout->browser_bank_combo->visible = false;
    if (layout->browser_program_combo) layout->browser_program_combo->visible = false;
    osti_apply_page_button_state(layout);
}

static void osti_style_widget(TunefishWidget *widget)
{
    if (!widget) return;

    tf_apply_tunefish_styling(widget);
    switch (widget->type)
    {
        case TF_WIDGET_BUTTON:
            tf_style_as_main_button(widget);
            break;
        case TF_WIDGET_ROTARY_SLIDER:
            tf_style_as_parameter_knob(widget);
            widget->value = 0.5f;
            break;
        case TF_WIDGET_COMBO_BOX:
            tf_apply_combo_styling(widget);
            break;
        case TF_WIDGET_LEVEL_METER:
            tf_style_as_level_indicator(widget);
            break;
        case TF_WIDGET_LABEL:
            widget->textColor = TF_COL_TEXT_NORMAL;
            break;
        case TF_WIDGET_TOGGLE_BUTTON:
            widget->textColor = TF_COL_TEXT_NORMAL;
            break;
        case TF_WIDGET_GROUP_BOX:
        case TF_WIDGET_ENVELOPE_DISPLAY:
        default:
            break;
    }

    if (strcmp(widget->name, "title_label") == 0)
        widget->textColor = TF_COL_TEXT_NORMAL;
}

static void osti_apply_all_widget_styling(OsTirusCompleteLayout *layout)
{
    if (!layout) return;
    for (int i = 0; i < layout->all_widget_count; ++i)
        osti_style_widget(layout->all_widgets[i]);
}

static void osti_update_caption(OsTirusCompleteLayout *layout)
{
    if (!layout || !layout->page_caption_label) return;
    const char *caption = k_page_names[osti_clampi(layout->current_page, 0, OSTI_PAGE_COUNT - 1)];
    tf_widget_set_label(layout->page_caption_label, caption);
}

static void osti_update_slot_label(OsTirusCompleteLayout *layout)
{
    if (!layout || !layout->slot_label) return;

    const int instrID = osti_current_instr_id(layout);
    int slot = (instrID > 0) ? ft2_ostirus_get_slot_for_instrument(instrID) : -1;
    if (slot < 0)
        tf_widget_set_label(layout->slot_label, "Part --");
    else
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "Part %02d", slot + 1);
        tf_widget_set_label(layout->slot_label, buf);
    }
}

static void osti_update_preset_name_label(OsTirusCompleteLayout *layout)
{
    if (!layout || !layout->preset_name_label) return;

    const int instrID = osti_current_instr_id(layout);
    const char *presetName = (instrID > 0) ? ft2_ostirus_get_current_preset_name_for_instrument(instrID) : NULL;
    if (presetName && *presetName)
        tf_widget_set_label(layout->preset_name_label, presetName);
    else
        tf_widget_set_label(layout->preset_name_label, ft2_ostirus_is_initialized() ? "Init" : "No ROM");
}

static void osti_update_voice_meter(OsTirusCompleteLayout *layout)
{
    if (!layout || !layout->voice_meter) return;

    const int instrID = osti_current_instr_id(layout);
    int voices = (instrID > 0) ? ft2_ostirus_get_active_voice_count(instrID) : 0;
    if (voices < 0) voices = 0;
    if (voices > FT2_OSTIRUS_MAX_SLOTS) voices = FT2_OSTIRUS_MAX_SLOTS;
    layout->voice_meter->value = (FT2_OSTIRUS_MAX_SLOTS > 0) ? ((float)voices / (float)FT2_OSTIRUS_MAX_SLOTS) : 0.0f;
    layout->voice_meter->peakLevel = layout->voice_meter->value;
}

static void osti_populate_preset_combo(OsTirusCompleteLayout *layout)
{
    if (!layout || !layout->preset_combo) return;

    tf_widget_clear_combo_items(layout->preset_combo);

    const int count = ft2_ostirus_get_factory_preset_count();
    for (int i = 0; i < count; ++i)
    {
        const char *name = ft2_ostirus_get_factory_preset_name(i);
        if (name && *name)
            tf_widget_add_combo_item(layout->preset_combo, name);
    }

    if (count <= 0)
    {
        layout->preset_combo->selectedIndex = 0;
        layout->preset_combo->value = 0.0f;
        return;
    }

    const int instrID = osti_current_instr_id(layout);
    int presetIndex = (instrID > 0) ? ft2_ostirus_get_current_preset_for_instrument(instrID) : -1;
    presetIndex = osti_clampi(presetIndex, 0, count - 1);
    layout->preset_combo->selectedIndex = presetIndex;
    layout->preset_combo->value = (count > 1) ? ((float)presetIndex / (float)(count - 1)) : 0.0f;
}

static void osti_sync_preset_combo_selection(OsTirusCompleteLayout *layout)
{
    if (!layout || !layout->preset_combo) return;
    const int count = layout->preset_combo->comboItemCount;
    if (count <= 0)
    {
        layout->preset_combo->selectedIndex = 0;
        layout->preset_combo->value = 0.0f;
        return;
    }

    const int instrID = osti_current_instr_id(layout);
    int presetIndex = (instrID > 0) ? ft2_ostirus_get_current_preset_for_instrument(instrID) : 0;
    presetIndex = osti_clampi(presetIndex, 0, count - 1);
    layout->preset_combo->selectedIndex = presetIndex;
    layout->preset_combo->value = (count > 1) ? ((float)presetIndex / (float)(count - 1)) : 0.0f;
}

static void osti_populate_combo(TunefishWidget *combo)
{
    if (!combo) return;

    tf_widget_clear_combo_items(combo);

    if (strcmp(combo->name, "cc_17_osc1_shape") == 0 ||
        strcmp(combo->name, "cc_22_osc2_shape") == 0)
    {
        osti_fill_osc_shape_items(combo);
    }
    else if (strcmp(combo->name, "cc_19_osc1_wavetable") == 0 ||
             strcmp(combo->name, "cc_24_osc2_wave_select") == 0)
    {
        osti_fill_osc_wave_items(combo);
    }
    else if (strcmp(combo->name, "cc_35_subosc_shape") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_subosc_shape_items, (int)(sizeof(k_subosc_shape_items) / sizeof(k_subosc_shape_items[0])));
    }
    else if (strcmp(combo->name, "cc_51_filter1_mode") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_filter1_mode_items, (int)(sizeof(k_filter1_mode_items) / sizeof(k_filter1_mode_items[0])));
    }
    else if (strcmp(combo->name, "cc_52_filter2_mode") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_filter2_mode_items, (int)(sizeof(k_filter2_mode_items) / sizeof(k_filter2_mode_items[0])));
    }
    else if (strcmp(combo->name, "cc_53_filter_routing") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_filter_routing_items, (int)(sizeof(k_filter_routing_items) / sizeof(k_filter_routing_items[0])));
    }
    else if (strcmp(combo->name, "cc_68_lfo1_shape") == 0 ||
             strcmp(combo->name, "cc_80_lfo2_shape") == 0)
    {
        osti_fill_lfo_shape_items(combo);
    }
    else if (strcmp(combo->name, "cc_69_lfo1_env_mode") == 0 ||
             strcmp(combo->name, "cc_70_lfo1_mode") == 0 ||
             strcmp(combo->name, "cc_81_lfo2_env_mode") == 0 ||
             strcmp(combo->name, "cc_82_lfo2_mode") == 0 ||
             strcmp(combo->name, "cc_73_lfo1_keytrigger") == 0 ||
             strcmp(combo->name, "cc_85_lfo2_keytrigger") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_bool_items, (int)(sizeof(k_bool_items) / sizeof(k_bool_items[0])));
    }
    else if (strcmp(combo->name, "cc_103_chorus_type") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_chorus_type_items, (int)(sizeof(k_chorus_type_items) / sizeof(k_chorus_type_items[0])));
    }
    else if (strcmp(combo->name, "cc_110_chorus_lfo_shape") == 0 ||
             strcmp(combo->name, "cc_118_delay_lfo_shape") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_delay_lfo_shape_items, (int)(sizeof(k_delay_lfo_shape_items) / sizeof(k_delay_lfo_shape_items[0])));
    }
    else if (strcmp(combo->name, "cc_112_delay_mode") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_delay_mode_items, (int)(sizeof(k_delay_mode_items) / sizeof(k_delay_mode_items[0])));
    }
    else if (strcmp(combo->name, "cc_94_key_mode") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_key_mode_items, (int)(sizeof(k_key_mode_items) / sizeof(k_key_mode_items[0])));
    }
    else if (strcmp(combo->name, "cc_102_arp_mode") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_arp_mode_items, (int)(sizeof(k_arp_mode_items) / sizeof(k_arp_mode_items[0])));
    }
    else if (strcmp(combo->name, "cc_104_arp_clock") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_arp_clock_items, (int)(sizeof(k_arp_clock_items) / sizeof(k_arp_clock_items[0])));
    }
    else if (strcmp(combo->name, "cc_107_arp_pattern") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_arp_pattern_items, (int)(sizeof(k_arp_pattern_items) / sizeof(k_arp_pattern_items[0])));
    }
    else if (strcmp(combo->name, "cc_106_arp_direction") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_arp_direction_items, (int)(sizeof(k_arp_direction_items) / sizeof(k_arp_direction_items[0])));
    }
    else if (strcmp(combo->name, "cc_108_arp_note_order") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_arp_note_order_items, (int)(sizeof(k_arp_note_order_items) / sizeof(k_arp_note_order_items[0])));
    }
    else if (strcmp(combo->name, "cc_109_arp_velocity") == 0)
    {
        osti_fill_combo_items_from_array(combo, k_arp_velocity_items, (int)(sizeof(k_arp_velocity_items) / sizeof(k_arp_velocity_items[0])));
    }
    else if (strncmp(combo->name, "cc_", 3) == 0)
    {
        int paramId = -1;
        if (sscanf(combo->name, "cc_%d_", &paramId) == 1 && paramId >= 192 && paramId <= 239)
        {
            const int slotParam = (paramId - 192) % 3;
            if (slotParam == 0)
            {
                osti_fill_combo_items_from_array(combo, k_mod_source_items, (int)(sizeof(k_mod_source_items) / sizeof(k_mod_source_items[0])));
            }
            else if (slotParam == 2)
            {
                osti_fill_combo_items_from_array(combo, k_mod_dest_items, (int)(sizeof(k_mod_dest_items) / sizeof(k_mod_dest_items[0])));
            }
        }
    }
    else if (strcmp(combo->name, "cc_32_bank_select") == 0)
    {
        osti_populate_bank_select_combo(combo);
    }
}

static void osti_sync_widget_from_parameter(OsTirusCompleteLayout *layout, TunefishWidget *widget)
{
    if (!layout || !widget) return;

    const int instrID = osti_current_instr_id(layout);
    if (instrID <= 0) return;

    int paramId = -1;
    if (sscanf(widget->name, "cc_%d_", &paramId) != 1)
        return;

    float value = ft2_ostirus_get_param_for_instrument(instrID, paramId);
    value = osti_clampf(value, 0.0f, 1.0f);

    switch (widget->type)
    {
        case TF_WIDGET_ROTARY_SLIDER:
        case TF_WIDGET_LINEAR_SLIDER:
        case TF_WIDGET_PARAMETER_CONTROL:
            widget->value = value;
            break;

        case TF_WIDGET_TOGGLE_BUTTON:
            widget->pressed = (value >= 0.5f);
            widget->value = widget->pressed ? 1.0f : 0.0f;
            break;

        case TF_WIDGET_COMBO_BOX:
            if (widget->comboItemCount > 1)
            {
                int idx = (int)lroundf(value * (float)(widget->comboItemCount - 1));
                idx = osti_clampi(idx, 0, widget->comboItemCount - 1);
                widget->selectedIndex = idx;
                widget->value = (float)idx / (float)(widget->comboItemCount - 1);
            }
            else if (widget->comboItemCount == 1)
            {
                widget->selectedIndex = 0;
                widget->value = 0.0f;
            }
            break;

        default:
            break;
    }
}

void osti_update_all_widgets_from_synth(OsTirusCompleteLayout *layout)
{
    if (!layout) return;

    osti_populate_preset_combo(layout);
    osti_sync_preset_combo_selection(layout);
    osti_update_preset_name_label(layout);
    osti_update_slot_label(layout);
    osti_update_voice_meter(layout);
    osti_sync_arp_widgets_from_state(layout);
    osti_refresh_browser_controls(layout);

    for (int i = 0; i < layout->all_widget_count; ++i)
        osti_sync_widget_from_parameter(layout, layout->all_widgets[i]);

    if (layout->title_label)
        tf_widget_set_label(layout->title_label, "OsTIrus");

    osti_update_caption(layout);
    osti_update_widget_visibility(layout);
}

static void osti_sync_env_drag_state(int envIndex, const TunefishWidget *widget, int mouseX, int mouseY, bool pressed);

static void osti_apply_callbacks(OsTirusCompleteLayout *layout, TunefishWidget *widget)
{
    if (!layout || !widget) return;

    if (strcmp(widget->name, "page_common_btn") == 0 ||
        strcmp(widget->name, "page_filter_btn") == 0 ||
        strcmp(widget->name, "page_mod_matrix_btn") == 0 ||
        strcmp(widget->name, "page_fx_btn") == 0 ||
        strcmp(widget->name, "page_lfo_btn") == 0 ||
        strcmp(widget->name, "page_arp_btn") == 0 ||
        strcmp(widget->name, "page_browser_btn") == 0)
    {
        widget->onClick = NULL;
    }
    else if (strcmp(widget->name, "close_btn") == 0)
    {
        widget->onClick = NULL;
    }
    else if (strcmp(widget->name, "preset_combo") == 0 ||
             strcmp(widget->name, "browser_bank_combo") == 0 ||
             strcmp(widget->name, "browser_program_combo") == 0)
    {
        widget->onComboSelect = NULL;
    }
    else if (strncmp(widget->name, "cc_", 3) == 0)
    {
        if (widget->type == TF_WIDGET_COMBO_BOX)
            widget->onComboSelect = NULL;
        else
            widget->onValueChange = NULL;
    }
}

static void osti_schema_register_widget(OsTirusCompleteLayout *layout, TunefishWidget *widget, ft2_ui_widget_page_t page)
{
    if (!layout || !widget)
    {
        tf_widget_destroy(widget);
        return;
    }

    osti_style_widget(widget);

    if (strcmp(widget->name, "preset_combo") == 0)
    {
        tf_widget_clear_combo_items(widget);
        osti_populate_preset_combo(layout);
    }
    else if (widget->type == TF_WIDGET_COMBO_BOX && strncmp(widget->name, "cc_", 3) == 0)
    {
        osti_populate_combo(widget);
    }
    else if (widget->type == TF_WIDGET_TOGGLE_BUTTON && strncmp(widget->name, "cc_", 3) == 0)
    {
        widget->pressed = false;
        widget->value = 0.0f;
        tf_widget_set_label(widget, widget->text);
    }

    if (widget->type == TF_WIDGET_BUTTON &&
        (strcmp(widget->name, "page_common_btn") == 0 ||
         strcmp(widget->name, "page_filter_btn") == 0 ||
         strcmp(widget->name, "page_mod_matrix_btn") == 0 ||
         strcmp(widget->name, "page_fx_btn") == 0 ||
         strcmp(widget->name, "page_lfo_btn") == 0 ||
         strcmp(widget->name, "page_arp_btn") == 0 ||
         strcmp(widget->name, "page_browser_btn") == 0 ||
         strcmp(widget->name, "browser_load_btn") == 0 ||
         strcmp(widget->name, "browser_prev_btn") == 0 ||
         strcmp(widget->name, "browser_next_btn") == 0 ||
         strcmp(widget->name, "browser_default_btn") == 0 ||
         strcmp(widget->name, "browser_clear_btn") == 0 ||
         strcmp(widget->name, "close_btn") == 0))
    {
        tf_style_as_main_button(widget);
    }

    if (widget->type == TF_WIDGET_LEVEL_METER)
        tf_style_as_level_indicator(widget);

    if (widget->type == TF_WIDGET_ROTARY_SLIDER)
        tf_style_as_parameter_knob(widget);

    if (widget->type == TF_WIDGET_COMBO_BOX)
        tf_apply_combo_styling(widget);

    if (widget->type == TF_WIDGET_LABEL)
        widget->textColor = TF_COL_TEXT_NORMAL;

    if (widget->type == TF_WIDGET_BUTTON &&
        (strcmp(widget->name, "page_common_btn") == 0 ||
         strcmp(widget->name, "page_filter_btn") == 0 ||
         strcmp(widget->name, "page_mod_matrix_btn") == 0 ||
         strcmp(widget->name, "page_fx_btn") == 0 ||
         strcmp(widget->name, "page_lfo_btn") == 0 ||
         strcmp(widget->name, "page_arp_btn") == 0 ||
         strcmp(widget->name, "page_browser_btn") == 0))
    {
        widget->onClick = NULL;
        widget->pressed = false;
    }

    if (widget->type == TF_WIDGET_BUTTON && strcmp(widget->name, "close_btn") == 0)
        widget->onClick = NULL;

    if (widget->type == TF_WIDGET_ROTARY_SLIDER ||
        widget->type == TF_WIDGET_LINEAR_SLIDER ||
        widget->type == TF_WIDGET_TOGGLE_BUTTON ||
        widget->type == TF_WIDGET_COMBO_BOX)
    {
        int paramId = -1;
        if (sscanf(widget->name, "cc_%d_", &paramId) == 1)
        {
            if (layout->param_binding_count < OSTI_MAX_PARAM_BINDINGS)
            {
                OsTirusParamBinding *binding = &layout->param_bindings[layout->param_binding_count++];
                binding->widget = widget;
                binding->paramId = paramId;
                binding->isCombo = (widget->type == TF_WIDGET_COMBO_BOX);
                binding->isToggle = (widget->type == TF_WIDGET_TOGGLE_BUTTON);
            }
        }
    }

    osti_register_widget(layout, widget, page);
    osti_apply_callbacks(layout, widget);
}

static TunefishWidget *osti_create_label_from_desc(const ft2_ui_tf_label_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_label(d->name ? d->name : "label", d->text ? d->text : "", d->x, d->y, d->w, d->h);
}

static TunefishWidget *osti_create_button_from_desc(const ft2_ui_tf_button_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_button(d->name ? d->name : "btn", d->text ? d->text : "", d->x, d->y, d->w, d->h);
}

static TunefishWidget *osti_create_toggle_from_desc(const ft2_ui_tf_toggle_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_toggle_button(d->name ? d->name : "toggle", d->text ? d->text : "", d->x, d->y, d->w, d->h);
}

static TunefishWidget *osti_create_rotary_from_desc(const ft2_ui_tf_rotary_slider_desc_t *d)
{
    if (!d) return NULL;
    const int cx = d->x + (int)d->radius;
    const int cy = d->y + (int)d->radius;
    TunefishWidget *widget = tf_create_rotary_slider(d->name ? d->name : "rotary", cx, cy, (int)d->radius, d->start_angle, d->end_angle);
    if (widget)
    {
        widget->modRingMode = d->mod_ring.mode ? d->mod_ring.mode : FT2_UI_MOD_RING_AUTO_BY_NAME;
        widget->modMatrixSlot = d->mod_ring.matrix_slot;
        widget->modTargetParam = d->mod_ring.target_param;
        widget->modAmountScale = d->mod_ring.amount_scale > 0.0f ? d->mod_ring.amount_scale : 1.0f;
        if (d->label)
            tf_widget_set_label(widget, d->label);
    }
    return widget;
}

static TunefishWidget *osti_create_combo_from_desc(const ft2_ui_tf_combo_box_desc_t *d)
{
    if (!d) return NULL;
    TunefishWidget *w = tf_create_combo_box(d->name ? d->name : "combo", d->x, d->y, d->w, d->h, NULL, 0);
    if (w)
        w->selectedIndex = d->selected_index;
    return w;
}

static TunefishWidget *osti_create_meter_from_desc(const ft2_ui_tf_level_meter_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_level_meter(d->name ? d->name : "meter", d->x, d->y, d->w, d->h, d->num_leds > 0 ? d->num_leds : 12, d->show_peak);
}

static TunefishWidget *osti_create_linear_from_desc(const ft2_ui_tf_linear_slider_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_linear_slider(d->name ? d->name : "linear", d->x, d->y, d->w, d->h, d->vertical);
}

static TunefishWidget *osti_create_group_from_desc(const ft2_ui_tf_group_box_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_group_box(d->name ? d->name : "group", d->title ? d->title : "", d->x, d->y, d->w, d->h);
}

static TunefishWidget *osti_create_env_from_desc(const ft2_ui_tf_envelope_display_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_envelope_display(d->name ? d->name : "env", d->x, d->y, d->w, d->h);
}

static TunefishWidget *osti_create_wave_from_desc(const ft2_ui_waveform_view_desc_t *d)
{
    if (!d) return NULL;
    return tf_create_waveform_view(d->name ? d->name : "waveform_view", d->x, d->y, d->w, d->h);
}

static const ft2_ui_bitmap_asset_t *osti_find_bitmap_asset(uint16_t id)
{
    for (uint16_t i = 0; i < ft2_ui_assets.bitmap_count; i++)
    {
        if (ft2_ui_assets.bitmaps[i].id == id)
            return &ft2_ui_assets.bitmaps[i];
    }
    return NULL;
}

static TunefishWidget *osti_create_bitmap_from_desc(const ft2_ui_bitmap_desc_t *d)
{
    if (!d) return NULL;

    const ft2_ui_bitmap_asset_t *asset = osti_find_bitmap_asset(d->bitmap_id);
    if (!asset || !asset->bmp)
        return NULL;

    int32_t bmp_w = 0;
    int32_t bmp_h = 0;
    TunefishWidget *w = NULL;

    if (asset->fmt == FT2_UI_BMP_FMT_RLE4)
    {
        uint8_t *pixels = ft2_bmp_decode_rle4_to_pal(asset->bmp, &bmp_w, &bmp_h);
        if (!pixels) return NULL;
        w = tf_create_bitmap("osti_bitmap", d->x, d->y, d->w, d->h, pixels, bmp_w, bmp_h, true);
    }
    else if (asset->fmt == FT2_UI_BMP_FMT_RGB)
    {
        uint32_t *pixels = ft2_bmp_decode_to_rgb32(asset->bmp, &bmp_w, &bmp_h);
        if (!pixels) return NULL;
        w = tf_create_bitmap32("osti_bitmap", d->x, d->y, d->w, d->h, pixels, bmp_w, bmp_h, true);
    }

    if (w)
    {
        w->bitmapLayer = d->layer;
        w->bitmapSkinPart = d->skin_part;
    }

    return w;
}

static OsTirusCompleteLayout *osti_create_complete_layout_from_schema(const ft2_ui_layout_desc_t *desc)
{
    if (!desc || desc->version != FT2_UI_SCHEMA_VERSION) return NULL;

    OsTirusCompleteLayout *layout = (OsTirusCompleteLayout *)calloc(1, sizeof(OsTirusCompleteLayout));
    if (!layout) return NULL;

    layout->current_page = OSTI_PAGE_COMMON;
    layout->active_instrument_id = osti_current_instr_id(layout);
    layout->initialized = true;
    layout->visible = false;
    layout->sync_interval_ms = 100;
    layout->last_sync_ticks = 0;
    layout->param_binding_count = 0;
    layout->all_widget_count = 0;
    layout->arp_drag_step_index = -1;
    for (int i = 0; i < OSTI_ARP_STEP_COUNT; ++i)
        layout->arp_step_widgets[i] = NULL;

    if (desc->bitmaps.count > 0 && desc->bitmap_desc)
    {
        for (uint16_t i = 0; i < desc->bitmaps.count; ++i)
        {
            const ft2_ui_bitmap_desc_t *d = &desc->bitmap_desc[i];
            TunefishWidget *w = osti_create_bitmap_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->waveform_views.count > 0 && desc->waveform_view_desc)
    {
        for (uint16_t i = 0; i < desc->waveform_views.count; ++i)
        {
            const ft2_ui_waveform_view_desc_t *d = &desc->waveform_view_desc[i];
            TunefishWidget *w = osti_create_wave_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_buttons.count > 0 && desc->tf_button_desc)
    {
        for (uint16_t i = 0; i < desc->tf_buttons.count; ++i)
        {
            const ft2_ui_tf_button_desc_t *d = &desc->tf_button_desc[i];
            TunefishWidget *w = osti_create_button_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_toggles.count > 0 && desc->tf_toggle_desc)
    {
        for (uint16_t i = 0; i < desc->tf_toggles.count; ++i)
        {
            const ft2_ui_tf_toggle_desc_t *d = &desc->tf_toggle_desc[i];
            TunefishWidget *w = osti_create_toggle_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_labels.count > 0 && desc->tf_label_desc)
    {
        for (uint16_t i = 0; i < desc->tf_labels.count; ++i)
        {
            const ft2_ui_tf_label_desc_t *d = &desc->tf_label_desc[i];
            TunefishWidget *w = osti_create_label_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_rotary_sliders.count > 0 && desc->tf_rotary_slider_desc)
    {
        for (uint16_t i = 0; i < desc->tf_rotary_sliders.count; ++i)
        {
            const ft2_ui_tf_rotary_slider_desc_t *d = &desc->tf_rotary_slider_desc[i];
            TunefishWidget *w = osti_create_rotary_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_combo_boxes.count > 0 && desc->tf_combo_box_desc)
    {
        for (uint16_t i = 0; i < desc->tf_combo_boxes.count; ++i)
        {
            const ft2_ui_tf_combo_box_desc_t *d = &desc->tf_combo_box_desc[i];
            TunefishWidget *w = osti_create_combo_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_level_meters.count > 0 && desc->tf_level_meter_desc)
    {
        for (uint16_t i = 0; i < desc->tf_level_meters.count; ++i)
        {
            const ft2_ui_tf_level_meter_desc_t *d = &desc->tf_level_meter_desc[i];
            TunefishWidget *w = osti_create_meter_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_envelope_displays.count > 0 && desc->tf_envelope_display_desc)
    {
        for (uint16_t i = 0; i < desc->tf_envelope_displays.count; ++i)
        {
            const ft2_ui_tf_envelope_display_desc_t *d = &desc->tf_envelope_display_desc[i];
            TunefishWidget *w = osti_create_env_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_linear_sliders.count > 0 && desc->tf_linear_slider_desc)
    {
        for (uint16_t i = 0; i < desc->tf_linear_sliders.count; ++i)
        {
            const ft2_ui_tf_linear_slider_desc_t *d = &desc->tf_linear_slider_desc[i];
            TunefishWidget *w = osti_create_linear_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_group_boxes.count > 0 && desc->tf_group_box_desc)
    {
        for (uint16_t i = 0; i < desc->tf_group_boxes.count; ++i)
        {
            const ft2_ui_tf_group_box_desc_t *d = &desc->tf_group_box_desc[i];
            TunefishWidget *w = osti_create_group_from_desc(d);
            if (!w) continue;
            osti_schema_register_widget(layout, w, d->page);
        }
    }

    /* Bind top-bar widgets that are referenced frequently. */
    layout->title_label = NULL;
    layout->page_caption_label = NULL;
    layout->preset_combo = NULL;
    layout->preset_name_label = NULL;
    layout->slot_label = NULL;
    layout->voice_meter = NULL;
    layout->close_button = NULL;
    layout->page_common_button = NULL;
    layout->page_filter_button = NULL;
    layout->page_mod_matrix_button = NULL;
    layout->page_fx_button = NULL;
    layout->page_lfo_button = NULL;
    layout->page_arp_button = NULL;
    layout->page_browser_button = NULL;
    layout->browser_bank_combo = NULL;
    layout->browser_program_combo = NULL;
    layout->browser_status_label = NULL;
    layout->browser_selected_label = NULL;
    layout->browser_current_label = NULL;
    layout->browser_load_button = NULL;
    layout->browser_prev_button = NULL;
    layout->browser_next_button = NULL;
    layout->browser_default_button = NULL;
    layout->browser_clear_button = NULL;
    layout->browser_bank_count = 0;
    layout->browser_category_count = 0;
    layout->browser_filtered_preset_count = 0;
    layout->browser_selected_bank = -1;
    layout->browser_selected_preset_index = -1;
    memset(layout->browser_category_selected, 0, sizeof(layout->browser_category_selected));
    memset(layout->browser_scroll_offsets, 0, sizeof(layout->browser_scroll_offsets));
    layout->browser_scroll_dragging = false;
    layout->browser_scroll_drag_column = -1;
    layout->browser_scroll_drag_offset = 0;
    layout->browser_search_text[0] = '\0';
    layout->browser_search_active = false;
    for (int i = 0; i < OSTI_ARP_STEP_COUNT; ++i)
        layout->arp_step_widgets[i] = NULL;

    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (!w) continue;
        if (strcmp(w->name, "title_label") == 0) layout->title_label = w;
        else if (strcmp(w->name, "page_caption_label") == 0) layout->page_caption_label = w;
        else if (strcmp(w->name, "preset_combo") == 0) layout->preset_combo = w;
        else if (strcmp(w->name, "preset_name_label") == 0) layout->preset_name_label = w;
        else if (strcmp(w->name, "slot_label") == 0) layout->slot_label = w;
        else if (strcmp(w->name, "voice_meter") == 0) layout->voice_meter = w;
        else if (strcmp(w->name, "close_btn") == 0) layout->close_button = w;
        else if (strcmp(w->name, "page_common_btn") == 0) layout->page_common_button = w;
        else if (strcmp(w->name, "page_filter_btn") == 0) layout->page_filter_button = w;
        else if (strcmp(w->name, "page_mod_matrix_btn") == 0) layout->page_mod_matrix_button = w;
        else if (strcmp(w->name, "page_fx_btn") == 0) layout->page_fx_button = w;
        else if (strcmp(w->name, "page_lfo_btn") == 0) layout->page_lfo_button = w;
        else if (strcmp(w->name, "page_arp_btn") == 0) layout->page_arp_button = w;
        else if (strcmp(w->name, "page_browser_btn") == 0) layout->page_browser_button = w;
        else if (strcmp(w->name, "browser_bank_combo") == 0) layout->browser_bank_combo = w;
        else if (strcmp(w->name, "browser_program_combo") == 0) layout->browser_program_combo = w;
        else if (strcmp(w->name, "browser_status_label") == 0) layout->browser_status_label = w;
        else if (strcmp(w->name, "browser_selected_label") == 0) layout->browser_selected_label = w;
        else if (strcmp(w->name, "browser_current_label") == 0) layout->browser_current_label = w;
        else if (strcmp(w->name, "browser_load_btn") == 0) layout->browser_load_button = w;
        else if (strcmp(w->name, "browser_prev_btn") == 0) layout->browser_prev_button = w;
        else if (strcmp(w->name, "browser_next_btn") == 0) layout->browser_next_button = w;
        else if (strcmp(w->name, "browser_default_btn") == 0) layout->browser_default_button = w;
        else if (strcmp(w->name, "browser_clear_btn") == 0) layout->browser_clear_button = w;
        else if (osti_is_arp_step_widget(w))
        {
            int stepIndex = osti_arp_step_index_from_name(w->name);
            if (stepIndex >= 0 && stepIndex < OSTI_ARP_STEP_COUNT)
                layout->arp_step_widgets[stepIndex] = w;
        }
    }

    osti_apply_all_widget_styling(layout);
    osti_update_all_widgets_from_synth(layout);
    osti_update_widget_visibility(layout);
    return layout;
}

OsTirusCompleteLayout* osti_create_complete_layout(void)
{
    return osti_create_complete_layout_from_schema(&ft2_ostirus_complete_layout_layout);
}

void osti_destroy_complete_layout(OsTirusCompleteLayout *layout)
{
    if (!layout) return;

    for (int i = 0; i < layout->all_widget_count; ++i)
        tf_widget_destroy(layout->all_widgets[i]);

    if (g_active_ostirus_layout == layout)
        g_active_ostirus_layout = NULL;

    free(layout);
}

static void osti_set_active_instrument(OsTirusCompleteLayout *layout)
{
    if (!layout) return;
    layout->active_instrument_id = osti_current_instr_id(layout);
}

void osti_show_layout(OsTirusCompleteLayout *layout)
{
    if (!layout) return;

    g_active_ostirus_layout = layout;
    layout->visible = true;
    osti_set_active_instrument(layout);

    if (layout->active_instrument_id > 0)
    {
        ft2_ostirus_assign_slot_for_instrument(layout->active_instrument_id);

        if (ft2_ostirus_get_current_preset_for_instrument(layout->active_instrument_id) < 0 &&
            ft2_ostirus_get_factory_preset_count() > 0)
        {
            ft2_ostirus_load_factory_preset_for_instrument(layout->active_instrument_id, 0);
        }
    }

    osti_sync_browser_selection_to_current(layout);
    osti_update_all_widgets_from_synth(layout);
}

void osti_hide_layout(OsTirusCompleteLayout *layout)
{
    if (!layout) return;
    layout->visible = false;
    if (g_active_ostirus_layout == layout)
        g_active_ostirus_layout = NULL;

    g_env_state[0].dragging = false;
    g_env_state[1].dragging = false;
    g_env_state[0].dragIndex = -1;
    g_env_state[1].dragIndex = -1;
    layout->arp_drag_step_index = -1;
}

void osti_switch_to_page(OsTirusCompleteLayout *layout, int page)
{
    if (!layout) return;
    layout->current_page = osti_clampi(page, 0, OSTI_PAGE_COUNT - 1);
    osti_update_caption(layout);
    osti_update_widget_visibility(layout);
    osti_apply_page_button_state(layout);
}

static void osti_param_changed(OsTirusCompleteLayout *layout, TunefishWidget *widget, float value)
{
    if (!layout || !widget) return;

    const int instrID = osti_current_instr_id(layout);
    if (instrID <= 0) return;

    int bindIdx = osti_binding_index_for_widget(layout, widget);
    if (bindIdx < 0) return;

    OsTirusParamBinding *binding = &layout->param_bindings[bindIdx];
    float norm = osti_clampf(value, 0.0f, 1.0f);

    if (binding->isToggle)
        norm = (widget->pressed || value >= 0.5f) ? 1.0f : 0.0f;
    else if (binding->isCombo && widget->comboItemCount > 1)
        norm = (float)widget->selectedIndex / (float)(widget->comboItemCount - 1);

    ft2_ostirus_set_param_for_instrument(instrID, binding->paramId, norm);
    osti_sync_widgets_with_parameters(layout);
}

static void osti_combo_selected(TunefishWidget *widget, int selectedIndex)
{
    OsTirusCompleteLayout *layout = g_active_ostirus_layout;
    if (!layout || !widget) return;

    if (strcmp(widget->name, "preset_combo") == 0)
    {
        const int instrID = osti_current_instr_id(layout);
        if (instrID <= 0) return;

        const int count = ft2_ostirus_get_factory_preset_count();
        if (count <= 0) return;

        int index = osti_clampi(selectedIndex, 0, count - 1);
        if (ft2_ostirus_load_factory_preset_for_instrument(instrID, index))
        {
            osti_sync_browser_selection_to_current(layout);
            osti_update_all_widgets_from_synth(layout);
        }
        return;
    }

    int bindIdx = osti_binding_index_for_widget(layout, widget);
    if (bindIdx < 0) return;

    OsTirusParamBinding *binding = &layout->param_bindings[bindIdx];
    const int instrID = osti_current_instr_id(layout);
    if (instrID <= 0) return;

    float norm = 0.0f;
    if (widget->comboItemCount > 1)
        norm = (float)osti_clampi(selectedIndex, 0, widget->comboItemCount - 1) / (float)(widget->comboItemCount - 1);

    ft2_ostirus_set_param_for_instrument(instrID, binding->paramId, norm);
    osti_sync_widgets_with_parameters(layout);
}

static void osti_button_clicked(TunefishWidget *widget)
{
    OsTirusCompleteLayout *layout = g_active_ostirus_layout;
    if (!layout || !widget) return;

    if (widget == layout->page_common_button) osti_switch_to_page(layout, OSTI_PAGE_COMMON);
    else if (widget == layout->page_filter_button) osti_switch_to_page(layout, OSTI_PAGE_FILTER);
    else if (widget == layout->page_mod_matrix_button) osti_switch_to_page(layout, OSTI_PAGE_MOD_MATRIX);
    else if (widget == layout->page_fx_button) osti_switch_to_page(layout, OSTI_PAGE_FX);
    else if (widget == layout->page_lfo_button) osti_switch_to_page(layout, OSTI_PAGE_LFO);
    else if (widget == layout->page_arp_button) osti_switch_to_page(layout, OSTI_PAGE_ARP);
    else if (widget == layout->page_browser_button) osti_switch_to_page(layout, OSTI_PAGE_BROWSER);
    else if (widget == layout->browser_load_button)
    {
        osti_load_browser_selection(layout);
    }
    else if (widget == layout->browser_prev_button)
    {
        osti_step_browser_preset(layout, -1);
    }
    else if (widget == layout->browser_next_button)
    {
        osti_step_browser_preset(layout, +1);
    }
    else if (widget == layout->browser_default_button)
    {
        const int defaultIndex = ft2_ostirus_get_default_factory_preset_index();
        if (defaultIndex >= 0)
        {
            layout->browser_selected_preset_index = defaultIndex;
            layout->browser_selected_bank = ft2_ostirus_get_factory_preset_bank(defaultIndex);
            osti_load_browser_selection(layout);
        }
    }
    else if (widget == layout->browser_clear_button)
    {
        osti_browser_reset_filters(layout);
    }
    else if (widget == layout->close_button)
    {
        osti_hide_layout(layout);
        ft2_close_synth_editor();
    }
}

static void osti_apply_env_params(int envIndex, float attack, float decay, float sustain, float release)
{
    if (envIndex < 0 || envIndex > 1) return;

    const int instrID = osti_current_instr_id(g_active_ostirus_layout);
    if (instrID <= 0) return;

    const int *ids = k_env_param_ids[envIndex];
    ft2_ostirus_set_param_for_instrument(instrID, ids[0], osti_clampf(attack, 0.0f, 1.0f));
    ft2_ostirus_set_param_for_instrument(instrID, ids[1], osti_clampf(decay, 0.0f, 1.0f));
    ft2_ostirus_set_param_for_instrument(instrID, ids[2], osti_clampf(sustain, 0.0f, 1.0f));
    ft2_ostirus_set_param_for_instrument(instrID, ids[3], osti_clampf(release, 0.0f, 1.0f));
}

static void osti_env_get_points(int envIndex, const TunefishWidget *w, int pts[5][2])
{
    const int pad = 5;
    int left = w->x + pad;
    int top = w->y + pad;
    int right = w->x + w->w - pad - 1;
    int bottom = w->y + w->h - pad - 1;
    int width = right - left;
    int height = bottom - top;

    const int instrID = osti_current_instr_id(g_active_ostirus_layout);
    float a = 0.0f, d = 0.0f, s = 0.0f, r = 0.0f;
    if (instrID > 0)
    {
        const int *ids = k_env_param_ids[envIndex];
        a = ft2_ostirus_get_param_for_instrument(instrID, ids[0]);
        d = ft2_ostirus_get_param_for_instrument(instrID, ids[1]);
        s = ft2_ostirus_get_param_for_instrument(instrID, ids[2]);
        r = ft2_ostirus_get_param_for_instrument(instrID, ids[3]);
    }

    a = osti_clampf(a, 0.0f, 1.0f);
    d = osti_clampf(d, 0.0f, 1.0f);
    s = osti_clampf(s, 0.0f, 1.0f);
    r = osti_clampf(r, 0.0f, 1.0f);

    int x1 = left + (int)lroundf(a * 0.35f * (float)width);
    int x2 = x1 + (int)lroundf(d * 0.25f * (float)width);
    int x3 = right - (int)lroundf(r * 0.35f * (float)width);
    int yS = top + (int)lroundf((1.0f - s) * (float)height);

    x1 = osti_clampi(x1, left, right - 12);
    x2 = osti_clampi(x2, x1 + 4, right - 8);
    x3 = osti_clampi(x3, x2 + 4, right - 4);
    yS = osti_clampi(yS, top, bottom);

    pts[0][0] = left;  pts[0][1] = bottom;
    pts[1][0] = x1;    pts[1][1] = top;
    pts[2][0] = x2;    pts[2][1] = yS;
    pts[3][0] = x3;    pts[3][1] = yS;
    pts[4][0] = right; pts[4][1] = bottom;
}

static void osti_env_draw_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    video.frameBuffer[(size_t)y * SCREEN_W + (size_t)x] = video.palette[color];
}

static void osti_draw_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        osti_env_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void osti_draw_env_display(OsTirusCompleteLayout *layout, const TunefishWidget *w, int envIndex)
{
    (void)layout;
    if (!w || !w->visible) return;

    const char *title = (envIndex == 0) ? "Filter Env" : "Amp Env";
    const int pad = 5;
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
    textOut(w->x + 6, w->y + 3, PAL_FORGRND, title);

    int pts[5][2];
    osti_env_get_points(envIndex, w, pts);

    int prevX = pts[0][0];
    int prevY = pts[0][1];
    for (int i = 1; i < 5; ++i)
    {
        int x1 = pts[i][0];
        int y1 = pts[i][1];
        if (x1 > prevX)
        {
            for (int x = prevX; x <= x1; ++x)
            {
                float t = (x1 == prevX) ? 0.0f : (float)(x - prevX) / (float)(x1 - prevX);
                int y = prevY + (int)lroundf(t * (float)(y1 - prevY));
                if (y < top) y = top;
                if (y > bottom) y = bottom;
                fillRect(x, y, 1, bottom - y + 1, PAL_BLCKMRK);
            }
        }
        prevX = x1;
        prevY = y1;
    }

    for (int i = 0; i < 5; ++i)
    {
        int px = pts[i][0];
        int py = pts[i][1];
        fillRect(px - 1, py - 1, 3, 3, PAL_BLCKTXT);
        if (i == g_env_state[envIndex].selectedIndex)
        {
            osti_draw_line(px - 3, py - 3, px - 3, py + 3, PAL_BLCKTXT);
            osti_draw_line(px + 3, py - 3, px + 3, py + 3, PAL_BLCKTXT);
        }
    }

    for (int i = 0; i <= height / 2; ++i)
        osti_env_draw_pixel(left - 1, top + 1 + i * 2, PAL_PATTEXT);
    for (int i = 0; i <= height / 8; ++i)
        osti_env_draw_pixel(left - 2, top + 1 + i * 8, PAL_PATTEXT);
    for (int i = 0; i <= width / 2; ++i)
        osti_env_draw_pixel(left + 1 + i * 2, bottom + 1, PAL_PATTEXT);
    for (int i = 0; i <= width / 50; ++i)
        osti_env_draw_pixel(left + 1 + i * 50, bottom + 2, PAL_PATTEXT);
}

static bool osti_env_mouse(OsTirusCompleteLayout *layout, const TunefishWidget *w, int envIndex, int mouseX, int mouseY, bool pressed)
{
    if (!layout || !w || !w->visible) return false;

    const int pad = 5;
    int left = w->x + pad;
    int top = w->y + pad;
    int right = w->x + w->w - pad - 1;
    int bottom = w->y + w->h - pad - 1;

    int pts[5][2];
    osti_env_get_points(envIndex, w, pts);

    if (!pressed)
    {
        if (g_env_state[envIndex].dragging)
        {
            g_env_state[envIndex].dragging = false;
            g_env_state[envIndex].dragIndex = -1;
            return true;
        }
        return false;
    }

    if (!g_env_state[envIndex].dragging)
    {
        for (int i = 1; i < 4; ++i)
        {
            int dx = abs(mouseX - pts[i][0]);
            int dy = abs(mouseY - pts[i][1]);
            if (dx <= 4 && dy <= 4)
            {
                g_env_state[envIndex].dragging = true;
                g_env_state[envIndex].dragIndex = i;
                g_env_state[envIndex].selectedIndex = i;
                break;
            }
        }

        if (!g_env_state[envIndex].dragging)
        {
            if (!(mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom))
                return false;

            /* Select nearest editable point if user clicks inside the display. */
            int bestIdx = 1;
            int bestDist = 0x7FFFFFFF;
            for (int i = 1; i < 4; ++i)
            {
                int dx = mouseX - pts[i][0];
                int dy = mouseY - pts[i][1];
                int dist = (dx * dx) + (dy * dy);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestIdx = i;
                }
            }
            g_env_state[envIndex].dragging = true;
            g_env_state[envIndex].dragIndex = bestIdx;
            g_env_state[envIndex].selectedIndex = bestIdx;
        }
    }

    const int idx = g_env_state[envIndex].dragIndex;
    if (idx < 1 || idx > 3) return false;

    const float widthA = 0.35f;
    const float widthD = 0.25f;
    const float widthR = 0.35f;
    const int width = right - left;
    const int height = bottom - top;
    if (width <= 0 || height <= 0) return false;

    float a = ft2_ostirus_get_param_for_instrument(osti_current_instr_id(layout), k_env_param_ids[envIndex][0]);
    float d = ft2_ostirus_get_param_for_instrument(osti_current_instr_id(layout), k_env_param_ids[envIndex][1]);
    float s = ft2_ostirus_get_param_for_instrument(osti_current_instr_id(layout), k_env_param_ids[envIndex][2]);
    float r = ft2_ostirus_get_param_for_instrument(osti_current_instr_id(layout), k_env_param_ids[envIndex][3]);

    if (idx == 1)
    {
        a = (float)(mouseX - left) / (widthA * (float)width);
    }
    else if (idx == 2)
    {
        d = (float)(mouseX - pts[1][0]) / (widthD * (float)width);
        s = 1.0f - ((float)(mouseY - top) / (float)height);
    }
    else if (idx == 3)
    {
        r = (float)(right - mouseX) / (widthR * (float)width);
        s = 1.0f - ((float)(mouseY - top) / (float)height);
    }

    a = osti_clampf(a, 0.0f, 1.0f);
    d = osti_clampf(d, 0.0f, 1.0f);
    s = osti_clampf(s, 0.0f, 1.0f);
    r = osti_clampf(r, 0.0f, 1.0f);

    osti_apply_env_params(envIndex, a, d, s, r);
    osti_update_all_widgets_from_synth(layout);
    return true;
}

static void osti_browser_column_title(OsTirusBrowserColumn column, char *out, size_t outSize)
{
    if (!out || outSize == 0)
        return;

    switch (column)
    {
        default:
        case OSTI_BROWSER_COLUMN_ROM: snprintf(out, outSize, "ROM"); break;
        case OSTI_BROWSER_COLUMN_BANK: snprintf(out, outSize, "Bank"); break;
        case OSTI_BROWSER_COLUMN_CATEGORY: snprintf(out, outSize, "Category"); break;
        case OSTI_BROWSER_COLUMN_PATCH: snprintf(out, outSize, "Patch"); break;
    }
}

static int osti_browser_column_item_count(const OsTirusCompleteLayout *layout, OsTirusBrowserColumn column)
{
    if (!layout)
        return 0;

    switch (column)
    {
        default:
        case OSTI_BROWSER_COLUMN_ROM: return 1;
        case OSTI_BROWSER_COLUMN_BANK: return layout->browser_bank_count;
        case OSTI_BROWSER_COLUMN_CATEGORY: return layout->browser_category_count;
        case OSTI_BROWSER_COLUMN_PATCH: return layout->browser_filtered_preset_count;
    }
}

static bool osti_browser_column_item_text(const OsTirusCompleteLayout *layout, OsTirusBrowserColumn column, int itemIndex, char *out, size_t outSize)
{
    if (!layout || !out || outSize == 0 || itemIndex < 0)
        return false;

    switch (column)
    {
        case OSTI_BROWSER_COLUMN_ROM:
            snprintf(out, outSize, "%s", ft2_ostirus_get_rom_model_name());
            return true;

        case OSTI_BROWSER_COLUMN_BANK:
            if (itemIndex >= layout->browser_bank_count)
                return false;
            osti_format_bank_name(layout->browser_bank_values[itemIndex], out, outSize);
            return true;

        case OSTI_BROWSER_COLUMN_CATEGORY:
            if (itemIndex >= layout->browser_category_count)
                return false;
            snprintf(out, outSize, "%s", ft2_ostirus_get_category_name(layout->browser_category_values[itemIndex]));
            return true;

        case OSTI_BROWSER_COLUMN_PATCH:
            if (itemIndex >= layout->browser_filtered_preset_count)
                return false;
            {
                const int presetIndex = layout->browser_filtered_preset_indices[itemIndex];
                const int program = ft2_ostirus_get_factory_preset_program(presetIndex);
                const char *name = ft2_ostirus_get_factory_preset_plain_name(presetIndex);
                snprintf(out, outSize, "%02d %s", program + 1, (name && *name) ? name : "Init");
                return true;
            }
    }

    return false;
}

static bool osti_browser_column_item_selected(const OsTirusCompleteLayout *layout, OsTirusBrowserColumn column, int itemIndex)
{
    if (!layout || itemIndex < 0)
        return false;

    switch (column)
    {
        case OSTI_BROWSER_COLUMN_ROM:
            return true;

        case OSTI_BROWSER_COLUMN_BANK:
            return itemIndex < layout->browser_bank_count &&
                layout->browser_selected_bank == layout->browser_bank_values[itemIndex];

        case OSTI_BROWSER_COLUMN_CATEGORY:
            return itemIndex < layout->browser_category_count &&
                layout->browser_category_selected[layout->browser_category_values[itemIndex]];

        case OSTI_BROWSER_COLUMN_PATCH:
            return itemIndex < layout->browser_filtered_preset_count &&
                layout->browser_selected_preset_index == layout->browser_filtered_preset_indices[itemIndex];
    }

    return false;
}

static osti_rect_t osti_browser_scrollbar_rect(OsTirusBrowserColumn column)
{
    osti_rect_t rect = osti_browser_column_rect(column);
    rect.x = rect.x + rect.w - 10;
    rect.w = 10;
    rect.y += 14;
    rect.h -= 14;
    return rect;
}

static osti_rect_t osti_browser_thumb_rect(const OsTirusCompleteLayout *layout, OsTirusBrowserColumn column)
{
    osti_rect_t track = osti_browser_scrollbar_rect(column);
    const int itemCount = osti_browser_column_item_count(layout, column);
    const osti_rect_t listRect = osti_browser_column_rect(column);
    const int visibleRows = osti_browser_visible_rows(&listRect);
    const int maxScroll = (itemCount > visibleRows) ? (itemCount - visibleRows) : 0;

    if (itemCount <= 0 || visibleRows <= 0 || maxScroll <= 0)
    {
        track.h = 0;
        return track;
    }

    int thumbH = (visibleRows * track.h) / itemCount;
    if (thumbH < 10) thumbH = 10;
    if (thumbH > track.h) thumbH = track.h;

    const int scroll = layout->browser_scroll_offsets[column];
    const int travel = track.h - thumbH;
    const int thumbY = track.y + (travel > 0 ? (scroll * travel) / maxScroll : 0);

    track.y = thumbY;
    track.h = thumbH;
    return track;
}

static void osti_draw_browser_page(OsTirusCompleteLayout *layout)
{
    if (!layout || layout->current_page != OSTI_PAGE_BROWSER)
        return;

    const osti_rect_t searchRect = osti_browser_search_rect();
    fillRect(searchRect.x, searchRect.y, searchRect.w, searchRect.h, PAL_BUTTON2);
    hLine(searchRect.x, searchRect.y, searchRect.w, PAL_BCKGRND);
    hLine(searchRect.x, searchRect.y + searchRect.h - 1, searchRect.w, PAL_BCKGRND);
    vLine(searchRect.x, searchRect.y, searchRect.h, PAL_BCKGRND);
    vLine(searchRect.x + searchRect.w - 1, searchRect.y, searchRect.h, PAL_BCKGRND);
    textOut(searchRect.x + 4, searchRect.y + 4, PAL_FORGRND, layout->browser_search_active ? "Search >" : "Search");
    textOut(searchRect.x + 70, searchRect.y + 4, PAL_FORGRND, layout->browser_search_text[0] ? layout->browser_search_text : "All");

    for (int col = 0; col < OSTI_BROWSER_COLUMN_COUNT; ++col)
    {
        const OsTirusBrowserColumn column = (OsTirusBrowserColumn)col;
        const osti_rect_t rect = osti_browser_column_rect(column);
        const osti_rect_t track = osti_browser_scrollbar_rect(column);
        const osti_rect_t thumb = osti_browser_thumb_rect(layout, column);
        const int visibleRows = osti_browser_visible_rows(&rect);
        const int scroll = layout->browser_scroll_offsets[col];
        char title[32];

        osti_browser_column_title(column, title, sizeof(title));

        fillRect(rect.x, rect.y, rect.w, rect.h, PAL_BUTTON2);
        hLine(rect.x, rect.y, rect.w, PAL_BCKGRND);
        hLine(rect.x, rect.y + rect.h - 1, rect.w, PAL_BCKGRND);
        vLine(rect.x, rect.y, rect.h, PAL_BCKGRND);
        vLine(rect.x + rect.w - 1, rect.y, rect.h, PAL_BCKGRND);
        textOut(rect.x + 4, rect.y + 3, PAL_FORGRND, title);
        hLine(rect.x + 1, rect.y + 13, rect.w - 2, PAL_DSKTOP2);

        for (int row = 0; row < visibleRows; ++row)
        {
            const int itemIndex = scroll + row;
            const int itemCount = osti_browser_column_item_count(layout, column);
            const int y = rect.y + 16 + row * 11;
            char itemText[192];
            if (itemIndex >= itemCount)
                break;

            if (!osti_browser_column_item_text(layout, column, itemIndex, itemText, sizeof(itemText)))
                continue;

            if (osti_browser_column_item_selected(layout, column, itemIndex))
            {
                fillRect(rect.x + 2, y - 1, rect.w - 14, 10, PAL_BUTTONS);
                textOut(rect.x + 4, y, PAL_FORGRND, itemText);
            }
            else
            {
                textOut(rect.x + 4, y, PAL_FORGRND, itemText);
            }
        }

        fillRect(track.x, track.y, track.w, track.h, PAL_DSKTOP2);
        vLine(track.x, track.y, track.h, PAL_BCKGRND);
        if (thumb.h > 0)
            fillRect(thumb.x + 1, thumb.y + 1, thumb.w - 2, thumb.h - 2, PAL_BUTTONS);
    }
}

static void osti_browser_reset_filters(OsTirusCompleteLayout *layout)
{
    if (!layout)
        return;

    layout->browser_selected_bank = -1;
    memset(layout->browser_category_selected, 0, sizeof(layout->browser_category_selected));
    layout->browser_search_text[0] = '\0';
    layout->browser_search_active = false;
    osti_sync_browser_selection_to_current(layout);
    osti_refresh_browser_controls(layout);
}

static bool osti_browser_handle_list_click(OsTirusCompleteLayout *layout, OsTirusBrowserColumn column, int mouseX, int mouseY)
{
    const osti_rect_t rect = osti_browser_column_rect(column);
    const int visibleRows = osti_browser_visible_rows(&rect);
    const int row = (mouseY - (rect.y + 16)) / 11;
    const int itemIndex = layout->browser_scroll_offsets[column] + row;

    if (row < 0 || row >= visibleRows)
        return false;

    if (column == OSTI_BROWSER_COLUMN_BANK)
    {
        if (itemIndex >= 0 && itemIndex < layout->browser_bank_count)
        {
            const int clickedBank = layout->browser_bank_values[itemIndex];
            layout->browser_selected_bank = (layout->browser_selected_bank == clickedBank) ? -1 : clickedBank;
            osti_refresh_browser_controls(layout);
            return true;
        }
    }
    else if (column == OSTI_BROWSER_COLUMN_CATEGORY)
    {
        if (itemIndex >= 0 && itemIndex < layout->browser_category_count)
        {
            const int category = layout->browser_category_values[itemIndex];
            layout->browser_category_selected[category] = !layout->browser_category_selected[category];
            osti_refresh_browser_controls(layout);
            return true;
        }
    }
    else if (column == OSTI_BROWSER_COLUMN_PATCH)
    {
        if (itemIndex >= 0 && itemIndex < layout->browser_filtered_preset_count)
        {
            layout->browser_selected_preset_index = layout->browser_filtered_preset_indices[itemIndex];
            if (!osti_load_browser_selection(layout))
                osti_update_browser_labels(layout);
            return true;
        }
    }

    return false;
}

static bool osti_browser_handle_scrollbar_mouse(OsTirusCompleteLayout *layout, OsTirusBrowserColumn column, int mouseX, int mouseY, bool pressed)
{
    const osti_rect_t track = osti_browser_scrollbar_rect(column);
    const osti_rect_t thumb = osti_browser_thumb_rect(layout, column);
    const int itemCount = osti_browser_column_item_count(layout, column);
    const osti_rect_t listRect = osti_browser_column_rect(column);
    const int visibleRows = osti_browser_visible_rows(&listRect);
    const int maxScroll = (itemCount > visibleRows) ? (itemCount - visibleRows) : 0;

    if (maxScroll <= 0 || !osti_rect_contains(&track, mouseX, mouseY))
        return false;

    if (!pressed)
    {
        layout->browser_scroll_dragging = false;
        layout->browser_scroll_drag_column = -1;
        return true;
    }

    if (osti_rect_contains(&thumb, mouseX, mouseY))
    {
        layout->browser_scroll_dragging = true;
        layout->browser_scroll_drag_column = column;
        layout->browser_scroll_drag_offset = mouseY - thumb.y;
        return true;
    }

    if (mouseY < thumb.y)
        layout->browser_scroll_offsets[column] -= visibleRows;
    else if (mouseY >= thumb.y + thumb.h)
        layout->browser_scroll_offsets[column] += visibleRows;

    osti_browser_clamp_scrolls(layout);
    return true;
}

static bool osti_browser_handle_mouse_event(OsTirusCompleteLayout *layout, int mouseX, int mouseY, bool pressed)
{
    if (!layout || layout->current_page != OSTI_PAGE_BROWSER)
        return false;

    const osti_rect_t searchRect = osti_browser_search_rect();
    if (pressed && osti_rect_contains(&searchRect, mouseX, mouseY))
    {
        layout->browser_search_active = true;
        return true;
    }

    if (!pressed && layout->browser_scroll_dragging)
    {
        layout->browser_scroll_dragging = false;
        layout->browser_scroll_drag_column = -1;
        return true;
    }

    for (int col = 0; col < OSTI_BROWSER_COLUMN_COUNT; ++col)
    {
        const OsTirusBrowserColumn column = (OsTirusBrowserColumn)col;
        const osti_rect_t rect = osti_browser_column_rect(column);

        if (osti_browser_handle_scrollbar_mouse(layout, column, mouseX, mouseY, pressed))
            return true;

        if (pressed && osti_rect_contains(&rect, mouseX, mouseY) && mouseX < rect.x + rect.w - 10)
        {
            return osti_browser_handle_list_click(layout, column, mouseX, mouseY);
        }
    }

    if (pressed)
        layout->browser_search_active = false;

    return false;
}

static bool osti_browser_handle_mouse_drag(OsTirusCompleteLayout *layout, int mouseX, int mouseY)
{
    if (!layout || !layout->browser_scroll_dragging)
        return false;

    const OsTirusBrowserColumn column = (OsTirusBrowserColumn)layout->browser_scroll_drag_column;
    const osti_rect_t track = osti_browser_scrollbar_rect(column);
    const osti_rect_t thumb = osti_browser_thumb_rect(layout, column);
    const int itemCount = osti_browser_column_item_count(layout, column);
    const osti_rect_t listRect = osti_browser_column_rect(column);
    const int visibleRows = osti_browser_visible_rows(&listRect);
    const int maxScroll = (itemCount > visibleRows) ? (itemCount - visibleRows) : 0;
    const int travel = track.h - thumb.h;

    if (maxScroll <= 0 || travel <= 0)
        return false;

    int thumbY = mouseY - layout->browser_scroll_drag_offset;
    if (thumbY < track.y)
        thumbY = track.y;
    if (thumbY > track.y + travel)
        thumbY = track.y + travel;

    layout->browser_scroll_offsets[column] = ((thumbY - track.y) * maxScroll + (travel / 2)) / travel;
    osti_browser_clamp_scrolls(layout);
    return true;
}

static bool osti_browser_handle_key_input(OsTirusCompleteLayout *layout, int key)
{
    if (!layout || layout->current_page != OSTI_PAGE_BROWSER)
        return false;

    if (key == SDLK_BACKSPACE && layout->browser_search_active)
    {
        size_t len = strlen(layout->browser_search_text);
        if (len > 0)
            layout->browser_search_text[len - 1] = '\0';
        osti_refresh_browser_controls(layout);
        return true;
    }

    if (key == SDLK_DELETE)
    {
        osti_browser_reset_filters(layout);
        return true;
    }

    if (key >= SDLK_a && key <= SDLK_z)
    {
        size_t len = strlen(layout->browser_search_text);
        if (len < sizeof(layout->browser_search_text) - 1)
        {
            layout->browser_search_text[len] = (char)('a' + (key - SDLK_a));
            layout->browser_search_text[len + 1] = '\0';
            layout->browser_search_active = true;
            osti_refresh_browser_controls(layout);
        }
        return true;
    }

    if (key >= SDLK_0 && key <= SDLK_9)
    {
        size_t len = strlen(layout->browser_search_text);
        if (len < sizeof(layout->browser_search_text) - 1)
        {
            layout->browser_search_text[len] = (char)('0' + (key - SDLK_0));
            layout->browser_search_text[len + 1] = '\0';
            layout->browser_search_active = true;
            osti_refresh_browser_controls(layout);
        }
        return true;
    }

    if (key == SDLK_SPACE || key == SDLK_MINUS)
    {
        size_t len = strlen(layout->browser_search_text);
        if (len < sizeof(layout->browser_search_text) - 1)
        {
            layout->browser_search_text[len] = (key == SDLK_SPACE) ? ' ' : '-';
            layout->browser_search_text[len + 1] = '\0';
            layout->browser_search_active = true;
            osti_refresh_browser_controls(layout);
        }
        return true;
    }

    if (key == SDLK_UP || key == SDLK_DOWN)
    {
        if (layout->browser_filtered_preset_count <= 0)
            return true;

        int selectedPos = 0;
        for (int i = 0; i < layout->browser_filtered_preset_count; ++i)
        {
            if (layout->browser_filtered_preset_indices[i] == layout->browser_selected_preset_index)
            {
                selectedPos = i;
                break;
            }
        }

        if (key == SDLK_UP && selectedPos > 0)
            selectedPos--;
        else if (key == SDLK_DOWN && selectedPos < layout->browser_filtered_preset_count - 1)
            selectedPos++;

        layout->browser_selected_preset_index = layout->browser_filtered_preset_indices[selectedPos];
        osti_update_browser_labels(layout);
        return true;
    }

    return false;
}

void osti_render_complete_layout(OsTirusCompleteLayout *layout)
{
    if (!layout || !layout->visible) return;

    osti_set_active_instrument(layout);

    if (layout->last_sync_ticks == 0 || (SDL_GetTicks() - layout->last_sync_ticks) >= layout->sync_interval_ms)
    {
        osti_update_all_widgets_from_synth(layout);
        layout->last_sync_ticks = SDL_GetTicks();
    }

    fillRect(0, 0, SCREEN_W, SCREEN_H, PAL_DESKTOP);

    /* Draw group boxes first. */
    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (!w || !w->visible || w->type != TF_WIDGET_GROUP_BOX) continue;
        tf_draw_widget(w);
    }

    /* Draw envelope displays on top of the groups but behind knobs. */
    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (!w || !w->visible || w->type != TF_WIDGET_ENVELOPE_DISPLAY) continue;
        int envIndex = (strcmp(w->name, "amp_env_display") == 0) ? 1 : 0;
        osti_draw_env_display(layout, w, envIndex);
    }

    /* Foreground widgets. */
    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (!w || !w->visible) continue;
        if (w->type == TF_WIDGET_GROUP_BOX || w->type == TF_WIDGET_ENVELOPE_DISPLAY || w->type == TF_WIDGET_LABEL)
            continue;
        tf_draw_widget(w);
    }

    osti_draw_browser_page(layout);

    /* Labels last so the top bar remains crisp. */
    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (!w || !w->visible || w->type != TF_WIDGET_LABEL) continue;
        tf_draw_widget(w);
    }
}

bool osti_handle_layout_mouse_event(OsTirusCompleteLayout *layout, int mouseX, int mouseY, bool pressed)
{
    if (!layout || !layout->visible) return false;

    osti_set_active_instrument(layout);

    if (pressed)
    {
        TunefishWidget *env = osti_find_widget_by_name(layout, "filter_env_display");
        if (env && env->visible && osti_env_mouse(layout, env, 0, mouseX, mouseY, true))
            return true;
        env = osti_find_widget_by_name(layout, "amp_env_display");
        if (env && env->visible && osti_env_mouse(layout, env, 1, mouseX, mouseY, true))
            return true;

        if (layout->current_page == OSTI_PAGE_ARP)
        {
            for (int i = 0; i < OSTI_ARP_STEP_COUNT; ++i)
            {
                TunefishWidget *step = layout->arp_step_widgets[i];
                if (!step || !step->visible)
                    continue;
                if (tf_widget_is_point_inside(step, mouseX, mouseY) &&
                    osti_update_arp_step_from_mouse(layout, step, mouseX, mouseY, true))
                {
                    return true;
                }
            }
        }
    }
    else
    {
        if (layout->browser_scroll_dragging)
        {
            layout->browser_scroll_dragging = false;
            layout->browser_scroll_drag_column = -1;
            return true;
        }

        if (layout->arp_drag_step_index >= 0 && layout->arp_drag_step_index < OSTI_ARP_STEP_COUNT)
        {
            TunefishWidget *step = layout->arp_step_widgets[layout->arp_drag_step_index];
            if (step)
            {
                osti_update_arp_step_from_mouse(layout, step, mouseX, mouseY, false);
                return true;
            }
        }

        if (g_env_state[0].dragging || g_env_state[1].dragging)
        {
            if (g_env_state[0].dragging)
            {
                TunefishWidget *env = osti_find_widget_by_name(layout, "filter_env_display");
                if (env) osti_env_mouse(layout, env, 0, mouseX, mouseY, false);
            }
            if (g_env_state[1].dragging)
            {
                TunefishWidget *env = osti_find_widget_by_name(layout, "amp_env_display");
                if (env) osti_env_mouse(layout, env, 1, mouseX, mouseY, false);
            }
            return true;
        }
    }

    if (osti_browser_handle_mouse_event(layout, mouseX, mouseY, pressed))
        return true;

    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (!w || !w->visible) continue;
        if (w->type == TF_WIDGET_ENVELOPE_DISPLAY) continue;

        if (tf_widget_handle_mouse_event(w, mouseX, mouseY, pressed))
        {
            if (w == layout->page_common_button || w == layout->page_filter_button ||
                w == layout->page_mod_matrix_button || w == layout->page_fx_button ||
                w == layout->page_lfo_button || w == layout->page_arp_button ||
                w == layout->page_browser_button ||
                w == layout->browser_load_button || w == layout->browser_prev_button ||
                w == layout->browser_next_button || w == layout->browser_default_button ||
                w == layout->browser_clear_button ||
                w == layout->close_button)
            {
                osti_button_clicked(w);
                return true;
            }

            if (w == layout->preset_combo)
            {
                if (pressed)
                    osti_combo_selected(w, w->selectedIndex);
                return true;
            }

            if (w->type == TF_WIDGET_COMBO_BOX)
            {
                osti_combo_selected(w, w->selectedIndex);
                return true;
            }

            if (w->type == TF_WIDGET_TOGGLE_BUTTON || w->type == TF_WIDGET_ROTARY_SLIDER || w->type == TF_WIDGET_PARAMETER_CONTROL)
            {
                osti_param_changed(layout, w, w->value);
                return true;
            }

            return true;
        }
    }

    return true;
}

bool osti_handle_layout_mouse_drag(OsTirusCompleteLayout *layout, int mouseX, int mouseY)
{
    if (!layout || !layout->visible) return false;

    if (g_env_state[0].dragging)
    {
        TunefishWidget *env = osti_find_widget_by_name(layout, "filter_env_display");
        if (env && env->visible)
            return osti_env_mouse(layout, env, 0, mouseX, mouseY, true);
    }

    if (g_env_state[1].dragging)
    {
        TunefishWidget *env = osti_find_widget_by_name(layout, "amp_env_display");
        if (env && env->visible)
            return osti_env_mouse(layout, env, 1, mouseX, mouseY, true);
    }

    if (layout->arp_drag_step_index >= 0 && layout->arp_drag_step_index < OSTI_ARP_STEP_COUNT)
    {
        TunefishWidget *step = layout->arp_step_widgets[layout->arp_drag_step_index];
        if (step && step->visible)
            return osti_update_arp_step_from_mouse(layout, step, mouseX, mouseY, true);
    }

    if (osti_browser_handle_mouse_drag(layout, mouseX, mouseY))
        return true;

    return false;
}

bool osti_handle_layout_keyboard_test(OsTirusCompleteLayout *layout, int key)
{
    if (!layout || !layout->visible) return false;

    if (osti_browser_handle_key_input(layout, key))
        return true;

    switch (key)
    {
        case SDLK_ESCAPE:
            osti_hide_layout(layout);
            ft2_close_synth_editor();
            return true;

        case SDLK_TAB:
            osti_switch_to_page(layout, (layout->current_page + 1) % OSTI_PAGE_COUNT);
            return true;

        case SDLK_F9:
            if (layout->preset_combo && layout->preset_combo->comboItemCount > 0)
            {
                layout->preset_combo->selectedIndex = (layout->preset_combo->selectedIndex + 1) % layout->preset_combo->comboItemCount;
                osti_combo_selected(layout->preset_combo, layout->preset_combo->selectedIndex);
            }
            return true;

        case SDLK_F10:
            if (layout->preset_combo && layout->preset_combo->comboItemCount > 0)
            {
                layout->preset_combo->selectedIndex--;
                if (layout->preset_combo->selectedIndex < 0)
                    layout->preset_combo->selectedIndex = layout->preset_combo->comboItemCount - 1;
                osti_combo_selected(layout->preset_combo, layout->preset_combo->selectedIndex);
            }
            return true;

        default:
            return false;
    }
}

TunefishWidget* osti_find_widget_by_name(OsTirusCompleteLayout *layout, const char *name)
{
    if (!layout || !name || !*name) return NULL;
    for (int i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *w = layout->all_widgets[i];
        if (w && strcmp(w->name, name) == 0)
            return w;
    }
    return NULL;
}

void osti_sync_widgets_with_parameters(OsTirusCompleteLayout *layout)
{
    if (!layout) return;
    osti_set_active_instrument(layout);
    osti_populate_preset_combo(layout);
    osti_sync_preset_combo_selection(layout);
    osti_update_preset_name_label(layout);
    osti_update_slot_label(layout);
    osti_update_voice_meter(layout);
    osti_refresh_browser_controls(layout);

    for (int i = 0; i < layout->all_widget_count; ++i)
        osti_sync_widget_from_parameter(layout, layout->all_widgets[i]);

    osti_update_caption(layout);
    osti_update_widget_visibility(layout);
}

void osti_style_all_widgets_authentic(OsTirusCompleteLayout *layout)
{
    if (!layout) return;
    for (int i = 0; i < layout->all_widget_count; ++i)
        osti_style_widget(layout->all_widgets[i]);
}

static void osti_sync_env_drag_state(int envIndex, const TunefishWidget *widget, int mouseX, int mouseY, bool pressed)
{
    (void)envIndex;
    (void)widget;
    (void)mouseX;
    (void)mouseY;
    (void)pressed;
}
