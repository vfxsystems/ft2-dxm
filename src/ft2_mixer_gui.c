#include <string.h>
#include <stdio.h>
#include <math.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_pushbuttons.h"
#include "ft2_video.h"
#include "ft2_mouse.h"
#include "ft2_events.h"
#include "ft2_mixer.h"
#include "ft2_mixer_gui.h"
#include "ft2_structs.h"
#include "ft2_scrollbars.h"

/* access to global scrollbar array */
extern scrollBar_t scrollBars[NUM_SCROLLBARS];
#include "ft2_audio.h"
#include "ft2_config.h"
#include "scopes/ft2_scopes.h"
#include "scopes/ft2_scopedraw.h"
#include "ft2_wav_renderer.h"
#include "ft2_dsp_editor.h"
#include "ft2_dsp.h"
#include "ft2_mixer_layout_schema.h"
#include "shared/ft2_ui_assets.h"
#include "ft2_bmp.h"

/* Inline DSP UI state and layout */
static int selectedDSPChannel = -1; /* -1 = none, 0-31 channels, 255 = master */
static int selectedDSPSlot   =  0; /* 0 .. DSP_MAX_SLOTS-1 */

#define NUM_STRIPS 16 // stereo pairs 1-16
#define MIXER_DSP_PARAM_MAX 16

typedef struct
{
    bool valid;
    const ft2_ui_layout_desc_t *desc;
    const ft2_ui_framebox_desc_t *frame_mixer;
    const ft2_ui_framebox_desc_t *frame_dsp;
    const ft2_ui_framebox_desc_t *frame_params;
    const ft2_ui_mixer_strip_desc_t *strip[NUM_STRIPS];
    const ft2_ui_mixer_gain_desc_t *gain[NUM_STRIPS];
    const ft2_ui_mixer_pan_desc_t *pan[NUM_STRIPS];
    const ft2_ui_mixer_mute_desc_t *mute[NUM_STRIPS];
    const ft2_ui_mixer_scope_desc_t *scope[NUM_STRIPS];
    const ft2_ui_mixer_master_desc_t *master;
    const ft2_ui_dsp_slot_desc_t *dsp_slot[DSP_MAX_SLOTS];
    const ft2_ui_dsp_menu_desc_t *dsp_menu;
    const ft2_ui_dsp_param_desc_t *dsp_param[MIXER_DSP_PARAM_MAX];
    const ft2_ui_scrollbar_desc_t *sb_dsp_param_scroll;
    const ft2_ui_pushbutton_desc_t *pb_exit;
    const ft2_ui_pushbutton_desc_t *pb_master_up;
    const ft2_ui_pushbutton_desc_t *pb_master_dn;
    const ft2_ui_pushbutton_desc_t *pb_master_fx;
    const ft2_ui_pushbutton_desc_t *pb_gain_up[NUM_STRIPS];
    const ft2_ui_pushbutton_desc_t *pb_gain_dn[NUM_STRIPS];
    const ft2_ui_pushbutton_desc_t *pb_fx[NUM_STRIPS];
} MixerLayoutCache;

static MixerLayoutCache mixerLayout = { 0 };

typedef struct
{
    bool valid;
    uint16_t id;
    uint8_t *pixels;
    int32_t w, h;
} MixerBitmapCache;

static MixerBitmapCache mixerBitmapCache[64];

/* Layout: 40%% width for slot selection, 60%% for parameters */
#define DSP_UI_Y           (MIXER_AREA_H + 4) // moved up from +12 to +4
#define DSP_UI_SLOT_H      14 // was 12, now 80%
#define DSP_UI_PARAM_ROW_H 22 // increased for taller scrollbar

/* Forward declarations for inline DSP */
static void handleInlineDSPClicks(void);
static void drawInlineDspSlots(uint8_t ch);
static void drawMixerInlineParams(uint8_t ch);
static void pbDspParamScrollUp(void);
static void pbDspParamScrollDown(void);

// Inline effect-selection menu state
static bool inlineMenuShown = false;
static int  inlineMenuSlot  = -1;
#define INLINE_MENU_ITEM_H 12
#define INLINE_MENU_W      80

static int dspParamScrollOffset = 0;

// Effect names for menu (match dsp_editor.c)
static const char *inlineEffectNames[DSP_TYPE_COUNT] = {
    "Empty", "Gainer", "Comp", "Limiter", "Delay", "Reverb",
    "Chorus", "Flanger", "Phaser", "Drive", "Amp Sim", "EQ",
    "Filter", "Comb", "Bitcrush"
};

static int getVisibleDspParamRows(void)
{
    int rows = 0;
    for (int i = 0; i < MIXER_DSP_PARAM_MAX; i++)
    {
        if (!mixerLayout.dsp_param[i])
            break;
        rows++;
    }
    return rows;
}

// Forward declare effect-selection menu draw function
static void drawInlineDspMenu(void);
static void drawMixerSchemaBitmaps(void);

static const ft2_ui_framebox_desc_t *find_framebox_title(const char *title)
{
    if (!mixerLayout.valid || !title || !mixerLayout.desc)
        return NULL;

    for (uint16_t i = 0; i < mixerLayout.desc->frameboxes.count; i++)
    {
        const ft2_ui_framebox_desc_t *d = &mixerLayout.desc->framebox_desc[i];
        if (d->title && strcmp(d->title, title) == 0)
            return d;
    }
    return NULL;
}

static const ft2_ui_pushbutton_desc_t *find_pushbutton_name(const char *name)
{
    if (!mixerLayout.valid || !name || !mixerLayout.desc)
        return NULL;

    for (uint16_t i = 0; i < mixerLayout.desc->pushbuttons.count; i++)
    {
        const ft2_ui_pushbutton_desc_t *d = &mixerLayout.desc->pushbutton_desc[i];
        if (d->name && strcmp(d->name, name) == 0)
            return d;
    }
    return NULL;
}

static const ft2_ui_scrollbar_desc_t *find_scrollbar_first(void)
{
    if (!mixerLayout.valid || !mixerLayout.desc)
        return NULL;
    if (mixerLayout.desc->scrollbars.count == 0 || !mixerLayout.desc->scrollbar_desc)
        return NULL;
    return &mixerLayout.desc->scrollbar_desc[0];
}

static void init_mixer_layout_cache(void)
{
    memset(&mixerLayout, 0, sizeof(mixerLayout));
    mixerLayout.desc = &ft2_mixer_layout_layout;
    if (!mixerLayout.desc || mixerLayout.desc->version != FT2_UI_SCHEMA_VERSION)
        return;

    mixerLayout.valid = true;

    mixerLayout.frame_mixer = find_framebox_title("mixer_box");
    mixerLayout.frame_dsp = find_framebox_title("dsp_box");
    mixerLayout.frame_params = find_framebox_title("params");

    if (mixerLayout.desc->mixer_strips.count > 0 && mixerLayout.desc->mixer_strip_desc)
    {
        for (uint16_t i = 0; i < mixerLayout.desc->mixer_strips.count; i++)
        {
            const ft2_ui_mixer_strip_desc_t *d = &mixerLayout.desc->mixer_strip_desc[i];
            if (d->channel_index < NUM_STRIPS)
                mixerLayout.strip[d->channel_index] = d;
        }
    }
    if (mixerLayout.desc->mixer_gains.count > 0 && mixerLayout.desc->mixer_gain_desc)
    {
        for (uint16_t i = 0; i < mixerLayout.desc->mixer_gains.count; i++)
        {
            const ft2_ui_mixer_gain_desc_t *d = &mixerLayout.desc->mixer_gain_desc[i];
            if (d->channel_index < NUM_STRIPS)
                mixerLayout.gain[d->channel_index] = d;
        }
    }
    if (mixerLayout.desc->mixer_pans.count > 0 && mixerLayout.desc->mixer_pan_desc)
    {
        for (uint16_t i = 0; i < mixerLayout.desc->mixer_pans.count; i++)
        {
            const ft2_ui_mixer_pan_desc_t *d = &mixerLayout.desc->mixer_pan_desc[i];
            if (d->channel_index < NUM_STRIPS)
                mixerLayout.pan[d->channel_index] = d;
        }
    }
    if (mixerLayout.desc->mixer_mutes.count > 0 && mixerLayout.desc->mixer_mute_desc)
    {
        for (uint16_t i = 0; i < mixerLayout.desc->mixer_mutes.count; i++)
        {
            const ft2_ui_mixer_mute_desc_t *d = &mixerLayout.desc->mixer_mute_desc[i];
            if (d->channel_index < NUM_STRIPS)
                mixerLayout.mute[d->channel_index] = d;
        }
    }
    if (mixerLayout.desc->mixer_scopes.count > 0 && mixerLayout.desc->mixer_scope_desc)
    {
        for (uint16_t i = 0; i < mixerLayout.desc->mixer_scopes.count; i++)
        {
            const ft2_ui_mixer_scope_desc_t *d = &mixerLayout.desc->mixer_scope_desc[i];
            if (d->channel_index < NUM_STRIPS)
                mixerLayout.scope[d->channel_index] = d;
        }
    }

    if (mixerLayout.desc->mixer_masters.count > 0 && mixerLayout.desc->mixer_master_desc)
        mixerLayout.master = &mixerLayout.desc->mixer_master_desc[0];

    if (mixerLayout.desc->dsp_slots.count > 0 && mixerLayout.desc->dsp_slot_desc)
    {
        for (uint16_t i = 0; i < mixerLayout.desc->dsp_slots.count; i++)
        {
            const ft2_ui_dsp_slot_desc_t *d = &mixerLayout.desc->dsp_slot_desc[i];
            if (d->slot_index < DSP_MAX_SLOTS)
                mixerLayout.dsp_slot[d->slot_index] = d;
        }
    }
    if (mixerLayout.desc->dsp_menus.count > 0 && mixerLayout.desc->dsp_menu_desc)
        mixerLayout.dsp_menu = &mixerLayout.desc->dsp_menu_desc[0];

    if (mixerLayout.desc->dsp_params.count > 0 && mixerLayout.desc->dsp_param_desc)
    {
        for (uint16_t i = 0; i < mixerLayout.desc->dsp_params.count; i++)
        {
            const ft2_ui_dsp_param_desc_t *d = &mixerLayout.desc->dsp_param_desc[i];
            if (d->param_index < MIXER_DSP_PARAM_MAX)
                mixerLayout.dsp_param[d->param_index] = d;
        }
    }

    mixerLayout.sb_dsp_param_scroll = find_scrollbar_first();

    mixerLayout.pb_exit = find_pushbutton_name("mixer_exit");
    mixerLayout.pb_master_up = find_pushbutton_name("mix_master_up");
    mixerLayout.pb_master_dn = find_pushbutton_name("mix_master_dn");
    mixerLayout.pb_master_fx = find_pushbutton_name("mix_master_fx");

    for (uint8_t ch = 0; ch < NUM_STRIPS; ch++)
    {
        char name[32];
        snprintf(name, sizeof(name), "mix_gain_up_%u", ch);
        mixerLayout.pb_gain_up[ch] = find_pushbutton_name(name);
        snprintf(name, sizeof(name), "mix_gain_dn_%u", ch);
        mixerLayout.pb_gain_dn[ch] = find_pushbutton_name(name);
        snprintf(name, sizeof(name), "mix_fx_%u", ch);
        mixerLayout.pb_fx[ch] = find_pushbutton_name(name);
    }
}

