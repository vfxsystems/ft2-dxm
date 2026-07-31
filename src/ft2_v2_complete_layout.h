#ifndef FT2_V2_COMPLETE_LAYOUT_H
#define FT2_V2_COMPLETE_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "ft2_tunefish_complete_layout.h"
#include "ft2_tunefish_widgets.h"
#include "ft2_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define V2_LAYOUT_WIDTH   TF_LAYOUT_WIDTH
#define V2_LAYOUT_HEIGHT  TF_LAYOUT_HEIGHT

#define V2_PAGE_COUNT     6
#define V2_MOD_ROWS       8
#define V2_MAX_BINDINGS   192
#define V2_MAX_WIDGETS    256

typedef enum
{
    V2_PAGE_VOICE_OSC = 0,
    V2_PAGE_FILTER = 1,
    V2_PAGE_LFO_ENV = 2,
    V2_PAGE_FX = 3,
    V2_PAGE_MASTER = 4,
    V2_PAGE_MOD = 5
} V2Page;

typedef enum
{
    V2_BIND_NONE = 0,
    V2_BIND_PATCH_PARAM,
    V2_BIND_GLOBAL_PARAM,
    V2_BIND_MOD_SOURCE,
    V2_BIND_MOD_AMOUNT,
    V2_BIND_MOD_DEST
} V2BindingKind;

typedef struct
{
    TunefishWidget *widget;
    V2BindingKind kind;
    int target;
} V2WidgetBinding;

typedef struct V2CompleteLayout
{
    TunefishWidget *title_label;
    TunefishWidget *page_caption_label;
    TunefishWidget *preset_combo;
    TunefishWidget *voice_meter;
    TunefishWidget *close_button;
    TunefishWidget *page_voice_button;
    TunefishWidget *page_filter_button;
    TunefishWidget *page_lfo_env_button;
    TunefishWidget *page_fx_button;
    TunefishWidget *page_master_button;
    TunefishWidget *page_mod_button;
    TunefishWidget *mod_bank_combo;
    TunefishWidget *amp_env_display;
    TunefishWidget *eg2_env_display;

    TunefishWidget *mod_slot_labels[V2_MOD_ROWS];
    TunefishWidget *mod_source_widgets[V2_MOD_ROWS];
    TunefishWidget *mod_amount_widgets[V2_MOD_ROWS];
    TunefishWidget *mod_dest_widgets[V2_MOD_ROWS];

    V2WidgetBinding bindings[V2_MAX_BINDINGS];
    int binding_count;

    TunefishWidget *all_widgets[V2_MAX_WIDGETS];
    int all_widget_count;

    int current_page;
    int current_mod_bank;
    int active_instrument_id;
    bool initialized;
    bool visible;
    uint32_t last_sync_ticks;
} V2CompleteLayout;

extern V2CompleteLayout *g_active_v2_layout;

V2CompleteLayout *v2_create_complete_layout(void);
void v2_destroy_complete_layout(V2CompleteLayout *layout);
void v2_show_layout(V2CompleteLayout *layout);
void v2_hide_layout(V2CompleteLayout *layout);
void v2_switch_to_page(V2CompleteLayout *layout, int page);

void v2_render_complete_layout(V2CompleteLayout *layout);
bool v2_handle_layout_mouse_event(V2CompleteLayout *layout, int mouseX, int mouseY, bool pressed);
bool v2_handle_layout_mouse_drag(V2CompleteLayout *layout, int mouseX, int mouseY);
bool v2_handle_layout_keyboard_test(V2CompleteLayout *layout, int key);

void v2_sync_widgets_with_parameters(V2CompleteLayout *layout);
void v2_update_all_widgets_from_synth(V2CompleteLayout *layout);
TunefishWidget *v2_find_widget_by_name(V2CompleteLayout *layout, const char *name);
void v2_style_all_widgets_authentic(V2CompleteLayout *layout);

#ifdef __cplusplus
}
#endif

#endif /* FT2_V2_COMPLETE_LAYOUT_H */
