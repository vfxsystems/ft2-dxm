/*
 * dx_complete_layout.c -- C-style Dexed editor layout implementation
 *
 * Creates a lightweight two-page Dexed editor using the existing Tunefish
 * widget primitives so it can be rendered by FT2's SDL-based drawing layer.
 *
 * This implementation approximates the DX look using color constants inspired
 * by DXLookNFeel (no JUCE dependency / no BinaryData images).
 *
 * The controls bind directly to the embedded engine's operator, global,
 * modulation, filter, and effect parameters.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dx_complete_layout.h"
#include "../ft2_header.h"
#include "../ft2_gui.h"                     /* for fillRect / textOut etc. */
#include "../ft2_video.h"
#include "../ft2_tunefish_widgets.h"
#include "../ft2_tunefish_complete_layout.h"
#include "../ft2_dexed.h"
#include "../ft2_bmp.h"
#include "../shared/ft2_ui_schema.h"
#include "../shared/ft2_ui_assets.h"
#include "../ft2_inst_ed.h"
#include "../ft2_structs.h"
#include <math.h>

#ifdef FT2_UI_TRACE
#define DX_UI_TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#define DX_UI_TRACE(...) ((void)0)
#endif

#ifdef DX_USE_SCHEMA_LAYOUT
#include "dx_complete_layout_schema.h"
#endif

/* Forward declarations for factory preset helpers implemented in ft2_dexed.c
 * These allow the UI code below to query and request factory preset loading
 * (the implementation extracts 155-byte DX7 patches from the bundled sysex).
 */
extern int  ft2_dx_get_factory_preset_count(void);
extern const char* ft2_dx_get_factory_preset_name(int index);
extern int  ft2_dx_load_factory_preset_for_current_instrument(int index);

#define DX_BG_COLOUR        0x3C322FFFu   /* dark brownish background (approx Dexed) */
#define DX_FILL_COLOUR      0x4D9F97FFu   /* teal-ish fill */
#define DX_ACCENT_COLOUR    0xFF8000FFu   /* orange accent */
#define DX_TEXT_COLOR       0xFFFFFFFFu   /* white text */

/* Global active layout pointer */
DexedCompleteLayout* g_active_dexed_layout = NULL;
static bool g_dx_close_pending = false;

static void dx_button_on_click(TunefishWidget* widget);
static void dx_knob_on_value_change(TunefishWidget* widget, float newValue);
static void dx_combo_on_select(TunefishWidget* widget, int selectedIndex);
static void dx_op_select_on_click(TunefishWidget* widget);
static void dx_register_widget(DexedCompleteLayout* l, TunefishWidget* w, int page);
static void dx_draw_operator_envelope(const DexedCompleteLayout* layout);
static void dx_widget_set_value_silent(TunefishWidget* widget, float value);

typedef struct {
    int x;
    int y;
} dx_env_point_t;

static bool dx_env_dragging = false;
static int dx_env_drag_index = -1;
static int dx_env_selected_index = -1;
static int dx_env_last_mouse_x = 0;
static int dx_env_last_mouse_y = 0;
static int dx_env_save_mouse_x = 0;
static int dx_env_save_mouse_y = 0;

#ifdef DX_USE_SCHEMA_LAYOUT
static int dx_page_from_schema(ft2_ui_widget_page_t page)
{
    switch (page) {
        case FT2_UI_WIDGET_PAGE_1: return DX_PAGE_MAIN;
        case FT2_UI_WIDGET_PAGE_2: return DX_PAGE_EFFECTS;
        case FT2_UI_WIDGET_PAGE_BOTH: return DX_PAGE_BOTH;
        default: return DX_PAGE_MAIN;
    }
}

static const char *dx_schema_name(const char *name, char *buffer, size_t buffer_len,
                                  const char *prefix, int index)
{
    if (name && name[0] != '\0') return name;
    if (!buffer || buffer_len == 0) return "";
    if (!prefix) prefix = "dx_widget";
    snprintf(buffer, buffer_len, "%s_%d", prefix, index);
    return buffer;
}

static void dx_schema_apply_knob_defaults(TunefishWidget* widget)
{
    if (!widget || widget->type != TF_WIDGET_ROTARY_SLIDER) return;

    float default_value;
    if (strcmp(widget->name, "dx_filter_cutoff") == 0) {
        default_value = 1.0f;
    } else if (strcmp(widget->name, "dx_filter_reso") == 0) {
        default_value = 0.0f;
    } else if (strcmp(widget->name, "dx_filter_gain") == 0) {
        default_value = 1.0f;
    } else if (strcmp(widget->name, "dx_lfo_rate") == 0) {
        default_value = 0.2f;
    } else if (strcmp(widget->name, "dx_lfo_delay") == 0) {
        default_value = 0.0f;
    } else if (strcmp(widget->name, "dx_lfo_pitch_depth") == 0) {
        default_value = 0.0f;
    } else if (strcmp(widget->name, "dx_lfo_amp_depth") == 0) {
        default_value = 0.0f;
    } else if (strcmp(widget->name, "dx_global_level") == 0) {
        default_value = 0.5f;
    } else if (strcmp(widget->name, "dx_feedback") == 0) {
        default_value = 0.0f;
    } else {
        return;
    }

    tf_apply_knob_styling(widget, default_value);
}

static void dx_schema_apply_callbacks(TunefishWidget* widget)
{
    if (!widget) return;

    switch (widget->type) {
        case TF_WIDGET_BUTTON: {
            int op = 0;
            if (sscanf(widget->name, "dx_op%d_select", &op) == 1 && op >= 1 && op <= 6) {
                widget->onClick = dx_op_select_on_click;
            } else if (strcmp(widget->name, "dx_page_toggle_btn") == 0 ||
                       strcmp(widget->name, "dx_close_btn") == 0) {
                widget->onClick = dx_button_on_click;
            }
        } break;

        case TF_WIDGET_TOGGLE_BUTTON:
        case TF_WIDGET_ROTARY_SLIDER:
        case TF_WIDGET_LINEAR_SLIDER:
        case TF_WIDGET_PARAMETER_CONTROL:
            widget->onValueChange = dx_knob_on_value_change;
            break;

        case TF_WIDGET_COMBO_BOX:
            widget->onComboSelect = dx_combo_on_select;
            if (strcmp(widget->name, "dx_lfo_waveform") == 0) {
                widget->onValueChange = dx_knob_on_value_change;
            }
            break;

        default:
            break;
    }
}

static const char *const dx_schema_alg_combo_items[] =
{
    "Alg 1", "Alg 2", "Alg 3", "Alg 4", "Alg 5", "Alg 6", "Alg 7", "Alg 8",
    "Alg 9", "Alg 10", "Alg 11", "Alg 12", "Alg 13", "Alg 14", "Alg 15", "Alg 16",
    "Alg 17", "Alg 18", "Alg 19", "Alg 20", "Alg 21", "Alg 22", "Alg 23", "Alg 24",
    "Alg 25", "Alg 26", "Alg 27", "Alg 28", "Alg 29", "Alg 30", "Alg 31", "Alg 32"
};

static const char *const dx_schema_lfo_waveform_items[] =
{
    "Triangle", "Saw Down", "Saw Up", "Square", "Sine", "S&H"
};

static const ft2_ui_bitmap_asset_t *dx_find_bitmap_asset(uint16_t id)
{
    for (uint16_t i = 0; i < ft2_ui_assets.bitmap_count; i++)
    {
        if (ft2_ui_assets.bitmaps[i].id == id)
            return &ft2_ui_assets.bitmaps[i];
    }
    return NULL;
}

static void dx_schema_register_widget(DexedCompleteLayout* layout, TunefishWidget* widget, ft2_ui_widget_page_t page)
{
    if (!layout || !widget) return;

    dx_apply_dexed_styling(widget);
    dx_schema_apply_knob_defaults(widget);
    dx_schema_apply_callbacks(widget);
    dx_register_widget(layout, widget, dx_page_from_schema(page));
}