/* ---------------------------------------------------------------------------
** Full-screen mixer page.
** ------------------------------------------------------------------------ */

/* Layout constants */
#define MIXER_BASE_X        12
#define MIXER_TOP_PAD       6// was 4, now 80%
#define STRIP_WIDTH         25      /* wider strip */
#define STRIP_SPACING       (STRIP_WIDTH + 4)

/* Y coordinates are relative to top of screen */
#define SCOPE_W             20    /* 24×24 scope fits in 28px strip */
#define SCOPE_X_OFFSET      ((STRIP_WIDTH - SCOPE_W) / 2)
#define SCOPE_Y             (MIXER_TOP_PAD + 5) // was +6, now 80%
#define SCOPE_H             20 // was 26, now 80%

// Layout around pan slider with 2px gaps
#define PAN_Y               (SCOPE_Y + SCOPE_H + 2) // keep gap at 2 for clarity
#define PAN_SLIDER_H        7 // was 7, now 80%
#define PAN_SLIDER_W        (STRIP_WIDTH - 5)

// Nudge button above gain slider, 2px gap from pan
#define NUDGE_BTN_H         12                /* was 12, now 80% */
#define NUDGE_BTN_W         (STRIP_WIDTH - 2)      /* full strip width, no horizontal padding */
#define NUDGE_BTN_UP_Y      (PAN_Y + PAN_SLIDER_H) // -2 px from previous gap

// Gain slider below nudge up, 2px gap
#define GAIN_Y              (NUDGE_BTN_UP_Y + NUDGE_BTN_H) // -2 px from previous gap
#define GAIN_SLIDER_H       80               /* was 100, now 80% */
#define GAIN_SLIDER_W       20

// Nudge button below gain slider, 2px gap
#define NUDGE_BTN_DN_Y      (GAIN_Y + GAIN_SLIDER_H) // -2 px from previous gap

#define FX_BTN_Y            (NUDGE_BTN_DN_Y + NUDGE_BTN_H) // -2 px from previous gap
#define BTN_H               12                /* was 14, now 80% */
#define BTN_W               (STRIP_WIDTH - 2)      /* full width inside strip, no padding */
#define MUTE_BTN_Y          (FX_BTN_Y + BTN_H) // -2 px from previous gap

#define LABEL_Y             (MUTE_BTN_Y + BTN_H + 2) // -2 px from previous gap
#define GAIN_LABEL_Y        (LABEL_Y + 12) // unchanged
#define PAN_LABEL_Y         (GAIN_LABEL_Y + 16) // unchanged

#define MIXER_AREA_H        (GAIN_LABEL_Y + 10) // unchanged


#define MIXER_EXIT_BUTTON_ID PB_RES_1
#define MIXER_PAGE_BUTTON_ID PB_RES_2   /* unused with stereo strips */
#define MIX_GAIN_BASE SB_MIX_GAIN_0
#define MIX_PAN_BASE  SB_MIX_PAN_0
#define MASTER_GAIN_ID SB_MIX_MASTER_GAIN

/* Master strip sizing */
#define MASTER_SLIDER_W     40
#define MASTER_BTN_W        (MASTER_SLIDER_W)

/* Slider ranges */
#define GAIN_SLIDER_END  200 /* maps to 0.0 .. 2.0 */
#define PAN_SLIDER_END   200 /* maps to -1.0 .. +1.0 (100 = center) */

/* Mixer state */
static uint8_t mixerPage = 0; /* 0 = channels 1-16, 1 = 17-32 */

/* Helpers to convert scrollbar position to parameter values */
static inline float gainPosToValue(uint32_t pos)  { return (float)(GAIN_SLIDER_END - pos) / 100.0f; }
static inline float panPosToValue(uint32_t pos)   { return ((float)pos - 100.0f) / 100.0f; } /* -1..1 */
static inline uint32_t valueToGainPos(float val)  { return (uint32_t)CLAMP(GAIN_SLIDER_END - (int32_t)lrintf(val * 100.0f), 0, GAIN_SLIDER_END); }
static inline uint32_t valueToPanPos(float val)   { return (uint32_t)CLAMP((int32_t)lrintf((val + 1.0f) * 100.0f), 0, PAN_SLIDER_END); }

static inline float masterGainPosToVal(uint32_t pos) { return (float)(GAIN_SLIDER_END - pos)/100.0f; }

static void sbMasterGainPos(uint32_t pos)
{
    mixerSetMasterGain(masterGainPosToVal(pos));
    /* Recalculate global audio normalization with new master gain */
    setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));
}

/* Forward declarations */
static void drawMixerBox(void);
static void drawMixerChannelStrip(uint8_t ch_idx);
static void drawMiniScope(uint8_t ch, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
static void hideMixerWidgets(uint8_t numChans);
static void setMixerPage(uint8_t page);
static void pbMixerPageToggle(void);
static void pbMasterGainUp(void);
static void pbMasterGainDown(void);
static void pbMasterFX(void);

/* Gain scrollbar callbacks */
#define GEN_GAIN_CB(ch) static void sbMixerGainPos_##ch(uint32_t pos) { \
    float v = gainPosToValue(pos); \
    mixerCh[ch].fader = v; \
    stereoMixerCh[CHANNEL_TO_PAIR_IDX(ch)].fader = v; \
    audioRequestMixerUpdate(ch); cacheMixerStateFromGUI(); }
GEN_GAIN_CB(0)  GEN_GAIN_CB(1)  GEN_GAIN_CB(2)  GEN_GAIN_CB(3)
GEN_GAIN_CB(4)  GEN_GAIN_CB(5)  GEN_GAIN_CB(6)  GEN_GAIN_CB(7)
GEN_GAIN_CB(8)  GEN_GAIN_CB(9)  GEN_GAIN_CB(10) GEN_GAIN_CB(11)
GEN_GAIN_CB(12) GEN_GAIN_CB(13) GEN_GAIN_CB(14) GEN_GAIN_CB(15)
GEN_GAIN_CB(16) GEN_GAIN_CB(17) GEN_GAIN_CB(18) GEN_GAIN_CB(19)
GEN_GAIN_CB(20) GEN_GAIN_CB(21) GEN_GAIN_CB(22) GEN_GAIN_CB(23)
GEN_GAIN_CB(24) GEN_GAIN_CB(25) GEN_GAIN_CB(26) GEN_GAIN_CB(27)
GEN_GAIN_CB(28) GEN_GAIN_CB(29) GEN_GAIN_CB(30) GEN_GAIN_CB(31)

static void (*gainCbs[MAX_MIXER_CHANNELS])(uint32_t) = {
 sbMixerGainPos_0, sbMixerGainPos_1, sbMixerGainPos_2, sbMixerGainPos_3,
 sbMixerGainPos_4, sbMixerGainPos_5, sbMixerGainPos_6, sbMixerGainPos_7,
 sbMixerGainPos_8, sbMixerGainPos_9, sbMixerGainPos_10, sbMixerGainPos_11,
 sbMixerGainPos_12, sbMixerGainPos_13, sbMixerGainPos_14, sbMixerGainPos_15,
 sbMixerGainPos_16, sbMixerGainPos_17, sbMixerGainPos_18, sbMixerGainPos_19,
 sbMixerGainPos_20, sbMixerGainPos_21, sbMixerGainPos_22, sbMixerGainPos_23,
 sbMixerGainPos_24, sbMixerGainPos_25, sbMixerGainPos_26, sbMixerGainPos_27,
 sbMixerGainPos_28, sbMixerGainPos_29, sbMixerGainPos_30, sbMixerGainPos_31 };

/* ------------------------------------------------------------------- */
/* Pan scrollbar callbacks (horizontal) */
#define GEN_PAN_CB(ch) static void sbMixerPanPos_##ch(uint32_t pos) { \
    float p = panPosToValue(pos); \
    mixerCh[ch].pan = p; \
    stereoMixerCh[CHANNEL_TO_PAIR_IDX(ch)].pan = p; \
    audioRequestMixerUpdate(ch); cacheMixerStateFromGUI(); } \

GEN_PAN_CB(0)  GEN_PAN_CB(1)  GEN_PAN_CB(2)  GEN_PAN_CB(3)  GEN_PAN_CB(4)  GEN_PAN_CB(5)  GEN_PAN_CB(6)  GEN_PAN_CB(7) \
GEN_PAN_CB(8)  GEN_PAN_CB(9)  GEN_PAN_CB(10) GEN_PAN_CB(11) GEN_PAN_CB(12) GEN_PAN_CB(13) GEN_PAN_CB(14) GEN_PAN_CB(15) \
GEN_PAN_CB(16) GEN_PAN_CB(17) GEN_PAN_CB(18) GEN_PAN_CB(19) GEN_PAN_CB(20) GEN_PAN_CB(21) GEN_PAN_CB(22) GEN_PAN_CB(23) \
GEN_PAN_CB(24) GEN_PAN_CB(25) GEN_PAN_CB(26) GEN_PAN_CB(27) GEN_PAN_CB(28) GEN_PAN_CB(29) GEN_PAN_CB(30) GEN_PAN_CB(31)

