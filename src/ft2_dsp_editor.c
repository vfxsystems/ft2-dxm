#include "ft2_dsp_editor.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_pushbuttons.h"
#include "ft2_structs.h"
#include "ft2_mixer.h"
#include "ft2_dsp.h"
#include "ft2_mouse.h"
#include "ft2_audio.h"
#include "ft2_mixer_layout_schema.h"
#include "shared/ft2_ui_assets.h"
#include "ft2_bmp.h"
#include <string.h>

static uint8_t editChannel = 0;

/* UI constants */
#define DSP_WIN_X  64
#define DSP_WIN_Y  24
#define DSP_WIN_W  192
#define DSP_WIN_H  120

static int dspSlotH = 14;
static int dspMenuItemH = 16;
static int dspMenuW = 80;
static int dspParamRowH = 18;
static int dspParamTrackH = 10;

/* Effect name table corresponding to dspEffectType_t order */
static const char *effectNames[DSP_TYPE_COUNT] = {
    "Empty", "Gainer", "Comp", "Limiter", "Delay", "Reverb",
    "Chorus", "Flanger", "Phaser", "Drive", "AmpSim", "EQ",
    "Filter", "Comb", "Bitcrush"
};

static bool menuShown = false;
static int menuSlot = -1;
static void dspApplyLayoutMetrics(void)
{
    const ft2_ui_layout_desc_t *desc = &ft2_mixer_layout_layout;
    if (!desc || desc->version != FT2_UI_SCHEMA_VERSION)
        return;

    if (desc->dsp_slots.count > 0 && desc->dsp_slot_desc)
    {
        if (desc->dsp_slot_desc[0].h > 0)
            dspSlotH = desc->dsp_slot_desc[0].h;
        dspMenuItemH = dspSlotH;
    }
    if (desc->dsp_menus.count > 0 && desc->dsp_menu_desc && desc->dsp_menu_desc[0].w > 0)
        dspMenuW = desc->dsp_menu_desc[0].w;
    if (desc->dsp_params.count > 0 && desc->dsp_param_desc && desc->dsp_param_desc[0].h > 0)
        dspParamTrackH = desc->dsp_param_desc[0].h;
    if (desc->dsp_params.count > 1 && desc->dsp_param_desc)
    {
        int row_h = (int)desc->dsp_param_desc[1].y - (int)desc->dsp_param_desc[0].y;
        if (row_h > 0)
            dspParamRowH = row_h;
    }
}

static int selectedSlot = -1;

typedef struct
{
    bool valid;
    uint16_t id;
    uint8_t *pixels;
    int32_t w, h;
} DspBitmapCache;

static DspBitmapCache dspBitmapCache[32];

static void pbDspClose(void)
{
    hideDspEditor();
}

static void pbGainUp(void)
{
    if (selectedSlot < 0) return;
    dspEffectInstance_t *eff = (editChannel == 255) ? &masterEffects[selectedSlot] : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(editChannel)].effects[selectedSlot];
    if (eff->type == DSP_TYPE_GAINER)
    {
        eff->params.gainer.gain = dspClampf(eff->params.gainer.gain + 0.1f, 0.0f, 2.0f);
    }
}

static void pbGainDown(void)
{
    if (selectedSlot < 0) return;
    dspEffectInstance_t *eff = (editChannel == 255) ? &masterEffects[selectedSlot] : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(editChannel)].effects[selectedSlot];
    if (eff->type == DSP_TYPE_GAINER)
    {
        eff->params.gainer.gain = dspClampf(eff->params.gainer.gain - 0.1f, 0.0f, 2.0f);
    }
}

void showChannelDspEditor(uint8_t channelIdx)
{
    if (ui.dspEditorShown)
        return;

    dspApplyLayoutMetrics();

    if (channelIdx >= MAX_CHANNELS && channelIdx != 255)
        channelIdx = 0;

    editChannel = channelIdx;

    /* Modal behavior like other tools */
    ui.sysReqShown = true;
    hideTopScreen();

    pushButton_t *pb = &pushButtons[PB_RES_6];
    memset(pb, 0, sizeof (pushButton_t));
    pb->x = DSP_WIN_X + DSP_WIN_W - 38;
    pb->y = DSP_WIN_Y + DSP_WIN_H - 16;
    pb->w = 32;
    pb->h = 14;
    pb->caption = "Close";
    pb->callbackFuncOnUp = pbDspClose;
    pb->visible = true;

    ui.dspEditorShown = true;
    dspEditorFrame();
}

void hideDspEditor(void)
{
    if (!ui.dspEditorShown)
        return;

    hidePushButton(PB_RES_6);
    fillRect(DSP_WIN_X, DSP_WIN_Y, DSP_WIN_W, DSP_WIN_H, PAL_DESKTOP);

    ui.dspEditorShown = false;
    ui.sysReqShown = false;
    showTopScreen(true);
}