static void dx_bind_layout_widgets(DexedCompleteLayout* layout)
{
    if (!layout) return;

    layout->title_label = dx_find_widget_by_name(layout, "dx_title_label");
    layout->preset_combo = dx_find_widget_by_name(layout, "dx_preset_combo");
    layout->page_toggle_button = dx_find_widget_by_name(layout, "dx_page_toggle_btn");
    layout->close_button = dx_find_widget_by_name(layout, "dx_close_btn");
    layout->main_level_meter = dx_find_widget_by_name(layout, "dx_out_meter_L");
    layout->cpu_meter = dx_find_widget_by_name(layout, "dx_cpu_meter");
    layout->operator_canvas = dx_find_widget_by_name(layout, "dx_operator_canvas");
    layout->program_name_label = dx_find_widget_by_name(layout, "dx_program_label");
    layout->alg_selector_combo = dx_find_widget_by_name(layout, "dx_alg_combo");
    layout->osc_lfo_knob = dx_find_widget_by_name(layout, "dx_lfo_rate");
    layout->global_level_knob = dx_find_widget_by_name(layout, "dx_global_level");
    layout->env_group = dx_find_widget_by_name(layout, "dx_env_group");
    layout->env_display = dx_find_widget_by_name(layout, "dx_env_display");

    for (int i = 0; i < 6; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dx_op%d_select", i + 1);
        layout->op_select_buttons[i] = dx_find_widget_by_name(layout, name);
    }

    layout->op_widgets.rate1_knob = dx_find_widget_by_name(layout, "dx_op_p1");
    layout->op_widgets.rate2_knob = dx_find_widget_by_name(layout, "dx_op_p2");
    layout->op_widgets.rate3_knob = dx_find_widget_by_name(layout, "dx_op_p3");
    layout->op_widgets.rate4_knob = dx_find_widget_by_name(layout, "dx_op_p4");
    layout->op_widgets.level1_knob = dx_find_widget_by_name(layout, "dx_op_p5");
    layout->op_widgets.level2_knob = dx_find_widget_by_name(layout, "dx_op_p6");
    layout->op_widgets.level3_knob = dx_find_widget_by_name(layout, "dx_op_p7");
    layout->op_widgets.level4_knob = dx_find_widget_by_name(layout, "dx_op_p8");
    layout->op_widgets.kbd_level_scl_bp_knob = dx_find_widget_by_name(layout, "dx_op_p9");
    layout->op_widgets.kbd_level_scl_ld_knob = dx_find_widget_by_name(layout, "dx_op_p10");
    layout->op_widgets.kbd_level_scl_rd_knob = dx_find_widget_by_name(layout, "dx_op_p11");
    layout->op_widgets.kbd_level_scl_lc_knob = dx_find_widget_by_name(layout, "dx_op_p12");
    layout->op_widgets.kbd_level_scl_rc_knob = dx_find_widget_by_name(layout, "dx_op_p13");
    layout->op_widgets.kbd_rate_scl_knob = dx_find_widget_by_name(layout, "dx_op_p14");
    layout->op_widgets.amp_mod_sens_knob = dx_find_widget_by_name(layout, "dx_op_p15");
    layout->op_widgets.key_vel_sens_knob = dx_find_widget_by_name(layout, "dx_op_p16");
    layout->op_widgets.output_level_knob = dx_find_widget_by_name(layout, "dx_op_p17");
    layout->op_widgets.osc_mode_switch = dx_find_widget_by_name(layout, "dx_op_p18");
    layout->op_widgets.osc_freq_coarse_knob = dx_find_widget_by_name(layout, "dx_op_p19");
    layout->op_widgets.osc_freq_fine_knob = dx_find_widget_by_name(layout, "dx_op_p20");
    layout->op_widgets.osc_detune_knob = dx_find_widget_by_name(layout, "dx_op_p21");

    if (layout->op_select_buttons[0]) {
        layout->op_select_buttons[0]->pressed = true;
    }
}