static void (*panCbs[MAX_MIXER_CHANNELS])(uint32_t) = {
 sbMixerPanPos_0 , sbMixerPanPos_1 , sbMixerPanPos_2 , sbMixerPanPos_3 , sbMixerPanPos_4 , sbMixerPanPos_5 , sbMixerPanPos_6 , sbMixerPanPos_7 ,
 sbMixerPanPos_8 , sbMixerPanPos_9 , sbMixerPanPos_10, sbMixerPanPos_11, sbMixerPanPos_12, sbMixerPanPos_13, sbMixerPanPos_14, sbMixerPanPos_15,
 sbMixerPanPos_16, sbMixerPanPos_17, sbMixerPanPos_18, sbMixerPanPos_19, sbMixerPanPos_20, sbMixerPanPos_21, sbMixerPanPos_22, sbMixerPanPos_23,
 sbMixerPanPos_24, sbMixerPanPos_25, sbMixerPanPos_26, sbMixerPanPos_27, sbMixerPanPos_28, sbMixerPanPos_29, sbMixerPanPos_30, sbMixerPanPos_31 };

/* ------------------------------------------------------------------- */
/* Exit button */
static void pbMixerExit(void)
{
    cacheMixerStateFromGUI();
    // Reset inline DSP state
    selectedDSPChannel = -1;
    selectedDSPSlot = 0;
    inlineMenuShown = false;
    inlineMenuSlot = -1;
    // Reset GUI focus
    unstuckLastUsedGUIElement();
    mouse.lastUsedObjectType = OBJECT_NONE;
    mouse.lastUsedObjectID = OBJECT_ID_NONE;
    // Return to pattern editor view
    ui.patternEditorShown = true;
    ui.extendedPatternEditor = false;
    ui.instEditorShown = false;
    ui.sampleEditorShown = false;
    // Close mixer and fully reinitialize GUI to clear ghosts
    hideMixerScreen();          /* close mixer and restore GUI */
    setupGUI();                 /* rebuild entire UI */
}

/* ------------------------------------------------------------------- */
/*                        Button Callbacks                             */
/* ------------------------------------------------------------------- */

#undef GEN_MIXER_PB_CALLBACKS
#define GEN_MIXER_PB_CALLBACKS(ch) \
static void pbMixerGainUp_##ch(void) { \
    int32_t pos = getScrollBarPos(MIX_GAIN_BASE + ch); \
    if (pos > 0) { \
        setScrollBarPos(MIX_GAIN_BASE + ch, pos - 1, true); \
        audioRequestMixerUpdate(ch); \
    } \
} \
static void pbMixerGainDown_##ch(void) { \
    int32_t pos = getScrollBarPos(MIX_GAIN_BASE + ch); \
    if (pos < GAIN_SLIDER_END) { \
        setScrollBarPos(MIX_GAIN_BASE + ch, pos + 1, true); \
        audioRequestMixerUpdate(ch); \
    } \
} \
static void pbMixerMute_##ch(void) { \
    editor.channelMuted[ch] ^= 1; \
    setChannelMute(ch, editor.channelMuted[ch]); \
} \
static void pbMixerDSP_##ch(void) { \
    if (selectedDSPChannel == ch) { \
        selectedDSPChannel = -1; \
    } else { \
        selectedDSPChannel = ch; \
        selectedDSPSlot = 0; \
        dspParamScrollOffset = 0; \
    } \
}

GEN_MIXER_PB_CALLBACKS(0)
GEN_MIXER_PB_CALLBACKS(1)
GEN_MIXER_PB_CALLBACKS(2)
GEN_MIXER_PB_CALLBACKS(3)
GEN_MIXER_PB_CALLBACKS(4)
GEN_MIXER_PB_CALLBACKS(5)
GEN_MIXER_PB_CALLBACKS(6)
GEN_MIXER_PB_CALLBACKS(7)
GEN_MIXER_PB_CALLBACKS(8)
GEN_MIXER_PB_CALLBACKS(9)
GEN_MIXER_PB_CALLBACKS(10)
GEN_MIXER_PB_CALLBACKS(11)
GEN_MIXER_PB_CALLBACKS(12)
GEN_MIXER_PB_CALLBACKS(13)
GEN_MIXER_PB_CALLBACKS(14)
GEN_MIXER_PB_CALLBACKS(15)
GEN_MIXER_PB_CALLBACKS(16)
GEN_MIXER_PB_CALLBACKS(17)
GEN_MIXER_PB_CALLBACKS(18)
GEN_MIXER_PB_CALLBACKS(19)
GEN_MIXER_PB_CALLBACKS(20)
GEN_MIXER_PB_CALLBACKS(21)
GEN_MIXER_PB_CALLBACKS(22)
GEN_MIXER_PB_CALLBACKS(23)
GEN_MIXER_PB_CALLBACKS(24)
GEN_MIXER_PB_CALLBACKS(25)
GEN_MIXER_PB_CALLBACKS(26)
GEN_MIXER_PB_CALLBACKS(27)
GEN_MIXER_PB_CALLBACKS(28)
GEN_MIXER_PB_CALLBACKS(29)
GEN_MIXER_PB_CALLBACKS(30)
GEN_MIXER_PB_CALLBACKS(31)

static void (*mixerGainUpCbs[MAX_MIXER_CHANNELS])(void) = {
    pbMixerGainUp_0, pbMixerGainUp_1, pbMixerGainUp_2,  pbMixerGainUp_3,
    pbMixerGainUp_4, pbMixerGainUp_5, pbMixerGainUp_6,  pbMixerGainUp_7,
    pbMixerGainUp_8, pbMixerGainUp_9, pbMixerGainUp_10, pbMixerGainUp_11,
    pbMixerGainUp_12, pbMixerGainUp_13, pbMixerGainUp_14, pbMixerGainUp_15,
    pbMixerGainUp_16, pbMixerGainUp_17, pbMixerGainUp_18, pbMixerGainUp_19,
    pbMixerGainUp_20, pbMixerGainUp_21, pbMixerGainUp_22, pbMixerGainUp_23,
    pbMixerGainUp_24, pbMixerGainUp_25, pbMixerGainUp_26, pbMixerGainUp_27,
    pbMixerGainUp_28, pbMixerGainUp_29, pbMixerGainUp_30, pbMixerGainUp_31
};
static void (*mixerGainDownCbs[MAX_MIXER_CHANNELS])(void) = {
    pbMixerGainDown_0, pbMixerGainDown_1, pbMixerGainDown_2,  pbMixerGainDown_3,
    pbMixerGainDown_4, pbMixerGainDown_5, pbMixerGainDown_6,  pbMixerGainDown_7,
    pbMixerGainDown_8, pbMixerGainDown_9, pbMixerGainDown_10, pbMixerGainDown_11,
    pbMixerGainDown_12, pbMixerGainDown_13, pbMixerGainDown_14, pbMixerGainDown_15,
    pbMixerGainDown_16, pbMixerGainDown_17, pbMixerGainDown_18, pbMixerGainDown_19,
    pbMixerGainDown_20, pbMixerGainDown_21, pbMixerGainDown_22, pbMixerGainDown_23,
    pbMixerGainDown_24, pbMixerGainDown_25, pbMixerGainDown_26, pbMixerGainDown_27,
    pbMixerGainDown_28, pbMixerGainDown_29, pbMixerGainDown_30, pbMixerGainDown_31
};
static void (*mixerMuteCbs[MAX_MIXER_CHANNELS])(void) = {
    pbMixerMute_0, pbMixerMute_1, pbMixerMute_2,  pbMixerMute_3,
    pbMixerMute_4, pbMixerMute_5, pbMixerMute_6,  pbMixerMute_7,
    pbMixerMute_8, pbMixerMute_9, pbMixerMute_10, pbMixerMute_11,
    pbMixerMute_12, pbMixerMute_13, pbMixerMute_14, pbMixerMute_15,
    pbMixerMute_16, pbMixerMute_17, pbMixerMute_18, pbMixerMute_19,
    pbMixerMute_20, pbMixerMute_21, pbMixerMute_22, pbMixerMute_23,
    pbMixerMute_24, pbMixerMute_25, pbMixerMute_26, pbMixerMute_27,
    pbMixerMute_28, pbMixerMute_29, pbMixerMute_30, pbMixerMute_31
};
static void (*mixerDSPCbs[MAX_MIXER_CHANNELS])(void) = {
    pbMixerDSP_0, pbMixerDSP_1, pbMixerDSP_2,  pbMixerDSP_3,
    pbMixerDSP_4, pbMixerDSP_5, pbMixerDSP_6,  pbMixerDSP_7,
    pbMixerDSP_8, pbMixerDSP_9, pbMixerDSP_10, pbMixerDSP_11,
    pbMixerDSP_12, pbMixerDSP_13, pbMixerDSP_14, pbMixerDSP_15,
    pbMixerDSP_16, pbMixerDSP_17, pbMixerDSP_18, pbMixerDSP_19,
    pbMixerDSP_20, pbMixerDSP_21, pbMixerDSP_22, pbMixerDSP_23,
    pbMixerDSP_24, pbMixerDSP_25, pbMixerDSP_26, pbMixerDSP_27,
    pbMixerDSP_28, pbMixerDSP_29, pbMixerDSP_30, pbMixerDSP_31
};

/* ------------------------------------------------------------------- */
/* Local helpers to open/close modal dialog (copy of pattern used in other tools) */
static void mixerWindowOpen(void)
{
    /* Treat mixer as a modal System Request so reserved push-buttons (0-7) work */
    ui.sysReqShown = true;
    ui.sysReqEnterPressed = false;

    /* Hide normal GUI and disable file drag while mixer is shown */
    hideTopScreen();
    unstuckLastUsedGUIElement();
    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
}

static void mixerWindowClose(void)
{
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    mouseAnimOff();

    /* Restore normal push-button handling */
    ui.sysReqShown = false;

    /* Force full top and bottom screen redraw */
    showTopScreen(true);
    showBottomScreen();
    ui.updatePatternEditor = true;
    ui.updatePosSections  = true;
}

