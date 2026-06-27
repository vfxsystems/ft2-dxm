#ifndef FT2_V2_COMPLETE_LAYOUT_H
#define FT2_V2_COMPLETE_LAYOUT_H

#include "ft2_tunefish_widgets.h"
#include "ft2_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define V2_LAYOUT_WIDTH  TF_LAYOUT_WIDTH
#define V2_LAYOUT_HEIGHT TF_LAYOUT_HEIGHT

#define V2_PAGE_COUNT 3
#define V2_MOD_ROWS   8

#define V2_MAX_PATCH_BINDINGS 128
#define V2_MAX_GLOBAL_BINDINGS 64
#define V2_MAX_WIDGETS 512

typedef enum {
    V2_PAGE_PATCH = 0,
    V2_PAGE_GLOBALS = 1,
    V2_PAGE_MOD = 2
} V2Page;

typedef struct {
    TunefishWidget* label;
    TunefishWidget* control;
    int paramId;
    int topicIndex;
    bool isGlobal;
    Ft2V2CtlType ctlType;
} V2ParamControlBinding;

typedef struct {
    TunefishWidget* slot_label;
    TunefishWidget* source_combo;
    TunefishWidget* amount_control;
    TunefishWidget* dest_combo;
    int slotIndex;
} V2ModRowWidgets;

typedef struct V2CompleteLayout {
    TunefishWidget* title_label;
    TunefishWidget* page_caption_label;
    TunefishWidget* preset_combo;
    TunefishWidget* page_patch_button;
    TunefishWidget* page_globals_button;
    TunefishWidget* page_mod_button;
    TunefishWidget* close_button;
    TunefishWidget* active_voices_meter;
    TunefishWidget* patch_topic_combo;
    TunefishWidget* patch_topic_group;
    TunefishWidget* global_topic_combo;
    TunefishWidget* global_topic_group;
    TunefishWidget* mod_bank_combo;
    TunefishWidget* mod_group;

    V2ParamControlBinding patch_params[V2_MAX_PATCH_BINDINGS];
    int patch_param_count;

    V2ParamControlBinding global_params[V2_MAX_GLOBAL_BINDINGS];
    int global_param_count;

    V2ModRowWidgets mod_rows[V2_MOD_ROWS];

    TunefishWidget* all_widgets[V2_MAX_WIDGETS];
    int all_widget_count;

    int current_page;
    int current_patch_topic;
    int current_global_topic;
    int current_mod_bank;
    int active_instrument_id;
    bool initialized;
    bool visible;
    uint32_t last_sync_ticks;
} V2CompleteLayout;

extern V2CompleteLayout* g_active_v2_layout;

V2CompleteLayout* v2_create_complete_layout(void);
void v2_destroy_complete_layout(V2CompleteLayout* layout);
void v2_show_layout(V2CompleteLayout* layout);
void v2_hide_layout(V2CompleteLayout* layout);
void v2_switch_to_page(V2CompleteLayout* layout, int page);

void v2_render_complete_layout(V2CompleteLayout* layout);
bool v2_handle_layout_mouse_event(V2CompleteLayout* layout, int mouseX, int mouseY, bool pressed);
bool v2_handle_layout_mouse_drag(V2CompleteLayout* layout, int mouseX, int mouseY);
bool v2_handle_layout_keyboard_test(V2CompleteLayout* layout, int key);

void v2_sync_widgets_with_parameters(V2CompleteLayout* layout);
void v2_update_all_widgets_from_synth(V2CompleteLayout* layout);
TunefishWidget* v2_find_widget_by_name(V2CompleteLayout* layout, const char* name);
void v2_style_all_widgets_authentic(V2CompleteLayout* layout);

#ifdef __cplusplus
}
#endif

#endif /* FT2_V2_COMPLETE_LAYOUT_H */