static DexedCompleteLayout* dx_create_complete_layout_from_schema(const ft2_ui_layout_desc_t* desc)
{
    if (!desc || desc->version != FT2_UI_SCHEMA_VERSION) return NULL;

    DexedCompleteLayout* layout = (DexedCompleteLayout*)calloc(1, sizeof(DexedCompleteLayout));
    if (!layout) return NULL;

    layout->current_page = 0;
    layout->initialized = false;
    layout->visible = false;
    layout->active_op = 1;
    layout->sync_interval_ms = 250;
    layout->last_sync_ticks = 0;
    layout->cached_param_count = 0;
    layout->cached_params_valid = false;

    if (desc->bitmaps.count > 0 && desc->bitmap_desc) {
        for (uint16_t i = 0; i < desc->bitmaps.count; i++) {
            const ft2_ui_bitmap_desc_t* d = &desc->bitmap_desc[i];
            const ft2_ui_bitmap_asset_t *asset = dx_find_bitmap_asset(d->bitmap_id);
            if (!asset || !asset->bmp)
                continue;

            int32_t bmp_w = 0, bmp_h = 0;
            TunefishWidget* w = NULL;
            if (asset->fmt == FT2_UI_BMP_FMT_RLE4) {
                uint8_t *pixels = ft2_bmp_decode_rle4_to_pal(asset->bmp, &bmp_w, &bmp_h);
                if (!pixels) continue;
                w = tf_create_bitmap("dx_bitmap", d->x, d->y, d->w, d->h, pixels, bmp_w, bmp_h, true);
            } else if (asset->fmt == FT2_UI_BMP_FMT_RGB) {
                uint32_t *pixels = ft2_bmp_decode_to_rgb32(asset->bmp, &bmp_w, &bmp_h);
                if (!pixels) continue;
                w = tf_create_bitmap32("dx_bitmap", d->x, d->y, d->w, d->h, pixels, bmp_w, bmp_h, true);
            }
            if (!w) continue;
            w->bitmapLayer = d->layer;
            w->bitmapSkinPart = d->skin_part;
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->waveform_views.count > 0 && desc->waveform_view_desc) {
        for (uint16_t i = 0; i < desc->waveform_views.count; i++) {
            const ft2_ui_waveform_view_desc_t* d = &desc->waveform_view_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_waveform", i);
            TunefishWidget* w = tf_create_waveform_view(name, d->x, d->y, d->w, d->h);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_buttons.count > 0 && desc->tf_button_desc) {
        for (uint16_t i = 0; i < desc->tf_buttons.count; i++) {
            const ft2_ui_tf_button_desc_t* d = &desc->tf_button_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_button", i);
            const char *text = d->text ? d->text : "";
            TunefishWidget* w = tf_create_button(name, text, d->x, d->y, d->w, d->h);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_toggles.count > 0 && desc->tf_toggle_desc) {
        for (uint16_t i = 0; i < desc->tf_toggles.count; i++) {
            const ft2_ui_tf_toggle_desc_t* d = &desc->tf_toggle_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_toggle", i);
            const char *text = d->text ? d->text : "";
            TunefishWidget* w = tf_create_toggle_button(name, text, d->x, d->y, d->w, d->h);
            if (w) {
                w->pressed = d->default_pressed;
                w->value = w->pressed ? 1.0f : 0.0f;
            }
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_labels.count > 0 && desc->tf_label_desc) {
        for (uint16_t i = 0; i < desc->tf_labels.count; i++) {
            const ft2_ui_tf_label_desc_t* d = &desc->tf_label_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_label", i);
            const char *text = d->text ? d->text : "";
            TunefishWidget* w = tf_create_label(name, text, d->x, d->y, d->w, d->h);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_rotary_sliders.count > 0 && desc->tf_rotary_slider_desc) {
        for (uint16_t i = 0; i < desc->tf_rotary_sliders.count; i++) {
            const ft2_ui_tf_rotary_slider_desc_t* d = &desc->tf_rotary_slider_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_rotary", i);
            int center_x = d->x + (int)d->radius;
            int center_y = d->y + (int)d->radius;
            TunefishWidget* w = tf_create_rotary_slider(name, center_x, center_y, d->radius, d->start_angle, d->end_angle);
            if (w && d->label) {
                tf_widget_set_label(w, d->label);
            }
            if (w) {
                w->modRingMode = d->mod_ring.mode ? d->mod_ring.mode : FT2_UI_MOD_RING_AUTO_BY_NAME;
                w->modMatrixSlot = d->mod_ring.matrix_slot;
                w->modTargetParam = d->mod_ring.target_param;
                w->modAmountScale = d->mod_ring.amount_scale > 0.0f ? d->mod_ring.amount_scale : 1.0f;
            }
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_linear_sliders.count > 0 && desc->tf_linear_slider_desc) {
        for (uint16_t i = 0; i < desc->tf_linear_sliders.count; i++) {
            const ft2_ui_tf_linear_slider_desc_t* d = &desc->tf_linear_slider_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_linear", i);
            TunefishWidget* w = tf_create_linear_slider(name, d->x, d->y, d->w, d->h, d->vertical);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_combo_boxes.count > 0 && desc->tf_combo_box_desc) {
        for (uint16_t i = 0; i < desc->tf_combo_boxes.count; i++) {
            const ft2_ui_tf_combo_box_desc_t* d = &desc->tf_combo_box_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_combo", i);
            const char *const *items = d->items;
            uint16_t item_count = d->item_count;
            if ((!items || item_count == 0) && name) {
                if (strcmp(name, "dx_alg_combo") == 0) {
                    items = dx_schema_alg_combo_items;
                    item_count = (uint16_t)(sizeof(dx_schema_alg_combo_items) / sizeof(dx_schema_alg_combo_items[0]));
                } else if (strcmp(name, "dx_lfo_waveform") == 0) {
                    items = dx_schema_lfo_waveform_items;
                    item_count = (uint16_t)(sizeof(dx_schema_lfo_waveform_items) / sizeof(dx_schema_lfo_waveform_items[0]));
                }
            }
            TunefishWidget* w = tf_create_combo_box(name, d->x, d->y, d->w, d->h, items, item_count);
            if (w && d->item_count > 0) {
                w->selectedIndex = d->selected_index;
            }
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_level_meters.count > 0 && desc->tf_level_meter_desc) {
        for (uint16_t i = 0; i < desc->tf_level_meters.count; i++) {
            const ft2_ui_tf_level_meter_desc_t* d = &desc->tf_level_meter_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_meter", i);
            int num_leds = d->num_leds > 0 ? d->num_leds : 12;
            TunefishWidget* w = tf_create_level_meter(name, d->x, d->y, d->w, d->h, num_leds, d->show_peak);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_parameter_controls.count > 0 && desc->tf_parameter_control_desc) {
        for (uint16_t i = 0; i < desc->tf_parameter_controls.count; i++) {
            const ft2_ui_tf_parameter_control_desc_t* d = &desc->tf_parameter_control_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_param", i);
            const char *label = d->label ? d->label : "";
            TunefishWidget* w = tf_create_parameter_control(name, label, d->x, d->y, d->w, d->h);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_envelope_displays.count > 0 && desc->tf_envelope_display_desc) {
        for (uint16_t i = 0; i < desc->tf_envelope_displays.count; i++) {
            const ft2_ui_tf_envelope_display_desc_t* d = &desc->tf_envelope_display_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_env", i);
            TunefishWidget* w = tf_create_envelope_display(name, d->x, d->y, d->w, d->h);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    if (desc->tf_group_boxes.count > 0 && desc->tf_group_box_desc) {
        for (uint16_t i = 0; i < desc->tf_group_boxes.count; i++) {
            const ft2_ui_tf_group_box_desc_t* d = &desc->tf_group_box_desc[i];
            char name_buf[64];
            const char *name = dx_schema_name(d->name, name_buf, sizeof(name_buf), "dx_group", i);
            const char *title = d->title ? d->title : "";
            TunefishWidget* w = tf_create_group_box(name, title, d->x, d->y, d->w, d->h);
            dx_schema_register_widget(layout, w, d->page);
        }
    }

    dx_bind_layout_widgets(layout);

    layout->initialized = true;
    return layout;
}
#endif

static void dx_sync_operator_widgets(DexedCompleteLayout* layout, const float* params, int paramCount); // Forward declaration
static void dx_draw_operator_envelope(const DexedCompleteLayout* layout);

static int dx_op_index_from_ui(int op)
{
    if (op < 1 || op > 6) return 0;
    return 6 - op; // UI shows OP1..OP6, patch storage is OP6..OP1
}

static int dx_op_base_from_ui(int op)
{
    return dx_op_index_from_ui(op) * 21;
}

static void dx_op_select_on_click(TunefishWidget* widget) {
    if (!widget || !g_active_dexed_layout) return;

    int op = -1;
    sscanf(widget->name, "dx_op%d_select", &op);

    if (op >= 1 && op <= 6) {
        g_active_dexed_layout->active_op = op;
        for (int i = 0; i < 6; ++i) {
            if (g_active_dexed_layout->op_select_buttons[i])
                g_active_dexed_layout->op_select_buttons[i]->pressed = (i == (op - 1));
        }
        dx_sync_operator_widgets(g_active_dexed_layout, NULL, 0);
    }
}

static void dx_env_pixel(int x, int y, uint8_t pal)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    video.frameBuffer[(y * SCREEN_W) + x] = video.palette[pal];
}

static void dx_widget_set_value_silent(TunefishWidget* widget, float value);

static void dx_env_get_points(const DexedCompleteLayout* layout, int left, int right, int top, int bottom,
                              dx_env_point_t points[4], float seg_raw[4])
{
    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (!layout || instrID <= 0) return;

    float rates[4];
    float levels[4];
    int base = dx_op_base_from_ui(layout->active_op);
    const bool haveCache = (layout->cached_params_valid && layout->cached_param_count >= (base + 8));
    for (int i = 0; i < 4; i++) {
        if (haveCache) {
            rates[i] = layout->cached_params[base + i];
            levels[i] = layout->cached_params[base + 4 + i];
        } else {
            rates[i] = ft2_dx_get_param_for_instrument(instrID, base + i);
            levels[i] = ft2_dx_get_param_for_instrument(instrID, base + 4 + i);
        }
        if (rates[i] < 0.0f) rates[i] = 0.0f;
        if (rates[i] > 1.0f) rates[i] = 1.0f;
        if (levels[i] < 0.0f) levels[i] = 0.0f;
        if (levels[i] > 1.0f) levels[i] = 1.0f;
    }

    float sum = 0.0f;
    for (int i = 0; i < 4; i++) {
        seg_raw[i] = 0.2f + (1.0f - rates[i]) * 0.8f;
        sum += seg_raw[i];
    }
    if (sum <= 0.0f) sum = 1.0f;

    int width = right - left;
    int height = bottom - top;
    float scale = (float)width / sum;

    int x0 = left;
    for (int i = 0; i < 4; i++) {
        int seg = (int)lroundf(seg_raw[i] * scale);
        if (seg < 1) seg = 1;
        int x1 = x0 + seg;
        if (x1 > right) x1 = right;
        int y1 = bottom - (int)lroundf(levels[i] * (float)height);
        if (y1 < top) y1 = top;
        if (y1 > bottom) y1 = bottom;
        points[i].x = x1;
        points[i].y = y1;
        x0 = x1;
        if (x0 >= right) break;
    }
}

static void dx_draw_operator_envelope(const DexedCompleteLayout* layout)
{
    if (!layout || !layout->env_display || !layout->env_display->visible) return;

    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (instrID <= 0) return;

    const TunefishWidget* w = layout->env_display;
    const int pad = 6;
    int left = w->x + pad;
    int right = w->x + w->w - pad - 1;
    int top = w->y + pad;
    int bottom = w->y + w->h - pad - 1;
    int width = right - left;
    int height = bottom - top;
    if (width <= 0 || height <= 0) return;

    fillRect(w->x + 1, w->y + 1, w->w - 2, w->h - 2, PAL_BUTTON2);
    hLine(w->x, w->y, w->w, PAL_BCKGRND);
    hLine(w->x, w->y + w->h - 1, w->w, PAL_BCKGRND);
    vLine(w->x, w->y, w->h, PAL_BCKGRND);
    vLine(w->x + w->w - 1, w->y, w->h, PAL_BCKGRND);

    char label[16];
    snprintf(label, sizeof(label), "Op %d", layout->active_op);
    textOut(w->x + 6, w->y + 3, PAL_FORGRND, label);

    dx_env_point_t points[4];
    float segRaw[4];
    dx_env_get_points(layout, left, right, top, bottom, points, segRaw);

    // Fill lower portion (area under the envelope) with PAL_BLCKMRK.
    int prevX = left;
    int prevY = bottom;
    for (int i = 0; i < 4; i++) {
        int x1 = points[i].x;
        int y1 = points[i].y;
        if (x1 <= prevX) continue;
        for (int x = prevX; x <= x1; x++) {
            float t = (x1 == prevX) ? 0.0f : (float)(x - prevX) / (float)(x1 - prevX);
            int y = prevY + (int)lroundf(t * (float)(y1 - prevY));
            if (y < top) y = top;
            if (y > bottom) y = bottom;
            fillRect(x, y, 1, bottom - y + 1, PAL_BLCKMRK);
        }
        prevX = x1;
        prevY = y1;
    }

    // Dotted margins/grid (match FT2 instrument envelope style)
    for (int i = 0; i <= height / 2; i++) dx_env_pixel(left - 1, top + 1 + i * 2, PAL_PATTEXT);
    for (int i = 0; i <= height / 8; i++) dx_env_pixel(left - 2, top + 1 + i * 8, PAL_PATTEXT);
    for (int i = 0; i <= width / 2; i++) dx_env_pixel(left + 1 + i * 2, bottom + 1, PAL_PATTEXT);
    for (int i = 0; i <= width / 50; i++) dx_env_pixel(left + 1 + i * 50, bottom + 2, PAL_PATTEXT);

    // Draw envelope line on top of the fill
    int x0 = left;
    int y0 = bottom;
    for (int i = 0; i < 4; i++) {
        int x1 = points[i].x;
        int y1 = points[i].y;
        line((int16_t)x0, (int16_t)x1, (int16_t)y0, (int16_t)y1, PAL_FORGRND);
        x0 = x1;
        y0 = y1;
        if (x0 >= right) break;
    }

    // Draw draggable points + selected marker
    for (int i = 0; i < 4; i++) {
        int px = points[i].x;
        int py = points[i].y;
        fillRect(px - 1, py - 1, 3, 3, PAL_BLCKTXT);
        if (i == dx_env_selected_index) {
            line((int16_t)(px - 3), (int16_t)(px - 3), (int16_t)(py - 3), (int16_t)(py + 3), PAL_BLCKTXT);
            line((int16_t)(px + 3), (int16_t)(px + 3), (int16_t)(py - 3), (int16_t)(py + 3), PAL_BLCKTXT);
            dx_env_pixel(left - 1, py, PAL_BLCKTXT);
            dx_env_pixel(px, bottom + 1, PAL_BLCKTXT);
        }
    }
}

static void dx_env_update_from_drag(DexedCompleteLayout* layout, int mouseX, int mouseY)
{
    if (!layout || !layout->env_display) return;
    if (dx_env_drag_index < 0 || dx_env_drag_index > 3) return;

    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (instrID <= 0) return;

    const TunefishWidget* w = layout->env_display;
    const int pad = 6;
    int left = w->x + pad;
    int right = w->x + w->w - pad - 1;
    int top = w->y + pad;
    int bottom = w->y + w->h - pad - 1;
    int width = right - left;
    int height = bottom - top;
    if (width <= 0 || height <= 0) return;

    int idx = dx_env_drag_index;

    dx_env_point_t points[4];
    float segRaw[4];
    dx_env_get_points(layout, left, right, top, bottom, points, segRaw);

    int minX = left;
    int maxX = right;
    if (idx > 0) minX = points[idx - 1].x + 4;
    if (idx < 3) maxX = points[idx + 1].x - 4;
    if (minX < left) minX = left;
    if (maxX > right) maxX = right;
    int clampedX = CLAMP(mouseX, minX, maxX);

    float level = (bottom - mouseY) / (float)height;
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    // Keep neighbor point X positions stable by rebalancing adjacent segment lengths.
    float newRates[4];
    for (int i = 0; i < 4; i++) {
        float r = 1.0f - ((segRaw[i] - 0.2f) / 0.8f);
        if (r < 0.0f) r = 0.0f;
        if (r > 1.0f) r = 1.0f;
        newRates[i] = r;
    }

    if (idx < 3) {
        float sumOther = 0.0f;
        for (int i = 0; i < 4; i++) {
            if (i == idx || i == idx + 1) continue;
            sumOther += segRaw[i];
        }

        float segPx = (float)(clampedX - ((idx == 0) ? left : points[idx - 1].x));
        float segPxNext = (float)(points[idx + 1].x - clampedX);

        float span = segPx + segPxNext;
        if (span < 1.0f) span = 1.0f;
        if (span > (float)(width - 1)) span = (float)(width - 1);

        float denom = 1.0f - (span / (float)width);
        if (denom < 0.05f) denom = 0.05f;
        float S = sumOther / denom;
        float a = (segPx * S) / (float)width;
        float b = (segPxNext * S) / (float)width;

        if (a < 0.2f) a = 0.2f; if (a > 1.0f) a = 1.0f;
        if (b < 0.2f) b = 0.2f; if (b > 1.0f) b = 1.0f;

        float rateA = 1.0f - ((a - 0.2f) / 0.8f);
        float rateB = 1.0f - ((b - 0.2f) / 0.8f);
        if (rateA < 0.0f) rateA = 0.0f; if (rateA > 1.0f) rateA = 1.0f;
        if (rateB < 0.0f) rateB = 0.0f; if (rateB > 1.0f) rateB = 1.0f;

        newRates[idx] = rateA;
        newRates[idx + 1] = rateB;
    } else {
        float rate = 1.0f - ((clampedX - left) / (float)width);
        if (rate < 0.0f) rate = 0.0f;
        if (rate > 1.0f) rate = 1.0f;
        newRates[idx] = rate;
    }

    int base = dx_op_base_from_ui(layout->active_op);
    for (int i = 0; i < 4; i++) {
        dx_set_param_for_current_instrument(base + i, newRates[i]);
    }
    dx_set_param_for_current_instrument(base + 4 + idx, level);

    float rate = newRates[idx];

    TunefishWidget* rateKnobs[4] = {
        layout->op_widgets.rate1_knob,
        layout->op_widgets.rate2_knob,
        layout->op_widgets.rate3_knob,
        layout->op_widgets.rate4_knob
    };
    TunefishWidget* levelKnobs[4] = {
        layout->op_widgets.level1_knob,
        layout->op_widgets.level2_knob,
        layout->op_widgets.level3_knob,
        layout->op_widgets.level4_knob
    };
    if (rateKnobs[idx]) dx_widget_set_value_silent(rateKnobs[idx], rate);
    if (levelKnobs[idx]) dx_widget_set_value_silent(levelKnobs[idx], level);
}

static void dx_widget_set_value_silent(TunefishWidget* widget, float value)
{
    if (!widget) return;
    if (value < widget->minValue) value = widget->minValue;
    if (value > widget->maxValue) value = widget->maxValue;
    widget->value = value;
}

static void dx_sync_operator_widgets(DexedCompleteLayout* layout, const float* params, int paramCount) {
    if (!layout) return;

    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (instrID <= 0) return;

    TunefishWidget* op_widgets[] = {
        layout->op_widgets.rate1_knob, layout->op_widgets.rate2_knob, layout->op_widgets.rate3_knob, layout->op_widgets.rate4_knob,
        layout->op_widgets.level1_knob, layout->op_widgets.level2_knob, layout->op_widgets.level3_knob, layout->op_widgets.level4_knob,
        layout->op_widgets.kbd_level_scl_bp_knob, layout->op_widgets.kbd_level_scl_ld_knob, layout->op_widgets.kbd_level_scl_rd_knob,
        layout->op_widgets.kbd_level_scl_lc_knob, layout->op_widgets.kbd_level_scl_rc_knob, layout->op_widgets.kbd_rate_scl_knob,
        layout->op_widgets.amp_mod_sens_knob, layout->op_widgets.key_vel_sens_knob, layout->op_widgets.output_level_knob,
        layout->op_widgets.osc_mode_switch, layout->op_widgets.osc_freq_coarse_knob, layout->op_widgets.osc_freq_fine_knob,
        layout->op_widgets.osc_detune_knob
    };

    for (int i = 0; i < 21; i++) {
        int paramId = dx_op_base_from_ui(layout->active_op) + i;
        float val = (params && paramId < paramCount) ? params[paramId] : ft2_dx_get_param_for_instrument(instrID, paramId);
        if (op_widgets[i]) dx_widget_set_value_silent(op_widgets[i], val);
    }
}

/* Helper: derive a small paramId from widget name.
 * This is intentionally lightweight: it maps a handful of known widget name
 * patterns to numeric param identifiers which are passed to the instrument
 * parameter setter. The mapping is wrapper-defined and can be expanded later.
 */
static int dx_param_id_from_widget_name(const char* name)
{
    if (!name) return -1;

    if (strncmp(name, "dx_op_p", 7) == 0) {
        int p = 0;
        if (sscanf(name, "dx_op_p%d", &p) == 1) {
            if (p >= 1 && p <= 21) {
                return p - 1; // Return 0-20 (active op will be applied by caller)
            }
        }
    }

    if (strncmp(name, "dx_op", 5) == 0) {
        int op = 0, p = 0;
        if (sscanf(name, "dx_op%d_p%d", &op, &p) == 2) {
            if (op >= 1 && op <= 6 && p >= 1 && p <= 21) {
                return dx_op_base_from_ui(op) + (p - 1);
            }
        }
    }

    /* LFO parameters (bytes 137-140) */
    if (strcmp(name, "dx_lfo_rate") == 0) return 137;
    if (strcmp(name, "dx_lfo_delay") == 0) return 138;
    if (strcmp(name, "dx_lfo_pitch_depth") == 0) return 139;
    if (strcmp(name, "dx_lfo_amp_depth") == 0) return 140;

    /* System parameters (bytes 146-154) */
    if (strcmp(name, "dx_global_level") == 0) return 154; /* master output level */

    /* Algorithm selection (byte 134, 0-31) */
    if (strcmp(name, "dx_alg_combo") == 0) return 134;
    if (strcmp(name, "dx_feedback") == 0) return 135;

    /* LFO waveform/sync (bytes 141-142) */
    if (strcmp(name, "dx_lfo_sync") == 0) return 141;
    if (strcmp(name, "dx_lfo_waveform") == 0) return 142;

    /* Filter parameters */
    if (strcmp(name, "dx_filter_cutoff") == 0) return 1002;
    if (strcmp(name, "dx_filter_reso") == 0) return 1003;
    if (strcmp(name, "dx_filter_gain") == 0) return 1004;

    /* Portamento + mono */
    if (strcmp(name, "dx_porta_time") == 0) return 1005;
    if (strcmp(name, "dx_mono") == 0) return 1006;

    /* Fallback: unknown widget name */
    return -1;
}

/* Callback invoked when a rotary/linear widget value changes.
 * Forwards a normalized value (0.0 - 1.0) to the active instrument via the
 * convenience helper dx_set_param_for_current_instrument().
 */
static void dx_knob_on_value_change(TunefishWidget* widget, float newValue)
{
    if (!widget || !g_active_dexed_layout) return;
    int baseParamId = dx_param_id_from_widget_name(widget->name);
    if (baseParamId < 0) return;

    int finalParamId;
    if (baseParamId < 21) {
        finalParamId = dx_op_base_from_ui(g_active_dexed_layout->active_op) + baseParamId;
    } else {
        finalParamId = baseParamId;
    }

    dx_set_param_for_current_instrument(finalParamId, newValue);
}

/* Callback invoked when a ComboBox selection changes.
 * Normalizes the selected index to [0..1] and forwards it as a parameter.
 */
/* Populate the preset combo box with available factory presets */
static void dx_populate_preset_combo(DexedCompleteLayout* layout)
{
    if (!layout || !layout->preset_combo) return;

    int count = ft2_dx_get_factory_preset_count();
    if (count <= 0) {
        tf_widget_clear_combo_items(layout->preset_combo);
        tf_widget_add_combo_item(layout->preset_combo, "No presets available");
        layout->preset_combo->selectedIndex = 0;
        layout->preset_combo->enabled = false;
        return;
    }

    layout->preset_combo->enabled = true;

    /* Clear existing combo items */
    tf_widget_clear_combo_items(layout->preset_combo);

    /* Add factory presets to combo */
    for (int i = 0; i < count; i++) {
        const char* presetName = ft2_dx_get_factory_preset_name(i);
        if (presetName) {
            tf_widget_add_combo_item(layout->preset_combo, presetName);
        }
    }

    /* Update combo to show current preset */
    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (instrID > 0) {
        int currentPreset = ft2_dx_get_current_preset_for_instrument(instrID);
        if (currentPreset >= 0 && currentPreset < layout->preset_combo->comboItemCount) {
            layout->preset_combo->selectedIndex = currentPreset;
        } else {
            layout->preset_combo->selectedIndex = 0;
        }
    } else {
        layout->preset_combo->selectedIndex = 0;
    }
}

/* Simple button click handler for page toggle / close actions that maps to
 * page and visibility changes where appropriate.
 */
static void dx_button_on_click(TunefishWidget* widget)
{
    if (!widget) return;

    /* Page toggle */
    if (strcmp(widget->name, "dx_page_toggle_btn") == 0) {
        if (g_active_dexed_layout) {
            dx_switch_to_page(g_active_dexed_layout, (g_active_dexed_layout->current_page + 1) % DX_PAGE_COUNT);
        }
    } else if (strcmp(widget->name, "dx_close_btn") == 0) {
        g_dx_close_pending = true;
    }
}

/* Internal helper: add widget to all_widgets and appropriate page/global arrays */
static void dx_register_widget(DexedCompleteLayout* l, TunefishWidget* w, int page)
{
    if (!l || !w) return;

    w->page = page;

    /* add to all_widgets first available slot */
    for (int i = 0; i < DX_TOTAL_WIDGETS; i++) {
        if (l->all_widgets[i] == NULL) { l->all_widgets[i] = w; break; }
    }

    if (page == DX_PAGE_BOTH) {
        for (int i = 0; i < DX_GLOBAL_WIDGETS; i++) {
            if (l->global_widgets[i] == NULL) { l->global_widgets[i] = w; break; }
        }
    } else if (page == DX_PAGE_MAIN) {
        for (int i = 0; i < DX_PAGE1_WIDGETS; i++) {
            if (l->page1_widgets[i] == NULL) { l->page1_widgets[i] = w; break; }
        }
    } else if (page == DX_PAGE_EFFECTS) {
        for (int i = 0; i < DX_PAGE2_WIDGETS; i++) {
            if (l->page2_widgets[i] == NULL) { l->page2_widgets[i] = w; break; }
        }
    }
}

/* Styling: apply Dexed-like colors/sizing to a widget */
void dx_apply_dexed_styling(TunefishWidget* widget)
{
    if (!widget) return;
    widget->bgColor = DX_BG_COLOUR;
    widget->textColor = DX_TEXT_COLOR;
    widget->accentColor = DX_ACCENT_COLOUR;

    /* Apply knob styling for rotary sliders */
    if (widget->type == TF_WIDGET_ROTARY_SLIDER) {
        /* Use same rotary angles as Tunefish knobs */
        widget->rotaryStartAngle = -2.35f;
        widget->rotaryEndAngle =  2.35f;
    }
}

/* Find widget by name */
TunefishWidget* dx_find_widget_by_name(DexedCompleteLayout* l, const char* name)
{
    if (!l || !name) return NULL;
    for (int i = 0; i < DX_TOTAL_WIDGETS; i++) {
        TunefishWidget* w = l->all_widgets[i];
        if (w && strcmp(w->name, name) == 0) return w;
    }
    return NULL;
}

/* Sync helpers (populate UI widgets from current instrument parameters) */
void dx_sync_widgets_with_parameters(DexedCompleteLayout* layout)
{
    /* Populate widgets by querying the Dexed wrapper for the currently
     * selected instrument's parameter values. We use the same paramId
     * mapping used for sending changes (dx_param_id_from_widget_name).
     *
     * This function is intended to be called from the UI thread when the
     * editor opens so it's safe to update widget fields directly.
     */

    if (!layout) {
        /* Sync current active layout if none provided */
        layout = g_active_dexed_layout;
        if (!layout) return;
    }

    /* Sync preset combo with current instrument state */
    if (layout->preset_combo) {
        /* Only repopulate if combo has no items (first time) */
        if (layout->preset_combo->comboItemCount == 0) {
            dx_populate_preset_combo(layout);
        } else {
            extern struct editor_t editor;
            const int currentPreset = ft2_dx_get_current_preset_for_instrument(editor.curInstr);
            if (currentPreset >= 0 && currentPreset < layout->preset_combo->comboItemCount)
                layout->preset_combo->selectedIndex = currentPreset;
        }
    }

    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (instrID <= 0) {
        /* No instrument selected -> nothing to sync */
        return;
    }

    float params[156];
    int paramCount = ft2_dx_get_params_for_instrument(instrID, params, 156);
    const bool haveParams = (paramCount > 0);
    if (haveParams) {
        layout->cached_param_count = paramCount;
        memcpy(layout->cached_params, params, (size_t)paramCount * sizeof(float));
        layout->cached_params_valid = true;
    } else {
        layout->cached_params_valid = false;
        layout->cached_param_count = 0;
    }

    /* Iterate all created widgets and attempt to fetch a parameter for each
     * widget that has a known param mapping. If the underlying Dexed wrapper
     * does not support a queried param, ft2_dx_get_param_for_instrument() is
     * expected to return a sensible default (0.0f).
     */
    for (int i = 0; i < DX_TOTAL_WIDGETS; i++) {
        TunefishWidget* w = layout->all_widgets[i];
        if (!w) continue;

        if (strcmp(w->name, "dx_preset_combo") == 0) {
            if (w->comboItemCount != ft2_dx_get_factory_preset_count()) {
                dx_populate_preset_combo(layout);
            }
            int currentPreset = ft2_dx_get_current_preset_for_instrument(instrID);
            if (currentPreset >= 0 && currentPreset < w->comboItemCount) {
                w->selectedIndex = currentPreset;
            }
            continue;
        }

        /* derive param id from name using the same mapping helper */
        int paramId = dx_param_id_from_widget_name(w->name);
        if (paramId < 0) continue;

        /* Query wrapper for normalized parameter (0.0 .. 1.0) */
        float val = (haveParams && paramId < paramCount) ? params[paramId] : ft2_dx_get_param_for_instrument(instrID, paramId);

        /* Clamp to safe bounds */
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;

        /* Update widget according to type */
        switch (w->type) {
            case TF_WIDGET_ROTARY_SLIDER:
            case TF_WIDGET_LINEAR_SLIDER:
            case TF_WIDGET_LEVEL_METER:
            case TF_WIDGET_PARAMETER_CONTROL:
                dx_widget_set_value_silent(w, val);
                break;

            case TF_WIDGET_COMBO_BOX:
                if (w->comboItemCount > 1) {
                    int idx = (int)lroundf(val * (float)(w->comboItemCount - 1));
                    if (idx < 0) idx = 0;
                    if (idx >= w->comboItemCount) idx = w->comboItemCount - 1;
                    w->selectedIndex = idx;
                }
                break;

            case TF_WIDGET_TOGGLE_BUTTON:
            case TF_WIDGET_BUTTON:
                /* Treat boolean-ish knobs: on if val > 0.5 */
                if (val > 0.5f) w->pressed = true; else w->pressed = false;
                break;

            case TF_WIDGET_LABEL:
            case TF_WIDGET_GROUP_BOX:
            case TF_WIDGET_WAVEFORM_VIEW:
            case TF_WIDGET_ENVELOPE_DISPLAY:
            default:
                /* No direct numeric mapping for these widget types */
                break;
        }
    }

    DX_UI_TRACE("[DX_SYNC] Completed parameter synchronization for instrument %d\n", instrID);

    /* Finally request a redraw of the UI to reflect updated widget visuals */
    ui.updatePatternEditor = true;
    ui.updatePosSections = true;

    dx_sync_operator_widgets(layout, params, paramCount);
}

void dx_update_widget_from_parameter(DexedCompleteLayout* layout, const char* widgetName, int value)
{
    TunefishWidget* w = dx_find_widget_by_name(layout, widgetName);
    if (!w) return;
    /* Map int value (0..127) to normalized float [0..1] */
    float norm = 0.0f;
    if (value <= 0) norm = 0.0f;
    else if (value >= 127) norm = 1.0f;
    else norm = (float)value / 127.0f;
    tf_widget_set_value(w, norm);
}

void dx_update_all_widgets_from_synth(DexedCompleteLayout* layout)
{
    dx_sync_widgets_with_parameters(layout);
}

/* Create the Dexed complete layout (basic widgets only) */
DexedCompleteLayout* dx_create_complete_layout(void)
{
#ifdef DX_USE_SCHEMA_LAYOUT
    DexedCompleteLayout* schema_layout = dx_create_complete_layout_from_schema(&dx_complete_layout_layout);
    if (schema_layout) return schema_layout;
#endif

    DexedCompleteLayout* l = (DexedCompleteLayout*)calloc(1, sizeof(DexedCompleteLayout));
    if (!l) return NULL;

    l->current_page = 0;
    l->initialized = false;
    l->visible = false;
    l->active_op = 1;
    l->sync_interval_ms = 250;
    l->last_sync_ticks = 0;
    l->cached_param_count = 0;
    l->cached_params_valid = false;

    /* Create global widgets using same layout as Tunefish editor */
    {
        const int gsp = 8;
        const int btnW = 60, btnH = 22;
        const int dropW = 130, dropH = 22;
        int gy = 8;

        // Title and top row widgets
        const int titleW = 180, titleH = 22;
        int titleX = (DX_LAYOUT_WIDTH - titleW) - 440;
        const int meterW = 40, meterH = 18;
        const int pageBtnW = 48, closeBtnW = 48;

        l->title_label = tf_create_label("dx_title_label", "Dexed FM Editor", titleX, gy, titleW, titleH);
        dx_apply_dexed_styling(l->title_label);

        l->preset_combo = tf_create_combo_box("dx_preset_combo", titleX + 260 + gsp, gy, dropW, dropH, NULL, 0);
        dx_apply_dexed_styling(l->preset_combo);
        if (l->preset_combo) l->preset_combo->onComboSelect = dx_combo_on_select;

        l->page_toggle_button = tf_create_button("dx_page_toggle_btn", "Page", DX_LAYOUT_WIDTH - pageBtnW - closeBtnW - gsp*2, gy, pageBtnW, btnH);
        dx_apply_dexed_styling(l->page_toggle_button);
        if (l->page_toggle_button) l->page_toggle_button->onClick = dx_button_on_click;

        l->close_button = tf_create_button("dx_close_btn", "Close", DX_LAYOUT_WIDTH - closeBtnW - gsp, gy, closeBtnW, btnH);
        dx_apply_dexed_styling(l->close_button);
        if (l->close_button) l->close_button->onClick = dx_button_on_click;

        int meterBaseX = DX_LAYOUT_WIDTH - meterW*2 - gsp*4 - 100;
        l->main_level_meter = tf_create_level_meter("dx_out_meter_L", meterBaseX, gy + 2, meterW, meterH, 12, true);
        dx_apply_dexed_styling(l->main_level_meter);

        l->cpu_meter = tf_create_level_meter("dx_cpu_meter", meterBaseX + meterW + gsp, gy + 2, meterW, meterH, 12, true);
        dx_apply_dexed_styling(l->cpu_meter);

        /* Register all global widgets */
        TunefishWidget* globals[] = {
            l->title_label, l->preset_combo, l->page_toggle_button,
            l->close_button, l->main_level_meter, l->cpu_meter
        };

        for (int i = 0; i < DX_GLOBAL_WIDGETS; i++) {
            dx_register_widget(l, globals[i], DX_PAGE_BOTH);
        }
    }

    /* Page 1: Main operator canvas */
    l->operator_canvas = tf_create_group_box("dx_operator_canvas", "Operators", 20, 40, DX_LAYOUT_WIDTH - 40, 300);
    dx_apply_dexed_styling(l->operator_canvas);
    dx_register_widget(l, l->operator_canvas, DX_PAGE_MAIN);

    l->program_name_label = tf_create_label("dx_program_label", "Program: (none)", 28, 46, 240, 12);
    dx_apply_dexed_styling(l->program_name_label);
    dx_register_widget(l, l->program_name_label, DX_PAGE_MAIN);

    l->alg_selector_combo = tf_create_combo_box("dx_alg_combo", 280, 46, 140, 16,
                                                (const char* const[]){"Alg 1","Alg 2","Alg 3","Alg 4"}, 4);
    dx_apply_dexed_styling(l->alg_selector_combo);
    if (l->alg_selector_combo) l->alg_selector_combo->onComboSelect = dx_combo_on_select;
    dx_register_widget(l, l->alg_selector_combo, DX_PAGE_MAIN);

    {
        const int opBtnX = 36;
        const int opBtnY = 70;
        const int opBtnW = 40;
        const int opBtnH = 20;
        const int opBtnGap = 8;
        for (int op = 0; op < 6; op++) {
            char name[64];
            snprintf(name, sizeof(name), "dx_op%d_select", op + 1);
            l->op_select_buttons[op] = tf_create_button(name, name, opBtnX + op * (opBtnW + opBtnGap), opBtnY, opBtnW, opBtnH);
            dx_apply_dexed_styling(l->op_select_buttons[op]);
            l->op_select_buttons[op]->onClick = dx_op_select_on_click;
            dx_register_widget(l, l->op_select_buttons[op], DX_PAGE_MAIN);
        }
    }
    l->op_select_buttons[0]->pressed = true;

    l->env_group = tf_create_group_box("dx_env_group", "Op Envelope", 32, 228, 300, 96);
    dx_apply_dexed_styling(l->env_group);
    dx_register_widget(l, l->env_group, DX_PAGE_MAIN);

    l->env_display = tf_create_waveform_view("dx_env_display", 40, 244, 284, 72);
    dx_apply_dexed_styling(l->env_display);
    dx_register_widget(l, l->env_display, DX_PAGE_MAIN);

    // Operator panel grid
    const int knobR = 18;
    const int knobD = knobR * 2;
    const int knobGapX = 10;
    const int knobGapY = 12;
    const int groupPadX = 10;
    const int groupPadY = 12;
    const int groupTitleH = 12;
    const int groupGap = 12;
    const int groupY = 104;

    // EG Group (4x2)
    const int egCols = 4;
    const int egW = (groupPadX * 2) + knobD + ((egCols - 1) * (knobD + knobGapX));
    const int egH = groupTitleH + (groupPadY * 2) + (knobD * 2) + knobGapY;
    const int egX = 32;
    TunefishWidget* eg_group = tf_create_group_box("dx_op_eg_group", "EG", egX, groupY, egW, egH);
    dx_apply_dexed_styling(eg_group);
    dx_register_widget(l, eg_group, DX_PAGE_MAIN);

    int currentX = egX + groupPadX + knobR;
    int currentY = groupY + groupTitleH + groupPadY + knobR;

    l->op_widgets.rate1_knob = tf_create_rotary_slider("dx_op_p1", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.rate1_knob, "Rate 1");
    currentX += knobD + knobGapX;
    l->op_widgets.rate2_knob = tf_create_rotary_slider("dx_op_p2", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.rate2_knob, "Rate 2");
    currentX += knobD + knobGapX;
    l->op_widgets.rate3_knob = tf_create_rotary_slider("dx_op_p3", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.rate3_knob, "Rate 3");
    currentX += knobD + knobGapX;
    l->op_widgets.rate4_knob = tf_create_rotary_slider("dx_op_p4", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.rate4_knob, "Rate 4");

    currentX = egX + groupPadX + knobR;
    currentY += knobD + knobGapY;

    l->op_widgets.level1_knob = tf_create_rotary_slider("dx_op_p5", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.level1_knob, "Level 1");
    currentX += knobD + knobGapX;
    l->op_widgets.level2_knob = tf_create_rotary_slider("dx_op_p6", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.level2_knob, "Level 2");
    currentX += knobD + knobGapX;
    l->op_widgets.level3_knob = tf_create_rotary_slider("dx_op_p7", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.level3_knob, "Level 3");
    currentX += knobD + knobGapX;
    l->op_widgets.level4_knob = tf_create_rotary_slider("dx_op_p8", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.level4_knob, "Level 4");

    // KBD SCL Group
    const int kbdCols = 5;
    const int kbdGapX = 8;
    const int kbdW = (groupPadX * 2) + knobD + ((kbdCols - 1) * (knobD + kbdGapX));
    const int kbdH = egH;
    const int kbdX = egX + egW + groupGap;
    TunefishWidget* kbd_group = tf_create_group_box("dx_op_kbd_group", "KBD SCL", kbdX, groupY, kbdW, kbdH);
    dx_apply_dexed_styling(kbd_group);
    dx_register_widget(l, kbd_group, DX_PAGE_MAIN);

    currentX = kbdX + groupPadX + knobR;
    currentY = groupY + groupTitleH + groupPadY + knobR;

    l->op_widgets.kbd_level_scl_bp_knob = tf_create_rotary_slider("dx_op_p9", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.kbd_level_scl_bp_knob, "Scl BP");
    currentX += knobD + kbdGapX;
    l->op_widgets.kbd_level_scl_ld_knob = tf_create_rotary_slider("dx_op_p10", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.kbd_level_scl_ld_knob, "Scl LD");
    currentX += knobD + kbdGapX;
    l->op_widgets.kbd_level_scl_rd_knob = tf_create_rotary_slider("dx_op_p11", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.kbd_level_scl_rd_knob, "Scl RD");
    currentX += knobD + kbdGapX;
    l->op_widgets.kbd_level_scl_lc_knob = tf_create_rotary_slider("dx_op_p12", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.kbd_level_scl_lc_knob, "Scl LC");
    currentX += knobD + kbdGapX;
    l->op_widgets.kbd_level_scl_rc_knob = tf_create_rotary_slider("dx_op_p13", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.kbd_level_scl_rc_knob, "Scl RC");

    currentX = kbdX + groupPadX + knobR;
    currentY += knobD + knobGapY;

    l->op_widgets.kbd_rate_scl_knob = tf_create_rotary_slider("dx_op_p14", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.kbd_rate_scl_knob, "Rate Scl");

    // OSC Group
    const int oscCols = 3;
    const int oscW = (groupPadX * 2) + knobD + ((oscCols - 1) * (knobD + knobGapX));
    const int oscH = egH;
    const int oscX = kbdX + kbdW + groupGap;
    TunefishWidget* osc_group = tf_create_group_box("dx_op_osc_group", "OSC", oscX, groupY, oscW, oscH);
    dx_apply_dexed_styling(osc_group);
    dx_register_widget(l, osc_group, DX_PAGE_MAIN);

    currentX = oscX + groupPadX + knobR;
    currentY = groupY + groupTitleH + groupPadY + knobR;

    l->op_widgets.amp_mod_sens_knob = tf_create_rotary_slider("dx_op_p15", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.amp_mod_sens_knob, "AMS");
    currentX += knobD + knobGapX;
    l->op_widgets.key_vel_sens_knob = tf_create_rotary_slider("dx_op_p16", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.key_vel_sens_knob, "KVS");
    currentX += knobD + knobGapX;
    l->op_widgets.output_level_knob = tf_create_rotary_slider("dx_op_p17", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.output_level_knob, "Out Lvl");

    currentX = oscX + groupPadX + knobR;
    currentY += knobD + knobGapY;

    l->op_widgets.osc_mode_switch = tf_create_toggle_button("dx_op_p18", "Mode", currentX - knobR, currentY - 10, 40, 20);
    currentX += knobD + knobGapX;
    l->op_widgets.osc_freq_coarse_knob = tf_create_rotary_slider("dx_op_p19", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.osc_freq_coarse_knob, "Crs Freq");
    currentX += knobD + knobGapX;
    l->op_widgets.osc_freq_fine_knob = tf_create_rotary_slider("dx_op_p20", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.osc_freq_fine_knob, "Fin Freq");
    currentX += knobD + knobGapX;
    l->op_widgets.osc_detune_knob = tf_create_rotary_slider("dx_op_p21", currentX, currentY, knobR, -2.35f, 2.35f);
    tf_widget_set_label(l->op_widgets.osc_detune_knob, "Detune");

    // Register all operator widgets
    TunefishWidget* op_widgets_to_register[] = {
        l->op_widgets.rate1_knob, l->op_widgets.rate2_knob, l->op_widgets.rate3_knob, l->op_widgets.rate4_knob,
        l->op_widgets.level1_knob, l->op_widgets.level2_knob, l->op_widgets.level3_knob, l->op_widgets.level4_knob,
        l->op_widgets.kbd_level_scl_bp_knob, l->op_widgets.kbd_level_scl_ld_knob, l->op_widgets.kbd_level_scl_rd_knob,
        l->op_widgets.kbd_level_scl_lc_knob, l->op_widgets.kbd_level_scl_rc_knob, l->op_widgets.kbd_rate_scl_knob,
        l->op_widgets.amp_mod_sens_knob, l->op_widgets.key_vel_sens_knob, l->op_widgets.output_level_knob,
        l->op_widgets.osc_mode_switch, l->op_widgets.osc_freq_coarse_knob, l->op_widgets.osc_freq_fine_knob,
        l->op_widgets.osc_detune_knob
    };

    for (int i = 0; i < sizeof(op_widgets_to_register) / sizeof(op_widgets_to_register[0]); i++) {
        dx_apply_dexed_styling(op_widgets_to_register[i]);
        if (op_widgets_to_register[i]) {
            op_widgets_to_register[i]->onValueChange = dx_knob_on_value_change;
        }
        dx_register_widget(l, op_widgets_to_register[i], DX_PAGE_MAIN);
    }

    /* Page 2: Global + LFO + Filter */
    const int fxStartY = 50;
    const int fxGroupH = 112;

    TunefishWidget* filter_group = tf_create_group_box("dx_filter_group", "Filter", 20, fxStartY, 220, fxGroupH);
    dx_apply_dexed_styling(filter_group);
    dx_register_widget(l, filter_group, DX_PAGE_EFFECTS);

    const int fxKnobR = 18;
    TunefishWidget* cutoff_knob = tf_create_rotary_slider("dx_filter_cutoff", 50, fxStartY + 52, fxKnobR, -2.35f, 2.35f);
    tf_widget_set_label(cutoff_knob, "Cutoff");
    dx_apply_dexed_styling(cutoff_knob);
    tf_apply_knob_styling(cutoff_knob, 1.0f);
    cutoff_knob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, cutoff_knob, DX_PAGE_EFFECTS);

    TunefishWidget* reso_knob = tf_create_rotary_slider("dx_filter_reso", 120, fxStartY + 52, fxKnobR, -2.35f, 2.35f);
    tf_widget_set_label(reso_knob, "Reso");
    dx_apply_dexed_styling(reso_knob);
    tf_apply_knob_styling(reso_knob, 0.0f);
    reso_knob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, reso_knob, DX_PAGE_EFFECTS);

    TunefishWidget* gain_knob = tf_create_rotary_slider("dx_filter_gain", 190, fxStartY + 52, fxKnobR, -2.35f, 2.35f);
    tf_widget_set_label(gain_knob, "Gain");
    dx_apply_dexed_styling(gain_knob);
    tf_apply_knob_styling(gain_knob, 1.0f);
    gain_knob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, gain_knob, DX_PAGE_EFFECTS);

    TunefishWidget* lfo_group = tf_create_group_box("dx_lfo_group", "LFO", 260, fxStartY, 340, fxGroupH);
    dx_apply_dexed_styling(lfo_group);
    dx_register_widget(l, lfo_group, DX_PAGE_EFFECTS);

    l->osc_lfo_knob = tf_create_rotary_slider("dx_lfo_rate", 290, fxStartY + 52, fxKnobR, -2.35f, 2.35f);
    dx_apply_dexed_styling(l->osc_lfo_knob);
    tf_apply_knob_styling(l->osc_lfo_knob, 0.2f);
    if (l->osc_lfo_knob) l->osc_lfo_knob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, l->osc_lfo_knob, DX_PAGE_EFFECTS);

    TunefishWidget* lfoDelayKnob = tf_create_rotary_slider("dx_lfo_delay", 350, fxStartY + 52, fxKnobR, -2.35f, 2.35f);
    dx_apply_dexed_styling(lfoDelayKnob);
    tf_apply_knob_styling(lfoDelayKnob, 0.0f);
    if (lfoDelayKnob) lfoDelayKnob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, lfoDelayKnob, DX_PAGE_EFFECTS);

    TunefishWidget* lfoPitchDepthKnob = tf_create_rotary_slider("dx_lfo_pitch_depth", 410, fxStartY + 52, fxKnobR, -2.35f, 2.35f);
    dx_apply_dexed_styling(lfoPitchDepthKnob);
    tf_apply_knob_styling(lfoPitchDepthKnob, 0.0f);
    if (lfoPitchDepthKnob) lfoPitchDepthKnob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, lfoPitchDepthKnob, DX_PAGE_EFFECTS);

    TunefishWidget* lfoAmpDepthKnob = tf_create_rotary_slider("dx_lfo_amp_depth", 470, fxStartY + 52, fxKnobR, -2.35f, 2.35f);
    dx_apply_dexed_styling(lfoAmpDepthKnob);
    tf_apply_knob_styling(lfoAmpDepthKnob, 0.0f);
    if (lfoAmpDepthKnob) lfoAmpDepthKnob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, lfoAmpDepthKnob, DX_PAGE_EFFECTS);

    const char* lfoWaveforms[] = {"Triangle", "Saw Down", "Saw Up", "Square", "Sine", "S&H"};
    TunefishWidget* lfoWaveformCombo = tf_create_combo_box("dx_lfo_waveform", 510, fxStartY + 40, 80, 18, lfoWaveforms, 6);
    dx_apply_dexed_styling(lfoWaveformCombo);
    if (lfoWaveformCombo) lfoWaveformCombo->onComboSelect = dx_combo_on_select;
    dx_register_widget(l, lfoWaveformCombo, DX_PAGE_EFFECTS);

    TunefishWidget* lfoSyncButton = tf_create_toggle_button("dx_lfo_sync", "Sync", 510, fxStartY + 64, 60, 18);
    dx_apply_dexed_styling(lfoSyncButton);
    if (lfoSyncButton) lfoSyncButton->onClick = dx_button_on_click;
    dx_register_widget(l, lfoSyncButton, DX_PAGE_EFFECTS);

    TunefishWidget* global_group = tf_create_group_box("dx_global_group", "Global", 20, fxStartY + fxGroupH + 16, 220, fxGroupH);
    dx_apply_dexed_styling(global_group);
    dx_register_widget(l, global_group, DX_PAGE_EFFECTS);

    l->global_level_knob = tf_create_rotary_slider("dx_global_level", 50, fxStartY + fxGroupH + 68, fxKnobR, -2.35f, 2.35f);
    dx_apply_dexed_styling(l->global_level_knob);
    tf_apply_knob_styling(l->global_level_knob, 0.5f);
    if (l->global_level_knob) l->global_level_knob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, l->global_level_knob, DX_PAGE_EFFECTS);

    TunefishWidget* feedback_knob = tf_create_rotary_slider("dx_feedback", 120, fxStartY + fxGroupH + 68, fxKnobR, -2.35f, 2.35f);
    tf_widget_set_label(feedback_knob, "Feedback");
    dx_apply_dexed_styling(feedback_knob);
    tf_apply_knob_styling(feedback_knob, 0.0f);
    feedback_knob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, feedback_knob, DX_PAGE_EFFECTS);

    TunefishWidget* porta_knob = tf_create_rotary_slider("dx_porta_time", 190, fxStartY + fxGroupH + 68, fxKnobR, -2.35f, 2.35f);
    tf_widget_set_label(porta_knob, "Glide");
    dx_apply_dexed_styling(porta_knob);
    tf_apply_knob_styling(porta_knob, 0.0f);
    porta_knob->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, porta_knob, DX_PAGE_EFFECTS);

    TunefishWidget* mono_toggle = tf_create_toggle_button("dx_mono", "Mono", 190, fxStartY + fxGroupH + 92, 50, 18);
    dx_apply_dexed_styling(mono_toggle);
    mono_toggle->onValueChange = dx_knob_on_value_change;
    dx_register_widget(l, mono_toggle, DX_PAGE_EFFECTS);

    l->initialized = true;
    return l;
}

/* Destroy layout and all widgets */
void dx_destroy_complete_layout(DexedCompleteLayout* layout)
{
    if (!layout) return;

    if (g_active_dexed_layout == layout)
        g_active_dexed_layout = NULL;

    for (int i = 0; i < DX_TOTAL_WIDGETS; i++) {
        TunefishWidget* w = layout->all_widgets[i];
        if (w == NULL) continue;
        for (int previous = 0; previous < i; ++previous) {
            if (layout->all_widgets[previous] == w) {
                w = NULL;
                break;
            }
        }
        if (w != NULL) tf_widget_destroy(w);
    }

    free(layout);
}

/* Switch page helper */
static void dx_update_widget_visibility(DexedCompleteLayout* layout)
{
    if (!layout) return;

    for (int i = 0; i < DX_TOTAL_WIDGETS; i++) {
        TunefishWidget* w = layout->all_widgets[i];
        if (w) {
            if (w->page == DX_PAGE_BOTH) {
                w->visible = true;
            } else {
                w->visible = (w->page == layout->current_page);
            }
        }
    }
}

void dx_switch_to_page(DexedCompleteLayout* layout, int page)
{
    if (!layout) return;
    if (page < 0 || page >= DX_PAGE_COUNT) return;
    layout->current_page = page;
    /* update page toggle button label */
    if (layout->page_toggle_button) {
        snprintf(layout->page_toggle_button->text, sizeof(layout->page_toggle_button->text), "Page %d", page + 1);
    }
    dx_update_widget_visibility(layout);
    ui.updatePatternEditor = true;
}

/* Show/hide layout */
void dx_show_layout(DexedCompleteLayout* layout)
{
    if (!layout) return;
    g_active_dexed_layout = layout;
    g_dx_close_pending = false;
    if (layout->close_button) layout->close_button->pressed = false;
    dx_update_all_widgets_from_synth(layout);
    layout->last_sync_ticks = SDL_GetTicks();
    layout->visible = true;
    dx_switch_to_page(layout, 0);
    dx_update_widget_visibility(layout);
}

void dx_hide_layout(DexedCompleteLayout* layout)
{
    if (!layout) return;
    if (g_active_dexed_layout == layout) g_active_dexed_layout = NULL;
    g_dx_close_pending = false;
    if (layout->close_button) layout->close_button->pressed = false;
    layout->visible = false;
}

/* Render layout: draw background + widgets for active page */
void dx_render_complete_layout(DexedCompleteLayout* layout)
{
    if (!layout || !layout->visible) return;

    // Keep UI controls in sync with live synth state while visible (throttled)
    if (layout->sync_interval_ms == 0) {
        layout->sync_interval_ms = 250;
    }

    uint32_t nowTicks = SDL_GetTicks();
    if (layout->last_sync_ticks == 0 || (uint32_t)(nowTicks - layout->last_sync_ticks) >= layout->sync_interval_ms) {
        dx_update_all_widgets_from_synth(layout);
        layout->last_sync_ticks = nowTicks;
    }

    /* Clear background */
    fillRect(0, 0, SCREEN_W, SCREEN_H, PAL_DESKTOP);

    /* Draw global bitmap widgets first */
    for (int i = 0; i < DX_GLOBAL_WIDGETS; i++) {
        TunefishWidget* w = layout->global_widgets[i];
        if (w && w->visible && w->type == TF_WIDGET_BITMAP) tf_draw_widget(w);
    }

    /* Draw remaining global widgets */
    for (int i = 0; i < DX_GLOBAL_WIDGETS; i++) {
        TunefishWidget* w = layout->global_widgets[i];
        if (w && w->visible && w->type != TF_WIDGET_BITMAP) tf_draw_widget(w);
    }

    /* Draw page widgets */
    TunefishWidget** pageWidgets = (layout->current_page == DX_PAGE_MAIN) ? layout->page1_widgets : layout->page2_widgets;
    int pageCount = (layout->current_page == DX_PAGE_MAIN) ? DX_PAGE1_WIDGETS : DX_PAGE2_WIDGETS;

    /* First draw bitmap widgets (background) */
    for (int i = 0; i < pageCount; i++) {
        TunefishWidget* w = pageWidgets[i];
        if (w && w->visible && w->type == TF_WIDGET_BITMAP) tf_draw_widget(w);
    }

    /* Then draw group boxes (frames) */
    for (int i = 0; i < pageCount; i++) {
        TunefishWidget* w = pageWidgets[i];
        if (w && w->visible && w->type == TF_WIDGET_GROUP_BOX) tf_draw_widget(w);
    }

    /* Then draw non-group widgets */
    for (int i = 0; i < pageCount; i++) {
        TunefishWidget* w = pageWidgets[i];
        if (w && w->visible && w->type != TF_WIDGET_GROUP_BOX && w->type != TF_WIDGET_BITMAP && w != layout->env_display) {
            tf_draw_widget(w);
        }
    }

    if (layout->current_page == DX_PAGE_MAIN) {
        dx_draw_operator_envelope(layout);
    }
}

/* Mouse event handling: route to global widgets then to page widgets */
bool dx_handle_layout_mouse_event(DexedCompleteLayout* layout, int mouseX, int mouseY, bool pressed)
{
    if (!layout || !layout->visible) return false;

    if (!pressed && g_dx_close_pending) {
        g_dx_close_pending = false;
        dx_hide_layout(layout);
        ft2_close_synth_editor();
        return true;
    }

    if (layout->current_page == DX_PAGE_MAIN && layout->env_display && layout->env_display->visible) {
        const TunefishWidget* w = layout->env_display;
        const int pad = 6;
        int left = w->x + pad;
        int right = w->x + w->w - pad - 1;
        int top = w->y + pad;
        int bottom = w->y + w->h - pad - 1;

        const bool inEnv = (mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom);

        if (pressed && !dx_env_dragging && inEnv) {
            dx_env_point_t points[4];
            float segRaw[4];
            dx_env_get_points(layout, left, right, top, bottom, points, segRaw);
            dx_env_last_mouse_x = mouseX;
            dx_env_last_mouse_y = mouseY;

            bool hit = false;
            for (int i = 0; i < 4; i++) {
                int dx = mouseX - points[i].x;
                int dy = mouseY - points[i].y;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx <= 2 && dy <= 2) {
                    dx_env_dragging = true;
                    dx_env_drag_index = i;
                    dx_env_selected_index = i;
                    dx_env_save_mouse_x = left + (dx_env_last_mouse_x - points[i].x);
                    dx_env_save_mouse_y = top + (dx_env_last_mouse_y - points[i].y);
                    hit = true;
                    break;
                }
            }
            if (!hit && dx_env_selected_index >= 0 && dx_env_selected_index < 4) {
                dx_env_dragging = true;
                dx_env_drag_index = dx_env_selected_index;
                dx_env_save_mouse_x = left + (dx_env_last_mouse_x - points[dx_env_drag_index].x);
                dx_env_save_mouse_y = top + (dx_env_last_mouse_y - points[dx_env_drag_index].y);
            }
            return true;
        }

        if (dx_env_dragging) {
            if (pressed) {
                dx_env_update_from_drag(layout, mouseX, mouseY);
                return true;
            }
            dx_env_dragging = false;
            dx_env_drag_index = -1;
        }
    }

    /* Check global widgets first */
    for (int i = 0; i < DX_GLOBAL_WIDGETS; i++) {
        TunefishWidget* w = layout->global_widgets[i];
        if (w && w->visible) {
            if (tf_widget_handle_mouse_event(w, mouseX, mouseY, pressed)) {
                /* Special actions for some globals */
                if (w == layout->close_button && pressed) {
                    dx_hide_layout(layout);
                    ft2_close_synth_editor();
                    return true;
                } else if (w == layout->preset_combo && pressed) {
                    /* combo box will handle selection internally */
                    return true;
                }
                return true;
            }
        }
    }

    /* Route to page widgets */
    TunefishWidget** pageArray = (layout->current_page == DX_PAGE_MAIN) ? layout->page1_widgets : layout->page2_widgets;
    int pageCount = (layout->current_page == DX_PAGE_MAIN) ? DX_PAGE1_WIDGETS : DX_PAGE2_WIDGETS;
    for (int i = 0; i < pageCount; i++) {
        TunefishWidget* w = pageArray[i];
        if (!w || !w->visible) {
            continue;
        }
        if (tf_widget_handle_mouse_event(w, mouseX, mouseY, pressed)) {
            return true;
        }
    }

    return true;
}