/* ------------------------------------------------------------------- */
static void setupChannelStrip(uint8_t ch)
{
    uint16_t x_base = mixerLayout.strip[ch] ? mixerLayout.strip[ch]->x : (MIXER_BASE_X + (ch * STRIP_SPACING));
    uint16_t nudge_x = x_base + 1;     /* full strip width, no padding */
    uint16_t btn_x   = x_base + 1;     /* full strip width, no padding */

    pushButton_t *b;
    const ft2_ui_pushbutton_desc_t *pb_desc = NULL;

    // Gain Nudge Up
    b = &pushButtons[PB_MIX_GAIN_UP_0 + ch];
    pb_desc = mixerLayout.pb_gain_up[ch];
    b->x = pb_desc ? pb_desc->x : nudge_x;
    b->y = pb_desc ? pb_desc->y : NUDGE_BTN_UP_Y;
    b->w = pb_desc ? pb_desc->w : NUDGE_BTN_W;
    b->h = pb_desc ? pb_desc->h : NUDGE_BTN_H;
    b->caption = ARROW_UP_STRING; b->callbackFuncOnUp = mixerGainUpCbs[ch]; b->visible = true;

    // Gain Nudge Down
    b = &pushButtons[PB_MIX_GAIN_DN_0 + ch];
    pb_desc = mixerLayout.pb_gain_dn[ch];
    b->x = pb_desc ? pb_desc->x : nudge_x;
    b->y = pb_desc ? pb_desc->y : NUDGE_BTN_DN_Y;
    b->w = pb_desc ? pb_desc->w : NUDGE_BTN_W;
    b->h = pb_desc ? pb_desc->h : NUDGE_BTN_H;
    b->caption = ARROW_DOWN_STRING; b->callbackFuncOnUp = mixerGainDownCbs[ch]; b->visible = true;

    // DSP/FX button
    b = &pushButtons[PB_MIX_DSP_0 + ch];
    pb_desc = mixerLayout.pb_fx[ch];
    b->x = pb_desc ? pb_desc->x : btn_x;
    b->y = pb_desc ? pb_desc->y : FX_BTN_Y;
    b->w = pb_desc ? pb_desc->w : BTN_W;
    b->h = pb_desc ? pb_desc->h : BTN_H;
    b->caption = "FX"; b->callbackFuncOnUp = mixerDSPCbs[ch]; b->visible = true;

    // Mute button
    b = &pushButtons[PB_MIX_MUTE_0 + ch];
    const ft2_ui_mixer_mute_desc_t *mute_desc = mixerLayout.mute[ch];
    b->x = mute_desc ? mute_desc->x : btn_x;
    b->y = mute_desc ? mute_desc->y : MUTE_BTN_Y;
    b->w = mute_desc ? mute_desc->w : BTN_W;
    b->h = mute_desc ? mute_desc->h : BTN_H;
    b->caption = "M"; b->callbackFuncOnUp = mixerMuteCbs[ch]; b->visible = true;

    /* Gain slider (vertical) */
    scrollBar_t *sGain = &scrollBars[MIX_GAIN_BASE + ch];
    memset(sGain, 0, sizeof(scrollBar_t));
    const ft2_ui_mixer_gain_desc_t *gain_desc = mixerLayout.gain[ch];
    sGain->x = gain_desc ? gain_desc->x : (x_base + (STRIP_WIDTH - GAIN_SLIDER_W) / 2);
    sGain->y = gain_desc ? gain_desc->y : GAIN_Y;
    sGain->w = gain_desc ? gain_desc->w : GAIN_SLIDER_W;
    sGain->h = gain_desc ? gain_desc->h : GAIN_SLIDER_H;
    sGain->type = SCROLLBAR_VERTICAL;
    sGain->thumbType = SCROLLBAR_DYNAMIC_THUMB_SIZE;
    sGain->callbackFunc = gainCbs[ch];
    sGain->visible = true;
    setScrollBarPageLength(MIX_GAIN_BASE + ch, 1);
    setScrollBarEnd(MIX_GAIN_BASE + ch, GAIN_SLIDER_END);
    setScrollBarPos(MIX_GAIN_BASE + ch, valueToGainPos(mixerCh[ch].fader), false);

    /* Pan slider (horizontal mini) */
    scrollBar_t *sPan = &scrollBars[MIX_PAN_BASE + ch];
    memset(sPan, 0, sizeof(scrollBar_t));
    const ft2_ui_mixer_pan_desc_t *pan_desc = mixerLayout.pan[ch];
    sPan->x = pan_desc ? pan_desc->x : (x_base + (STRIP_WIDTH - PAN_SLIDER_W) / 2);
    sPan->y = pan_desc ? pan_desc->y : PAN_Y;
    sPan->w = pan_desc ? pan_desc->w : PAN_SLIDER_W;
    sPan->h = pan_desc ? pan_desc->h : PAN_SLIDER_H;
    sPan->type = SCROLLBAR_HORIZONTAL;
    sPan->thumbType = SCROLLBAR_DYNAMIC_THUMB_SIZE;
    sPan->callbackFunc = panCbs[ch];
    sPan->visible = true;
    setScrollBarPageLength(MIX_PAN_BASE + ch, 1);
    setScrollBarEnd(MIX_PAN_BASE + ch, PAN_SLIDER_END);
    setScrollBarPos(MIX_PAN_BASE + ch, valueToPanPos(mixerCh[ch].pan), false);

    /* Show buttons (already visible, but force draw) */
    showPushButton(PB_MIX_GAIN_UP_0 + ch);
    showPushButton(PB_MIX_GAIN_DN_0 + ch);
    showPushButton(PB_MIX_DSP_0     + ch);
    showPushButton(PB_MIX_MUTE_0    + ch);
}

/* ------------------------------------------------------------------- */
static void setupMixerWidgets(uint8_t numChans)
{
    /* Exit button (pushButtons[0]) */
    pushButton_t *pb = &pushButtons[MIXER_EXIT_BUTTON_ID];
    memset(pb, 0, sizeof (pushButton_t));
    if (mixerLayout.pb_exit)
    {
        pb->x = mixerLayout.pb_exit->x;
        pb->y = mixerLayout.pb_exit->y;
        pb->w = mixerLayout.pb_exit->w;
        pb->h = mixerLayout.pb_exit->h;
    }
    else
    {
        pb->x = SCREEN_W - 60;
        pb->y = SCREEN_H - 390;
        pb->w = 50;
        pb->h = 16;
    }
    pb->caption = "Exit";
    pb->callbackFuncOnUp = pbMixerExit; /* close on release */
    pb->visible = true;

    /* DSP param scroll nudge buttons (positions set during draw) */
    pushButton_t *pbUp = &pushButtons[PB_RES_6];
    memset(pbUp, 0, sizeof (pushButton_t));
    pbUp->caption = ARROW_UP_STRING;
    pbUp->callbackFuncOnUp = pbDspParamScrollUp;
    pbUp->visible = false;

    pushButton_t *pbDn = &pushButtons[PB_RES_7];
    memset(pbDn, 0, sizeof (pushButton_t));
    pbDn->caption = ARROW_DOWN_STRING;
    pbDn->callbackFuncOnUp = pbDspParamScrollDown;
    pbDn->visible = false;


    for (uint8_t ch = 0; ch < numChans; ch++)
    {
        setupChannelStrip(ch);
    }

    /* Master gain slider */
    scrollBar_t *sM = &scrollBars[MASTER_GAIN_ID];
    memset(sM, 0, sizeof(scrollBar_t));

    /* Position relative to first page width (visible area) */
    if (mixerLayout.master)
    {
        sM->x = mixerLayout.master->x;
        sM->y = mixerLayout.master->y;
        sM->w = mixerLayout.master->w;
        sM->h = mixerLayout.master->h;
    }
    else
    {
        sM->x = MIXER_BASE_X + (NUM_STRIPS * STRIP_SPACING) + 12;
        sM->y = GAIN_Y;
        sM->w = MASTER_SLIDER_W;
        sM->h = GAIN_SLIDER_H;
    }
    sM->type = SCROLLBAR_VERTICAL;
    sM->thumbType = SCROLLBAR_DYNAMIC_THUMB_SIZE;
    sM->callbackFunc = sbMasterGainPos;
    sM->visible = true;

    setScrollBarPageLength(MASTER_GAIN_ID, 1);
    setScrollBarEnd(MASTER_GAIN_ID, GAIN_SLIDER_END);
    setScrollBarPos(MASTER_GAIN_ID, valueToGainPos(mixerMasterGain), false);

    /* Master nudge/FX buttons */
    uint16_t master_btn_x = sM->x - ((MASTER_BTN_W - sM->w) / 2);

    pushButton_t *pbMU = &pushButtons[PB_RES_3]; /* master up */
    memset(pbMU, 0, sizeof (pushButton_t));
    if (mixerLayout.pb_master_up)
    {
        pbMU->x = mixerLayout.pb_master_up->x;
        pbMU->y = mixerLayout.pb_master_up->y;
        pbMU->w = mixerLayout.pb_master_up->w;
        pbMU->h = mixerLayout.pb_master_up->h;
    }
    else
    {
        pbMU->x = master_btn_x;
        pbMU->y = NUDGE_BTN_UP_Y;
        pbMU->w = MASTER_BTN_W;
        pbMU->h = NUDGE_BTN_H;
    }
    pbMU->caption = ARROW_UP_STRING;
    pbMU->callbackFuncOnUp = pbMasterGainUp;
    pbMU->visible = true;

    pushButton_t *pbMD = &pushButtons[PB_RES_4]; /* master down */
    memset(pbMD, 0, sizeof (pushButton_t));
    if (mixerLayout.pb_master_dn)
    {
        pbMD->x = mixerLayout.pb_master_dn->x;
        pbMD->y = mixerLayout.pb_master_dn->y;
        pbMD->w = mixerLayout.pb_master_dn->w;
        pbMD->h = mixerLayout.pb_master_dn->h;
    }
    else
    {
        pbMD->x = master_btn_x;
        pbMD->y = NUDGE_BTN_DN_Y;
        pbMD->w = MASTER_BTN_W;
        pbMD->h = NUDGE_BTN_H;
    }
    pbMD->caption = ARROW_DOWN_STRING;
    pbMD->callbackFuncOnUp = pbMasterGainDown;
    pbMD->visible = true;

    pushButton_t *pbMF = &pushButtons[PB_RES_5]; /* master FX */
    memset(pbMF, 0, sizeof (pushButton_t));
    if (mixerLayout.pb_master_fx)
    {
        pbMF->x = mixerLayout.pb_master_fx->x;
        pbMF->y = mixerLayout.pb_master_fx->y;
        pbMF->w = mixerLayout.pb_master_fx->w;
        pbMF->h = mixerLayout.pb_master_fx->h;
    }
    else
    {
        pbMF->x = master_btn_x;
        pbMF->y = FX_BTN_Y;
        pbMF->w = MASTER_BTN_W;
        pbMF->h = BTN_H;
    }
    pbMF->caption = "FX";
    pbMF->callbackFuncOnUp = pbMasterFX;
    pbMF->visible = true;

    /* Show first page (channels 1-16) by default */
    setMixerPage(0);
}

