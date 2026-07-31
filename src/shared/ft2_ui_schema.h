#pragma once

#include <stdint.h>
#include <stdbool.h>

// Schema versioning for layout exporters/importers.
#define FT2_UI_SCHEMA_VERSION 8

// Common invalid ID sentinel.
#define FT2_UI_INVALID_ID (-1)

typedef uint16_t ft2_ui_action_id_t;

typedef struct
{
    uint16_t base;
    uint16_t count;
} ft2_ui_id_range_t;

typedef enum
{
    FT2_UI_WIDGET_NONE = 0,
    FT2_UI_WIDGET_PUSHBUTTON,
    FT2_UI_WIDGET_RADIOBUTTON,
    FT2_UI_WIDGET_CHECKBOX,
    FT2_UI_WIDGET_SCROLLBAR,
    FT2_UI_WIDGET_TEXTBOX,
    FT2_UI_WIDGET_FRAMEBOX,
    FT2_UI_WIDGET_POPUP_LIST,
    FT2_UI_WIDGET_BITMAP,
    FT2_UI_WIDGET_WAVEFORM_VIEW,
    FT2_UI_WIDGET_TF_BUTTON,
    FT2_UI_WIDGET_TF_TOGGLE_BUTTON,
    FT2_UI_WIDGET_TF_LABEL,
    FT2_UI_WIDGET_TF_ROTARY_SLIDER,
    FT2_UI_WIDGET_TF_LINEAR_SLIDER,
    FT2_UI_WIDGET_TF_COMBO_BOX,
    FT2_UI_WIDGET_TF_LEVEL_METER,
    FT2_UI_WIDGET_TF_PARAMETER_CONTROL,
    FT2_UI_WIDGET_TF_ENVELOPE_DISPLAY,
    FT2_UI_WIDGET_TF_GROUP_BOX,
    FT2_UI_WIDGET_TF_ARP_STEP,
    FT2_UI_WIDGET_MIXER_STRIP,
    FT2_UI_WIDGET_MIXER_GAIN,
    FT2_UI_WIDGET_MIXER_PAN,
    FT2_UI_WIDGET_MIXER_MUTE,
    FT2_UI_WIDGET_MIXER_SCOPE,
    FT2_UI_WIDGET_MIXER_MASTER,
    FT2_UI_WIDGET_DSP_WINDOW,
    FT2_UI_WIDGET_DSP_SLOT,
    FT2_UI_WIDGET_DSP_MENU,
    FT2_UI_WIDGET_DSP_PARAM
} ft2_ui_widget_kind_t;

typedef struct
{
    ft2_ui_widget_kind_t kind;
    const char *name;
    uint16_t default_w;
    uint16_t default_h;
    bool fixed_size;
} ft2_ui_widget_type_desc_t;

extern const ft2_ui_widget_type_desc_t ft2_ui_widget_types[];
extern const uint16_t ft2_ui_widget_type_count;

// Fixed widget sizes (classic FT2).
#define FT2_UI_CHECKBOX_W 13
#define FT2_UI_CHECKBOX_H 12
#define FT2_UI_RADIOBUTTON_W 11
#define FT2_UI_RADIOBUTTON_H 11

typedef enum
{
    FT2_UI_WIDGET_PAGE_BOTH = 0,
    FT2_UI_WIDGET_PAGE_1 = 1,
    FT2_UI_WIDGET_PAGE_2 = 2,
    FT2_UI_WIDGET_PAGE_3 = 3,
    FT2_UI_WIDGET_PAGE_4 = 4,
    FT2_UI_WIDGET_PAGE_5 = 5,
    FT2_UI_WIDGET_PAGE_6 = 6
} ft2_ui_widget_page_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint8_t pre_delay;
    uint8_t delay_frames;
    const char *caption;
    const char *caption2;
    ft2_ui_action_id_t action_down;
    ft2_ui_action_id_t action_up;
} ft2_ui_pushbutton_desc_t;

typedef struct
{
    uint16_t id;
    uint16_t x, y;
    uint16_t click_w, click_h;
    ft2_ui_action_id_t action;
    bool default_checked;
} ft2_ui_checkbox_desc_t;