bool dx_handle_layout_mouse_drag(DexedCompleteLayout* layout, int mouseX, int mouseY)
{
    if (!layout || !layout->visible) return false;
    if (layout->current_page != DX_PAGE_MAIN) return false;
    if (!layout->env_display || !layout->env_display->visible) return false;

    if (dx_env_dragging) {
        dx_env_update_from_drag(layout, mouseX, mouseY);
        return true;
    }

    return false;
}

/* Keyboard test handler (simple) */
bool dx_handle_layout_keyboard_test(DexedCompleteLayout* layout, int key)
{
    if (!layout || !layout->visible) return false;

    switch (key) {
        case SDLK_TAB:
            dx_switch_to_page(layout, (layout->current_page + 1) % DX_PAGE_COUNT);
            return true;
        case SDLK_F5:
            /* quick test: bump global level */
            if (layout->global_level_knob) {
                float v = layout->global_level_knob->value;
                v += 0.1f; if (v > 1.0f) v = 0.0f;
                tf_widget_set_value(layout->global_level_knob, v);
            }
            return true;
        case SDLK_ESCAPE:
            /* Close the Dexed overlay on Escape for a quick exit */
            dx_hide_layout(layout);
            return true;
        default:
            return false;
    }
}

/* Convenience wrapper used by widget callbacks to set a Dexed parameter on the currently selected instrument */
void dx_set_param_for_current_instrument(int paramId, float normalizedValue)
{
    extern struct editor_t editor;
    int instrID = editor.curInstr;
    if (instrID <= 0) return;
    /* Map normalized [0..1] to 0..127 and call ft2_dx_set_param_for_instrument (wrapper) */
    ft2_dx_set_param_for_instrument(instrID, paramId, normalizedValue);
}