static void drawMiniScope(uint8_t ch, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    // Check if channel is muted
    if (editor.channelMuted[ch])
    {
        fillRect(x, y, w, h, PAL_DESKTOP);
        return; /* Red 'X' overlay is added later in drawMixerChannelStrip() */
    }

    clearRect(x, y, w, h);

    const int32_t lineY = y + (h / 2);

    // cache volatile scope state
    volatile scope_t s_vol = scope[ch];
    if (!s_vol.active || s_vol.volume <= 0 || audio.locked)
    {
        hLine(x, lineY, w, PAL_PATTEXT);
        scope[ch].wasCleared = true;
        return;
    }

    scope[ch].wasCleared = false;

    const uint32_t color = video.palette[PAL_PATTEXT];

    // this is what the main scopes use to tie waveform display speed to note frequency
    const uint64_t drawDelta = (uint64_t)(s_vol.delta * ((double)SCOPE_HZ / ((double)C4_FREQ / 2.0)));

    // use a non-volatile copy for the drawing loop
    scope_t s;
    memcpy(&s, (const void *)&s_vol, sizeof(s));

    int32_t position = s.position;
    uint64_t positionFrac = 0;
    bool samplingBackwards = s.samplingBackwards;

    for (uint16_t i = 0; i < w; i++)
    {
        int32_t readPos = position;
        if (s.loopType == LOOP_BIDI && samplingBackwards)
            readPos = (s.sampleEnd - 1) - (position - s.loopStart);

        int32_t sample;
        if (s.sample16Bit)
            sample = (s.base16[readPos] * s.volume) >> (16 + 2);
        else
            sample = (s.base8[readPos] * s.volume) >> (8 + 2);

        // scale sample to fit mini-scope height `h`
        sample = (sample * h) / SCOPE_HEIGHT;

        int32_t y_coord = lineY - sample;
        if (y_coord < y) y_coord = y;
        if (y_coord >= y + h) y_coord = y + h - 1;

        video.frameBuffer[y_coord * SCREEN_W + (x + i)] = color;

        positionFrac += drawDelta;
        position += (int32_t)(positionFrac >> 32);
        positionFrac &= 0xFFFFFFFF;

        if (position >= s.sampleEnd)
        {
            if (s.loopType == LOOP_BIDI)
            {
                if (s.loopLength >= 2)
                {
                    const uint32_t overflow = position - s.sampleEnd;
                    const uint32_t cycles = overflow / s.loopLength;
                    const uint32_t phase = overflow % s.loopLength;
                    position = s.loopStart + phase;
                    if (cycles & 1) samplingBackwards = !samplingBackwards;
                }
                else
                {
                    position = s.loopStart;
                }
            }
            else if (s.loopType == LOOP_FORWARD)
            {
                if (s.loopLength >= 2)
                    position = s.loopStart + ((position - s.sampleEnd) % s.loopLength);
                else
                    position = s.loopStart;
            }
            else // no loop
            {
                break; // stop drawing
            }
        }
    }
}

static void drawMixerChannelStrip(uint8_t ch_idx)
{
    char label[16];
    const ft2_ui_mixer_strip_desc_t *strip_desc = mixerLayout.strip[ch_idx];
    const ft2_ui_mixer_scope_desc_t *scope_desc = mixerLayout.scope[ch_idx];

    uint16_t strip_x = strip_desc ? strip_desc->x : (MIXER_BASE_X + (ch_idx * STRIP_SPACING));
    uint16_t strip_y = strip_desc ? strip_desc->y : (SCOPE_Y - 2);
    uint16_t strip_w = strip_desc ? strip_desc->w : STRIP_WIDTH;
    uint16_t strip_h = strip_desc ? strip_desc->h : (MIXER_AREA_H - 20);

    drawFramework(strip_x, strip_y, strip_w, strip_h, FRAMEWORK_TYPE2);

    const uint16_t scope_x = scope_desc ? scope_desc->x : (strip_x + SCOPE_X_OFFSET);
    const uint16_t scope_y = scope_desc ? scope_desc->y : SCOPE_Y;
    const uint16_t scope_w = scope_desc ? scope_desc->w : SCOPE_W;
    const uint16_t scope_h = scope_desc ? scope_desc->h : SCOPE_H;
    drawMiniScope(ch_idx, scope_x, scope_y, scope_w, scope_h);

    if (editor.channelMuted[ch_idx])
    {
        const uint32_t color = video.palette[PAL_BUTTON2];
        // simple "X"
        line(scope_x, scope_x + scope_w, scope_y, scope_y + scope_h, color);
        line(scope_x, scope_x + scope_w, scope_y + scope_h, scope_y, color);
    }

    uint16_t gain_val = (uint16_t)lrintf(mixerCh[ch_idx].fader * 100.0f);
    sprintf(label, "%03d", gain_val);
    uint16_t label_y = (mixerLayout.mute[ch_idx]) ? (mixerLayout.mute[ch_idx]->y + mixerLayout.mute[ch_idx]->h + 2) : LABEL_Y;
    uint16_t gain_label_y = label_y + 12;
    textOutTiny(strip_x + 7, gain_label_y + 2, label, video.palette[PAL_FORGRND]);

    sprintf(label, "%02d", ch_idx + 1);
    textOut(strip_x + ((int)strip_w - textWidth(label)) / 2, label_y, PAL_FORGRND, label);
}

/* ------------------------------------------------------------------- */
static void drawMixerBox(void)
{
    fillRect(0, 0, SCREEN_W, SCREEN_H, PAL_DESKTOP);

    drawMixerSchemaBitmaps();

    if (mixerLayout.frame_mixer)
        drawFramework(mixerLayout.frame_mixer->x, mixerLayout.frame_mixer->y,
                      mixerLayout.frame_mixer->w, mixerLayout.frame_mixer->h, FRAMEWORK_TYPE1);



    for (uint8_t ch = 0; ch < NUM_STRIPS; ch++)
    {
        drawMixerChannelStrip(ch);
    }

    uint16_t master_x_base = mixerLayout.master
        ? mixerLayout.master->x
        : (MIXER_BASE_X + (NUM_STRIPS * STRIP_SPACING) + 16);
    uint16_t master_y = mixerLayout.master ? mixerLayout.master->y : GAIN_Y;
    uint16_t master_h = mixerLayout.master ? mixerLayout.master->h : GAIN_SLIDER_H;

    vLine(master_x_base - 6, master_y, master_h, PAL_DSKTOP2);
    uint16_t label_y = (mixerLayout.mute[0]) ? (mixerLayout.mute[0]->y + mixerLayout.mute[0]->h + 2) : LABEL_Y;
    textOutTinyOutline(master_x_base, label_y, "MASTER");

    char label[16];
    uint16_t master_gain_val = (uint16_t)lrintf(mixerMasterGain * 100.0f);
    sprintf(label, "G:%03d", master_gain_val);
    textOutTinyOutline(master_x_base + 2, label_y + 12, label);
}

static const ft2_ui_bitmap_asset_t *mixer_find_bitmap_asset(uint16_t id)
{
    for (uint16_t i = 0; i < ft2_ui_assets.bitmap_count; i++)
    {
        if (ft2_ui_assets.bitmaps[i].id == id)
            return &ft2_ui_assets.bitmaps[i];
    }
    return NULL;
}

static uint8_t *mixer_get_bitmap_pixels(uint16_t id, int32_t *w, int32_t *h)
{
    for (size_t i = 0; i < sizeof(mixerBitmapCache) / sizeof(mixerBitmapCache[0]); i++)
    {
        if (mixerBitmapCache[i].valid && mixerBitmapCache[i].id == id)
        {
            if (w) *w = mixerBitmapCache[i].w;
            if (h) *h = mixerBitmapCache[i].h;
            return mixerBitmapCache[i].pixels;
        }
    }

    const ft2_ui_bitmap_asset_t *asset = mixer_find_bitmap_asset(id);
    if (!asset || !asset->bmp || asset->fmt != FT2_UI_BMP_FMT_RLE4)
        return NULL;

    int32_t bmp_w = 0, bmp_h = 0;
    uint8_t *pixels = ft2_bmp_decode_rle4_to_pal(asset->bmp, &bmp_w, &bmp_h);
    if (!pixels) return NULL;

    for (size_t i = 0; i < sizeof(mixerBitmapCache) / sizeof(mixerBitmapCache[0]); i++)
    {
        if (!mixerBitmapCache[i].valid)
        {
            mixerBitmapCache[i].valid = true;
            mixerBitmapCache[i].id = id;
            mixerBitmapCache[i].pixels = pixels;
            mixerBitmapCache[i].w = bmp_w;
            mixerBitmapCache[i].h = bmp_h;
            break;
        }
    }

    if (w) *w = bmp_w;
    if (h) *h = bmp_h;
    return pixels;
}

static void drawMixerSchemaBitmaps(void)
{
    if (!mixerLayout.valid || !mixerLayout.desc || mixerLayout.desc->bitmaps.count == 0 || !mixerLayout.desc->bitmap_desc)
        return;

    for (uint16_t i = 0; i < mixerLayout.desc->bitmaps.count; i++)
    {
        const ft2_ui_bitmap_desc_t *d = &mixerLayout.desc->bitmap_desc[i];
        int32_t w = 0, h = 0;
        uint8_t *pixels = mixer_get_bitmap_pixels(d->bitmap_id, &w, &h);
        if (!pixels || w <= 0 || h <= 0)
            continue;

        if (d->x >= SCREEN_W || d->y >= SCREEN_H)
            continue;

        int32_t clip_w = w;
        int32_t clip_h = h;
        if ((int32_t)d->x + clip_w > SCREEN_W)
            clip_w = (int32_t)SCREEN_W - (int32_t)d->x;
        if ((int32_t)d->y + clip_h > SCREEN_H)
            clip_h = (int32_t)SCREEN_H - (int32_t)d->y;

        if (clip_w <= 0 || clip_h <= 0)
            continue;

        blitClipX(d->x, d->y, pixels, (uint16_t)w, (uint16_t)clip_h, (uint16_t)clip_w);
    }
}

