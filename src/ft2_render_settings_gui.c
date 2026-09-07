/* Modal settings for WAV export and render-to-sample. */

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
    bool toSlot;
    uint8_t slotIndex;
    uint8_t bitDepth;
    uint32_t sampleRate;
} renderSettings_t;

static renderSettings_t renderSettingsLocal;

#define RS_PB_BASE PB_RENDER_SETTINGS_FIRST
static bool rsExitCancel;
static radioButton_t savedRadioButtons[4];

static void pbRSOk(void) { ui.sysReqShown = false; rsExitCancel = false; }
static void pbRSCancel(void) { ui.sysReqShown = false; rsExitCancel = true; }
static void pbRSSlotDec(void) { if (renderSettingsLocal.slotIndex > 0) renderSettingsLocal.slotIndex--; }
static void pbRSSlotInc(void) { if (renderSettingsLocal.slotIndex < MAX_SMP_PER_INST - 1) renderSettingsLocal.slotIndex++; }
static void pbRSRateDec(void)
{
    if (renderSettingsLocal.sampleRate == 384000) renderSettingsLocal.sampleRate = 192000;
    else if (renderSettingsLocal.sampleRate == 192000) renderSettingsLocal.sampleRate = 96000;
    else if (renderSettingsLocal.sampleRate == 96000) renderSettingsLocal.sampleRate = 48000;
    else if (renderSettingsLocal.sampleRate == 48000) renderSettingsLocal.sampleRate = 44100;
}
static void pbRSRateInc(void)
{
    if (renderSettingsLocal.sampleRate == 44100) renderSettingsLocal.sampleRate = 48000;
    else if (renderSettingsLocal.sampleRate == 48000) renderSettingsLocal.sampleRate = 96000;
    else if (renderSettingsLocal.sampleRate == 96000) renderSettingsLocal.sampleRate = 192000;
    else if (renderSettingsLocal.sampleRate == 192000) renderSettingsLocal.sampleRate = 384000;
}

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
    p->caption = ARROW_LEFT_STRING; p->x = x + 80; p->y = y + 64; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSSlotDec; p->visible = renderSettingsLocal.toSlot;

    // Slot index increment
    p = &pushButtons[RS_PB_BASE + 3]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_RIGHT_STRING; p->x = x + 128; p->y = y + 64; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSSlotInc; p->visible = renderSettingsLocal.toSlot;

    // Sample rate decrement
    p = &pushButtons[RS_PB_BASE + 4]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_LEFT_STRING; p->x = x + 120; p->y = y + 112; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSRateDec; p->visible = true;

    // Sample rate increment
    p = &pushButtons[RS_PB_BASE + 5]; memset(p, 0, sizeof(*p));
    p->caption = ARROW_RIGHT_STRING; p->x = x + 200; p->y = y + 112; p->w = 13; p->h = 13; p->callbackFuncOnDown = pbRSRateInc; p->visible = true;

    // These radio IDs are also used by the underlying WAV screen. Save and
    // restore their complete definitions so the modal cannot corrupt it.
    memcpy(savedRadioButtons, &radioButtons[RB_WAV_RENDER_BITDEPTH16], sizeof(savedRadioButtons));
    radioButtons[RB_WAV_RENDER_BITDEPTH16].x = x + 120;
    radioButtons[RB_WAV_RENDER_BITDEPTH16].y = y + 87;
    radioButtons[RB_WAV_RENDER_BITDEPTH16].clickAreaWidth = 55;
    radioButtons[RB_WAV_RENDER_BITDEPTH32].x = x + 185;
    radioButtons[RB_WAV_RENDER_BITDEPTH32].y = y + 87;
    radioButtons[RB_WAV_RENDER_BITDEPTH32].clickAreaWidth = 55;
    radioButtons[RB_WAV_RENDER_TARGET_FILE].x = x + 120;
    radioButtons[RB_WAV_RENDER_TARGET_FILE].y = y + 39;
    radioButtons[RB_WAV_RENDER_TARGET_FILE].clickAreaWidth = 70;
    radioButtons[RB_WAV_RENDER_TARGET_SLOT].x = x + 200;
    radioButtons[RB_WAV_RENDER_TARGET_SLOT].y = y + 39;
    radioButtons[RB_WAV_RENDER_TARGET_SLOT].clickAreaWidth = 80;

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
    textOutShadow(x+16,  y+40,  PAL_FORGRND, PAL_DSKTOP2, "Destination:");
    textOutShadow(x+16,  y+64,  PAL_FORGRND, PAL_DSKTOP2, "Sample Slot:");
    textOutShadow(x+16,  y+88, PAL_FORGRND, PAL_DSKTOP2, "Bit Depth:");
    textOutShadow(x+16,  y+112, PAL_FORGRND, PAL_DSKTOP2, "Sample Rate:");

    // Values
    textOutShadow(x+134, y+40, PAL_FORGRND, PAL_DSKTOP2, "File");
    textOutShadow(x+214, y+40, PAL_FORGRND, PAL_DSKTOP2, "Sample");
    textOutShadow(x+134, y+88, PAL_FORGRND, PAL_DSKTOP2, "16-bit");
    textOutShadow(x+199, y+88, PAL_FORGRND, PAL_DSKTOP2, "32-bit float");
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%u Hz", renderSettingsLocal.sampleRate);
    textOutShadow(x+137, y+112, PAL_FORGRND, PAL_DSKTOP2, tmp);

    // Buttons
    drawPushButton(RS_PB_BASE);
    drawPushButton(RS_PB_BASE + 1);

    // Slot controls
    pushButtons[RS_PB_BASE + 2].visible = renderSettingsLocal.toSlot;
    pushButtons[RS_PB_BASE + 3].visible = renderSettingsLocal.toSlot;
    if (renderSettingsLocal.toSlot)
    {
        drawPushButton(RS_PB_BASE + 2);
        char idxBuf[8];
        snprintf(idxBuf, sizeof(idxBuf), "%d", renderSettingsLocal.slotIndex);
        textOutShadow(x + 96, y + 64, PAL_FORGRND, PAL_DSKTOP2, idxBuf);
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
    /* Treat every non-OK exit, including Escape and application shutdown, as
     * cancellation. The OK callback is the only path that commits settings. */
    rsExitCancel = true;
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
    memcpy(&radioButtons[RB_WAV_RENDER_BITDEPTH16], savedRadioButtons, sizeof(savedRadioButtons));
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

#ifdef FT2_STABILITY_TESTS
bool runRenderSettingsRegressionTests(void)
{
    const renderSettings_t saved = renderSettingsLocal;

    renderSettingsLocal.slotIndex = 0;
    pbRSSlotDec();
    if (renderSettingsLocal.slotIndex != 0) goto fail;
    renderSettingsLocal.slotIndex = MAX_SMP_PER_INST - 1;
    pbRSSlotInc();
    if (renderSettingsLocal.slotIndex != MAX_SMP_PER_INST - 1) goto fail;

    renderSettingsLocal.sampleRate = 44100;
    pbRSRateInc();
    pbRSRateInc();
    pbRSRateInc();
    pbRSRateInc();
    pbRSRateInc();
    if (renderSettingsLocal.sampleRate != 384000) goto fail;
    pbRSRateDec();
    pbRSRateDec();
    pbRSRateDec();
    pbRSRateDec();
    pbRSRateDec();
    if (renderSettingsLocal.sampleRate != 44100) goto fail;

    pbRSBitDepth16();
    if (renderSettingsLocal.bitDepth != 16) goto fail;
    pbRSBitDepth32();
    if (renderSettingsLocal.bitDepth != 32) goto fail;
    pbRSTargetSlot();
    if (!renderSettingsLocal.toSlot) goto fail;
    pbRSTargetFile();
    if (renderSettingsLocal.toSlot) goto fail;

    renderSettingsLocal = saved;
    return true;

fail:
    renderSettingsLocal = saved;
    return false;
}
#endif