// Return human-readable name for effect slot
const char *getEffectName(dspEffectInstance_t *e)
{
    dspEffectType_t t = e->type;
    if (t >= 0 && t < DSP_TYPE_COUNT)
        return effectNames[t];
    return "Unknown";
}

static void showMenu(int slot)
{
    menuShown = true;
    menuSlot = slot;
}

static const ft2_ui_bitmap_asset_t *dsp_find_bitmap_asset(uint16_t id)
{
    for (uint16_t i = 0; i < ft2_ui_assets.bitmap_count; i++)
    {
        if (ft2_ui_assets.bitmaps[i].id == id)
            return &ft2_ui_assets.bitmaps[i];
    }
    return NULL;
}

static uint8_t *dsp_get_bitmap_pixels(uint16_t id, int32_t *w, int32_t *h)
{
    for (size_t i = 0; i < sizeof(dspBitmapCache) / sizeof(dspBitmapCache[0]); i++)
    {
        if (dspBitmapCache[i].valid && dspBitmapCache[i].id == id)
        {
            if (w) *w = dspBitmapCache[i].w;
            if (h) *h = dspBitmapCache[i].h;
            return dspBitmapCache[i].pixels;
        }
    }

    const ft2_ui_bitmap_asset_t *asset = dsp_find_bitmap_asset(id);
    if (!asset || !asset->bmp || asset->fmt != FT2_UI_BMP_FMT_RLE4)
        return NULL;

    int32_t bmp_w = 0, bmp_h = 0;
    uint8_t *pixels = ft2_bmp_decode_rle4_to_pal(asset->bmp, &bmp_w, &bmp_h);
    if (!pixels) return NULL;

    for (size_t i = 0; i < sizeof(dspBitmapCache) / sizeof(dspBitmapCache[0]); i++)
    {
        if (!dspBitmapCache[i].valid)
        {
            dspBitmapCache[i].valid = true;
            dspBitmapCache[i].id = id;
            dspBitmapCache[i].pixels = pixels;
            dspBitmapCache[i].w = bmp_w;
            dspBitmapCache[i].h = bmp_h;
            break;
        }
    }

    if (w) *w = bmp_w;
    if (h) *h = bmp_h;
    return pixels;
}

static void dsp_draw_schema_bitmaps(void)
{
    const ft2_ui_layout_desc_t *desc = &ft2_mixer_layout_layout;
    if (!desc || desc->version != FT2_UI_SCHEMA_VERSION)
        return;
    if (desc->bitmaps.count == 0 || !desc->bitmap_desc)
        return;

    for (uint16_t i = 0; i < desc->bitmaps.count; i++)
    {
        const ft2_ui_bitmap_desc_t *d = &desc->bitmap_desc[i];
        int32_t w = 0, h = 0;
        uint8_t *pixels = dsp_get_bitmap_pixels(d->bitmap_id, &w, &h);
        if (!pixels || w <= 0 || h <= 0)
            continue;

        // only draw if bitmap intersects DSP window bounds
        if ((int32_t)d->x + w <= DSP_WIN_X || d->x >= DSP_WIN_X + DSP_WIN_W ||
            (int32_t)d->y + h <= DSP_WIN_Y || d->y >= DSP_WIN_Y + DSP_WIN_H)
            continue;

        blit(d->x, d->y, pixels, (uint16_t)w, (uint16_t)h);
    }
}

static void hideMenu(void)
{
    menuShown = false;
    menuSlot = -1;
}

static void drawMenu(void)
{
    if (!menuShown) return;
    uint16_t x = DSP_WIN_X + DSP_WIN_W - dspMenuW;
    uint16_t y = DSP_WIN_Y + 16 + (menuSlot * dspSlotH);
    uint16_t items = DSP_TYPE_COUNT;
    uint16_t h = items * dspMenuItemH + 4;
    drawFramework(x, y, dspMenuW, h, FRAMEWORK_TYPE1);
    for (uint16_t i = 0; i < items; i++)
    {
        uint16_t iy = y + 4 + i * dspMenuItemH;
        textOut(x + 4, iy, PAL_FORGRND, effectNames[i]);
    }
}