static void checkMixerButtons(void)
{
    if (!mouse.leftButtonReleased)
        return;

    for (uint8_t ch = 0; ch < NUM_STRIPS; ch++)
    {
        const ft2_ui_mixer_scope_desc_t *scope_desc = mixerLayout.scope[ch];
        uint16_t scope_x = scope_desc ? scope_desc->x : (MIXER_BASE_X + (ch * STRIP_SPACING) + SCOPE_X_OFFSET);
        uint16_t scope_y = scope_desc ? scope_desc->y : SCOPE_Y;
        uint16_t scope_w = scope_desc ? scope_desc->w : SCOPE_W;
        uint16_t scope_h = scope_desc ? scope_desc->h : SCOPE_H;

        /* Scope click -> mute */
        if (mouse.x >= scope_x && mouse.x < scope_x + scope_w && mouse.y >= scope_y && mouse.y < scope_y + scope_h)
        {
            editor.channelMuted[ch] ^= 1;
            setChannelMute(ch, editor.channelMuted[ch]);
            mouse.leftButtonReleased = false;
        }
    }
}

/* ------------------------------------------------------------------- */
/*                        Main entry points                            */
/* ------------------------------------------------------------------- */
void pbOpenMixer(void)
{
    if (ui.mixerScreenShown)
        return;

    const uint8_t numMixerChans = 16; /* create widgets for all channels */

    init_mixer_layout_cache();

    mixerWindowOpen();               /* hides normal GUI */
    setupMixerWidgets(numMixerChans);
    // Apply persistent mixer/DSP cache to GUI
    applyMixerStateToGUI();
    // Debug: print cached mixer/DSP parameters
    printf("[Mixer Debug] Cached mixer/DSP state:\n");
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
        printf(" Ch %d: fader=%.2f pan=%.2f\n", ch, gMixerState[ch].fader, gMixerState[ch].pan);
        for (int slot = 0; slot < DSP_MAX_SLOTS; slot++) {
            dspEffectInstance_t *eff = &gMixerState[ch].effects[slot];
            printf("  DSP ch%d slot%d: type=%d enabled=%d\n", ch, slot, eff->type, eff->enabled);
        }
    }
    printf(" Master gain=%.2f\n", gMasterState.masterGain);
    for (int slot = 0; slot < DSP_MAX_SLOTS; slot++) {
        dspEffectInstance_t *eff = &gMasterState.effects[slot];
        printf("  DSP master slot%d: type=%d enabled=%d\n", slot, eff->type, eff->enabled);
    }
    /* initial draw */
    mixerScreenFrame();

    /* capture mouse focus */
    mouse.lastUsedObjectType = OBJECT_SCROLLBAR;
    mouse.lastUsedObjectID = SB_MIX_GAIN_0;

    ui.mixerScreenShown = true;
}

void mixerScreenFrame(void)
{
    if (!ui.mixerScreenShown)
        return;

    /* Handle clicks in inline DSP region first */
    handleInlineDSPClicks();

    drawMixerBox();
    checkMixerButtons();

    /* Draw widgets on top of box */
    drawScrollBar(MASTER_GAIN_ID);
    drawPushButton(MIXER_EXIT_BUTTON_ID);
    drawPushButton(PB_RES_3);
    drawPushButton(PB_RES_4);
    drawPushButton(PB_RES_5);

    for (uint8_t ch = 0; ch < NUM_STRIPS; ch++)
    {
        drawScrollBar(MIX_GAIN_BASE + ch);
        // Only draw channel-strip pan when not showing inline DSP for this channel
        if (!(selectedDSPChannel >= 0 && ch == selectedDSPChannel))
            drawScrollBar(MIX_PAN_BASE  + ch);
        drawPushButton(PB_MIX_GAIN_UP_0 + ch);
        drawPushButton(PB_MIX_GAIN_DN_0 + ch);
        drawPushButton(PB_MIX_DSP_0     + ch);
        drawPushButton(PB_MIX_MUTE_0    + ch);
    }

    /* draw page toggle button */
    drawPushButton(MIXER_PAGE_BUTTON_ID);

    /* Inline DSP UI: under mixer strips, show only selected channel */
    if (selectedDSPChannel >= 0)
    {
        /* Clear DSP UI region */
        uint16_t y0 = DSP_UI_Y + DSP_UI_SLOT_H;
        uint16_t h0 = DSP_UI_SLOT_H + DSP_UI_PARAM_ROW_H * DSP_MAX_SLOTS + 12;
        uint16_t clipW = SCREEN_W;
        if (mixerLayout.frame_mixer)
            clipW = mixerLayout.frame_mixer->x + mixerLayout.frame_mixer->w;
        else
            clipW = 478; /* mixer strip frame right edge */
        fillRect(0, y0, clipW, h0, PAL_DESKTOP);
        

        /* Draw slot selection on left menu width */
        drawInlineDspSlots((uint8_t)selectedDSPChannel);
        /* Draw parameter sliders on right remaining area */
        drawMixerInlineParams((uint8_t)selectedDSPChannel);

        // Draw horizontal pan slider inside inline DSP UI, below the param rows
        if (selectedDSPChannel != 255)
        {
            int numParams = 0;
            dspEffectInstance_t *eff = &stereoMixerCh[CHANNEL_TO_PAIR_IDX(selectedDSPChannel)].effects[selectedDSPSlot];
            const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
            if (pi)
            {
                uint16_t slotAreaW = INLINE_MENU_W;
                uint16_t paramX0  = slotAreaW + 4;
                uint16_t paramW   = SCREEN_W - paramX0 - 12;
                // place pan slider just below last parameter row + small padding
                uint16_t panY = DSP_UI_Y + DSP_UI_SLOT_H + 4 + numParams * DSP_UI_PARAM_ROW_H + 4;
                scrollBar_t *sPan = &scrollBars[MIX_PAN_BASE - 2 + selectedDSPChannel];
                // back up original widget geometry
                uint16_t oldX = sPan->x, oldY = sPan->y, oldW = sPan->w, oldH = sPan->h;
                // override for inline placement
                sPan->x = paramX0; sPan->y = panY; sPan->w = paramW; sPan->h = PAN_SLIDER_H + 6;
                drawScrollBar(MIX_PAN_BASE + 2 + selectedDSPChannel);
                // restore original geometry
                sPan->x = oldX; sPan->y = oldY; sPan->w = oldW; sPan->h = oldH;
            }
        }
    }

    drawInlineDspMenu();
}

void hideMixerScreen(void)
{
    if (!ui.mixerScreenShown)
        return;

    cacheMixerStateFromGUI();
    const uint8_t numMixerChans = MAX_STEREO_PAIRS;
    hideMixerWidgets(numMixerChans);

    hidePushButton(MIXER_EXIT_BUTTON_ID);
    hidePushButton(PB_RES_3);
    hidePushButton(PB_RES_4);
    hidePushButton(PB_RES_5);
    hidePushButton(PB_RES_6);
    hidePushButton(PB_RES_7);
    hidePushButton(MIXER_PAGE_BUTTON_ID);
    hideScrollBar(MASTER_GAIN_ID);
    hideScrollBar(SB_DSP_PARAM_SCROLL);

    /* Make absolutely sure no mixer graphics remain and widgets can't be
       clicked afterwards. Clear the entire mixer area before restoring GUI. */
    fillRect(0, 0, SCREEN_W, SCREEN_H, PAL_DESKTOP);

    mixerWindowClose();
    ui.mixerScreenShown = false;
}

static void hideMixerWidgets(uint8_t numChans)
{
    for (uint8_t ch = 0; ch < numChans; ch++)
    {
        hidePushButton(PB_MIX_GAIN_UP_0  + ch);
        hidePushButton(PB_MIX_GAIN_DN_0  + ch);
        hidePushButton(PB_MIX_DSP_0      + ch);
        hidePushButton(PB_MIX_MUTE_0     + ch);
        hideScrollBar(MIX_GAIN_BASE + ch);
        hideScrollBar(MIX_PAN_BASE  + ch);
    }

    /* Recalculate X positions of visible widgets to keep them within viewport */
    uint8_t startCh = mixerPage * NUM_STRIPS;
    for (uint8_t visIdx = 0; visIdx < NUM_STRIPS; visIdx++)
    {
        uint8_t ch = startCh + visIdx;

        /* Re-use setupChannelStrip to reposition gadgets without altering values */
        setupChannelStrip(ch);
    }
}

/* Externally used mixer screen functions */

static void setMixerPage(uint8_t page) { (void)page; /* paging disabled */ }

static void pbMixerPageToggle(void)
{
    /* paging disabled */
}

static void pbMasterGainUp(void)
{
    int32_t pos = getScrollBarPos(MASTER_GAIN_ID);
    if (pos > 0)
        setScrollBarPos(MASTER_GAIN_ID, pos - 1, true);
}

static void pbMasterGainDown(void)
{
    int32_t pos = getScrollBarPos(MASTER_GAIN_ID);
    if (pos < GAIN_SLIDER_END)
        setScrollBarPos(MASTER_GAIN_ID, pos + 1, true);
}

static void pbMasterFX(void)
{
    if (selectedDSPChannel == 255) { selectedDSPChannel = -1; }
    else                           { selectedDSPChannel = 255; selectedDSPSlot = 0; }
}

