/*
 * dx_complete_layout.h -- C-style Dexed editor layout (FT2 tunefish-style widgets)
 *
 * This header declares a lightweight C layout that mirrors the Tunefish complete layout
 * but styled and organized for the embedded Dexed engine. The implementation creates
 * a two-page editor (Main / Effects) using the existing Tunefish widget primitives so
 * the UI is drawn by FT2's SDL-based renderer (no JUCE dependency).
 *
 * The design:
 *  - Uses the Tunefish widget system (knobs, toggles, combo boxes, group boxes, meters).
 *  - Applies Dexed-inspired colors and sizes via dx_apply_dexed_styling().
 *  - Exposes create/show/hide/render and event handlers so the FT2 GUI can present the
 *    Dexed editor overlay and forward mouse/keyboard events to it.
 *
 * Notes:
 *  - Parameter bindings are wrapper-defined and call into ft2_dexed wrapper functions.
 *  - This header only contains declarations. Implementation lives in dx_complete_layout.c.
 *
 * Author: generated for ft2-dxm integration
 */

#ifndef FT2_DEXED_COMPLETE_LAYOUT_H
#define FT2_DEXED_COMPLETE_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "../ft2_tunefish_widgets.h"  /* Tunefish widget primitives (Knobs, Buttons, etc.) */
#include "../ft2_tunefish_complete_layout.h" /* useful layout constants and helpers */
#include "../ft2_dexed.h"            /* Dexed engine wrapper API (ft2_dx_* helpers) */

