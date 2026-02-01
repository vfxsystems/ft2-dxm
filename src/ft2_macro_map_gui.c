#include "ft2_macro_map.h"
#include "ft2_macromap.h"
#include "ft2_gui.h"
#include "ft2_events.h"
#include "ft2_video.h"
#include "ft2_scrollbars.h"
#include "ft2_pushbuttons.h"
#include "ft2_structs.h"
#include "ft2_replayer.h"
#include "ft2_pattern_ed.h"
#include "ft2_mouse.h" // for mouseAnimOff()
#include "ft2_unified_synth.h"
#include "ft2_dsp.h"
#include "ft2_mixer.h"

// Unique bases for Macro Map Editor widgets to avoid ID collisions
#define MM_NUM_SLOTS 16
#define MM_SB_BASE (NUM_SCROLLBARS - MM_NUM_SLOTS)      // 16 scrollbars
#define MM_PB_COUNT 102
#define MM_PB_BASE (NUM_PUSHBUTTONS - MM_PB_COUNT)
#define MM_PB_OK (MM_PB_BASE + 0)
#define MM_PB_CANCEL (MM_PB_BASE + 1)
#define MM_PB_PARAM_BASE (MM_PB_BASE + 2) // 32 buttons (left/right interleaved)
#define MM_PB_TARGET_BASE (MM_PB_PARAM_BASE + (MM_NUM_SLOTS * 2)) // 16 buttons
#define MM_PB_SCOPE_BASE (MM_PB_TARGET_BASE + MM_NUM_SLOTS) // 16 buttons
#define MM_PB_SLOT_BASE (MM_PB_SCOPE_BASE + MM_NUM_SLOTS) // 32 buttons (left/right interleaved)
#define MM_PB_TRACK_BASE (MM_PB_SLOT_BASE + (MM_NUM_SLOTS * 2)) // 4 buttons (track up/down/display/toggle)



// Local state for Macro Map Editor dialog
static bool macroMapExitFlag = false;
static uint16_t macroParamLocal[MM_NUM_SLOTS];
static uint8_t macroTargetLocal[MM_NUM_SLOTS];
static uint8_t macroScaleLocal[MM_NUM_SLOTS];
static uint8_t macroDspScopeLocal[MM_NUM_SLOTS];
static uint8_t macroDspSlotLocal[MM_NUM_SLOTS];
static uint8_t macroTrackSel = 0;

// Callback for OK and Cancel buttons
static void pbMacroMapOk(void)    { ui.sysReqShown = false; macroMapExitFlag = false; }
static void pbMacroMapCancel(void){ ui.sysReqShown = false; macroMapExitFlag = true; }

// Scrollbar change callbacks
static void sbSetMacroMap0(uint32_t pos)  { macroParamLocal[0]  = (uint16_t)pos; }
static void sbSetMacroMap1(uint32_t pos)  { macroParamLocal[1]  = (uint16_t)pos; }
static void sbSetMacroMap2(uint32_t pos)  { macroParamLocal[2]  = (uint16_t)pos; }
static void sbSetMacroMap3(uint32_t pos)  { macroParamLocal[3]  = (uint16_t)pos; }
static void sbSetMacroMap4(uint32_t pos)  { macroParamLocal[4]  = (uint16_t)pos; }
static void sbSetMacroMap5(uint32_t pos)  { macroParamLocal[5]  = (uint16_t)pos; }
static void sbSetMacroMap6(uint32_t pos)  { macroParamLocal[6]  = (uint16_t)pos; }
static void sbSetMacroMap7(uint32_t pos)  { macroParamLocal[7]  = (uint16_t)pos; }
static void sbSetMacroMap8(uint32_t pos)  { macroParamLocal[8]  = (uint16_t)pos; }
static void sbSetMacroMap9(uint32_t pos)  { macroParamLocal[9]  = (uint16_t)pos; }
static void sbSetMacroMap10(uint32_t pos) { macroParamLocal[10] = (uint16_t)pos; }
static void sbSetMacroMap11(uint32_t pos) { macroParamLocal[11] = (uint16_t)pos; }
static void sbSetMacroMap12(uint32_t pos) { macroParamLocal[12] = (uint16_t)pos; }
static void sbSetMacroMap13(uint32_t pos) { macroParamLocal[13] = (uint16_t)pos; }
static void sbSetMacroMap14(uint32_t pos) { macroParamLocal[14] = (uint16_t)pos; }
static void sbSetMacroMap15(uint32_t pos) { macroParamLocal[15] = (uint16_t)pos; }

