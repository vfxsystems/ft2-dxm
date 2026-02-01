#ifndef FT2_TUNEFISH_COMPLETE_LAYOUT_H
#define FT2_TUNEFISH_COMPLETE_LAYOUT_H

#include "ft2_tunefish_widgets.h"
#include "ft2_waveform_view.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Layout Constants
#define TF_LAYOUT_WIDTH         632  // Match FT2's SCREEN_W
#define TF_LAYOUT_HEIGHT        400  // Keep existing height
#define TF_PAGE_COUNT           2

// Widget Counts per Page
#define TF_GLOBAL_WIDGETS       6    // Title, preset, page buttons, exit
#define TF_PAGE1_WIDGETS        99   // Enlarged to fit Main-synth widgets incl. unisono+octave rows + glide/poly
#define TF_PAGE2_WIDGETS        171  // Increased capacity for new FX grid, larger mod matrix, and FX stack
#define TF_TOTAL_WIDGETS        (TF_GLOBAL_WIDGETS + TF_PAGE1_WIDGETS + TF_PAGE2_WIDGETS)

// Layout Positioning Constants
typedef struct {
    // Global area positions
    int title_x, title_y;
    int preset_x, preset_y, preset_w, preset_h;
    int page_toggle_x, page_toggle_y, page_toggle_w, page_toggle_h;
    int exit_x, exit_y, exit_w, exit_h;
    int main_meter_x, main_meter_y, main_meter_w, main_meter_h;
    int cpu_meter_x, cpu_meter_y, cpu_meter_w, cpu_meter_h;

    // Page 1 positions
    struct {
        int global_group_x, global_group_y, global_group_w, global_group_h;
        int generator_group_x, generator_group_y, generator_group_w, generator_group_h;
        int lfo1_group_x, lfo1_group_y, lfo1_group_w, lfo1_group_h;
        int lfo2_group_x, lfo2_group_y, lfo2_group_w, lfo2_group_h;
        int filter_group_x, filter_group_y, filter_group_w, filter_group_h;
        int adsr1_group_x, adsr1_group_y, adsr1_group_w, adsr1_group_h;
        int adsr2_group_x, adsr2_group_y, adsr2_group_w, adsr2_group_h;
    } page1;

    // Page 2 positions
    struct {
        int fx1_group_x, fx1_group_y, fx1_group_w, fx1_group_h;
        int fx2_group_x, fx2_group_y, fx2_group_w, fx2_group_h;
        int formant_group_x, formant_group_y, formant_group_w, formant_group_h;
        int mod_group_x, mod_group_y, mod_group_w, mod_group_h;
    } page2;
} TunefishLayoutPositions;