/* Handle mouse clicks in the inline DSP area */
static void handleInlineDSPClicks(void)
{
    if (selectedDSPChannel < 0 || !mouse.leftButtonReleased)
        return;
    // If menu open, handle selection or cancel (dropdown under slot)
    if (inlineMenuShown)
    {
        // Rolling selector window of 9 effects centered on current type
        dspEffectInstance_t *eff =
            (selectedDSPChannel == 255)
                ? &masterEffects[inlineMenuSlot]
                : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(selectedDSPChannel)].effects[inlineMenuSlot];
        int current = eff->type;
        int total = DSP_TYPE_COUNT;
        int visible = total < 9 ? total : 9;
        const ft2_ui_dsp_slot_desc_t *slot_desc = (inlineMenuSlot >= 0 && inlineMenuSlot < DSP_MAX_SLOTS)
            ? mixerLayout.dsp_slot[inlineMenuSlot]
            : NULL;
        uint16_t menuX = mixerLayout.dsp_menu ? mixerLayout.dsp_menu->x : 20;
        uint16_t menuW = mixerLayout.dsp_menu ? mixerLayout.dsp_menu->w : (INLINE_MENU_W - 8);
        uint16_t menuY = slot_desc ? (slot_desc->y + slot_desc->h) : (DSP_UI_Y + inlineMenuSlot * DSP_UI_SLOT_H + DSP_UI_SLOT_H);
        uint16_t menuH = mixerLayout.dsp_menu ? mixerLayout.dsp_menu->h : (uint16_t)((visible + 2) * INLINE_MENU_ITEM_H + 4);
        int max_visible = (int)((menuH - (2 * INLINE_MENU_ITEM_H) - 4) / INLINE_MENU_ITEM_H);
        if (max_visible < 1) max_visible = 1;
        if (visible > max_visible) visible = max_visible;
        int half = visible / 2;
        int windowStart = current - half;
        if (windowStart < 0) windowStart = 0;
        if (windowStart > total - visible) windowStart = total - visible;
        // Up arrow region
        if (mouse.x >= menuX && mouse.x < menuX + menuW &&
            mouse.y >= menuY && mouse.y < menuY + INLINE_MENU_ITEM_H)
        {
            if (current > 0)
            {
                dspFreeEffect(eff);
                dspInitEffect(eff, (dspEffectType_t)(current - 1), audio.freq);
                dspParamScrollOffset = 0;
                cacheMixerStateFromGUI();
            }
            mouse.leftButtonReleased = false;
            return;
        }
        // Down arrow region
        uint16_t downY = menuY + INLINE_MENU_ITEM_H + visible * INLINE_MENU_ITEM_H;
        if (mouse.x >= menuX && mouse.x < menuX + menuW &&
            mouse.y >= downY && mouse.y < downY + INLINE_MENU_ITEM_H)
        {
            if (current < total - 1)
            {
                dspFreeEffect(eff);
                dspInitEffect(eff, (dspEffectType_t)(current + 1), audio.freq);
                dspParamScrollOffset = 0;
                cacheMixerStateFromGUI();
            }
            mouse.leftButtonReleased = false;
            return;
        }
        // Item selection region
        uint16_t itemsY = menuY + INLINE_MENU_ITEM_H;
        if (mouse.x >= menuX && mouse.x < menuX + menuW &&
            mouse.y >= itemsY && mouse.y < itemsY + visible * INLINE_MENU_ITEM_H)
        {
            int idx = (mouse.y - itemsY) / INLINE_MENU_ITEM_H;
            int effIdx = windowStart + idx;
            if (effIdx >= 0 && effIdx < total)
            {
                dspFreeEffect(eff);
                dspInitEffect(eff, (dspEffectType_t)effIdx, audio.freq);
                dspParamScrollOffset = 0;
                cacheMixerStateFromGUI();
            }
            inlineMenuShown = false;
            mouse.leftButtonReleased = false;
            return;
        }
        // Click outside: close menu
        inlineMenuShown = false;
        mouse.leftButtonReleased = false;
        return;
    }
    // Slot navigation and menu open (layout: menu width / remaining area)
    // check if click in slot rows
    for (int i = 0; i < DSP_MAX_SLOTS; i++)
    {
        const ft2_ui_dsp_slot_desc_t *slot_desc = mixerLayout.dsp_slot[i];
        uint16_t sx = slot_desc ? slot_desc->x : 10;
        uint16_t sy = slot_desc ? slot_desc->y : (DSP_UI_Y + i * DSP_UI_SLOT_H);
        uint16_t sw = slot_desc ? slot_desc->w : (INLINE_MENU_W - 8);
        uint16_t sh = slot_desc ? slot_desc->h : DSP_UI_SLOT_H;
        if (mouse.x >= sx && mouse.x < sx + sw && mouse.y >= sy && mouse.y < sy + sh)
        {
            if (i == selectedDSPSlot)
            {
                inlineMenuShown = true;
                inlineMenuSlot  = i;
            }
            else
            {
                selectedDSPSlot = i;
            }
            mouse.leftButtonReleased = false;
            return;
        }
    }
    // Parameter clicks (unchanged)
    int numParams = 0;
    dspEffectInstance_t *eff = (selectedDSPChannel == 255)
        ? &masterEffects[selectedDSPSlot]
        : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(selectedDSPChannel)].effects[selectedDSPSlot];
    const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
    if (!pi)
        return;
    int visibleRows = getVisibleDspParamRows();
    if (visibleRows <= 0) return;
    for (int i = 0; i < numParams && i < visibleRows; i++)
    {
        int paramIdx = dspParamScrollOffset + i;
        if (paramIdx >= numParams) break;
        const ft2_ui_dsp_param_desc_t *param_desc = mixerLayout.dsp_param[i];
        uint16_t px = param_desc ? param_desc->x : (INLINE_MENU_W + 10);
        uint16_t py = param_desc ? param_desc->y : (DSP_UI_Y + DSP_UI_SLOT_H + i * DSP_UI_PARAM_ROW_H + 12);
        uint16_t pw = param_desc ? param_desc->w : (SCREEN_W - px - 10);
        uint16_t ph = param_desc ? param_desc->h : 11;
        if (mouse.y >= py && mouse.y <= py + ph &&
            mouse.x >= px && mouse.x <= px + pw)
        {
            float norm = (float)(mouse.x - px) / pw;
            norm = fmaxf(0.0f, fminf(1.0f, norm));
            float newVal = pi[paramIdx].min + norm * (pi[paramIdx].max - pi[paramIdx].min);
            float *p = getParamPtr(eff, paramIdx);
            if (p) *p = newVal;
            mouse.leftButtonReleased = false;
            return;
        }
    }
}

/* Draw DSP slot buttons for the selected channel */
static void drawInlineDspSlots(uint8_t ch)
{
    for (int i = 0; i < DSP_MAX_SLOTS; i++)
    {
        const ft2_ui_dsp_slot_desc_t *slot_desc = mixerLayout.dsp_slot[i];
        uint16_t x0 = slot_desc ? slot_desc->x : 10;
        uint16_t y = slot_desc ? slot_desc->y : (DSP_UI_Y + i * DSP_UI_SLOT_H);
        uint16_t slotW = slot_desc ? slot_desc->w : (INLINE_MENU_W - 8);
        uint16_t slotH = slot_desc ? slot_desc->h : DSP_UI_SLOT_H;
        drawFramework(x0, y, slotW, slotH, FRAMEWORK_TYPE2);
        dspEffectInstance_t *eff = (ch == 255)
            ? &masterEffects[i]
            : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(ch)].effects[i];
        const char *name = getEffectName(eff);
        int tw = textWidth(name);
        // Draw selection highlight before text so text remains visible
        if (i == selectedDSPSlot)
        {
            drawFramework(x0, y, slotW, slotH, FRAMEWORK_TYPE1);
        }
        // Center text within padded slot rectangle
        uint16_t textX = x0 + (slotW > (uint16_t)tw ? (slotW - tw) / 2 : 0);
        textOutTiny(textX, y + 2, (char *)name, video.palette[PAL_FORGRND]);
    }
}

/* ---------------------------------------------------------------------------
** DSP inline parameter scrollbars
** ------------------------------------------------------------------------ */

#define DSP_SB_BASE SB_DSP_PARAM_0
#define DSP_SB_END  SB_DSP_PARAM_15
#define DSP_SB_COUNT (DSP_SB_END - DSP_SB_BASE + 1)
#define DSP_SB_SCROLL SB_DSP_PARAM_SCROLL

/* Map scrollbar position (0..999) <-> normalized 0..1 */
static inline float sbPosToNorm(uint32_t pos) { return (float)pos / 999.0f; }
static inline uint32_t normToSbPos(float norm) {
    if (norm < 0.0f) norm = 0.0f; else if (norm > 1.0f) norm = 1.0f;
    return (uint32_t)lrintf(norm * 999.0f);
}

static void sbDspParamScrollPos(uint32_t pos)
{
    int numParams = 0;
    dspEffectInstance_t *eff = (selectedDSPChannel == 255)
        ? &masterEffects[selectedDSPSlot]
        : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(selectedDSPChannel)].effects[selectedDSPSlot];
    const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
    if (!pi)
        return;

    int visibleRows = getVisibleDspParamRows();
    if (visibleRows <= 0)
        return;

    int maxOffset = numParams - visibleRows;
    if (maxOffset < 0) maxOffset = 0;
    if ((int)pos > maxOffset) pos = maxOffset;
    dspParamScrollOffset = (int)pos;
}

static void pbDspParamScrollUp(void)
{
    int32_t pos = getScrollBarPos(DSP_SB_SCROLL);
    if (pos > 0)
        setScrollBarPos(DSP_SB_SCROLL, pos - 1, true);
}

static void pbDspParamScrollDown(void)
{
    int32_t pos = getScrollBarPos(DSP_SB_SCROLL);
    int32_t end = (int32_t)scrollBars[DSP_SB_SCROLL].end;
    if (pos < end)
        setScrollBarPos(DSP_SB_SCROLL, pos + 1, true);
}

/* Generic setter used by all 16 callbacks */
static void setDspParamFromPos(uint8_t paramIdx, uint32_t pos)
{
    if (paramIdx >= DSP_SB_COUNT) return;

    /* Fetch current effect */
    int numParams = 0;
    dspEffectInstance_t *eff = (selectedDSPChannel == 255)
        ? &masterEffects[selectedDSPSlot]
        : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(selectedDSPChannel)].effects[selectedDSPSlot];
    const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
    int paramIndex = (int)paramIdx + dspParamScrollOffset;
    if (!pi || paramIndex >= numParams) return;

    float norm = sbPosToNorm(pos);
    const dspParamInfo_t *info = &pi[paramIndex];
    float newVal = info->min + norm * (info->max - info->min);
    float *p = getParamPtr(eff, paramIndex);
    if (p) *p = newVal;
}

/* Generate 16 callbacks */
#define GEN_DSP_PARAM_CB(idx) static void sbDspParamPos_##idx(uint32_t pos) { setDspParamFromPos(idx, pos); cacheMixerStateFromGUI(); }
GEN_DSP_PARAM_CB(0)  GEN_DSP_PARAM_CB(1)  GEN_DSP_PARAM_CB(2)  GEN_DSP_PARAM_CB(3)
GEN_DSP_PARAM_CB(4)  GEN_DSP_PARAM_CB(5)  GEN_DSP_PARAM_CB(6)  GEN_DSP_PARAM_CB(7)
GEN_DSP_PARAM_CB(8)  GEN_DSP_PARAM_CB(9)  GEN_DSP_PARAM_CB(10) GEN_DSP_PARAM_CB(11)
GEN_DSP_PARAM_CB(12) GEN_DSP_PARAM_CB(13) GEN_DSP_PARAM_CB(14) GEN_DSP_PARAM_CB(15)

