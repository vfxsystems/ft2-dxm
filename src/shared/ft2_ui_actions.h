#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "shared/ft2_ui_schema.h"

#define FT2_UI_ACTION_MAX 4096

typedef enum
{
    FT2_UI_ACTION_KIND_NONE = 0,
    FT2_UI_ACTION_KIND_PUSHBUTTON_DOWN,
    FT2_UI_ACTION_KIND_PUSHBUTTON_UP,
    FT2_UI_ACTION_KIND_CHECKBOX,
    FT2_UI_ACTION_KIND_RADIOBUTTON,
    FT2_UI_ACTION_KIND_SCROLLBAR
} ft2_ui_action_kind_t;

void ft2_ui_actions_reset(void);
bool ft2_ui_actions_register_void(ft2_ui_action_id_t id, ft2_ui_action_kind_t kind, void (*cb)(void));
bool ft2_ui_actions_register_u32(ft2_ui_action_id_t id, ft2_ui_action_kind_t kind, void (*cb)(uint32_t));
bool ft2_ui_actions_call_void(ft2_ui_action_id_t id);
bool ft2_ui_actions_call_u32(ft2_ui_action_id_t id, uint32_t value);
ft2_ui_action_kind_t ft2_ui_actions_get_kind(ft2_ui_action_id_t id);