typedef struct
{
    uint16_t id;
    uint16_t x, y;
    uint16_t group_id;
    ft2_ui_action_id_t action;
    bool default_checked;
} ft2_ui_radiobutton_desc_t;

typedef enum
{
    FT2_UI_SCROLLBAR_HORIZONTAL = 0,
    FT2_UI_SCROLLBAR_VERTICAL = 1
} ft2_ui_scrollbar_type_t;

typedef enum
{
    FT2_UI_SCROLLBAR_FIXED_THUMB_SIZE = 0,
    FT2_UI_SCROLLBAR_DYNAMIC_THUMB_SIZE = 1
} ft2_ui_scrollbar_thumb_t;

typedef struct
{
    uint16_t id;
    uint16_t x, y, w, h;
    ft2_ui_scrollbar_type_t type;
    ft2_ui_scrollbar_thumb_t thumb_type;
    uint32_t pos;
    uint32_t page;
    uint32_t end;
    ft2_ui_action_id_t action;
    bool has_nudge_buttons;
} ft2_ui_scrollbar_desc_t;

typedef struct
{
    uint16_t id;
    uint16_t x, y, w;
    uint8_t h, tx, ty;
    uint16_t max_chars;
    bool right_mouse_button;
    bool change_mouse_cursor;
    uint16_t text_binding_id;
} ft2_ui_textbox_desc_t;

typedef enum
{
    FT2_UI_BITMAP_LAYER_WIDGET = 0,
    FT2_UI_BITMAP_LAYER_BACKGROUND = 1,
    FT2_UI_BITMAP_LAYER_SKIN = 2
} ft2_ui_bitmap_layer_t;

typedef enum
{
    FT2_UI_BITMAP_FLAG_NONE = 0,
    FT2_UI_BITMAP_FLAG_CLICK_THROUGH = 1u << 0,
    FT2_UI_BITMAP_FLAG_TRUECOLOR = 1u << 1
} ft2_ui_bitmap_flags_t;

typedef enum
{
    FT2_UI_SKIN_PART_NONE = 0,
    FT2_UI_SKIN_PART_BACKGROUND,
    FT2_UI_SKIN_PART_PANEL,
    FT2_UI_SKIN_PART_BUTTON,
    FT2_UI_SKIN_PART_KNOB,
    FT2_UI_SKIN_PART_SLIDER,
    FT2_UI_SKIN_PART_METER
} ft2_ui_skin_part_t;

typedef struct
{
    uint16_t id;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    uint16_t bitmap_id;
    uint8_t layer;
    uint8_t flags;
    uint8_t skin_part;
    uint8_t opacity;
} ft2_ui_bitmap_desc_t;

typedef struct
{
    uint16_t id;
    uint16_t x, y, w, h;
    uint8_t border_type;
    bool filled;
    const char *title;
} ft2_ui_framebox_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
} ft2_ui_waveform_view_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    const char *text;
} ft2_ui_tf_button_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    const char *text;
    bool default_pressed;
} ft2_ui_tf_toggle_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    const char *text;
} ft2_ui_tf_label_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y;
    uint16_t radius;
    ft2_ui_widget_page_t page;
    float start_angle;
    float end_angle;
    const char *label;
} ft2_ui_tf_rotary_slider_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    bool vertical;
} ft2_ui_tf_linear_slider_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    const char * const *items;
    uint16_t item_count;
    uint16_t selected_index;
} ft2_ui_tf_combo_box_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    uint8_t num_leds;
    bool show_peak;
} ft2_ui_tf_level_meter_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    const char *label;
} ft2_ui_tf_parameter_control_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
} ft2_ui_tf_envelope_display_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    ft2_ui_widget_page_t page;
    const char *title;
} ft2_ui_tf_group_box_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint16_t channel_index;
} ft2_ui_mixer_strip_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint16_t channel_index;
} ft2_ui_mixer_gain_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint16_t channel_index;
} ft2_ui_mixer_pan_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint16_t channel_index;
} ft2_ui_mixer_mute_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint16_t channel_index;
} ft2_ui_mixer_scope_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
} ft2_ui_mixer_master_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
} ft2_ui_dsp_window_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint16_t slot_index;
} ft2_ui_dsp_slot_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
} ft2_ui_dsp_menu_desc_t;

