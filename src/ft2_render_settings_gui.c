/* ft2_render_settings_gui.c
 * Minimal placeholder implementation for a future fully-featured
 * advanced render settings dialog.
 *
 * Currently it just pops up an okBox so that the new Settings push-button
 * works and the codebase continues compiling. The proper modal dialog with
 * widgets can be added incrementally without blocking other work.
 */

#include <stdbool.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include <stdint.h>
#include <stdio.h>
#include "ft2_wav_renderer.h"
#include "ft2_structs.h"
#include "ft2_events.h"
#include "ft2_pushbuttons.h"
#include "ft2_mouse.h"

typedef struct {
    bool postDSP;
    bool toSlot;
    uint8_t slotIndex;
    uint8_t bitDepth;
    uint32_t sampleRate;
} renderSettings_t;

static renderSettings_t renderSettingsLocal;

#include "ft2_pushbuttons.h"
#include "ft2_events.h"

#define RS_PB_BASE (NUM_PUSHBUTTONS - 6) // OK/Cancel reserved
static bool rsExitCancel;

static void pbRSOk(void) { ui.sysReqShown = false; rsExitCancel = false; }
static void pbRSCancel(void) { ui.sysReqShown = false; rsExitCancel = true; }
static void pbRSSlotDec(void) { if (renderSettingsLocal.slotIndex > 0) renderSettingsLocal.slotIndex--; }
static void pbRSSlotInc(void) { if (renderSettingsLocal.slotIndex < MAX_SMP_PER_INST - 1) renderSettingsLocal.slotIndex++; }
static void pbRSRateDec(void) { if (renderSettingsLocal.sampleRate > MIN_WAV_RENDER_FREQ) renderSettingsLocal.sampleRate -= MIN_WAV_RENDER_FREQ; }
static void pbRSRateInc(void) { if (renderSettingsLocal.sampleRate < MAX_WAV_RENDER_FREQ) renderSettingsLocal.sampleRate += MIN_WAV_RENDER_FREQ; }

static void pbRSBitDepth16(void);
static void pbRSBitDepth32(void);
static void pbRSTargetFile(void);
static void pbRSTargetSlot(void);

static void setupRenderSettingsDialogWidgets(void)
{
    int16_t w = 300, h = 180;
    int16_t x = (SCREEN_W - w) / 2;
    int16_t y = (SCREEN_H - h) / 2;
    pushButton_t *p;

    // OK button
    p = &pushButtons[RS_PB_BASE]; memset(p, 0, sizeof(*p));
    p->x = x + 84;   p->y = y + 152; p->w = 45; p->h = 12;
    p->caption = "OK"; p->callbackFuncOnDown = pbRSOk; p->visible = true;

    // Cancel button
    p = &pushButtons[RS_PB_BASE + 1]; memset(p, 0, sizeof(*p));
    p->x = x + 168;  p->y = y + 152; p->w = 45; p->h = 12;
    p->caption = "Cancel"; p->callbackFuncOnDown = pbRSCancel; p->visible = true;

    // Slot index decrement
    p = &pushButtons[RS_PB_BASE + 2]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_LEFT_STRING; p->x = x + 80; p->y = y + 80; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSSlotDec; p->visible = true;

    // Slot index increment
    p = &pushButtons[RS_PB_BASE + 3]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_RIGHT_STRING; p->x = x + 128; p->y = y + 80; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSSlotInc; p->visible = true;

    // Sample rate decrement
    p = &pushButtons[RS_PB_BASE + 4]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_LEFT_STRING; p->x = x + 120; p->y = y + 128; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSRateDec; p->visible = true;

    // Sample rate increment
    p = &pushButtons[RS_PB_BASE + 5]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_RIGHT_STRING; p->x = x + 168; p->y = y + 128; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSRateInc; p->visible = true;

    // Radio buttons for Render Settings Dialog
    // Bit depth options
    radioButtons[RB_WAV_RENDER_BITDEPTH16].callbackFunc = pbRSBitDepth16;
    radioButtons[RB_WAV_RENDER_BITDEPTH32].callbackFunc = pbRSBitDepth32;
    uncheckRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);
    if (renderSettingsLocal.bitDepth == 16) checkRadioButton(RB_WAV_RENDER_BITDEPTH16);
    else checkRadioButton(RB_WAV_RENDER_BITDEPTH32);
    showRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);

    // Target options
    radioButtons[RB_WAV_RENDER_TARGET_FILE].callbackFunc = pbRSTargetFile;
    radioButtons[RB_WAV_RENDER_TARGET_SLOT].callbackFunc = pbRSTargetSlot;
    uncheckRadioButtonGroup(RB_GROUP_WAV_RENDER_TARGET);
    if (renderSettingsLocal.toSlot) checkRadioButton(RB_WAV_RENDER_TARGET_SLOT);
    else checkRadioButton(RB_WAV_RENDER_TARGET_FILE);
    showRadioButtonGroup(RB_GROUP_WAV_RENDER_TARGET);
}