// Arrow button callbacks (left)
static void pbMacroMapLeft0(void)  { scrollBarScrollLeft(MM_SB_BASE + 0,  1); }
static void pbMacroMapLeft1(void)  { scrollBarScrollLeft(MM_SB_BASE + 1,  1); }
static void pbMacroMapLeft2(void)  { scrollBarScrollLeft(MM_SB_BASE + 2,  1); }
static void pbMacroMapLeft3(void)  { scrollBarScrollLeft(MM_SB_BASE + 3,  1); }
static void pbMacroMapLeft4(void)  { scrollBarScrollLeft(MM_SB_BASE + 4,  1); }
static void pbMacroMapLeft5(void)  { scrollBarScrollLeft(MM_SB_BASE + 5,  1); }
static void pbMacroMapLeft6(void)  { scrollBarScrollLeft(MM_SB_BASE + 6,  1); }
static void pbMacroMapLeft7(void)  { scrollBarScrollLeft(MM_SB_BASE + 7,  1); }
static void pbMacroMapLeft8(void)  { scrollBarScrollLeft(MM_SB_BASE + 8,  1); }
static void pbMacroMapLeft9(void)  { scrollBarScrollLeft(MM_SB_BASE + 9,  1); }
static void pbMacroMapLeft10(void) { scrollBarScrollLeft(MM_SB_BASE + 10, 1); }
static void pbMacroMapLeft11(void) { scrollBarScrollLeft(MM_SB_BASE + 11, 1); }
static void pbMacroMapLeft12(void) { scrollBarScrollLeft(MM_SB_BASE + 12, 1); }
static void pbMacroMapLeft13(void) { scrollBarScrollLeft(MM_SB_BASE + 13, 1); }
static void pbMacroMapLeft14(void) { scrollBarScrollLeft(MM_SB_BASE + 14, 1); }
static void pbMacroMapLeft15(void) { scrollBarScrollLeft(MM_SB_BASE + 15, 1); }

// Arrow button callbacks (right)
static void pbMacroMapRight0(void)  { scrollBarScrollRight(MM_SB_BASE + 0,  1); }
static void pbMacroMapRight1(void)  { scrollBarScrollRight(MM_SB_BASE + 1,  1); }
static void pbMacroMapRight2(void)  { scrollBarScrollRight(MM_SB_BASE + 2,  1); }
static void pbMacroMapRight3(void)  { scrollBarScrollRight(MM_SB_BASE + 3,  1); }
static void pbMacroMapRight4(void)  { scrollBarScrollRight(MM_SB_BASE + 4,  1); }
static void pbMacroMapRight5(void)  { scrollBarScrollRight(MM_SB_BASE + 5,  1); }
static void pbMacroMapRight6(void)  { scrollBarScrollRight(MM_SB_BASE + 6,  1); }
static void pbMacroMapRight7(void)  { scrollBarScrollRight(MM_SB_BASE + 7,  1); }
static void pbMacroMapRight8(void)  { scrollBarScrollRight(MM_SB_BASE + 8,  1); }
static void pbMacroMapRight9(void)  { scrollBarScrollRight(MM_SB_BASE + 9,  1); }
static void pbMacroMapRight10(void) { scrollBarScrollRight(MM_SB_BASE + 10, 1); }
static void pbMacroMapRight11(void) { scrollBarScrollRight(MM_SB_BASE + 11, 1); }
static void pbMacroMapRight12(void) { scrollBarScrollRight(MM_SB_BASE + 12, 1); }
static void pbMacroMapRight13(void) { scrollBarScrollRight(MM_SB_BASE + 13, 1); }
static void pbMacroMapRight14(void) { scrollBarScrollRight(MM_SB_BASE + 14, 1); }
static void pbMacroMapRight15(void) { scrollBarScrollRight(MM_SB_BASE + 15, 1); }

// Callback arrays
static void (*const macroMapScrollCbs[16])(uint32_t) = {
    sbSetMacroMap0, sbSetMacroMap1, sbSetMacroMap2, sbSetMacroMap3,
    sbSetMacroMap4, sbSetMacroMap5, sbSetMacroMap6, sbSetMacroMap7,
    sbSetMacroMap8, sbSetMacroMap9, sbSetMacroMap10, sbSetMacroMap11,
    sbSetMacroMap12, sbSetMacroMap13, sbSetMacroMap14, sbSetMacroMap15
};
static void (*const macroMapLeftCbs[16])(void) = {
    pbMacroMapLeft0, pbMacroMapLeft1, pbMacroMapLeft2, pbMacroMapLeft3,
    pbMacroMapLeft4, pbMacroMapLeft5, pbMacroMapLeft6, pbMacroMapLeft7,
    pbMacroMapLeft8, pbMacroMapLeft9, pbMacroMapLeft10, pbMacroMapLeft11,
    pbMacroMapLeft12, pbMacroMapLeft13, pbMacroMapLeft14, pbMacroMapLeft15
};
static void (*const macroMapRightCbs[16])(void) = {
    pbMacroMapRight0, pbMacroMapRight1, pbMacroMapRight2, pbMacroMapRight3,
    pbMacroMapRight4, pbMacroMapRight5, pbMacroMapRight6, pbMacroMapRight7,
    pbMacroMapRight8, pbMacroMapRight9, pbMacroMapRight10, pbMacroMapRight11,
    pbMacroMapRight12, pbMacroMapRight13, pbMacroMapRight14, pbMacroMapRight15
};

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
        case MACRO_TARGET_TF4:   return "TF4";
        case MACRO_TARGET_DEXED: return "DX";
        case MACRO_TARGET_DSP:   return "DSP";
        default:                 return "---";
    }
}

