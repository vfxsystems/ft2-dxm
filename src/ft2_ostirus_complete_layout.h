#ifndef FT2_OSTIRUS_COMPLETE_LAYOUT_H
#define FT2_OSTIRUS_COMPLETE_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "ft2_tunefish_widgets.h"
#include "ft2_tunefish_complete_layout.h"
#include "ft2_ostirus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSTI_LAYOUT_WIDTH   TF_LAYOUT_WIDTH
#define OSTI_LAYOUT_HEIGHT  TF_LAYOUT_HEIGHT

#define OSTI_PAGE_COUNT 7
#define OSTI_ARP_STEP_COUNT 16
#define OSTI_MAX_BROWSER_BANKS 32
#define OSTI_MAX_BROWSER_PROGRAMS 256
#define OSTI_MAX_BROWSER_PRESETS 4096
#define OSTI_BROWSER_COLUMN_COUNT 4

#define OSTI_MAX_PARAM_BINDINGS 192
#define OSTI_MAX_WIDGETS        256

typedef enum
{
    OSTI_PAGE_COMMON = 0,
    OSTI_PAGE_FILTER = 1,
    OSTI_PAGE_MOD_MATRIX = 2,
    OSTI_PAGE_FX = 3,
    OSTI_PAGE_LFO = 4,
    OSTI_PAGE_ARP = 5,
    OSTI_PAGE_BROWSER = 6
} OsTirusPage;

typedef enum
{
    OSTI_BROWSER_COLUMN_ROM = 0,
    OSTI_BROWSER_COLUMN_BANK = 1,
    OSTI_BROWSER_COLUMN_CATEGORY = 2,
    OSTI_BROWSER_COLUMN_PATCH = 3
} OsTirusBrowserColumn;

typedef struct
{
    TunefishWidget* widget;
    int paramId;
    bool isCombo;
    bool isToggle;
} OsTirusParamBinding;

typedef struct OsTirusCompleteLayout
{
    TunefishWidget* title_label;
    TunefishWidget* page_caption_label;
    TunefishWidget* preset_combo;
    TunefishWidget* preset_name_label;
    TunefishWidget* slot_label;
    TunefishWidget* voice_meter;
    TunefishWidget* close_button;
    TunefishWidget* page_common_button;
    TunefishWidget* page_filter_button;
    TunefishWidget* page_mod_matrix_button;
    TunefishWidget* page_fx_button;
    TunefishWidget* page_lfo_button;
    TunefishWidget* page_arp_button;
    TunefishWidget* page_browser_button;
    TunefishWidget* browser_bank_combo;
    TunefishWidget* browser_program_combo;
    TunefishWidget* browser_status_label;
    TunefishWidget* browser_selected_label;
    TunefishWidget* browser_current_label;
    TunefishWidget* browser_load_button;
    TunefishWidget* browser_prev_button;
    TunefishWidget* browser_next_button;
    TunefishWidget* browser_default_button;
    TunefishWidget* browser_clear_button;
    TunefishWidget* arp_step_widgets[OSTI_ARP_STEP_COUNT];
    int arp_drag_step_index;
    int browser_bank_values[OSTI_MAX_BROWSER_BANKS];
    int browser_bank_count;
    int browser_category_values[FT2_OSTIRUS_MAX_CATEGORIES];
    int browser_category_count;
    int browser_filtered_preset_indices[OSTI_MAX_BROWSER_PRESETS];
    int browser_filtered_preset_count;
    int browser_selected_bank;
    int browser_selected_preset_index;
    bool browser_category_selected[FT2_OSTIRUS_MAX_CATEGORIES];
    char browser_search_text[48];
    bool browser_search_active;
    int browser_scroll_offsets[OSTI_BROWSER_COLUMN_COUNT];
    bool browser_scroll_dragging;
    int browser_scroll_drag_column;
    int browser_scroll_drag_offset;

    OsTirusParamBinding param_bindings[OSTI_MAX_PARAM_BINDINGS];
    int param_binding_count;

    TunefishWidget* all_widgets[OSTI_MAX_WIDGETS];
    int all_widget_count;

    int current_page;
    int active_instrument_id;
    bool initialized;
    bool visible;
    uint32_t last_sync_ticks;
    uint32_t sync_interval_ms;
} OsTirusCompleteLayout;

extern OsTirusCompleteLayout* g_active_ostirus_layout;

OsTirusCompleteLayout* osti_create_complete_layout(void);
void osti_destroy_complete_layout(OsTirusCompleteLayout* layout);
void osti_show_layout(OsTirusCompleteLayout* layout);
void osti_hide_layout(OsTirusCompleteLayout* layout);
void osti_switch_to_page(OsTirusCompleteLayout* layout, int page);

void osti_render_complete_layout(OsTirusCompleteLayout* layout);
bool osti_handle_layout_mouse_event(OsTirusCompleteLayout* layout, int mouseX, int mouseY, bool pressed);
bool osti_handle_layout_mouse_drag(OsTirusCompleteLayout* layout, int mouseX, int mouseY);
bool osti_handle_layout_keyboard_test(OsTirusCompleteLayout* layout, int key);

void osti_sync_widgets_with_parameters(OsTirusCompleteLayout* layout);
void osti_update_all_widgets_from_synth(OsTirusCompleteLayout* layout);
TunefishWidget* osti_find_widget_by_name(OsTirusCompleteLayout* layout, const char* name);
void osti_style_all_widgets_authentic(OsTirusCompleteLayout* layout);

#ifdef __cplusplus
}
#endif

#endif /* FT2_OSTIRUS_COMPLETE_LAYOUT_H */