typedef struct
{
    uint16_t id;
    const char *name;
    uint16_t x, y, w, h;
    uint16_t param_index;
} ft2_ui_dsp_param_desc_t;

typedef struct
{
    const char *name;
    uint32_t version;

    ft2_ui_id_range_t pushbuttons;
    const ft2_ui_pushbutton_desc_t *pushbutton_desc;

    ft2_ui_id_range_t checkboxes;
    const ft2_ui_checkbox_desc_t *checkbox_desc;

    ft2_ui_id_range_t radiobuttons;
    const ft2_ui_radiobutton_desc_t *radiobutton_desc;

    ft2_ui_id_range_t scrollbars;
    const ft2_ui_scrollbar_desc_t *scrollbar_desc;

    ft2_ui_id_range_t textboxes;
    const ft2_ui_textbox_desc_t *textbox_desc;

    ft2_ui_id_range_t frameboxes;
    const ft2_ui_framebox_desc_t *framebox_desc;

    ft2_ui_id_range_t bitmaps;
    const ft2_ui_bitmap_desc_t *bitmap_desc;

    ft2_ui_id_range_t waveform_views;
    const ft2_ui_waveform_view_desc_t *waveform_view_desc;

    ft2_ui_id_range_t tf_buttons;
    const ft2_ui_tf_button_desc_t *tf_button_desc;

    ft2_ui_id_range_t tf_toggles;
    const ft2_ui_tf_toggle_desc_t *tf_toggle_desc;

    ft2_ui_id_range_t tf_labels;
    const ft2_ui_tf_label_desc_t *tf_label_desc;

    ft2_ui_id_range_t tf_rotary_sliders;
    const ft2_ui_tf_rotary_slider_desc_t *tf_rotary_slider_desc;

    ft2_ui_id_range_t tf_linear_sliders;
    const ft2_ui_tf_linear_slider_desc_t *tf_linear_slider_desc;

    ft2_ui_id_range_t tf_combo_boxes;
    const ft2_ui_tf_combo_box_desc_t *tf_combo_box_desc;

    ft2_ui_id_range_t tf_level_meters;
    const ft2_ui_tf_level_meter_desc_t *tf_level_meter_desc;

    ft2_ui_id_range_t tf_parameter_controls;
    const ft2_ui_tf_parameter_control_desc_t *tf_parameter_control_desc;

    ft2_ui_id_range_t tf_envelope_displays;
    const ft2_ui_tf_envelope_display_desc_t *tf_envelope_display_desc;

    ft2_ui_id_range_t tf_group_boxes;
    const ft2_ui_tf_group_box_desc_t *tf_group_box_desc;

    ft2_ui_id_range_t mixer_strips;
    const ft2_ui_mixer_strip_desc_t *mixer_strip_desc;

    ft2_ui_id_range_t mixer_gains;
    const ft2_ui_mixer_gain_desc_t *mixer_gain_desc;

    ft2_ui_id_range_t mixer_pans;
    const ft2_ui_mixer_pan_desc_t *mixer_pan_desc;

    ft2_ui_id_range_t mixer_mutes;
    const ft2_ui_mixer_mute_desc_t *mixer_mute_desc;

    ft2_ui_id_range_t mixer_scopes;
    const ft2_ui_mixer_scope_desc_t *mixer_scope_desc;

    ft2_ui_id_range_t mixer_masters;
    const ft2_ui_mixer_master_desc_t *mixer_master_desc;

    ft2_ui_id_range_t dsp_windows;
    const ft2_ui_dsp_window_desc_t *dsp_window_desc;

    ft2_ui_id_range_t dsp_slots;
    const ft2_ui_dsp_slot_desc_t *dsp_slot_desc;

    ft2_ui_id_range_t dsp_menus;
    const ft2_ui_dsp_menu_desc_t *dsp_menu_desc;

    ft2_ui_id_range_t dsp_params;
    const ft2_ui_dsp_param_desc_t *dsp_param_desc;
} ft2_ui_layout_desc_t;
