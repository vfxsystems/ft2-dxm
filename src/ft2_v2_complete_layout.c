#include "ft2_v2_complete_layout.h"

#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_inst_ed.h"
#include "ft2_mouse.h"
#include "ft2_structs.h"
#include "ft2_textboxes.h"
#include "ft2_video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern editor_t editor;
extern instr_t *instr[128 + 4];

V2CompleteLayout* g_active_v2_layout = NULL;

typedef struct {
    int paramId;
    int topicIndex;
    bool isGlobal;
    bool isCombo;
} V2BindingMeta;

static V2BindingMeta g_patch_meta[V2_MAX_PATCH_BINDINGS];
static V2BindingMeta g_global_meta[V2_MAX_GLOBAL_BINDINGS];

static const char* k_page_names[V2_PAGE_COUNT] = { "Patch", "Globals", "Mod" };
static const int k_grid_cols = 5;
static const int k_grid_cell_w = 118;
static const int k_grid_cell_h = 86;
static const int k_grid_origin_x = 12;
static const int k_grid_origin_y = 84;

static int clamp_int(int value, int minValue, int maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static const char* safe_str(const char* s)
{
    return (s && *s) ? s : "";
}

static void register_widget(V2CompleteLayout* layout, TunefishWidget* widget)
{
    if (!layout || !widget) return;
    if (layout->all_widget_count < V2_MAX_WIDGETS) {
        layout->all_widgets[layout->all_widget_count++] = widget;
    }
}

static V2BindingMeta* find_patch_meta_by_widget(V2CompleteLayout* layout, TunefishWidget* widget)
{
    if (!layout || !widget) return NULL;
    for (int i = 0; i < layout->patch_param_count; ++i) {
        if (layout->patch_params[i].control == widget || layout->patch_params[i].label == widget)
            return &g_patch_meta[i];
    }
    return NULL;
}

static V2BindingMeta* find_global_meta_by_widget(V2CompleteLayout* layout, TunefishWidget* widget)
{
    if (!layout || !widget) return NULL;
    for (int i = 0; i < layout->global_param_count; ++i) {
        if (layout->global_params[i].control == widget || layout->global_params[i].label == widget)
            return &g_global_meta[i];
    }
    return NULL;
}

static V2ParamControlBinding* find_patch_binding(V2CompleteLayout* layout, TunefishWidget* widget)
{
    if (!layout || !widget) return NULL;
    for (int i = 0; i < layout->patch_param_count; ++i) {
        if (layout->patch_params[i].control == widget || layout->patch_params[i].label == widget)
            return &layout->patch_params[i];
    }
    return NULL;
}

static V2ParamControlBinding* find_global_binding(V2CompleteLayout* layout, TunefishWidget* widget)
{
    if (!layout || !widget) return NULL;
    for (int i = 0; i < layout->global_param_count; ++i) {
        if (layout->global_params[i].control == widget || layout->global_params[i].label == widget)
            return &layout->global_params[i];
    }
    return NULL;
}

static V2ModRowWidgets* find_mod_row(V2CompleteLayout* layout, TunefishWidget* widget)
{
    if (!layout || !widget) return NULL;
    for (int i = 0; i < V2_MOD_ROWS; ++i) {
        V2ModRowWidgets* row = &layout->mod_rows[i];
        if (row->slot_label == widget || row->source_combo == widget || row->amount_control == widget || row->dest_combo == widget)
            return row;
    }
    return NULL;
}

static int current_instrument_id(void)
{
    if (g_active_v2_layout != NULL &&
        g_active_v2_layout->active_instrument_id >= 1 &&
        g_active_v2_layout->active_instrument_id <= MAX_INST) {
        return g_active_v2_layout->active_instrument_id;
    }

    if (editor.curInstr < 1 || editor.curInstr > MAX_INST) return 0;
    return editor.curInstr;
}

static int topic_visible_param_count(bool global, int topicIndex)
{
    if (global) return ft2_v2_get_global_topic_param_count(topicIndex);
    return ft2_v2_get_topic_param_count(topicIndex);
}

static int topic_param_start(bool global, int topicIndex)
{
    if (global) return ft2_v2_get_global_topic_param_start(topicIndex);
    return ft2_v2_get_topic_param_start(topicIndex);
}

static const Ft2V2TopicInfo* topic_info(bool global, int topicIndex)
{
    if (global) return ft2_v2_get_global_topic_info(topicIndex);
    return ft2_v2_get_topic_info(topicIndex);
}

static void clear_combo_items(TunefishWidget* combo)
{
    if (!combo) return;
    tf_widget_clear_combo_items(combo);
}

static void add_items_from_ctlstr(TunefishWidget* combo, const char* ctlstr)
{
    if (!combo || !ctlstr) return;

    const char* p = ctlstr;
    if (*p == '!') ++p;

    const char* start = p;
    while (*p) {
        if (*p == '|') {
            if (p > start) {
                char tmp[128];
                size_t len = (size_t)(p - start);
                if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
                memcpy(tmp, start, len);
                tmp[len] = '\0';
                tf_widget_add_combo_item(combo, tmp);
            }
            start = p + 1;
        }
        ++p;
    }
    if (p > start) {
        char tmp[128];
        size_t len = (size_t)(p - start);
        if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
        memcpy(tmp, start, len);
        tmp[len] = '\0';
        tf_widget_add_combo_item(combo, tmp);
    }
}

static void set_widget_position(TunefishWidget* widget, int x, int y, int w, int h)
{
    if (!widget) return;
    tf_widget_set_position(widget, x, y);
    tf_widget_set_size(widget, w, h);
}

static void v2_sync_page_buttons(V2CompleteLayout* layout)
{
    if (!layout) return;
    if (layout->page_patch_button) layout->page_patch_button->pressed = (layout->current_page == V2_PAGE_PATCH);
    if (layout->page_globals_button) layout->page_globals_button->pressed = (layout->current_page == V2_PAGE_GLOBALS);
    if (layout->page_mod_button) layout->page_mod_button->pressed = (layout->current_page == V2_PAGE_MOD);
}

static void v2_update_caption(V2CompleteLayout* layout)
{
    if (!layout || !layout->page_caption_label) return;

    char buf[128];
    const int instrID = current_instrument_id();
    switch (layout->current_page) {
        case V2_PAGE_PATCH: {
            const Ft2V2TopicInfo* info = topic_info(false, layout->current_patch_topic);
            snprintf(buf, sizeof(buf), "Patch: %s", info ? safe_str(info->name) : "Patch");
            break;
        }
        case V2_PAGE_GLOBALS: {
            const Ft2V2TopicInfo* info = topic_info(true, layout->current_global_topic);
            snprintf(buf, sizeof(buf), "Globals: %s", info ? safe_str(info->name) : "Globals");
            break;
        }
        case V2_PAGE_MOD:
        default:
            if (instrID > 0)
                snprintf(buf, sizeof(buf), "Mod Matrix: Instr %d", instrID);
            else
                snprintf(buf, sizeof(buf), "Mod Matrix");
            break;
    }
    tf_widget_set_label(layout->page_caption_label, buf);
}

static void v2_update_top_level_visibility(V2CompleteLayout* layout)
{
    if (!layout) return;
    if (layout->patch_topic_combo) layout->patch_topic_combo->visible = (layout->current_page == V2_PAGE_PATCH);
    if (layout->patch_topic_group) layout->patch_topic_group->visible = (layout->current_page == V2_PAGE_PATCH);
    if (layout->global_topic_combo) layout->global_topic_combo->visible = (layout->current_page == V2_PAGE_GLOBALS);
    if (layout->global_topic_group) layout->global_topic_group->visible = (layout->current_page == V2_PAGE_GLOBALS);
    if (layout->mod_bank_combo) layout->mod_bank_combo->visible = (layout->current_page == V2_PAGE_MOD);
    if (layout->mod_group) layout->mod_group->visible = (layout->current_page == V2_PAGE_MOD);
}

static void v2_update_patch_binding_visibility(V2CompleteLayout* layout)
{
    if (!layout) return;
    for (int i = 0; i < layout->patch_param_count; ++i) {
        const bool visible = (layout->current_page == V2_PAGE_PATCH && layout->patch_params[i].topicIndex == layout->current_patch_topic);
        if (layout->patch_params[i].label) layout->patch_params[i].label->visible = visible;
        if (layout->patch_params[i].control) layout->patch_params[i].control->visible = visible;
    }
}

static void v2_update_global_binding_visibility(V2CompleteLayout* layout)
{
    if (!layout) return;
    for (int i = 0; i < layout->global_param_count; ++i) {
        const bool visible = (layout->current_page == V2_PAGE_GLOBALS && layout->global_params[i].topicIndex == layout->current_global_topic);
        if (layout->global_params[i].label) layout->global_params[i].label->visible = visible;
        if (layout->global_params[i].control) layout->global_params[i].control->visible = visible;
    }
}

static void v2_update_mod_visibility(V2CompleteLayout* layout)
{
    if (!layout) return;
    const int instrID = current_instrument_id();
    const int modCount = (instrID > 0) ? ft2_v2_get_mod_count_for_instrument(instrID) : 0;
    const int bankBase = layout->current_mod_bank * V2_MOD_ROWS;

    for (int i = 0; i < V2_MOD_ROWS; ++i) {
        const int slot = bankBase + i;
        const bool visible = (layout->current_page == V2_PAGE_MOD && slot < modCount);
        V2ModRowWidgets* row = &layout->mod_rows[i];
        if (row->slot_label) row->slot_label->visible = visible;
        if (row->source_combo) row->source_combo->visible = visible;
        if (row->amount_control) row->amount_control->visible = visible;
        if (row->dest_combo) row->dest_combo->visible = visible;
    }
}

static void v2_update_all_visibility(V2CompleteLayout* layout)
{
    if (!layout) return;
    v2_update_top_level_visibility(layout);
    v2_update_patch_binding_visibility(layout);
    v2_update_global_binding_visibility(layout);
    v2_update_mod_visibility(layout);
    v2_update_caption(layout);
    v2_sync_page_buttons(layout);
}

static void v2_patch_param_changed(TunefishWidget* widget, float value);
static void v2_global_param_changed(TunefishWidget* widget, float value);
static void v2_page_button_clicked(TunefishWidget* widget);
static void v2_close_button_clicked(TunefishWidget* widget);
static void v2_preset_combo_changed(TunefishWidget* widget, int selectedIndex);
static void v2_topic_combo_changed(TunefishWidget* widget, int selectedIndex);
static void v2_mod_bank_combo_changed(TunefishWidget* widget, int selectedIndex);
static void v2_mod_amount_changed(TunefishWidget* widget, float value);

static void v2_build_patch_param_widgets(V2CompleteLayout* layout)
{
    if (!layout) return;

    layout->patch_param_count = 0;
    for (int topic = 0; topic < ft2_v2_get_topic_count(); ++topic) {
        const int start = topic_param_start(false, topic);
        const int count = topic_visible_param_count(false, topic);
        int visibleIndex = 0;

        for (int i = 0; i < count; ++i) {
            const int paramId = start + i;
            const Ft2V2ParamInfo* info = ft2_v2_get_param_info(paramId);
            if (!info || info->ctltype == FT2_V2_CTL_SKIP || !info->name || !*info->name) {
                continue;
            }

            if (layout->patch_param_count >= V2_MAX_PATCH_BINDINGS) {
                return;
            }

            V2ParamControlBinding* binding = &layout->patch_params[layout->patch_param_count];
            memset(binding, 0, sizeof(*binding));
            binding->paramId = paramId;
            binding->topicIndex = topic;
            binding->isGlobal = false;
            binding->ctlType = info->ctltype;

            const int col = visibleIndex % k_grid_cols;
            const int row = visibleIndex / k_grid_cols;
            const int cellX = k_grid_origin_x + (col * k_grid_cell_w);
            const int cellY = k_grid_origin_y + (row * k_grid_cell_h);

            char labelName[64];
            char controlName[64];
            snprintf(labelName, sizeof(labelName), "v2_p_label_%d", paramId);
            snprintf(controlName, sizeof(controlName), "v2_p_ctrl_%d", paramId);

            if (info->ctltype == FT2_V2_CTL_MB) {
                binding->label = tf_create_label(labelName, safe_str(info->name), cellX, cellY, 112, 10);
                binding->control = tf_create_combo_box(controlName, cellX, cellY + 12, 112, 18, NULL, 0);
                if (binding->control) {
                    add_items_from_ctlstr(binding->control, info->ctlstr);
                    binding->control->onValueChange = v2_patch_param_changed;
                }
                register_widget(layout, binding->label);
                register_widget(layout, binding->control);
            } else {
                binding->label = NULL;
                binding->control = tf_create_rotary_slider(controlName, cellX + 56, cellY + 32, 22, -2.35f, 2.35f);
                if (binding->control) {
                    tf_widget_set_label(binding->control, safe_str(info->name));
                    binding->control->onValueChange = v2_patch_param_changed;
                }
                register_widget(layout, binding->control);
            }

            g_patch_meta[layout->patch_param_count] = (V2BindingMeta){ paramId, topic, false, info->ctltype == FT2_V2_CTL_MB };
            layout->patch_param_count++;
            ++visibleIndex;
        }
    }
}

static void v2_build_global_param_widgets(V2CompleteLayout* layout)
{
    if (!layout) return;

    layout->global_param_count = 0;
    for (int topic = 0; topic < ft2_v2_get_global_topic_count(); ++topic) {
        const int start = topic_param_start(true, topic);
        const int count = topic_visible_param_count(true, topic);
        int visibleIndex = 0;

        for (int i = 0; i < count; ++i) {
            const int paramId = start + i;
            const Ft2V2ParamInfo* info = ft2_v2_get_global_param_info(paramId);
            if (!info || info->ctltype == FT2_V2_CTL_SKIP || !info->name || !*info->name) {
                continue;
            }

            if (layout->global_param_count >= V2_MAX_GLOBAL_BINDINGS) {
                return;
            }

            V2ParamControlBinding* binding = &layout->global_params[layout->global_param_count];
            memset(binding, 0, sizeof(*binding));
            binding->paramId = paramId;
            binding->topicIndex = topic;
            binding->isGlobal = true;
            binding->ctlType = info->ctltype;

            const int col = visibleIndex % k_grid_cols;
            const int row = visibleIndex / k_grid_cols;
            const int cellX = k_grid_origin_x + (col * k_grid_cell_w);
            const int cellY = k_grid_origin_y + (row * k_grid_cell_h);

            char labelName[64];
            char controlName[64];
            snprintf(labelName, sizeof(labelName), "v2_g_label_%d", paramId);
            snprintf(controlName, sizeof(controlName), "v2_g_ctrl_%d", paramId);

            if (info->ctltype == FT2_V2_CTL_MB) {
                binding->label = tf_create_label(labelName, safe_str(info->name), cellX, cellY, 112, 10);
                binding->control = tf_create_combo_box(controlName, cellX, cellY + 12, 112, 18, NULL, 0);
                if (binding->control) {
                    add_items_from_ctlstr(binding->control, info->ctlstr);
                    binding->control->onValueChange = v2_global_param_changed;
                }
                register_widget(layout, binding->label);
                register_widget(layout, binding->control);
            } else {
                binding->label = NULL;
                binding->control = tf_create_rotary_slider(controlName, cellX + 56, cellY + 32, 22, -2.35f, 2.35f);
                if (binding->control) {
                    tf_widget_set_label(binding->control, safe_str(info->name));
                    binding->control->onValueChange = v2_global_param_changed;
                }
                register_widget(layout, binding->control);
            }

            g_global_meta[layout->global_param_count] = (V2BindingMeta){ paramId, topic, true, info->ctltype == FT2_V2_CTL_MB };
            layout->global_param_count++;
            ++visibleIndex;
        }
    }
}

static void v2_build_mod_widgets(V2CompleteLayout* layout)
{
    if (!layout) return;

    const int labelY = 110;
    const int rowBaseY = 128;
    const int rowH = 26;

    for (int i = 0; i < V2_MOD_ROWS; ++i) {
        V2ModRowWidgets* row = &layout->mod_rows[i];
        memset(row, 0, sizeof(*row));
        row->slotIndex = i;

        const int y = rowBaseY + (i * rowH);
        char tmp[64];

        snprintf(tmp, sizeof(tmp), "v2_mod_slot_%d", i);
        row->slot_label = tf_create_label(tmp, "Slot", 18, y + 2, 56, 10);

        snprintf(tmp, sizeof(tmp), "v2_mod_src_%d", i);
        row->source_combo = tf_create_combo_box(tmp, 72, y, 140, 18, NULL, 0);
        if (row->source_combo) {
            for (int s = 0; s < ft2_v2_get_mod_source_count(); ++s) {
                tf_widget_add_combo_item(row->source_combo, ft2_v2_get_mod_source_name(s));
            }
            row->source_combo->onComboSelect = v2_mod_bank_combo_changed;
        }

        snprintf(tmp, sizeof(tmp), "v2_mod_amt_%d", i);
        row->amount_control = tf_create_parameter_control(tmp, "Amt", 220, y, 110, 18);
        if (row->amount_control) {
            row->amount_control->minValue = 0.0f;
            row->amount_control->maxValue = 127.0f;
            row->amount_control->value = 0.0f;
            row->amount_control->onValueChange = v2_mod_amount_changed;
        }

        snprintf(tmp, sizeof(tmp), "v2_mod_dst_%d", i);
        row->dest_combo = tf_create_combo_box(tmp, 342, y, 276, 18, NULL, 0);
        if (row->dest_combo) {
            for (int d = 0; d < ft2_v2_get_mod_dest_count(); ++d) {
                tf_widget_add_combo_item(row->dest_combo, ft2_v2_get_mod_dest_name(d));
            }
            row->dest_combo->onComboSelect = v2_mod_bank_combo_changed;
        }

        register_widget(layout, row->slot_label);
        register_widget(layout, row->source_combo);
        register_widget(layout, row->amount_control);
        register_widget(layout, row->dest_combo);
    }

    (void)labelY;
}

static void v2_populate_preset_combo(V2CompleteLayout* layout)
{
    if (!layout || !layout->preset_combo) return;

    clear_combo_items(layout->preset_combo);
    const int count = ft2_v2_get_factory_preset_count();
    for (int i = 0; i < count; ++i) {
        const char* name = ft2_v2_get_preset_name_for_instrument(current_instrument_id(), i);
        char buf[160];
        if (!name || !*name) {
            snprintf(buf, sizeof(buf), "Preset %03d", i + 1);
            name = buf;
        }
        tf_widget_add_combo_item(layout->preset_combo, name);
    }
    if (count > 0) {
        layout->preset_combo->selectedIndex = clamp_int(ft2_v2_get_current_preset_for_instrument(current_instrument_id()), 0, count - 1);
        layout->preset_combo->value = (count > 1) ? ((float)layout->preset_combo->selectedIndex / (float)(count - 1)) : 0.0f;
    } else {
        layout->preset_combo->selectedIndex = 0;
        layout->preset_combo->value = 0.0f;
    }
}

static void v2_sync_preset_combo_selection(V2CompleteLayout* layout)
{
    if (!layout || !layout->preset_combo) return;

    const int count = layout->preset_combo->comboItemCount;
    if (count <= 0) {
        layout->preset_combo->selectedIndex = 0;
        layout->preset_combo->value = 0.0f;
        return;
    }

    const int preset = clamp_int(ft2_v2_get_current_preset_for_instrument(current_instrument_id()), 0, count - 1);
    layout->preset_combo->selectedIndex = preset;
    layout->preset_combo->value = (count > 1) ? ((float)preset / (float)(count - 1)) : 0.0f;
}

static void v2_populate_topic_combo(V2CompleteLayout* layout, bool global)
{
    if (!layout) return;
    TunefishWidget* combo = global ? layout->global_topic_combo : layout->patch_topic_combo;
    if (!combo) return;

    clear_combo_items(combo);
    const int count = global ? ft2_v2_get_global_topic_count() : ft2_v2_get_topic_count();
    for (int i = 0; i < count; ++i) {
        const Ft2V2TopicInfo* info = topic_info(global, i);
        if (!info) continue;
        tf_widget_add_combo_item(combo, safe_str(info->name));
    }
    if (count > 0) {
        combo->selectedIndex = clamp_int(global ? layout->current_global_topic : layout->current_patch_topic, 0, count - 1);
        combo->value = (count > 1) ? ((float)combo->selectedIndex / (float)(count - 1)) : 0.0f;
    } else {
        combo->selectedIndex = 0;
        combo->value = 0.0f;
    }
}

static void v2_populate_mod_bank_combo(V2CompleteLayout* layout)
{
    if (!layout || !layout->mod_bank_combo) return;
    clear_combo_items(layout->mod_bank_combo);
    for (int i = 0; i < 32; ++i) {
        char buf[32];
        const int start = (i * V2_MOD_ROWS) + 1;
        const int end = start + V2_MOD_ROWS - 1;
        snprintf(buf, sizeof(buf), "%03d-%03d", start, end);
        tf_widget_add_combo_item(layout->mod_bank_combo, buf);
    }
    layout->mod_bank_combo->selectedIndex = clamp_int(layout->current_mod_bank, 0, 31);
    layout->mod_bank_combo->value = (float)layout->mod_bank_combo->selectedIndex / 31.0f;
}

static void v2_sync_mod_rows(V2CompleteLayout* layout)
{
    if (!layout) return;
    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    const int modCount = ft2_v2_get_mod_count_for_instrument(instrID);
    const int base = layout->current_mod_bank * V2_MOD_ROWS;

    for (int i = 0; i < V2_MOD_ROWS; ++i) {
        const int slot = base + i;
        V2ModRowWidgets* row = &layout->mod_rows[i];
        if (!row->source_combo || !row->amount_control || !row->dest_combo) continue;

        row->slotIndex = slot;
        char slotLabel[32];
        snprintf(slotLabel, sizeof(slotLabel), "Slot %d", slot + 1);
        tf_widget_set_label(row->slot_label, slotLabel);

        if (slot < modCount) {
            int source = 0, amount = 0, dest = 0;
            ft2_v2_get_mod_slot_for_instrument(instrID, slot, &source, &amount, &dest);

            if (row->source_combo->comboItemCount > 0) {
                row->source_combo->selectedIndex = clamp_int(source, 0, row->source_combo->comboItemCount - 1);
            } else {
                row->source_combo->selectedIndex = 0;
            }
            if (row->source_combo->comboItemCount > 1)
                row->source_combo->value = (float)row->source_combo->selectedIndex / (float)(row->source_combo->comboItemCount - 1);
            else
                row->source_combo->value = 0.0f;

            row->amount_control->value = clamp_int(amount, 0, 127) / 127.0f;
            row->amount_control->minValue = 0.0f;
            row->amount_control->maxValue = 127.0f;

            const int destListIndex = ft2_v2_find_mod_dest_list_index(dest);
            if (row->dest_combo->comboItemCount > 0) {
                row->dest_combo->selectedIndex = clamp_int(destListIndex, 0, row->dest_combo->comboItemCount - 1);
            } else {
                row->dest_combo->selectedIndex = 0;
            }
            if (row->dest_combo->comboItemCount > 1)
                row->dest_combo->value = (float)row->dest_combo->selectedIndex / (float)(row->dest_combo->comboItemCount - 1);
            else
                row->dest_combo->value = 0.0f;
        }
        else {
            if (row->source_combo) {
                row->source_combo->selectedIndex = 0;
                row->source_combo->value = 0.0f;
            }
            if (row->amount_control) {
                row->amount_control->value = 0.0f;
            }
            if (row->dest_combo) {
                row->dest_combo->selectedIndex = 0;
                row->dest_combo->value = 0.0f;
            }
        }
    }
}

static void v2_sync_patch_controls(V2CompleteLayout* layout)
{
    if (!layout) return;
    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    for (int i = 0; i < layout->patch_param_count; ++i) {
        V2ParamControlBinding* binding = &layout->patch_params[i];
        if (!binding->control) continue;
        float value = ft2_v2_get_param_for_instrument(instrID, binding->paramId);
        value = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
        binding->control->value = value;
        if (binding->ctlType == FT2_V2_CTL_MB && binding->control->comboItemCount > 1) {
            const int idx = clamp_int((int)(value * (float)(binding->control->comboItemCount - 1) + 0.5f), 0, binding->control->comboItemCount - 1);
            binding->control->selectedIndex = idx;
        }
    }
}

static void v2_sync_global_controls(V2CompleteLayout* layout)
{
    if (!layout) return;
    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    for (int i = 0; i < layout->global_param_count; ++i) {
        V2ParamControlBinding* binding = &layout->global_params[i];
        if (!binding->control) continue;
        float value = ft2_v2_get_global_param_for_instrument(instrID, binding->paramId);
        value = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
        binding->control->value = value;
        if (binding->ctlType == FT2_V2_CTL_MB && binding->control->comboItemCount > 1) {
            const int idx = clamp_int((int)(value * (float)(binding->control->comboItemCount - 1) + 0.5f), 0, binding->control->comboItemCount - 1);
            binding->control->selectedIndex = idx;
        }
    }
}

static void v2_sync_voice_meter(V2CompleteLayout* layout)
{
    if (!layout || !layout->active_voices_meter) return;
    const int instrID = current_instrument_id();
    if (instrID <= 0) {
        layout->active_voices_meter->value = 0.0f;
        return;
    }
    const int voices = ft2_v2_get_active_voice_count(instrID);
    const float norm = (voices <= 0) ? 0.0f : (voices >= 16 ? 1.0f : (float)voices / 16.0f);
    layout->active_voices_meter->value = norm;
    layout->active_voices_meter->peakLevel = norm;
}

static void v2_sync_current_page_selection(V2CompleteLayout* layout)
{
    if (!layout) return;
    if (layout->current_page == V2_PAGE_PATCH && layout->patch_topic_combo) {
        const int count = ft2_v2_get_topic_count();
        layout->patch_topic_combo->selectedIndex = (count > 0) ? clamp_int(layout->current_patch_topic, 0, count - 1) : 0;
    } else if (layout->current_page == V2_PAGE_GLOBALS && layout->global_topic_combo) {
        const int count = ft2_v2_get_global_topic_count();
        layout->global_topic_combo->selectedIndex = (count > 0) ? clamp_int(layout->current_global_topic, 0, count - 1) : 0;
    } else if (layout->current_page == V2_PAGE_MOD && layout->mod_bank_combo) {
        layout->mod_bank_combo->selectedIndex = clamp_int(layout->current_mod_bank, 0, 31);
    }
}

static void v2_sync_all_controls(V2CompleteLayout* layout)
{
    if (!layout) return;
    v2_populate_preset_combo(layout);
    v2_populate_topic_combo(layout, false);
    v2_populate_topic_combo(layout, true);
    v2_populate_mod_bank_combo(layout);
    v2_sync_patch_controls(layout);
    v2_sync_global_controls(layout);
    v2_sync_mod_rows(layout);
    v2_sync_voice_meter(layout);
    v2_update_all_visibility(layout);
    v2_sync_current_page_selection(layout);
}

static void v2_sync_controls_for_current_preset(V2CompleteLayout* layout)
{
    if (!layout) return;
    v2_sync_preset_combo_selection(layout);
    v2_sync_patch_controls(layout);
    v2_sync_global_controls(layout);
    v2_sync_mod_rows(layout);
    v2_sync_voice_meter(layout);
    v2_update_all_visibility(layout);
    v2_sync_current_page_selection(layout);
}

static void v2_set_page(V2CompleteLayout* layout, int page)
{
    if (!layout) return;
    layout->current_page = clamp_int(page, 0, V2_PAGE_COUNT - 1);
    v2_update_all_visibility(layout);
}

static void v2_patch_param_changed(TunefishWidget* widget, float value)
{
    V2CompleteLayout* layout = g_active_v2_layout;
    V2ParamControlBinding* binding = find_patch_binding(layout, widget);
    if (!layout || !binding) return;
    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    ft2_v2_set_param_for_instrument(instrID, binding->paramId, value);
    v2_sync_patch_controls(layout);
}

static void v2_global_param_changed(TunefishWidget* widget, float value)
{
    V2CompleteLayout* layout = g_active_v2_layout;
    V2ParamControlBinding* binding = find_global_binding(layout, widget);
    if (!layout || !binding) return;
    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    ft2_v2_set_global_param_for_instrument(instrID, binding->paramId, value);
    v2_sync_global_controls(layout);
}

static void v2_mod_amount_changed(TunefishWidget* widget, float value)
{
    V2CompleteLayout* layout = g_active_v2_layout;
    if (!layout || !widget) return;

    V2ModRowWidgets* row = find_mod_row(layout, widget);
    if (!row) return;

    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    int source = 0;
    int dest = 0;
    ft2_v2_get_mod_slot_for_instrument(instrID, row->slotIndex, &source, NULL, &dest);
    const int rawAmount = clamp_int((int)(value * 127.0f + 0.5f), 0, 127);
    ft2_v2_set_mod_slot_for_instrument(instrID, row->slotIndex, source, rawAmount, dest);
    v2_sync_mod_rows(layout);
}

static void v2_page_button_clicked(TunefishWidget* widget)
{
    if (!widget || !g_active_v2_layout) return;
    if (widget == g_active_v2_layout->page_patch_button) {
        v2_set_page(g_active_v2_layout, V2_PAGE_PATCH);
    } else if (widget == g_active_v2_layout->page_globals_button) {
        v2_set_page(g_active_v2_layout, V2_PAGE_GLOBALS);
    } else if (widget == g_active_v2_layout->page_mod_button) {
        v2_set_page(g_active_v2_layout, V2_PAGE_MOD);
    }
}

static void v2_close_button_clicked(TunefishWidget* widget)
{
    (void)widget;
    if (!g_active_v2_layout) return;
    v2_hide_layout(g_active_v2_layout);
    ft2_close_synth_editor();
}

static void v2_preset_combo_changed(TunefishWidget* widget, int selectedIndex)
{
    V2CompleteLayout* layout = g_active_v2_layout;
    if (!layout) return;

    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    const int count = ft2_v2_get_factory_preset_count();
    const int preset = (count > 0) ? clamp_int(selectedIndex, 0, count - 1) : 0;
    if (!ft2_v2_load_preset_for_instrument(instrID, preset))
        return;

    if (widget) {
        widget->selectedIndex = preset;
        widget->value = (count > 1) ? ((float)preset / (float)(count - 1)) : 0.0f;
    }

    v2_sync_controls_for_current_preset(layout);
}

static void v2_topic_combo_changed(TunefishWidget* widget, int selectedIndex)
{
    if (!widget || !g_active_v2_layout) return;
    if (widget == g_active_v2_layout->patch_topic_combo) {
        g_active_v2_layout->current_patch_topic = clamp_int(selectedIndex, 0, ft2_v2_get_topic_count() - 1);
    } else if (widget == g_active_v2_layout->global_topic_combo) {
        g_active_v2_layout->current_global_topic = clamp_int(selectedIndex, 0, ft2_v2_get_global_topic_count() - 1);
    }
    v2_update_all_visibility(g_active_v2_layout);
}

static void v2_mod_bank_combo_changed(TunefishWidget* widget, int selectedIndex)
{
    if (!widget || !g_active_v2_layout) return;

    if (widget == g_active_v2_layout->mod_bank_combo) {
        g_active_v2_layout->current_mod_bank = clamp_int(selectedIndex, 0, 31);
        v2_update_mod_visibility(g_active_v2_layout);
        v2_sync_mod_rows(g_active_v2_layout);
        return;
    }

    const int instrID = current_instrument_id();
    if (instrID <= 0) return;

    for (int i = 0; i < V2_MOD_ROWS; ++i) {
        V2ModRowWidgets* row = &g_active_v2_layout->mod_rows[i];
        const int slot = row->slotIndex;
        if (widget == row->source_combo) {
            int amount = 0, dest = 0;
            ft2_v2_get_mod_slot_for_instrument(instrID, slot, NULL, &amount, &dest);
            ft2_v2_set_mod_slot_for_instrument(instrID, slot, selectedIndex, amount, dest);
        } else if (widget == row->dest_combo) {
            int source = 0, amount = 0;
            ft2_v2_get_mod_slot_for_instrument(instrID, slot, &source, &amount, NULL);
            ft2_v2_set_mod_slot_for_instrument(instrID, slot, source, amount, selectedIndex);
        } else if (widget == row->amount_control) {
            int source = 0, dest = 0;
            ft2_v2_get_mod_slot_for_instrument(instrID, slot, &source, NULL, &dest);
            int rawAmount = clamp_int((int)(widget->value * 127.0f + 0.5f), 0, 127);
            ft2_v2_set_mod_slot_for_instrument(instrID, slot, source, rawAmount, dest);
        }
    }
    v2_sync_mod_rows(g_active_v2_layout);
}

V2CompleteLayout* v2_create_complete_layout(void)
{
    V2CompleteLayout* layout = (V2CompleteLayout*)calloc(1, sizeof(V2CompleteLayout));
    if (!layout) return NULL;

    layout->current_page = V2_PAGE_PATCH;
    layout->current_patch_topic = 0;
    layout->current_global_topic = 0;
    layout->current_mod_bank = 0;
    layout->active_instrument_id = (editor.curInstr >= 1 && editor.curInstr <= MAX_INST) ? editor.curInstr : 0;
    layout->initialized = true;

    layout->title_label = tf_create_label("v2_title_label", "V2 Synth Editor", 12, 10, 160, 18);
    layout->page_caption_label = tf_create_label("v2_caption_label", "Patch", 12, 34, 320, 14);
    layout->preset_combo = tf_create_combo_box("v2_preset_combo", 150, 8, 190, 18, NULL, 0);
    layout->page_patch_button = tf_create_button("v2_page_patch_btn", "Patch", 348, 8, 50, 18);
    layout->page_globals_button = tf_create_button("v2_page_globals_btn", "Globals", 402, 8, 58, 18);
    layout->page_mod_button = tf_create_button("v2_page_mod_btn", "Mod", 464, 8, 38, 18);
    layout->close_button = tf_create_button("v2_close_btn", "X", 596, 8, 24, 18);
    layout->active_voices_meter = tf_create_level_meter("v2_voices_meter", 528, 10, 58, 12, 12, true);
    layout->patch_topic_combo = tf_create_combo_box("v2_patch_topic_combo", 12, 54, 180, 18, NULL, 0);
    layout->patch_topic_group = tf_create_group_box("v2_patch_topic_group", "Patch", 10, 80, 612, 260);
    layout->global_topic_combo = tf_create_combo_box("v2_global_topic_combo", 12, 54, 180, 18, NULL, 0);
    layout->global_topic_group = tf_create_group_box("v2_global_topic_group", "Globals", 10, 80, 612, 260);
    layout->mod_bank_combo = tf_create_combo_box("v2_mod_bank_combo", 12, 54, 150, 18, NULL, 0);
    layout->mod_group = tf_create_group_box("v2_mod_group", "Mod Matrix", 10, 80, 612, 260);

    if (layout->preset_combo) layout->preset_combo->onComboSelect = v2_preset_combo_changed;
    if (layout->page_patch_button) layout->page_patch_button->onClick = v2_page_button_clicked;
    if (layout->page_globals_button) layout->page_globals_button->onClick = v2_page_button_clicked;
    if (layout->page_mod_button) layout->page_mod_button->onClick = v2_page_button_clicked;
    if (layout->close_button) layout->close_button->onClick = v2_close_button_clicked;
    if (layout->patch_topic_combo) layout->patch_topic_combo->onComboSelect = v2_topic_combo_changed;
    if (layout->global_topic_combo) layout->global_topic_combo->onComboSelect = v2_topic_combo_changed;
    if (layout->mod_bank_combo) layout->mod_bank_combo->onComboSelect = v2_mod_bank_combo_changed;

    register_widget(layout, layout->title_label);
    register_widget(layout, layout->page_caption_label);
    register_widget(layout, layout->preset_combo);
    register_widget(layout, layout->page_patch_button);
    register_widget(layout, layout->page_globals_button);
    register_widget(layout, layout->page_mod_button);
    register_widget(layout, layout->close_button);
    register_widget(layout, layout->active_voices_meter);
    register_widget(layout, layout->patch_topic_combo);
    register_widget(layout, layout->patch_topic_group);
    register_widget(layout, layout->global_topic_combo);
    register_widget(layout, layout->global_topic_group);
    register_widget(layout, layout->mod_bank_combo);
    register_widget(layout, layout->mod_group);

    v2_build_patch_param_widgets(layout);
    v2_build_global_param_widgets(layout);
    v2_build_mod_widgets(layout);

    v2_sync_all_controls(layout);
    v2_style_all_widgets_authentic(layout);
    v2_update_all_visibility(layout);
    return layout;
}

void v2_destroy_complete_layout(V2CompleteLayout* layout)
{
    if (!layout) return;
    for (int i = 0; i < layout->all_widget_count; ++i) {
        tf_widget_destroy(layout->all_widgets[i]);
    }
    free(layout);
    if (g_active_v2_layout == layout) g_active_v2_layout = NULL;
}

void v2_show_layout(V2CompleteLayout* layout)
{
    if (!layout) return;
    g_active_v2_layout = layout;
    layout->active_instrument_id = (editor.curInstr >= 1 && editor.curInstr <= MAX_INST) ? editor.curInstr : 0;
    layout->visible = true;
    v2_sync_all_controls(layout);
}

void v2_hide_layout(V2CompleteLayout* layout)
{
    if (!layout) return;
    layout->visible = false;
    if (g_active_v2_layout == layout) g_active_v2_layout = NULL;
}

void v2_switch_to_page(V2CompleteLayout* layout, int page)
{
    if (!layout) return;
    layout->current_page = clamp_int(page, 0, V2_PAGE_COUNT - 1);
    v2_update_all_visibility(layout);
}

void v2_render_complete_layout(V2CompleteLayout* layout)
{
    if (!layout || !layout->visible) return;
    v2_sync_controls_for_current_preset(layout);
    fillRect(0, 0, SCREEN_W, SCREEN_H, PAL_DESKTOP);

    for (int i = 0; i < layout->all_widget_count; ++i) {
        TunefishWidget* w = layout->all_widgets[i];
        if (w && w->visible && w->type != TF_WIDGET_BITMAP) {
            tf_draw_widget(w);
        }
    }
}

bool v2_handle_layout_mouse_event(V2CompleteLayout* layout, int mouseX, int mouseY, bool pressed)
{
    if (!layout || !layout->visible) return false;
    for (int i = 0; i < layout->all_widget_count; ++i) {
        TunefishWidget* w = layout->all_widgets[i];
        if (!w || !w->visible) continue;
        if (tf_widget_handle_mouse_event(w, mouseX, mouseY, pressed)) {
            return true;
        }
    }
    return false;
}

bool v2_handle_layout_mouse_drag(V2CompleteLayout* layout, int mouseX, int mouseY)
{
    (void)layout;
    (void)mouseX;
    (void)mouseY;
    return false;
}

bool v2_handle_layout_keyboard_test(V2CompleteLayout* layout, int key)
{
    if (!layout || !layout->visible) return false;
    switch (key) {
        case SDLK_ESCAPE:
            v2_hide_layout(layout);
            ft2_close_synth_editor();
            return true;
        case SDLK_TAB:
            v2_switch_to_page(layout, (layout->current_page + 1) % V2_PAGE_COUNT);
            return true;
        default:
            return false;
    }
}

void v2_sync_widgets_with_parameters(V2CompleteLayout* layout)
{
    if (!layout) return;
    v2_sync_all_controls(layout);
}

void v2_update_all_widgets_from_synth(V2CompleteLayout* layout)
{
    v2_sync_widgets_with_parameters(layout);
}

TunefishWidget* v2_find_widget_by_name(V2CompleteLayout* layout, const char* name)
{
    if (!layout || !name) return NULL;
    for (int i = 0; i < layout->all_widget_count; ++i) {
        TunefishWidget* w = layout->all_widgets[i];
        if (w && strcmp(w->name, name) == 0) return w;
    }
    return NULL;
}

void v2_style_all_widgets_authentic(V2CompleteLayout* layout)
{
    if (!layout) return;
    for (int i = 0; i < layout->all_widget_count; ++i) {
        TunefishWidget* w = layout->all_widgets[i];
        if (w) {
            tf_apply_tunefish_styling(w);
        }
    }
}