#ifdef __cplusplus
extern "C" {
#endif

/* Layout sizing */
#define DX_LAYOUT_WIDTH   TF_LAYOUT_WIDTH    /* keep consistent with TF layout (632) */
#define DX_LAYOUT_HEIGHT  TF_LAYOUT_HEIGHT   /* 400 */

/* Page count */
#define DX_PAGE_COUNT     2

/* Widget array capacities */
#define DX_GLOBAL_WIDGETS 6    /* title, preset, page toggle, exit, meters, etc. */
#define DX_PAGE1_WIDGETS  96   /* operator canvas + operator param rows + global controls */
#define DX_PAGE2_WIDGETS  140  /* FX grid, mod matrix, FX stack, formant controls */
#define DX_TOTAL_WIDGETS   (DX_GLOBAL_WIDGETS + DX_PAGE1_WIDGETS + DX_PAGE2_WIDGETS)


/* Page identifiers */
typedef enum { DX_PAGE_MAIN = 0, DX_PAGE_EFFECTS = 1, DX_PAGE_BOTH = 2 } DexedPage;

/* Forward declare layout struct */
typedef struct DexedCompleteLayout DexedCompleteLayout;

typedef struct {
    TunefishWidget* rate1_knob;
    TunefishWidget* rate2_knob;
    TunefishWidget* rate3_knob;
    TunefishWidget* rate4_knob;
    TunefishWidget* level1_knob;
    TunefishWidget* level2_knob;
    TunefishWidget* level3_knob;
    TunefishWidget* level4_knob;
    TunefishWidget* kbd_level_scl_bp_knob;
    TunefishWidget* kbd_level_scl_ld_knob;
    TunefishWidget* kbd_level_scl_rd_knob;
    TunefishWidget* kbd_level_scl_lc_knob;
    TunefishWidget* kbd_level_scl_rc_knob;
    TunefishWidget* kbd_rate_scl_knob;
    TunefishWidget* amp_mod_sens_knob;
    TunefishWidget* key_vel_sens_knob;
    TunefishWidget* output_level_knob;
    TunefishWidget* osc_mode_switch;
    TunefishWidget* osc_freq_coarse_knob;
    TunefishWidget* osc_freq_fine_knob;
    TunefishWidget* osc_detune_knob;
} DexedOperatorWidgets;

/* Layout structure
 *
 * Only a subset of Tunefish-style fields are required here. Implementation
 * fills these widget pointers and also keeps per-page widget arrays for easy
 * iteration (mirrors the Tunefish layout approach).
 */
struct DexedCompleteLayout {
    /* Global persistent widgets */
    TunefishWidget* title_label;
    TunefishWidget* preset_combo;       /* factory/program/preset selector */
    TunefishWidget* page_toggle_button;  /* switches between main/effects pages */
    TunefishWidget* close_button;        /* close editor */
    TunefishWidget* main_level_meter;
    TunefishWidget* cpu_meter;

    /* Operator / Main page widgets (page 0) */
    TunefishWidget* operator_canvas;     /* big area where operator graphics would be drawn */
    TunefishWidget* program_name_label;
    TunefishWidget* alg_selector_combo;
    TunefishWidget* osc_lfo_knob;
    TunefishWidget* global_level_knob;
    TunefishWidget* env_group;
    TunefishWidget* env_display;

    /* Operator selection and parameter widgets */
    TunefishWidget* op_select_buttons[6];
    DexedOperatorWidgets op_widgets;


    /* Effects / Modulation page widgets (page 1) */
    TunefishWidget* fx_groups[7];
    TunefishWidget* fx_wet_knobs[7];

    /* Layout state */
    int current_page;
    int active_op;
    bool initialized;
    bool visible;

    uint32_t sync_interval_ms;
    uint32_t last_sync_ticks;

    float cached_params[156];
    int cached_param_count;
    bool cached_params_valid;

    /* Arrays for management (mirrors TunefishCompleteLayout) */
    TunefishWidget* all_widgets[DX_TOTAL_WIDGETS];
    TunefishWidget* global_widgets[DX_GLOBAL_WIDGETS];
    TunefishWidget* page1_widgets[DX_PAGE1_WIDGETS];
    TunefishWidget* page2_widgets[DX_PAGE2_WIDGETS];
};

/* Public API */

/* Create/destroy layout
 * - Allocates and configures widgets, but does not make the layout visible.
 */
DexedCompleteLayout* dx_create_complete_layout(void);
void dx_destroy_complete_layout(DexedCompleteLayout* layout);

/* Show/hide layout overlay */
void dx_show_layout(DexedCompleteLayout* layout);
void dx_hide_layout(DexedCompleteLayout* layout);

/* Switch page (0 .. DX_PAGE_COUNT-1) */
void dx_switch_to_page(DexedCompleteLayout* layout, int page);

/* Render and event handling */
void dx_render_complete_layout(DexedCompleteLayout* layout);
bool dx_handle_layout_mouse_event(DexedCompleteLayout* layout, int mouseX, int mouseY, bool pressed);
bool dx_handle_layout_mouse_drag(DexedCompleteLayout* layout, int mouseX, int mouseY);

extern DexedCompleteLayout* g_dexed_layout_singleton;
bool dx_handle_layout_keyboard_test(DexedCompleteLayout* layout, int key);

/* Synchronization helpers
 * - Sync widget visuals from Dexed engine state (reads via ft2_dexed wrapper)
 */
void dx_sync_widgets_with_parameters(DexedCompleteLayout* layout);
void dx_update_widget_from_parameter(DexedCompleteLayout* layout, const char* widgetName, int value);
void dx_update_all_widgets_from_synth(DexedCompleteLayout* layout);

/* Find widget by name */
TunefishWidget* dx_find_widget_by_name(DexedCompleteLayout* layout, const char* name);

/* Styling helper: apply Dexed-inspired colors/sizing to a widget.
 * Implementation will use color constants informed by DXLookNFeel and map them
 * to the Tunefish widget styling fields.
 */
void dx_apply_dexed_styling(TunefishWidget* widget);

/* Convenience helper to set a Dexed parameter for the active/selected instrument.
 * This wraps ft2_dx_set_param_for_instrument() and resolves the current FT2
 * instrument index from the editor state.
 *
 * paramId is wrapper-defined (the implementation will document the small set
 * of supported paramIds used by the C-based editor).
 */
void dx_set_param_for_current_instrument(int paramId, float normalizedValue);

/* Expose a pointer to the active layout (useful for event callbacks) */
extern DexedCompleteLayout* g_active_dexed_layout;

#ifdef __cplusplus
}
#endif

#endif /* FT2_DEXED_COMPLETE_LAYOUT_H */