static void drawRenderSettingsBox(void)
{
    int16_t w = 300, h = 180;
    int16_t x = (SCREEN_W - w) / 2;
    int16_t y = (SCREEN_H - h) / 2;

    // Frame
    fillRect(x+1, y+1, w-2, h-2, PAL_BUTTONS);
    vLine(x,     y,     h-1, PAL_BUTTON1);
    hLine(x+1,   y,     w-2, PAL_BUTTON1);
    vLine(x+w-1, y,     h,   PAL_BUTTON2);
    hLine(x,     y+h-1, w-1, PAL_BUTTON2);
    vLine(x+2,   y+2,   h-5, PAL_BUTTON2);
    hLine(x+3,   y+2,   w-6, PAL_BUTTON2);
    vLine(x+w-3, y+2,   h-4, PAL_BUTTON1);
    hLine(x+2,   y+h-3, w-4, PAL_BUTTON1);

    // Title
    textOutShadow(x+16, y+8,   PAL_FORGRND, PAL_BUTTON2, "Render Settings");

    // Labels
    textOutShadow(x+16,  y+32,  PAL_FORGRND, PAL_DSKTOP2, "Signal Path:");
    textOutShadow(x+16,  y+56,  PAL_FORGRND, PAL_DSKTOP2, "Destination:");
    textOutShadow(x+16,  y+104, PAL_FORGRND, PAL_DSKTOP2, "Bit Depth:");
    textOutShadow(x+16,  y+128, PAL_FORGRND, PAL_DSKTOP2, "Sample Rate:");

    // Values
    textOutShadow(x+120, y+32,  PAL_FORGRND, PAL_DSKTOP2,
        renderSettingsLocal.postDSP ? "Post-DSP" : "Pre-Fade");
    textOutShadow(x+120, y+56,  PAL_FORGRND, PAL_DSKTOP2,
        renderSettingsLocal.toSlot ? "Sample Slot" : "Disk File");
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%u-bit", renderSettingsLocal.bitDepth);
    textOutShadow(x+120, y+104, PAL_FORGRND, PAL_DSKTOP2, tmp);
    snprintf(tmp, sizeof(tmp), "%u Hz", renderSettingsLocal.sampleRate);
    textOutShadow(x+120, y+128, PAL_FORGRND, PAL_DSKTOP2, tmp);

    // Buttons
    drawPushButton(RS_PB_BASE);
    drawPushButton(RS_PB_BASE + 1);

    // Slot controls
    if (renderSettingsLocal.toSlot)
    {
        drawPushButton(RS_PB_BASE + 2);
        char idxBuf[8];
        snprintf(idxBuf, sizeof(idxBuf), "%d", renderSettingsLocal.slotIndex);
        textOutShadow(x + 96, y + 80, PAL_FORGRND, PAL_DSKTOP2, idxBuf);
        drawPushButton(RS_PB_BASE + 3);
    }

    // Sample rate controls
    drawPushButton(RS_PB_BASE + 4);
    drawPushButton(RS_PB_BASE + 5);

    // Radio buttons for Render Settings Dialog
    drawRadioButton(RB_WAV_RENDER_BITDEPTH16);
    drawRadioButton(RB_WAV_RENDER_BITDEPTH32);
    drawRadioButton(RB_WAV_RENDER_TARGET_FILE);
    drawRadioButton(RB_WAV_RENDER_TARGET_SLOT);
}

static void resetRenderSettingsLocal(void)
{
    renderSettingsLocal.postDSP = false;
    renderSettingsLocal.toSlot = WDRenderToSlot;
    renderSettingsLocal.slotIndex = editor.curSmp;
    renderSettingsLocal.bitDepth = WDBitDepth;
    renderSettingsLocal.sampleRate = WDFrequency; 

}

static void applyRenderSettingsLocal(void)
{
    WDRenderToSlot = renderSettingsLocal.toSlot;
    WDBitDepth = renderSettingsLocal.bitDepth;
    WDFrequency = renderSettingsLocal.sampleRate;
    editor.curSmp = renderSettingsLocal.slotIndex;
}

// Radio button callbacks for Render Settings Dialog
static void pbRSBitDepth16(void) { renderSettingsLocal.bitDepth = 16; }
static void pbRSBitDepth32(void) { renderSettingsLocal.bitDepth = 32; }
static void pbRSTargetFile(void) { renderSettingsLocal.toSlot = false; }
static void pbRSTargetSlot(void) { renderSettingsLocal.toSlot = true; }

void showRenderSettingsDialog(void)
{
    resetRenderSettingsLocal();
    rsExitCancel = false;
    setupRenderSettingsDialogWidgets();

    // Open modal dialog
    unstuckLastUsedGUIElement();
    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
    ui.sysReqShown = true;
    bool prevLeftMousePressed = false;

    while (ui.sysReqShown)
    {
        readInput();
        if (ui.sysReqEnterPressed)
            pbRSOk();

        // Mouse input handling for render settings dialog
        bool currLeftPressed = mouse.leftButtonPressed;
        if (currLeftPressed && !prevLeftMousePressed)
        {
            // initial mouse down
            testPushButtonMouseDown();
            testRadioButtonMouseDown();
        }
        else if (currLeftPressed)
        {
            // handle hold
            handleLastGUIObjectDown();
        }
        else if (!currLeftPressed && prevLeftMousePressed)
        {
            // mouse release
            testPushButtonMouseRelease(true);
            testRadioButtonMouseRelease();
        }
        prevLeftMousePressed = currLeftPressed;

        handleRedrawing();
        drawRenderSettingsBox();
        flipFrame();
    }

        // Hide widgets for Render Settings Dialog
    for (int i = 0; i < 6; i++)
        hidePushButton(RS_PB_BASE + i);
    hideRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);
    hideRadioButtonGroup(RB_GROUP_WAV_RENDER_TARGET);
    // Close modal dialog
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    mouseAnimOff();
    showTopScreen(true);
    showBottomScreen();
    ui.updatePatternEditor = true;
    ui.updatePosSections  = true;

    if (!rsExitCancel)
    {
        applyRenderSettingsLocal();
        pbWavRender();
    }
}