static const char *getScopeLabel(uint8_t scope)
{
    return (scope == DSP_MACRO_SCOPE_MASTER) ? "MSTR" : "PAIR";
}

static int getDexedParamCount(void)
{
    const UnifiedSynthInterface *eng = ft2_unified_synth_get_engine(SYNTH_TYPE_DEXED);
    return (eng && eng->get_param_count) ? eng->get_param_count() : 0;
}

static const char *getDexedParamName(int paramId)
{
    const UnifiedSynthInterface *eng = ft2_unified_synth_get_engine(SYNTH_TYPE_DEXED);
    const int count = (eng && eng->get_param_count) ? eng->get_param_count() : 0;
    if (paramId < 0 || paramId >= count)
        return "Unknown";
    if (eng && eng->get_param_name)
    {
        const char *name = eng->get_param_name(paramId);
        return (name != NULL) ? name : "Unknown";
    }
    return "Unknown";
}

static const char *getMacroParamName(int slot)
{
    const uint8_t target = macroTargetLocal[slot];
    const uint16_t param = macroParamLocal[slot];

    if (target == MACRO_TARGET_TF4)
        return tf4_param_name((int)param);
    if (target == MACRO_TARGET_DEXED)
    {
        const int dCount = getDexedParamCount();
        if (dCount <= 0 || param >= (uint16_t)dCount)
            return "DX Param";
        return getDexedParamName((int)param);
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

    // Param controls visibility
    const bool showParams = (target != MACRO_TARGET_NONE);
    pushButtons[MM_PB_PARAM_BASE + slot * 2].visible = showParams;
    pushButtons[MM_PB_PARAM_BASE + slot * 2 + 1].visible = showParams;
    scrollBars[MM_SB_BASE + slot].visible = showParams;

    // Update param scrollbar range
    uint16_t end = 0;
    if (target == MACRO_TARGET_TF4)
    {
        if (tf4_param_name_count > 0)
            end = (uint16_t)(tf4_param_name_count - 1);
    }
    else if (target == MACRO_TARGET_DEXED)
    {
        const int dCount = getDexedParamCount();
        if (dCount > 0)
            end = (uint16_t)(dCount - 1);
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
        end = (pi && numParams > 0) ? (uint16_t)(numParams - 1) : 0;
    }

    setScrollBarEnd(MM_SB_BASE + slot, end);
    if (macroParamLocal[slot] > end)
        macroParamLocal[slot] = end;
    setScrollBarPos(MM_SB_BASE + slot, macroParamLocal[slot], false);

    (void)slot;
}

static void pbMacroMapTargetToggle(void)
{
    const int slot = (int)mouse.lastUsedObjectID - MM_PB_TARGET_BASE;
    if (slot < 0 || slot >= MM_NUM_SLOTS)
        return;

    switch (macroTargetLocal[slot])
    {
        case MACRO_TARGET_NONE:  macroTargetLocal[slot] = MACRO_TARGET_TF4; break;
        case MACRO_TARGET_TF4:   macroTargetLocal[slot] = MACRO_TARGET_DEXED; break;
        case MACRO_TARGET_DEXED: macroTargetLocal[slot] = MACRO_TARGET_DSP; break;
        default:                 macroTargetLocal[slot] = MACRO_TARGET_NONE; break;
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

// Initialize widgets (pushbuttons and scrollbars)
static void setupMacroMapBoxWidgets(void)
{
    const int16_t w = 380, h = 360;
    const int16_t x = (SCREEN_W - w) / 2;
    const int16_t y = (SCREEN_H - h) / 2;
    const int16_t arrowW = 13, arrowH = 13;
    const int16_t tgtW = 34, tgtH = 12;
    const int16_t scopeW = 34, scopeH = 12;
    const int16_t slotArrowW = 10, slotArrowH = 12;
    const int16_t slotWidth = (w - 32) / 2;  // 16px margin each side, 2 columns
    const int16_t paramStartOffset = 44; // space for target/scope/slot controls
    const int16_t scrollW = slotWidth - paramStartOffset - (arrowW * 2);
    const int16_t scrollH = arrowH;
    const int16_t rowH = 28;
    const int16_t rowsTop = 42;
    pushButton_t *p;
    scrollBar_t *s;

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

    // Arrows and scrollbars for each slot
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

        // Left arrow
        p = &pushButtons[MM_PB_PARAM_BASE + i * 2]; memset(p, 0, sizeof(*p));
        p->caption = ARROW_LEFT_STRING;
        p->x = sx + paramStartOffset; p->y = sy;
        p->w = arrowW; p->h = arrowH;
        p->preDelay = 1; p->delayFrames = 3;
        p->callbackFuncOnDown = macroMapLeftCbs[i];
        p->visible = (macroTargetLocal[i] != MACRO_TARGET_NONE);

        // Scrollbar
        s = &scrollBars[MM_SB_BASE + i]; memset(s, 0, sizeof(*s));
        s->x = sx + paramStartOffset + arrowW; s->y = sy;
        s->w = scrollW;  s->h = scrollH;
        s->type = SCROLLBAR_HORIZONTAL;
        s->thumbType = SCROLLBAR_FIXED_THUMB_SIZE;
        s->callbackFunc = macroMapScrollCbs[i];
        s->visible = (macroTargetLocal[i] != MACRO_TARGET_NONE);
        setScrollBarPageLength(MM_SB_BASE + i, 1);
        setScrollBarEnd(MM_SB_BASE + i, tf4_param_name_count - 1);
        setScrollBarPos(MM_SB_BASE + i, macroParamLocal[i], false);

        // Right arrow
        p = &pushButtons[MM_PB_PARAM_BASE + i * 2 + 1]; memset(p, 0, sizeof(*p));
        p->caption = ARROW_RIGHT_STRING;
        p->x = sx + paramStartOffset + arrowW + scrollW; p->y = sy;
        p->w = arrowW; p->h = arrowH;
        p->preDelay = 1; p->delayFrames = 3;
        p->callbackFuncOnDown = macroMapRightCbs[i];
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
    const int16_t w = 380;
    const int16_t h = 360;
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
        // Draw arrows, scrollbars, and parameter names
        {
            const int16_t boxW = 380, boxH = 360;
            const int16_t boxX = (SCREEN_W - boxW) / 2;
            const int16_t boxY = (SCREEN_H - boxH) / 2;
            const int16_t arrowW = 13;
            const int16_t slotWidth = (boxW - 32) / 2;  // margin 16px, 2 columns
            const int16_t paramStartOffset = 44;
            const int16_t scrollW = slotWidth - paramStartOffset - (arrowW * 2);
            const int16_t rowH = 28;
            const int16_t rowsTop = 42;
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
                drawPushButton(MM_PB_PARAM_BASE + i * 2);
                drawScrollBar(MM_SB_BASE + i);
                drawPushButton(MM_PB_PARAM_BASE + i * 2 + 1);
                /* Draw slot index and parameter name with tiny outlined font */
                char labelBuf[72];
                const uint8_t val = getMacroValueForInstrument(instID, i);
                const char *paramName = getMacroParamName(i);
                if (macroTargetLocal[i] == MACRO_TARGET_DSP)
                {
                    const char *scope = getScopeLabel(macroDspScopeLocal[i]);
                    snprintf(labelBuf, sizeof(labelBuf), "M%X %s S%u: %s  %02X",
                             i, scope, (unsigned)macroDspSlotLocal[i], paramName, val);
                }
                else
                {
                    const char *tgt = getTargetLabel(macroTargetLocal[i]);
                    snprintf(labelBuf, sizeof(labelBuf), "M%X %s: %s  %02X", i, tgt, paramName, val);
                }
                textOutTiny(sx + paramStartOffset + arrowW + 2, syTop + 15, labelBuf, PAL_BUTTON1);

            }
            // Track selector (bottom, global)
            drawPushButton(MM_PB_TRACK_BASE + 0);
            drawPushButton(MM_PB_TRACK_BASE + 1);
            drawPushButton(MM_PB_TRACK_BASE + 2);
            drawPushButton(MM_PB_TRACK_BASE + 3);
            textOutTiny(boxX + 16, boxY + boxH - 68, "Track", PAL_BUTTON1);
            textOutTiny(boxX + 46, boxY + boxH - 40, "Macro", PAL_BUTTON1);
        }
        flipFrame();
    }

    // Hide scrollbars
    for (int i = 0; i < MM_NUM_SLOTS; i++)
        hideScrollBar(MM_SB_BASE + i);

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
            ui_sync_from_instrument();
    
        }
    }
}