/* Combo box selection handler */
static void dx_combo_on_select(TunefishWidget* widget, int selectedIndex)
{
    if (!widget) return;

    /* Special-case: preset combo should trigger loading a full factory
     * preset into the current instrument (rather than mapping to a single byte).
     * We use the helper functions implemented in ft2_dexed.c (forward-declared above).
     */
    if (strcmp(widget->name, "dx_preset_combo") == 0) {
        int count = ft2_dx_get_factory_preset_count();
        if (count <= 0) {
            /* Nothing to load; keep UI consistent but do nothing */
            return;
        }
        /* Clamp selected index into available range and request load for current instrument */
        int idx = selectedIndex;
        if (idx < 0) idx = 0;
        if (idx >= count) idx = count - 1;
        /* Request loading the indexed factory preset into the currently selected instrument */
        if (!ft2_dx_load_factory_preset_for_current_instrument(idx)) {
            DX_UI_TRACE("[DX_CUI] Failed to load factory preset %d from UI combo\n", idx);
        } else {
            DX_UI_TRACE("[DX_CUI] Requested factory preset %d load from UI combo\n", idx);
            /* Update combo to reflect new selection */
            widget->selectedIndex = idx;
            if (g_active_dexed_layout && g_active_dexed_layout->program_name_label) {
                const char* presetName = ft2_dx_get_factory_preset_name(idx);
                if (presetName) {
                    tf_widget_set_label(g_active_dexed_layout->program_name_label, presetName);
                }
            }
            dx_sync_widgets_with_parameters(g_active_dexed_layout);
        }
        return;
    }

    /* Default behavior: map combo selection into a normalized parameter and forward */
    int paramId = dx_param_id_from_widget_name(widget->name);
    if (paramId < 0) return;
    float norm = 0.0f;
    if (widget->comboItemCount > 1) {
        norm = (float)selectedIndex / (float)(widget->comboItemCount - 1);
    }
    dx_set_param_for_current_instrument(paramId, norm);
}