static void (*dspParamCbs[DSP_SB_COUNT])(uint32_t) = {
    sbDspParamPos_0,  sbDspParamPos_1,  sbDspParamPos_2,  sbDspParamPos_3,
    sbDspParamPos_4,  sbDspParamPos_5,  sbDspParamPos_6,  sbDspParamPos_7,
    sbDspParamPos_8,  sbDspParamPos_9,  sbDspParamPos_10, sbDspParamPos_11,
    sbDspParamPos_12, sbDspParamPos_13, sbDspParamPos_14, sbDspParamPos_15 };

/* Draw parameter scrollbars for the selected DSP slot */
static void drawMixerInlineParams(uint8_t ch)
{
    int numParams = 0;
    dspEffectInstance_t *eff = (ch == 255)
        ? &masterEffects[selectedDSPSlot]
        : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(ch)].effects[selectedDSPSlot];
    const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
    if (!pi)
        return;

    int visibleRows = getVisibleDspParamRows();
    if (visibleRows <= 0)
        return;

    int maxOffset = numParams - visibleRows;
    if (maxOffset < 0) maxOffset = 0;
    if (dspParamScrollOffset > maxOffset) dspParamScrollOffset = maxOffset;

    /* Prepare visible scrollbars */
    for (int i = 0; i < DSP_SB_COUNT; i++)
        hideScrollBar(DSP_SB_BASE + i);
    if (mixerLayout.frame_params)
    {
        drawFramework(mixerLayout.frame_params->x, mixerLayout.frame_params->y,
                      mixerLayout.frame_params->w, mixerLayout.frame_params->h, FRAMEWORK_TYPE1);
    }
    else
    {
        const uint16_t fallback_x = INLINE_MENU_W + 6;
        drawFramework(fallback_x, DSP_UI_Y + DSP_UI_SLOT_H, 390, 150, FRAMEWORK_TYPE1);
    }
    for (int i = 0; i < numParams && i < visibleRows && i < DSP_SB_COUNT; i++)
    {
        int paramIdx = dspParamScrollOffset + i;
        if (paramIdx >= numParams)
            break;
        const ft2_ui_dsp_param_desc_t *param_desc = mixerLayout.dsp_param[i];
        uint16_t x = param_desc ? param_desc->x : (INLINE_MENU_W + 6 + 10);
        uint16_t y = param_desc ? param_desc->y : (DSP_UI_Y + DSP_UI_SLOT_H + i * DSP_UI_PARAM_ROW_H + 12);
        uint16_t w = param_desc ? param_desc->w : 360;
        uint16_t h = param_desc ? param_desc->h : 11;
        int label_x = (int)x - 10;
        int label_y = (int)y - 10;
        if (label_x < 0) label_x = 0;
        if (label_y < 0) label_y = 0;
        textOutTiny((uint16_t)label_x, (uint16_t)label_y, (char *)pi[paramIdx].name, video.palette[PAL_FORGRND]);

        /* Configure scrollbar */
        const uint16_t sbID = DSP_SB_BASE + i;
        scrollBar_t *s = &scrollBars[sbID];
        s->x = x;
        s->y = y;
        s->w = w;
        s->h = h;
        s->type = SCROLLBAR_HORIZONTAL;
        s->thumbType = SCROLLBAR_DYNAMIC_THUMB_SIZE;
        s->callbackFunc = dspParamCbs[i];
        s->visible = true;

        setScrollBarPageLength(sbID, 1);
        setScrollBarEnd(sbID, 999);

        float *paramPtr = getParamPtr(eff, paramIdx);
        if (!paramPtr)
            continue;
        float val = *paramPtr;
        float norm = (val - pi[paramIdx].min) / (pi[paramIdx].max - pi[paramIdx].min);
        uint32_t pos = normToSbPos(norm);
        setScrollBarPos(sbID, pos, false);

        drawScrollBar(sbID);
    }

    /* Vertical scroll for parameter list */
    {
        scrollBar_t *s = &scrollBars[DSP_SB_SCROLL];
        memset(s, 0, sizeof(scrollBar_t));

        uint16_t frameX = mixerLayout.frame_params ? mixerLayout.frame_params->x : (INLINE_MENU_W + 6);
        uint16_t frameY = mixerLayout.frame_params ? mixerLayout.frame_params->y : (DSP_UI_Y + DSP_UI_SLOT_H);
        uint16_t frameW = mixerLayout.frame_params ? mixerLayout.frame_params->w : 390;
        uint16_t frameH = mixerLayout.frame_params ? mixerLayout.frame_params->h : 150;

        if (mixerLayout.sb_dsp_param_scroll)
        {
            s->x = mixerLayout.sb_dsp_param_scroll->x;
            s->y = mixerLayout.sb_dsp_param_scroll->y;
            s->w = mixerLayout.sb_dsp_param_scroll->w;
            s->h = mixerLayout.sb_dsp_param_scroll->h;
        }
        else
        {
            s->x = frameX + frameW - 14;
            s->y = frameY + 14;
            s->w = 12;
            s->h = (frameH > 28) ? (frameH - 28) : frameH;
        }
        s->type = SCROLLBAR_VERTICAL;
        s->thumbType = SCROLLBAR_DYNAMIC_THUMB_SIZE;
        s->callbackFunc = sbDspParamScrollPos;
        s->visible = (maxOffset > 0);

        setScrollBarPageLength(DSP_SB_SCROLL, 1);
        setScrollBarEnd(DSP_SB_SCROLL, (uint32_t)maxOffset);
        setScrollBarPos(DSP_SB_SCROLL, (uint32_t)dspParamScrollOffset, false);

        if (s->visible)
        {
            drawScrollBar(DSP_SB_SCROLL);
        }

        // Nudge buttons (up/down)
        pushButton_t *pbUp = &pushButtons[PB_RES_6];
        pushButton_t *pbDn = &pushButtons[PB_RES_7];
        bool showNudge = mixerLayout.sb_dsp_param_scroll ? mixerLayout.sb_dsp_param_scroll->has_nudge_buttons : true;
        pbUp->x = s->x - 1;
        pbUp->y = (mixerLayout.sb_dsp_param_scroll ? (s->y - 12) : (frameY + 2));
        pbUp->w = 14;
        pbUp->h = 11;
        pbUp->caption = ARROW_UP_STRING;
        pbUp->callbackFuncOnUp = pbDspParamScrollUp;
        pbUp->visible = s->visible && showNudge;
        pbDn->x = s->x - 1;
        pbDn->y = (mixerLayout.sb_dsp_param_scroll ? (s->y + s->h + 1) : (frameY + frameH - 13));
        pbDn->w = 14;
        pbDn->h = 11;
        pbDn->caption = ARROW_DOWN_STRING;
        pbDn->callbackFuncOnUp = pbDspParamScrollDown;
        pbDn->visible = s->visible && showNudge;

        if (pbUp->visible) drawPushButton(PB_RES_6);
        if (pbDn->visible) drawPushButton(PB_RES_7);
    }
}

// Draw the inline effect-selection menu
static void drawInlineDspMenu(void)
{
    if (!inlineMenuShown || inlineMenuSlot < 0) return;
    // Rolling selector window of 9 effects centered on current type
    dspEffectInstance_t *eff =
        (selectedDSPChannel == 255)
            ? &masterEffects[inlineMenuSlot]
            : &stereoMixerCh[CHANNEL_TO_PAIR_IDX(selectedDSPChannel)].effects[inlineMenuSlot];
    int current = eff->type;
    int total = DSP_TYPE_COUNT;
    int visible = total < 9 ? total : 9;
    const ft2_ui_dsp_slot_desc_t *slot_desc = (inlineMenuSlot >= 0 && inlineMenuSlot < DSP_MAX_SLOTS)
        ? mixerLayout.dsp_slot[inlineMenuSlot]
        : NULL;
    uint16_t menuX = mixerLayout.dsp_menu ? mixerLayout.dsp_menu->x : 10;
    uint16_t menuW = mixerLayout.dsp_menu ? mixerLayout.dsp_menu->w : (INLINE_MENU_W - 8);
    uint16_t menuY = slot_desc ? (slot_desc->y + slot_desc->h) : (DSP_UI_Y + inlineMenuSlot * DSP_UI_SLOT_H + DSP_UI_SLOT_H);
    uint16_t menuH = mixerLayout.dsp_menu ? mixerLayout.dsp_menu->h : (uint16_t)((visible + 2) * INLINE_MENU_ITEM_H + 4);
    int max_visible = (int)((menuH - (2 * INLINE_MENU_ITEM_H) - 4) / INLINE_MENU_ITEM_H);
    if (max_visible < 1) max_visible = 1;
    if (visible > max_visible) visible = max_visible;
    int half = visible / 2;
    int windowStart = current - half;
    if (windowStart < 0) windowStart = 0;
    if (windowStart > total - visible) windowStart = total - visible;
    uint16_t h = menuH;
    drawFramework(menuX, menuY, menuW, h, FRAMEWORK_TYPE1);
    // Up arrow
    textOutTiny(menuX + (menuW - textWidth("up")) / 2 - 4, menuY + 2, "up" , video.palette[PAL_FORGRND]);
    // List items
    for (int i = 0; i < visible; i++)
    {
        int idx = windowStart + i;
        uint16_t y = menuY + INLINE_MENU_ITEM_H + 2 + i * INLINE_MENU_ITEM_H;
        if (idx == current)
            drawFramework(menuX, y - 2, menuW, INLINE_MENU_ITEM_H + 2, FRAMEWORK_TYPE1);
        textOutTiny(menuX + 4, y, inlineEffectNames[idx], video.palette[PAL_FORGRND]);
    }
    // Down arrow
    uint16_t downY = menuY + INLINE_MENU_ITEM_H + 2 + visible * INLINE_MENU_ITEM_H;
    textOutTiny(menuX + (menuW - textWidth("down")) / 2 - 4, downY + 2, "down" , video.palette[PAL_FORGRND]);
}
