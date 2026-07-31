#include "ft2_macro_map.h"
#include "ft2_macromap.h"
#include "ft2_gui.h"
#include "ft2_events.h"
#include "ft2_video.h"
#include "ft2_pushbuttons.h"
#include "ft2_structs.h"
#include "ft2_replayer.h"
#include "ft2_pattern_ed.h"
#include "ft2_mouse.h" // for mouseAnimOff()
#include "ft2_unified_synth.h"
#include "ft2_dsp.h"
#include "ft2_mixer.h"
#include "ft2_popup_list.h"

// Unique bases for Macro Map Editor widgets to avoid ID collisions
#define MM_NUM_SLOTS FT2_MACRO_MAP_NUM_SLOTS
#define MM_PB_COUNT FT2_MACRO_MAP_PUSHBUTTON_COUNT
#define MM_PB_BASE (NUM_PUSHBUTTONS - MM_PB_COUNT)
#define MM_PB_OK (MM_PB_BASE + 0)
#define MM_PB_CANCEL (MM_PB_BASE + 1)
#define MM_PB_PARAM_BASE (MM_PB_BASE + 2) // 16 buttons, one per slot (opens a dropdown)
#define MM_PB_TARGET_BASE (MM_PB_PARAM_BASE + MM_NUM_SLOTS) // 16 buttons
#define MM_PB_SCOPE_BASE (MM_PB_TARGET_BASE + MM_NUM_SLOTS) // 16 buttons
#define MM_PB_SLOT_BASE (MM_PB_SCOPE_BASE + MM_NUM_SLOTS) // 32 buttons (left/right interleaved)
#define MM_PB_TRACK_BASE (MM_PB_SLOT_BASE + (MM_NUM_SLOTS * 2)) // 4 buttons (track up/down/display/toggle)

// Max entries the per-slot parameter dropdown can show (OsTIrus's 256-entry param
// space is currently the largest source).
#define MM_MAX_PARAM_ITEMS 256

// Macro Map Editor popup box dimensions. Widened from the original 380x360 to
// comfortably fit the wide param-name dropdown buttons plus the longer engine
// labels/parameter names now reachable (V2/OsTIrus).
#define MM_BOX_W 460
#define MM_BOX_H 380

// Local state for Macro Map Editor dialog
static bool macroMapExitFlag = false;
static uint16_t macroParamLocal[MM_NUM_SLOTS];
static uint8_t macroTargetLocal[MM_NUM_SLOTS];
static uint8_t macroScaleLocal[MM_NUM_SLOTS];
static uint8_t macroDspScopeLocal[MM_NUM_SLOTS];
static uint8_t macroDspSlotLocal[MM_NUM_SLOTS];
static uint8_t macroTrackSel = 0;
static const char *macroParamPopupItems[MM_MAX_PARAM_ITEMS];

// Callback for OK and Cancel buttons
static void pbMacroMapOk(void)    { ui.sysReqShown = false; macroMapExitFlag = false; }
static void pbMacroMapCancel(void){ ui.sysReqShown = false; macroMapExitFlag = true; }