// Main Layout Structure
typedef struct {
    // Global persistent widgets (always visible)
    TunefishWidget* title_label;
    TunefishWidget* preset_combo;
    TunefishWidget* page_toggle_button;
    TunefishWidget* exit_button;
    TunefishWidget* main_level_meter;
    TunefishWidget* cpu_meter;
    TunefishWidget* waveform_view;

    // Page 1 - Main Synth (generators, filters, LFOs, ADSR)
    struct {
        // Global Section
        TunefishWidget* poly_control;
        TunefishWidget* pitch_up_control;
        TunefishWidget* pitch_down_control;
        TunefishWidget* global_group;

        // Generator Section
        TunefishWidget* gen_volume_knob;
        TunefishWidget* gen_panning_knob;
        TunefishWidget* gen_detune_knob;
        TunefishWidget* gen_spread_knob;
        TunefishWidget* gen_bandwidth_knob;
        TunefishWidget* gen_damp_knob;
        TunefishWidget* gen_harmonics_knob;
        TunefishWidget* gen_drive_knob;
        TunefishWidget* gen_scale_knob;
        TunefishWidget* gen_modulation_knob;
        TunefishWidget* gen_noise_knob;
        TunefishWidget* gen_noise_freq_knob;
        TunefishWidget* gen_noise_bw_knob;
        TunefishWidget* gen_glide_control;
        TunefishWidget* gen_waveform_view;
        TunefishWidget* gen_group;

        // LFO1 Section
        TunefishWidget* lfo1_freq_knob;
        TunefishWidget* lfo1_depth_knob;
        TunefishWidget* lfo1_shape_buttons[5]; //TF_LFO1_SHAPE [0]=Sine,[1]=Square,[2]=RampUp,[3]=RampDown,[4]=Random
        TunefishWidget* lfo1_sync_toggle;
        TunefishWidget* lfo1_group;

        // LFO2 Section
        TunefishWidget* lfo2_freq_knob;
        TunefishWidget* lfo2_depth_knob;
        TunefishWidget* lfo2_shape_buttons[5]; //TF_LFO1_SHAPE [0]=Sine,[1]=Square,[2]=RampUp,[3]=RampDown,[4]=Random
        TunefishWidget* lfo2_sync_toggle;
        TunefishWidget* lfo2_group;

        // Filter Section (4 filters)
        TunefishWidget* filter_cutoff_knob;
        TunefishWidget* filter_resonance_knob;
        TunefishWidget* filter_type_combo;
        TunefishWidget* filter1_on_toggle; // LP filter ON toggle
        TunefishWidget* filter_group;

        TunefishWidget* filter2_cutoff_knob;
        TunefishWidget* filter2_resonance_knob;
        TunefishWidget* filter2_type_label;
        TunefishWidget* filter2_on_toggle;
        TunefishWidget* filter2_group;

        TunefishWidget* filter3_cutoff_knob;
        TunefishWidget* filter3_resonance_knob;
        TunefishWidget* filter3_type_label;
        TunefishWidget* filter3_on_toggle;
        TunefishWidget* filter3_group;

        TunefishWidget* filter4_cutoff_knob;
        TunefishWidget* filter4_resonance_knob;
        TunefishWidget* filter4_type_label;
        TunefishWidget* filter4_on_toggle;
        TunefishWidget* filter4_group;

        // ADSR Envelopes
        TunefishWidget* adsr1_env_view;
        TunefishWidget* adsr1_slope_slider;
        TunefishWidget* adsr1_group;

        TunefishWidget* adsr2_env_view;
        TunefishWidget* adsr2_slope_slider;
        TunefishWidget* adsr2_group;

        // Voice Controls
        TunefishWidget* unisono_buttons[10];    // 1-10
        TunefishWidget* octave_buttons[9];      // -4 to +4
    } page1;

    // Page 2 - Effects and Modulation
    struct {
        // Legacy FX Stack (compatibility)
        TunefishWidget* fx1_type_combo;
        TunefishWidget* fx1_wet_knob;
        TunefishWidget* fx1_param1_knob;
        TunefishWidget* fx1_param2_knob;
        TunefishWidget* fx1_group;

        TunefishWidget* fx2_type_combo;
        TunefishWidget* fx2_wet_knob;
        TunefishWidget* fx2_param1_knob;
        TunefishWidget* fx2_param2_knob;
        TunefishWidget* fx2_group;

        // FX Grid (7 effect types)
        TunefishWidget* fx_groups[7];
        TunefishWidget* fx_wet_knobs[7];
        TunefishWidget* fx_param1_knobs[7];
        TunefishWidget* fx_param2_knobs[7];
        TunefishWidget* fx_param3_knobs[7];

        // Formant Filter
        TunefishWidget* formant_type_buttons[5]; // A, E, I, O, U
        TunefishWidget* formant_amount_knob;
        TunefishWidget* formant_group;

        // Modulation Matrix (8 slots)
        TunefishWidget* mod_source_combos[8];
        TunefishWidget* mod_dest_combos[8];
        TunefishWidget* mod_amount_knobs[8];
        TunefishWidget* modulation_group;

        // FX Stack (2 rows x 5)
        TunefishWidget* fx_stack_group;
        TunefishWidget* fx_stack_combos[10];
        TunefishWidget* fx_stack_wet_knobs[10];
    } page2;

    // Layout State
    int current_page;
    bool initialized;
    bool visible;

    // Widget Arrays for Easy Management
    TunefishWidget* all_widgets[TF_TOTAL_WIDGETS];
    TunefishWidget* global_widgets[TF_GLOBAL_WIDGETS];
    TunefishWidget* page1_widgets[TF_PAGE1_WIDGETS];
    TunefishWidget* page2_widgets[TF_PAGE2_WIDGETS];
} TunefishCompleteLayout;

// === Core Layout Management ===
TunefishCompleteLayout* tf_create_complete_layout(void);
void tf_destroy_complete_layout(TunefishCompleteLayout* layout);

// === Page Management ===
void tf_switch_to_page(TunefishCompleteLayout* layout, int page);
void tf_show_layout(TunefishCompleteLayout* layout);
void tf_hide_layout(TunefishCompleteLayout* layout);

// === Rendering ===
void tf_render_complete_layout(TunefishCompleteLayout* layout);

// === Event Handling ===
bool tf_handle_layout_mouse_event(TunefishCompleteLayout* layout, int mouseX, int mouseY, bool pressed);
bool tf_handle_layout_mouse_drag(TunefishCompleteLayout* layout, int mouseX, int mouseY);
bool tf_handle_layout_keyboard_test(TunefishCompleteLayout* layout, int key);

// === Parameter Management ===
void tf_sync_widgets_with_parameters(TunefishCompleteLayout* layout);
void tf_update_widget_from_parameter(TunefishCompleteLayout* layout, const char* widgetName, int value);
void tf_update_all_widgets_from_synth(TunefishCompleteLayout* layout);

// === Widget Management ===
TunefishWidget* tf_find_widget_by_name(TunefishCompleteLayout* layout, const char* name);

// === Styling ===
void tf_style_all_widgets_authentic(TunefishCompleteLayout* layout);
void tf_apply_knob_styling(TunefishWidget* knob, float defaultValue);
void tf_apply_button_styling(TunefishWidget* button);
void tf_apply_combo_styling(TunefishWidget* combo);
void tf_apply_meter_styling(TunefishWidget* meter);

extern TunefishCompleteLayout* g_active_tunefish_layout;

// === Utility Functions ===
const TunefishLayoutPositions* tf_get_layout_positions(void);

// === Global Variables ===
extern TunefishCompleteLayout* g_active_tunefish_layout;

#ifdef __cplusplus
}
#endif

#endif // FT2_TUNEFISH_COMPLETE_LAYOUT_H
