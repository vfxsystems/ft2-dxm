#include "shared/ft2_ui_actions.h"
#include <stddef.h>

typedef struct
{
    ft2_ui_action_kind_t kind;
    void (*cb_void)(void);
    void (*cb_u32)(uint32_t);
} ft2_ui_action_entry_t;

static ft2_ui_action_entry_t g_actions[FT2_UI_ACTION_MAX];

void ft2_ui_actions_reset(void)
{
    for (int i = 0; i < FT2_UI_ACTION_MAX; i++) {
        g_actions[i].kind = FT2_UI_ACTION_KIND_NONE;
        g_actions[i].cb_void = NULL;
        g_actions[i].cb_u32 = NULL;
    }
}

static bool valid_action_id(ft2_ui_action_id_t id)
{
    return id > 0 && id < FT2_UI_ACTION_MAX;
}

bool ft2_ui_actions_register_void(ft2_ui_action_id_t id, ft2_ui_action_kind_t kind, void (*cb)(void))
{
    if (!valid_action_id(id) || cb == NULL)
        return false;

    g_actions[id].kind = kind;
    g_actions[id].cb_void = cb;
    g_actions[id].cb_u32 = NULL;
    return true;
}

bool ft2_ui_actions_register_u32(ft2_ui_action_id_t id, ft2_ui_action_kind_t kind, void (*cb)(uint32_t))
{
    if (!valid_action_id(id) || cb == NULL)
        return false;

    g_actions[id].kind = kind;
    g_actions[id].cb_u32 = cb;
    g_actions[id].cb_void = NULL;
    return true;
}

bool ft2_ui_actions_call_void(ft2_ui_action_id_t id)
{
    if (!valid_action_id(id))
        return false;

    if (g_actions[id].cb_void == NULL)
        return false;

    g_actions[id].cb_void();
    return true;
}

bool ft2_ui_actions_call_u32(ft2_ui_action_id_t id, uint32_t value)
{
    if (!valid_action_id(id))
        return false;

    if (g_actions[id].cb_u32 == NULL)
        return false;

    g_actions[id].cb_u32(value);
    return true;
}

ft2_ui_action_kind_t ft2_ui_actions_get_kind(ft2_ui_action_id_t id)
{
    if (!valid_action_id(id))
        return FT2_UI_ACTION_KIND_NONE;

    return g_actions[id].kind;
}