static void handleMenuClick(void)
{
    if (!menuShown || !mouse.leftButtonPressed) return;
    uint16_t x = DSP_WIN_X + DSP_WIN_W - dspMenuW;
    uint16_t y = DSP_WIN_Y + 16 + (menuSlot * dspSlotH);
    uint16_t items = DSP_TYPE_COUNT;
    uint16_t h = items * dspMenuItemH + 4;
    if (mouse.x >= x && mouse.x < x + dspMenuW && mouse.y >= y && mouse.y < y + h)
    {
        uint16_t idx = (mouse.y - y - 2) / dspMenuItemH;
        if (idx < items)
        {
            dspEffectType_t type = (dspEffectType_t)idx;
            dspEffectInstance_t *eff = (editChannel == 255) ? &masterEffects[menuSlot] : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(editChannel)].effects[menuSlot];
            dspFreeEffect(eff);
            if (type != DSP_TYPE_NONE)
                dspInitEffect(eff, type, audio.freq);
            else
                eff->type = DSP_TYPE_NONE;
        }
        mouse.leftButtonReleased = true;
        hideMenu();
    }
    else if (mouse.leftButtonPressed)
    {
        // clicked outside menu
        hideMenu();
    }
}

/* modify handleClicks */
static void handleClicks(void)
{
    if (!mouse.leftButtonPressed) return;
    /* First check if clicking on slot opens menu */
    for (int i = 0; i < DSP_MAX_SLOTS; i++)
    {
        uint16_t y = DSP_WIN_Y + 16 + (i * dspSlotH);
        uint16_t x = DSP_WIN_X + 6;
        uint16_t w = DSP_WIN_W - 12;
        if (mouse.x >= x && mouse.x < x + w && mouse.y >= y && mouse.y < y + dspSlotH)
        {
            showMenu(i);
            selectedSlot = i;
            mouse.leftButtonReleased = true;
            return;
        }
    }
}

// Return pointer to the parameter value by index for the given effect
float *getParamPtr(dspEffectInstance_t *eff, int idx)
{
    switch (eff->type)
    {
    case DSP_TYPE_GAINER:
        if (idx == 0) return &eff->params.gainer.gain;
        break;
    case DSP_TYPE_DELAY:
        switch (idx)
        {
        case 0: return &eff->params.delay.timeMs;
        case 1: return &eff->params.delay.feedback;
        case 2: return &eff->params.delay.tone;
        case 3: return &eff->params.delay.mix;
        case 4: return &eff->params.delay.diffusionMs;
        case 5: return &eff->params.delay.diffusionMix;
        case 6: return &eff->params.delay.pingPong;
        case 7: return &eff->params.delay.character;
        case 8: return &eff->params.delay.sync;
        case 9: return &eff->params.delay.width;
        }
        break;
    case DSP_TYPE_COMPRESSOR:
        switch (idx)
        {
        case 0: return &eff->params.compressor.thresholdDb;
        case 1: return &eff->params.compressor.ratio;
        case 2: return &eff->params.compressor.attackMs;
        case 3: return &eff->params.compressor.releaseMs;
        case 4: return &eff->params.compressor.makeupDb;
        }
        break;
    case DSP_TYPE_LIMITER:
        switch (idx)
        {
        case 0: return &eff->params.limiter.thresholdDb;
        case 1: return &eff->params.limiter.releaseMs;
        }
        break;
    case DSP_TYPE_DRIVE:
        switch (idx)
        {
        case 0: return &eff->params.drive.gain;
        case 1: return &eff->params.drive.tone;
        case 2: return &eff->params.drive.mix;
        }
        break;
    case DSP_TYPE_REVERB:
        switch (idx)
        {
        case 0: return &eff->params.reverb.sizeMs;
        case 1: return &eff->params.reverb.feedback;
        case 2: return &eff->params.reverb.damp;
        case 3: return &eff->params.reverb.mix;
        case 4: return &eff->params.reverb.character;
        case 5: return &eff->params.reverb.convMix;
        case 6: return &eff->params.reverb.irIndex;
        }
        break;
    case DSP_TYPE_CHORUS:
        switch (idx)
        {
        case 0: return &eff->params.chorus.depthMs;
        case 1: return &eff->params.chorus.rateHz;
        case 2: return &eff->params.chorus.feedback;
        case 3: return &eff->params.chorus.mix;
        case 4: return &eff->params.chorus.stereoOffset;
        }
        break;
    case DSP_TYPE_FLANGER:
        switch (idx)
        {
        case 0: return &eff->params.flanger.depthMs;
        case 1: return &eff->params.flanger.rateHz;
        case 2: return &eff->params.flanger.feedback;
        case 3: return &eff->params.flanger.mix;
        }
        break;
    case DSP_TYPE_PHASER:
        switch (idx)
        {
        case 0: return &eff->params.phaser.depth;
        case 1: return &eff->params.phaser.rateHz;
        case 2: return &eff->params.phaser.mix;
        case 3: return &eff->params.phaser.feedback;
        case 4: return &eff->params.phaser.stereoOffset;
        }
        break;
    case DSP_TYPE_AMP_SIM:
        switch (idx)
        {
        case 0: return &eff->params.ampSim.drive;
        case 1: return &eff->params.ampSim.tone;
        case 2: return &eff->params.ampSim.mix;
        }
        break;
    case DSP_TYPE_EQ_5BAND:
        if (idx >= 0 && idx < 5)
            return &eff->params.eq5.gains[idx];
        break;
    case DSP_TYPE_FILTER:
        switch (idx)
        {
        case 0: return &eff->params.filter.cutoffHz;
        case 1: return &eff->params.filter.resonance;
        case 2: return &eff->params.filter.mode;
        }
        break;
    case DSP_TYPE_COMB_FILTER:
        switch (idx)
        {
        case 0: return &eff->params.comb.timeMs;
        case 1: return &eff->params.comb.feedback;
        case 2: return &eff->params.comb.mix;
        }
        break;
    case DSP_TYPE_BITCRUSHER:
        switch (idx)
        {
        case 0: return &eff->params.bitcrusher.rateHz;
        case 1: return &eff->params.bitcrusher.bitDepth;
        }
        break;
    default:
        break;
    }
    return NULL;
}