static uint8_t clampU8(uint8_t v, uint8_t lo, uint8_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static const char *getTargetLabel(uint8_t target)
{
    switch (target)
    {
        case MACRO_TARGET_TF4:     return "TF4";
        case MACRO_TARGET_DEXED:   return "DX";
        case MACRO_TARGET_DSP:     return "DSP";
        case MACRO_TARGET_V2:      return "V2";
        case MACRO_TARGET_OSTIRUS: return "OTI";
        default:                   return "---";
    }
}

static const char *getScopeLabel(uint8_t scope)
{
    return (scope == DSP_MACRO_SCOPE_MASTER) ? "MSTR" : "PAIR";
}

/* Unified per-engine parameter resolution, shared by every synth-backed macro target
   (TF4/Dexed/V2/OsTIrus) via the UnifiedSynthInterface vtable - eliminates the
   per-engine special-casing this file used to have (only Dexed went through the
   vtable; TF4 used a separate hardcoded array; V2/OsTIrus weren't reachable at all). */
static bool targetToEngine(uint8_t target, SynthEngineType *out)
{
    return ft2_macro_map_target_to_engine(target, out);
}

static const char *getMacroParamName(int slot)
{
    const uint8_t target = macroTargetLocal[slot];
    const uint16_t param = macroParamLocal[slot];

    if (targetToEngine(target, NULL))
        return ft2_macro_map_target_param_name(target, param);

    if (target == MACRO_TARGET_DSP)
    {
        const uint8_t scope = macroDspScopeLocal[slot];
        const uint8_t dspSlot = macroDspSlotLocal[slot];
        dspEffectInstance_t *eff = (scope == DSP_MACRO_SCOPE_MASTER)
            ? &masterEffects[dspSlot]
            : &stereoMixerCh[0].effects[dspSlot];
        int numParams = 0;
        const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
        if (pi && param < (uint16_t)numParams)
            return pi[param].name;
        return "DSP Param";
    }
    return "---";
}

static uint8_t getMacroValueForInstrument(int instID, int macroIdx)
{
    if (instID < 0 || instID >= MAX_INST || instr[instID] == NULL)
        return 0;

    const instr_t *ins = instr[instID];
    for (int ch = 0; ch < song.numChannels; ch++)
    {
        if (channel[ch].instrPtr == ins)
            return channel[ch].macroVal[macroIdx];
    }
    return 0;
}

static void updateMacroMapWidgetsForSlot(int slot)
{
    if (slot < 0 || slot >= MM_NUM_SLOTS)
        return;

    const uint8_t target = macroTargetLocal[slot];

    // Target button caption
    pushButtons[MM_PB_TARGET_BASE + slot].caption = (char *)getTargetLabel(target);

    // Scope button caption and visibility
    pushButtons[MM_PB_SCOPE_BASE + slot].caption = (char *)getScopeLabel(macroDspScopeLocal[slot]);
    pushButtons[MM_PB_SCOPE_BASE + slot].visible = (target == MACRO_TARGET_DSP);

    // DSP slot buttons visibility
    pushButtons[MM_PB_SLOT_BASE + slot * 2].visible = (target == MACRO_TARGET_DSP);
    pushButtons[MM_PB_SLOT_BASE + slot * 2 + 1].visible = (target == MACRO_TARGET_DSP);

    // Param dropdown button: visibility
    const bool showParams = (target != MACRO_TARGET_NONE);
    pushButtons[MM_PB_PARAM_BASE + slot].visible = showParams;

    // Clamp the stored param index to whatever range the (possibly just-changed) target
    // actually supports, so a stale index from a previous target doesn't linger.
    uint16_t maxIndex = 0;
    if (targetToEngine(target, NULL))
    {
        const int count = ft2_macro_map_target_param_count(target);
        if (count > 0)
            maxIndex = (uint16_t)(count - 1);
    }
    else if (target == MACRO_TARGET_DSP)
    {
        const uint8_t scope = macroDspScopeLocal[slot];
        const uint8_t dspSlot = macroDspSlotLocal[slot];
        dspEffectInstance_t *eff = (scope == DSP_MACRO_SCOPE_MASTER)
            ? &masterEffects[dspSlot]
            : &stereoMixerCh[0].effects[dspSlot];
        int numParams = 0;
        const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
        maxIndex = (pi && numParams > 0) ? (uint16_t)(numParams - 1) : 0;
    }

    if (macroParamLocal[slot] > maxIndex)
        macroParamLocal[slot] = maxIndex;

    pushButtons[MM_PB_PARAM_BASE + slot].caption = (char *)getMacroParamName(slot);
}

/* Fills macroParamPopupItems[] with the display names for whichever engine/DSP effect
   is currently targeted by the given slot. Returns the item count (0 if none). */
static int buildParamItems(int slot)
{
    const uint8_t target = macroTargetLocal[slot];

    if (targetToEngine(target, NULL))
    {
        int count = ft2_macro_map_target_param_count(target);
        if (count > MM_MAX_PARAM_ITEMS)
            count = MM_MAX_PARAM_ITEMS;
        for (int i = 0; i < count; i++)
            macroParamPopupItems[i] = ft2_macro_map_target_param_name(target, (uint16_t)i);
        return count;
    }

    if (target == MACRO_TARGET_DSP)
    {
        const uint8_t scope = macroDspScopeLocal[slot];
        const uint8_t dspSlot = macroDspSlotLocal[slot];
        dspEffectInstance_t *eff = (scope == DSP_MACRO_SCOPE_MASTER)
            ? &masterEffects[dspSlot]
            : &stereoMixerCh[0].effects[dspSlot];
        int numParams = 0;
        const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
        if (!pi || numParams <= 0)
            return 0;
        if (numParams > MM_MAX_PARAM_ITEMS)
            numParams = MM_MAX_PARAM_ITEMS;
        for (int i = 0; i < numParams; i++)
            macroParamPopupItems[i] = pi[i].name;
        return numParams;
    }

    return 0;
}

static void macroParamPopupCb(int32_t index, void *ctx)
{
    const int slot = (int)(intptr_t)ctx;
    if (slot < 0 || slot >= MM_NUM_SLOTS)
        return;

    if (index >= 0)
        macroParamLocal[slot] = (uint16_t)index;

    updateMacroMapWidgetsForSlot(slot);
}

static void pbMacroMapParamOpen(void)
{
    const int slot = (int)mouse.lastUsedObjectID - MM_PB_PARAM_BASE;
    if (slot < 0 || slot >= MM_NUM_SLOTS)
        return;

    if (macroTargetLocal[slot] == MACRO_TARGET_NONE)
        return;

    const int count = buildParamItems(slot);
    if (count <= 0)
        return;

    const pushButton_t *b = &pushButtons[MM_PB_PARAM_BASE + slot];
    popupListShow(b->x, (int16_t)(b->y + b->h), macroParamPopupItems, count,
                  macroParamLocal[slot], macroParamPopupCb, (void *)(intptr_t)slot);
}

static void pbMacroMapTargetToggle(void)
{
    const int slot = (int)mouse.lastUsedObjectID - MM_PB_TARGET_BASE;
    if (slot < 0 || slot >= MM_NUM_SLOTS)
        return;

    switch (macroTargetLocal[slot])
    {
        case MACRO_TARGET_NONE:    macroTargetLocal[slot] = MACRO_TARGET_TF4;     break;
        case MACRO_TARGET_TF4:     macroTargetLocal[slot] = MACRO_TARGET_DEXED;   break;
        case MACRO_TARGET_DEXED:   macroTargetLocal[slot] = MACRO_TARGET_V2;      break;
        case MACRO_TARGET_V2:      macroTargetLocal[slot] = MACRO_TARGET_OSTIRUS; break;
        case MACRO_TARGET_OSTIRUS: macroTargetLocal[slot] = MACRO_TARGET_DSP;     break;
        default:                   macroTargetLocal[slot] = MACRO_TARGET_NONE;    break;
    }

    updateMacroMapWidgetsForSlot(slot);
}

static void pbMacroMapScopeToggle(void)
{
    const int slot = (int)mouse.lastUsedObjectID - MM_PB_SCOPE_BASE;
    if (slot < 0 || slot >= MM_NUM_SLOTS)
        return;

    if (macroTargetLocal[slot] == MACRO_TARGET_DSP)
    {
        macroDspScopeLocal[slot] = (macroDspScopeLocal[slot] == DSP_MACRO_SCOPE_MASTER)
            ? DSP_MACRO_SCOPE_PAIR
            : DSP_MACRO_SCOPE_MASTER;
        updateMacroMapWidgetsForSlot(slot);
    }
}

static void pbMacroMapSlotAdjust(void)
{
    const int id = (int)mouse.lastUsedObjectID - MM_PB_SLOT_BASE;
    const int slot = id / 2;
    const bool right = (id & 1) != 0;
    if (slot < 0 || slot >= MM_NUM_SLOTS)
        return;

    if (macroTargetLocal[slot] == MACRO_TARGET_DSP)
    {
        uint8_t dspSlot = macroDspSlotLocal[slot];
        if (right)
            dspSlot = (uint8_t)((dspSlot + 1) % DSP_MAX_SLOTS);
        else
            dspSlot = (dspSlot == 0) ? (DSP_MAX_SLOTS - 1) : (dspSlot - 1);
        macroDspSlotLocal[slot] = dspSlot;
        updateMacroMapWidgetsForSlot(slot);
    }
}

static void updateMacroTrackWidgets(void)
{
    static char trkLabel[4];
    snprintf(trkLabel, sizeof(trkLabel), "%02u", (unsigned)macroTrackSel + 1);
    pushButtons[MM_PB_TRACK_BASE + 1].caption = trkLabel;
    pushButtons[MM_PB_TRACK_BASE + 3].caption =
        (macroTrackSel < MAX_STEREO_PAIRS && editor.macroMode[macroTrackSel]) ? "M" : "-";
}

static void pbMacroMapTrackAdjust(void)
{
    const int id = (int)mouse.lastUsedObjectID - MM_PB_TRACK_BASE;
    if (id < 0 || id > 3)
        return;

    if (id == 0)
    {
        if (macroTrackSel > 0)
            macroTrackSel--;
    }
    else if (id == 2)
    {
        if (macroTrackSel + 1 < MAX_STEREO_PAIRS)
            macroTrackSel++;
    }
    else if (id == 3)
    {
        editor.macroMode[macroTrackSel] = !editor.macroMode[macroTrackSel];
        if (editor.macroMode[macroTrackSel])
            allocatePattern(editor.editPattern);
    }

    updateMacroTrackWidgets();
    ui.updatePatternEditor = true;
}

// Initialize widgets (pushbuttons)
static void setupMacroMapBoxWidgets(void)
{
    const int16_t w = MM_BOX_W, h = MM_BOX_H;
    const int16_t x = (SCREEN_W - w) / 2;
    const int16_t y = (SCREEN_H - h) / 2;
    const int16_t tgtW = 34, tgtH = 12;
    const int16_t scopeW = 34, scopeH = 12;
    const int16_t slotArrowW = 10, slotArrowH = 12;
    const int16_t slotWidth = (w - 32) / 2;  // 16px margin each side, 2 columns
    const int16_t paramStartOffset = 44; // space for target/scope/slot controls
    const int16_t paramBtnW = slotWidth - paramStartOffset - 4;
    const int16_t paramBtnH = 13;
    const int16_t rowH = 28;
    const int16_t rowsTop = 42;
    pushButton_t *p;

    // OK button
    p = &pushButtons[MM_PB_OK]; memset(p, 0, sizeof(*p));
    p->caption = "OK";
    p->w = 73; p->h = 16;
    p->x = x + (w - (p->w * 2 + 8)) / 2;
    p->y = y + h - 24;
    p->callbackFuncOnUp = pbMacroMapOk;
    p->visible = true;

    // Cancel button
    p = &pushButtons[MM_PB_CANCEL]; memset(p, 0, sizeof(*p));
    p->caption = "Cancel";
    p->w = 73; p->h = 16;
    p->x = x + (w - (p->w * 2 + 8)) / 2 + p->w + 8;
    p->y = y + h - 24;
    p->callbackFuncOnUp = pbMacroMapCancel;
    p->visible = true;

    // Track selector (bottom, global)
    p = &pushButtons[MM_PB_TRACK_BASE + 0]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_UP_STRING;
    p->w = 12; p->h = 12;
    p->x = x + 16;
    p->y = y + h - 56;
    p->callbackFuncOnUp = pbMacroMapTrackAdjust;
    p->visible = true;

    p = &pushButtons[MM_PB_TRACK_BASE + 1]; memset(p, 0, sizeof(*p));
    p->caption = "01";
    p->w = 18; p->h = 12;
    p->x = x + 30;
    p->y = y + h - 56;
    p->visible = true;

    p = &pushButtons[MM_PB_TRACK_BASE + 2]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_DOWN_STRING;
    p->w = 12; p->h = 12;
    p->x = x + 50;
    p->y = y + h - 56;
    p->callbackFuncOnUp = pbMacroMapTrackAdjust;
    p->visible = true;

    p = &pushButtons[MM_PB_TRACK_BASE + 3]; memset(p, 0, sizeof(*p));
    p->caption = "-";
    p->w = 12; p->h = 12;
    p->x = x + 30;
    p->y = y + h - 40;
    p->callbackFuncOnUp = pbMacroMapTrackAdjust;
    p->visible = true;

    updateMacroTrackWidgets();

    // Per-slot controls
    for (int i = 0; i < 16; i++)
    {
        int row = i % 8;
        int col = i / 8;
        int16_t sx = x + 16 + col * slotWidth;
        int16_t sy = y + rowsTop + row * rowH;
        int16_t syTop = sy;

        // Target type button
        p = &pushButtons[MM_PB_TARGET_BASE + i]; memset(p, 0, sizeof(*p));
        p->caption = (char *)getTargetLabel(macroTargetLocal[i]);
        p->x = sx; p->y = syTop;
        p->w = tgtW; p->h = tgtH;
        p->callbackFuncOnUp = pbMacroMapTargetToggle;
        p->visible = true;

        // DSP scope toggle button
        p = &pushButtons[MM_PB_SCOPE_BASE + i]; memset(p, 0, sizeof(*p));
        p->caption = (char *)getScopeLabel(macroDspScopeLocal[i]);
        p->x = sx + tgtW + 4; p->y = syTop;
        p->w = scopeW; p->h = scopeH;
        p->callbackFuncOnUp = pbMacroMapScopeToggle;
        p->visible = (macroTargetLocal[i] == MACRO_TARGET_DSP);

        // DSP slot arrows (left/right)
        p = &pushButtons[MM_PB_SLOT_BASE + i * 2]; memset(p, 0, sizeof(*p));
        p->caption = ARROW_LEFT_STRING;
        p->x = sx + tgtW + scopeW + 10; p->y = syTop;
        p->w = slotArrowW; p->h = slotArrowH;
        p->callbackFuncOnUp = pbMacroMapSlotAdjust;
        p->visible = (macroTargetLocal[i] == MACRO_TARGET_DSP);

        p = &pushButtons[MM_PB_SLOT_BASE + i * 2 + 1]; memset(p, 0, sizeof(*p));
        p->caption = ARROW_RIGHT_STRING;
        p->x = sx + tgtW + scopeW + 10 + slotArrowW + 20; p->y = syTop;
        p->w = slotArrowW; p->h = slotArrowH;
        p->callbackFuncOnUp = pbMacroMapSlotAdjust;
        p->visible = (macroTargetLocal[i] == MACRO_TARGET_DSP);

        // Parameter dropdown button - opens a popupListShow() list of the current
        // target's parameter names when clicked.
        p = &pushButtons[MM_PB_PARAM_BASE + i]; memset(p, 0, sizeof(*p));
        p->caption = (char *)getMacroParamName(i);
        p->x = sx + paramStartOffset; p->y = sy;
        p->w = paramBtnW; p->h = paramBtnH;
        p->callbackFuncOnUp = pbMacroMapParamOpen;
        p->visible = (macroTargetLocal[i] != MACRO_TARGET_NONE);

        updateMacroMapWidgetsForSlot(i);
    }
}

// Draw the dialog frame
// Lightweight modal helpers (local copy since original ones are static in sample editor module)
static void windowOpenMacroMap(void)
{
    ui.sysReqShown = true;
    ui.sysReqEnterPressed = false;
    ui.macroMapEditorShown = true; // allow full widget range during modal dialog

    unstuckLastUsedGUIElement();
    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
}

static void windowCloseMacroMap(void)
{
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    ui.macroMapEditorShown = false; // restore flag
    mouseAnimOff();

    /* Force full top and bottom screen redraw */
    showTopScreen(true);
    showBottomScreen();
    ui.updatePatternEditor = true;
    ui.updatePosSections  = true;
}

static void drawMacroMapBox(void)
{
    const int16_t w = MM_BOX_W;
    const int16_t h = MM_BOX_H;
    int16_t x = (SCREEN_W - w) / 2;
    int16_t y = (SCREEN_H - h) / 2;

    // main fill
    fillRect(x + 1, y + 1, w - 2, h - 2, PAL_BUTTONS);
    // outer border
    vLine(x,         y,         h - 1, PAL_BUTTON1);
    hLine(x + 1,     y,         w - 2, PAL_BUTTON1);
    vLine(x + w - 1, y,         h,     PAL_BUTTON2);
    hLine(x,         y + h - 1, w - 1, PAL_BUTTON2);
    // inner border
    vLine(x + 2,     y + 2,     h - 5, PAL_BUTTON2);
    hLine(x + 3,     y + 2,     w - 6, PAL_BUTTON2);
    vLine(x + w - 3, y + 2,     h - 4, PAL_BUTTON1);
    hLine(x + 2,     y + h - 3, w - 4, PAL_BUTTON1);

    // Title
    textOutShadow(x + 16, y + 8, PAL_FORGRND, PAL_BUTTON2, "Macro Map Editor");
    textOutTiny(x + 16, y + 20, "Macro: Zx select, Y/U/N=val (00-FF)", PAL_BUTTON1);
}

// Main entry for Macro Map Editor dialog
void showMacroMapEditor(void)
{
    int instID = editor.curInstr;
    if (instID < 0 || instID >= MAX_INST)
        return;
    
    if (instr[instID] == NULL)
        return;

    ft2_macro_map_sanitize_instrument(instr[instID]);

    // Copy current macro settings
    macroTrackSel = 0;
    for (int i = 0; i < MM_NUM_SLOTS; i++)
    {
        macroTargetLocal[i] = instr[instID]->macroTargetType[i];
        macroScaleLocal[i] = instr[instID]->macroScale[i];
        macroParamLocal[i] = instr[instID]->macroParamID[i];

        if (macroTargetLocal[i] == MACRO_TARGET_DSP)
        {
            macroDspScopeLocal[i] = DSP_MACRO_SCOPE(macroParamLocal[i]);
            macroDspSlotLocal[i] = DSP_MACRO_SLOT(macroParamLocal[i]);
            macroParamLocal[i] = DSP_MACRO_PARAM(macroParamLocal[i]);
            macroDspSlotLocal[i] = clampU8(macroDspSlotLocal[i], 0, DSP_MAX_SLOTS - 1);
        }
        else
        {
            macroDspScopeLocal[i] = DSP_MACRO_SCOPE_PAIR;
            macroDspSlotLocal[i] = 0;
        }

    }

    macroMapExitFlag = false;
    setupMacroMapBoxWidgets();
    windowOpenMacroMap();

    while (ui.sysReqShown)
    {
        readInput();
        if (ui.sysReqEnterPressed)
            pbMacroMapOk();
        handleRedrawing();
        drawMacroMapBox();
        // Draw per-slot controls and parameter/value readouts
        {
            const int16_t boxW = MM_BOX_W, boxH = MM_BOX_H;
            const int16_t boxX = (SCREEN_W - boxW) / 2;
            const int16_t boxY = (SCREEN_H - boxH) / 2;
            const int16_t paramStartOffset = 44;
            const int16_t rowH = 28;
            const int16_t rowsTop = 42;
            const int16_t slotWidth = (boxW - 32) / 2;  // margin 16px, 2 columns
            for (int i = 0; i < 16; i++)
            {
                int row = i % 8, col = i / 8;
                int16_t sx = boxX + 16 + col * slotWidth;
                int16_t sy = boxY + rowsTop + row * rowH;
                int16_t syTop = sy;
                drawPushButton(MM_PB_TARGET_BASE + i);
                if (macroTargetLocal[i] == MACRO_TARGET_DSP)
                {
                    drawPushButton(MM_PB_SCOPE_BASE + i);
                    drawPushButton(MM_PB_SLOT_BASE + i * 2);
                    drawPushButton(MM_PB_SLOT_BASE + i * 2 + 1);
                    char slotLabel[8];
                    snprintf(slotLabel, sizeof(slotLabel), "S%u", (unsigned)macroDspSlotLocal[i]);
                    textOutTiny(sx + 92, syTop + 2, slotLabel, PAL_BUTTON1);
                }
                drawPushButton(MM_PB_PARAM_BASE + i);
                /* Macro index + current live value, shown below the param button
                   (the parameter name itself is already the button's caption). */
                char labelBuf[16];
                const uint8_t val = getMacroValueForInstrument(instID, i);
                snprintf(labelBuf, sizeof(labelBuf), "M%X: %02X", i, val);
                textOutTiny(sx + paramStartOffset, syTop + 16, labelBuf, PAL_BUTTON1);
            }
            // Track selector (bottom, global)
            drawPushButton(MM_PB_TRACK_BASE + 0);
            drawPushButton(MM_PB_TRACK_BASE + 1);
            drawPushButton(MM_PB_TRACK_BASE + 2);
            drawPushButton(MM_PB_TRACK_BASE + 3);
            textOutTiny(boxX + 16, boxY + boxH - 68, "Track", PAL_BUTTON1);
            textOutTiny(boxX + 46, boxY + boxH - 40, "Macro", PAL_BUTTON1);
        }
        // Draw the parameter dropdown last, on top of everything else in the box.
        popupListDraw();
        flipFrame();
    }

    // Hide all Macro Map Editor pushbuttons
    for (int i = 0; i < MM_PB_COUNT; i++)
        hidePushButton(MM_PB_BASE + i);
    windowCloseMacroMap();

    if (!macroMapExitFlag)
    {
        // Commit selection to instrument mapping
        if (instID >= 0 && instID < MAX_INST && instr[instID] != NULL)
        {
            instr_t *ins = instr[instID];
            for (int i = 0; i < MM_NUM_SLOTS; i++)
            {
                ins->macroTargetType[i] = macroTargetLocal[i];
                ins->macroScale[i]     = macroScaleLocal[i];
                if (macroTargetLocal[i] == MACRO_TARGET_NONE)
                {
                    ins->macroParamID[i] = 0;
                    ins->macroScale[i] = MACRO_CURVE_LINEAR;
                }
                else if (macroTargetLocal[i] == MACRO_TARGET_DSP)
                {
                    const uint16_t pid = DSP_MACRO_PARAMID(macroDspScopeLocal[i], macroDspSlotLocal[i], macroParamLocal[i]);
                    ins->macroParamID[i] = pid;
                }
                else
                {
                    ins->macroParamID[i] = macroParamLocal[i];
                }
            }
            ft2_macro_map_sanitize_instrument(ins);
            ui_sync_from_instrument();
    
        }
    }
}