static void drawParamRows(dspEffectInstance_t *eff)
{
    int numParams = 0;
    const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
    if (!pi) return;
    for (int i = 0; i < numParams; i++)
    {
        uint16_t y = DSP_WIN_Y + 70 + i * dspParamRowH;
        // parameter name
        textOut(DSP_WIN_X + 8, y, PAL_FORGRND, pi[i].name);
        // slider track
        uint16_t tx = DSP_WIN_X + 80;
        uint16_t tw = DSP_WIN_W - 90;
        // draw slider track background and border
        drawFramework(tx, y + 6, tw, (uint16_t)dspParamTrackH, FRAMEWORK_TYPE2);
        // thumb position
        float val = *getParamPtr(eff, i);
        float norm = (val - pi[i].min) / (pi[i].max - pi[i].min);
        if (norm < 0) norm = 0; else if (norm > 1) norm = 1;
        uint16_t pos = tx + (uint16_t)(norm * tw);
        fillRect(pos - 2, y + 4, 4, 8, PAL_FORGRND);
        // value text
        char vtxt[16]; sprintf(vtxt, "%.2f", val);
        textOut(tx + tw + 4, y, PAL_FORGRND, vtxt);
    }
}

static void handleParamClick(dspEffectInstance_t *eff)
{
    int numParams = 0;
    const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
    if (!pi) return;
    if (!mouse.leftButtonPressed) return;
    for (int i = 0; i < numParams; i++)
    {
        uint16_t y = DSP_WIN_Y + 70 + i * dspParamRowH;
        uint16_t tx = DSP_WIN_X + 80;
        uint16_t tw = DSP_WIN_W - 90;
        if (mouse.y >= y + 6 && mouse.y <= y + 6 + dspParamTrackH && mouse.x >= tx && mouse.x <= tx + tw)
        {
            float norm = (float)(mouse.x - tx) / (float)tw;
            if (norm < 0) norm = 0; else if (norm > 1) norm = 1;
            float newVal = pi[i].min + norm * (pi[i].max - pi[i].min);
            float *p = getParamPtr(eff, i);
            if (p) *p = newVal;
            mouse.leftButtonReleased = true;
            return;
        }
    }
}

void dspEditorFrame(void)
{
    if (!ui.dspEditorShown) return;

    handleMenuClick();
    handleClicks();

    dsp_draw_schema_bitmaps();

    /* Draw window */
    drawFramework(DSP_WIN_X, DSP_WIN_Y, DSP_WIN_W, DSP_WIN_H, FRAMEWORK_TYPE1);

    char title[32];
    if (editChannel == 255)
        sprintf(title, "DSP - Master");
    else
        sprintf(title, "DSP - Pair %02d", CHANNEL_TO_PAIR_IDX(editChannel) + 1);
    textOut(DSP_WIN_X + 4, DSP_WIN_Y + 3, PAL_FORGRND, title);

    /* List slots */
    for (int i = 0; i < DSP_MAX_SLOTS; i++)
    {
        const uint16_t y = DSP_WIN_Y + 16 + (i * dspSlotH);
        drawFramework(DSP_WIN_X + 6, y, DSP_WIN_W - 12, dspSlotH, FRAMEWORK_TYPE2);

        dspEffectInstance_t *eff = (editChannel == 255) ? &masterEffects[i] : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(editChannel)].effects[i];
        const char *name = getEffectName(eff);
        textOut(DSP_WIN_X + 10, y + 2, PAL_FORGRND, name);
    }

    // Close button will be drawn last for proper Z-order

    if (selectedSlot >=0)
    {
        dspEffectInstance_t *eff = (editChannel == 255) ? &masterEffects[selectedSlot] : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(editChannel)].effects[selectedSlot];
        drawParamRows(eff);
        handleParamClick(eff);
    }

    /* Draw effect selection menu on top */
    drawMenu();

    /* Now draw the Close button on top of everything */
    drawPushButton(PB_RES_6);
} 
