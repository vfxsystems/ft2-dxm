#include "ft2_v2_complete_layout.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_header.h"
#include "ft2_inst_ed.h"
#include "ft2_mouse.h"
#include "ft2_palette.h"
#include "ft2_structs.h"
#include "ft2_v2_complete_layout_schema.h"
#include "ft2_video.h"

extern editor_t editor;
extern instr_t *instr[128 + 4];

V2CompleteLayout *g_active_v2_layout = NULL;

static const char *const k_page_names[V2_PAGE_COUNT] =
{
    "Voice / Osc",
    "Filters",
    "LFO / Envelopes",
    "FX",
    "Master",
    "Mod Matrix"
};

static const int k_env_param_ids[2][5] =
{
    { 32, 33, 34, 35, 36 },
    { 38, 39, 40, 41, 42 }
};

#define V2_PREVIEW_SAMPLE_COUNT   256
#define V2_PREVIEW_HARMONICS      12
#define V2_PREVIEW_OSC_COUNT      3

typedef struct
{
    int mode;
    bool ring;
    float pitchSemis;
    float freqRatio;
    float color;
    float gain;
} v2_preview_osc_t;

static void v2_draw_line(int x0, int y0, int x1, int y1, uint8_t color);
void v2_update_all_widgets_from_synth(V2CompleteLayout *layout);

static int v2_clampi(int value, int minValue, int maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static float v2_clampf(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static float v2_wrap01(float phase)
{
    phase -= floorf(phase);
    if (phase < 0.0f)
        phase += 1.0f;
    return phase;
}

static int v2_current_instr_id(const V2CompleteLayout *layout)
{
    if (layout && layout->active_instrument_id >= 1 && layout->active_instrument_id <= MAX_INST)
        return layout->active_instrument_id;

    if (editor.curInstr >= 1 && editor.curInstr <= MAX_INST)
        return editor.curInstr;

    return 0;
}

static int v2_get_patch_param_raw(const V2CompleteLayout *layout, int paramId)
{
    const Ft2V2ParamInfo *info;
    int instrID;
    float normalized;
    int raw;

    instrID = v2_current_instr_id(layout);
    if (instrID <= 0)
        return 0;

    info = ft2_v2_get_param_info(paramId);
    if (!info)
        return 0;

    normalized = ft2_v2_get_param_for_instrument(instrID, paramId);
    raw = info->min + (int)lroundf(v2_clampf(normalized, 0.0f, 1.0f) * (float)(info->max - info->min));
    return v2_clampi(raw, info->min, info->max);
}

static const char *v2_preview_osc_mode_name(int mode)
{
    switch (mode & 7)
    {
        default:
        case 0: return "Off";
        case 1: return "SawTri";
        case 2: return "Pulse";
        case 3: return "Sin";
        case 4: return "Noise";
        case 5: return "FM";
        case 6: return "AuxA";
        case 7: return "AuxB";
    }
}

static float v2_preview_noise(int sampleIndex)
{
    uint32_t x = (uint32_t)(sampleIndex + 1) * 0x45d9f3bu;
    x ^= x >> 16;
    x *= 0x45d9f3bu;
    x ^= x >> 16;
    return ((float)(x & 0xFFFFu) / 32767.5f) - 1.0f;
}

static float v2_preview_eval_osc_sample(const v2_preview_osc_t *osc, float phase, float currentMix, int sampleIndex)
{
    float out = 0.0f;
    float shapedPhase;
    float width;

    if (!osc || osc->gain <= 0.0f || osc->mode <= 0)
        return 0.0f;

    shapedPhase = v2_wrap01(phase * osc->freqRatio);
    width = v2_clampf(osc->color, 0.03f, 0.97f);

    switch (osc->mode & 7)
    {
        default:
        case 0:
            out = 0.0f;
            break;

        case 1: /* Saw/Tri */
            if (shapedPhase < width)
                out = -1.0f + (2.0f * shapedPhase / width);
            else
                out = 1.0f - (2.0f * (shapedPhase - width) / (1.0f - width));
            break;

        case 2: /* Pulse */
            out = (shapedPhase < width) ? 1.0f : -1.0f;
            break;

        case 3: /* Sin */
            out = sinf(shapedPhase * 6.28318530718f);
            break;

        case 4: /* Noise */
            out = v2_preview_noise(sampleIndex + (int)(osc->freqRatio * 29.0f));
            break;

        case 5: /* FM */
            out = sinf((shapedPhase * 6.28318530718f) + (currentMix * 3.14159265359f));
            break;

        case 6: /* AuxA */
        case 7: /* AuxB */
            out = 0.0f;
            break;
    }

    return out * osc->gain;
}

static float v2_preview_apply_filter(float input, int mode, float cutoff, float resonance, float *state1, float *state2)
{
    float drive;
    float a;
    float low;
    float high;
    float band;

    if (mode <= 0)
        return input;

    drive = 1.0f + (resonance * 0.35f);
    a = 0.035f + (cutoff * cutoff * 0.85f);

    *state1 += a * ((input * drive) - *state1);
    *state2 += a * (*state1 - *state2);

    low = *state2;
    high = input - low;
    band = *state1 - *state2;

    switch (mode & 7)
    {
        default:
        case 0: return input;
        case 1: return low;
        case 2: return band;
        case 3: return high;
        case 4: return input - band;
        case 5: return input;
        case 6: return v2_clampf((low * 1.2f) - (band * resonance * 0.3f), -1.0f, 1.0f);
        case 7: return v2_clampf((high * 1.2f) + (band * resonance * 0.25f), -1.0f, 1.0f);
    }
}

static void v2_generate_waveform_preview(const V2CompleteLayout *layout, float *samples, float *harmonics)
{
    v2_preview_osc_t osc[V2_PREVIEW_OSC_COUNT];
    int i;
    int sampleIndex;
    int voiceTransposeRaw;
    int routingRaw;
    int filter1Mode;
    int filter2Mode;
    float filter1Cutoff;
    float filter2Cutoff;
    float filter1Reso;
    float filter2Reso;
    float routingBalance;
    float oscSync;
    float phaseOffset;
    float lp11 = 0.0f, lp12 = 0.0f;
    float lp21 = 0.0f, lp22 = 0.0f;

    if (!samples || !harmonics)
        return;

    memset(samples, 0, sizeof(float) * V2_PREVIEW_SAMPLE_COUNT);
    memset(harmonics, 0, sizeof(float) * V2_PREVIEW_HARMONICS);

    for (i = 0; i < V2_PREVIEW_OSC_COUNT; ++i)
    {
        const int base = 2 + (i * 6);
        const int pitchRaw = v2_get_patch_param_raw(layout, base + 2);
        const int detuneRaw = v2_get_patch_param_raw(layout, base + 3);

        osc[i].mode = v2_get_patch_param_raw(layout, base + 0);
        osc[i].ring = (i > 0) && (v2_get_patch_param_raw(layout, base + 1) > 0);
        osc[i].pitchSemis = 0.0f;
        osc[i].freqRatio = 1.0f;
        osc[i].color = (float)v2_get_patch_param_raw(layout, base + 4) / 127.0f;
        osc[i].gain = (float)v2_get_patch_param_raw(layout, base + 5) / 127.0f;

        osc[i].pitchSemis = (float)(pitchRaw - 64) + ((float)(detuneRaw - 64) / 128.0f);
        osc[i].freqRatio = powf(2.0f, osc[i].pitchSemis / 12.0f);
        osc[i].freqRatio = v2_clampf(osc[i].freqRatio, 0.125f, 8.0f);
    }

    voiceTransposeRaw = v2_get_patch_param_raw(layout, 1);
    routingRaw = v2_get_patch_param_raw(layout, 26);
    filter1Mode = v2_get_patch_param_raw(layout, 20);
    filter2Mode = v2_get_patch_param_raw(layout, 23);
    filter1Cutoff = (float)v2_get_patch_param_raw(layout, 21) / 127.0f;
    filter2Cutoff = (float)v2_get_patch_param_raw(layout, 24) / 127.0f;
    filter1Reso = (float)v2_get_patch_param_raw(layout, 22) / 127.0f;
    filter2Reso = (float)v2_get_patch_param_raw(layout, 25) / 127.0f;
    routingBalance = (float)v2_get_patch_param_raw(layout, 27) / 127.0f;
    oscSync = (float)v2_get_patch_param_raw(layout, 58) / 7.0f;

    phaseOffset = 0.0f;
    if (layout && v2_current_instr_id(layout) > 0 && ft2_v2_get_active_voice_count(v2_current_instr_id(layout)) > 0)
        phaseOffset = fmodf((float)SDL_GetTicks() * 0.00025f, 1.0f);

    for (sampleIndex = 0; sampleIndex < V2_PREVIEW_SAMPLE_COUNT; ++sampleIndex)
    {
        float basePhase = ((float)sampleIndex / (float)V2_PREVIEW_SAMPLE_COUNT) + phaseOffset;
        float syncPhase = v2_wrap01(basePhase * powf(2.0f, ((float)(voiceTransposeRaw - 64)) / 12.0f));
        float mix = 0.0f;
        float filtered;
        float filtered2;

        basePhase = v2_wrap01(basePhase);
        if (oscSync > 0.001f)
            basePhase = syncPhase;

        for (i = 0; i < V2_PREVIEW_OSC_COUNT; ++i)
        {
            const float oscSample = v2_preview_eval_osc_sample(&osc[i], basePhase, mix, sampleIndex);
            if (osc[i].ring)
                mix *= oscSample;
            else
                mix += oscSample;
        }

        mix = tanhf(mix * 1.35f);

        if (routingRaw == 1)
        {
            filtered = v2_preview_apply_filter(mix, filter1Mode, filter1Cutoff, filter1Reso, &lp11, &lp12);
            filtered = v2_preview_apply_filter(filtered, filter2Mode, filter2Cutoff, filter2Reso, &lp21, &lp22);
        }
        else if (routingRaw == 2)
        {
            filtered = v2_preview_apply_filter(mix, filter1Mode, filter1Cutoff, filter1Reso, &lp11, &lp12);
            filtered2 = v2_preview_apply_filter(mix, filter2Mode, filter2Cutoff, filter2Reso, &lp21, &lp22);
            filtered = (filtered * (1.0f - routingBalance)) + (filtered2 * routingBalance);
        }
        else
        {
            filtered = v2_preview_apply_filter(mix,
                filter1Mode > 0 ? filter1Mode : filter2Mode,
                filter1Mode > 0 ? filter1Cutoff : filter2Cutoff,
                filter1Mode > 0 ? filter1Reso : filter2Reso,
                &lp11, &lp12);
        }

        samples[sampleIndex] = v2_clampf(filtered, -1.0f, 1.0f);
    }

    for (i = 0; i < V2_PREVIEW_HARMONICS; ++i)
    {
        const float harmonic = (float)(i + 1);
        float realPart = 0.0f;
        float imagPart = 0.0f;

        for (sampleIndex = 0; sampleIndex < V2_PREVIEW_SAMPLE_COUNT; ++sampleIndex)
        {
            const float angle = 6.28318530718f * harmonic * (float)sampleIndex / (float)V2_PREVIEW_SAMPLE_COUNT;
            realPart += samples[sampleIndex] * cosf(angle);
            imagPart -= samples[sampleIndex] * sinf(angle);
        }

        harmonics[i] = sqrtf((realPart * realPart) + (imagPart * imagPart)) / (float)V2_PREVIEW_SAMPLE_COUNT;
    }
}

static void v2_draw_waveform_preview(const V2CompleteLayout *layout, int x, int y, int w, int h)
{
    float samples[V2_PREVIEW_SAMPLE_COUNT];
    float harmonics[V2_PREVIEW_HARMONICS];
    float harmonicMax = 0.0f;
    int plotX;
    int plotY;
    int plotW;
    int plotH;
    int barsY;
    int barsH;
    int centerY;
    int i;
    int frameX;
    int frameY;
    int frameW;
    int frameH;

    if (!layout)
        return;

    frameX = x;
    frameY = y;
    frameW = w;
    frameH = h;

    if (frameX < 0 || frameY < 0 || frameW <= 0 || frameH <= 0)
        return;
    if (frameX >= SCREEN_W || frameY >= SCREEN_H)
        return;
    if (frameX + frameW > SCREEN_W)
        frameW = SCREEN_W - frameX;
    if (frameY + frameH > SCREEN_H)
        frameH = SCREEN_H - frameY;
    if (frameW < 16 || frameH < 16)
        return;

    plotX = frameX + 6;
    plotY = frameY + 6;
    plotW = frameW - 12;
    plotH = (frameH * 2) / 3;
    barsY = plotY + plotH + 6;
    barsH = (frameY + frameH - 6) - barsY;
    centerY = plotY + (plotH / 2);

    fillRect(frameX, frameY, frameW, frameH, PAL_BUTTON2);
    hLine(frameX, frameY, frameW, PAL_BCKGRND);
    hLine(frameX, frameY + frameH - 1, frameW, PAL_BCKGRND);
    vLine(frameX, frameY, frameH, PAL_BCKGRND);
    vLine(frameX + frameW - 1, frameY, frameH, PAL_BCKGRND);

    if (plotW < 32 || plotH < 16)
        return;

    {
        const float t = (float)(SDL_GetTicks() & 0x7FFFFFFF) * 0.0015f;
        for (i = 0; i < V2_PREVIEW_SAMPLE_COUNT; ++i)
        {
            const float p = (float)i / (float)V2_PREVIEW_SAMPLE_COUNT;
            const float s1 = sinf((p * 6.28318530718f * 1.0f) + t);
            const float s2 = 0.35f * sinf((p * 6.28318530718f * 2.0f) - (t * 0.73f));
            const float s3 = 0.20f * sinf((p * 6.28318530718f * 5.0f) + (t * 1.37f));
            const float tri = (2.0f * fabsf((2.0f * (p + (t * 0.03f - floorf(t * 0.03f)))) - 1.0f)) - 1.0f;
            samples[i] = v2_clampf((s1 * 0.55f) + s2 + s3 + (tri * 0.18f), -1.0f, 1.0f);
        }

        for (i = 0; i < V2_PREVIEW_HARMONICS; ++i)
        {
            const float phase = t * (0.4f + (i * 0.09f));
            harmonics[i] = 0.18f + 0.82f * fabsf(sinf(phase));
            if (harmonics[i] > harmonicMax)
                harmonicMax = harmonics[i];
        }
    }

    if (harmonicMax < 0.0001f)
        harmonicMax = 0.0001f;

    for (i = 0; i <= 8; ++i)
    {
        const int gx = plotX + (i * plotW) / 8;
        const int gy = plotY + (i * plotH) / 4;
        if (i < 8)
            vLine(gx, plotY, plotH, PAL_DSKTOP2);
        if (i < 4)
            hLine(plotX, gy, plotW, PAL_DSKTOP2);
    }
    hLine(plotX, centerY, plotW, PAL_BUTTON1);

    for (i = 1; i < V2_PREVIEW_SAMPLE_COUNT; ++i)
    {
        const int x0 = plotX + ((i - 1) * (plotW - 1)) / (V2_PREVIEW_SAMPLE_COUNT - 1);
        const int x1 = plotX + (i * (plotW - 1)) / (V2_PREVIEW_SAMPLE_COUNT - 1);
        const int y0 = centerY - (int)lroundf(samples[i - 1] * ((float)(plotH - 6) * 0.5f));
        const int y1 = centerY - (int)lroundf(samples[i] * ((float)(plotH - 6) * 0.5f));
        v2_draw_line(x0, y0, x1, y1, PAL_PATTEXT);
    }

    if (barsH >= 10)
    {
        const int barW = plotW / V2_PREVIEW_HARMONICS;
        for (i = 0; i < V2_PREVIEW_HARMONICS; ++i)
        {
            const int x = plotX + (i * barW);
            const int usableW = barW - 3;
            const int barHeight = (int)lroundf((harmonics[i] / harmonicMax) * (float)(barsH - 4));
            if (usableW > 1 && barHeight > 0)
                fillRect(x + 1, barsY + barsH - 2 - barHeight, usableW, barHeight, PAL_TEXTMRK);
        }
    }
}

static bool v2_widget_visible_on_page(const TunefishWidget *widget, int currentPage)
{
    if (!widget) return false;
    if (widget->page == FT2_UI_WIDGET_PAGE_BOTH) return true;
    return widget->page == (currentPage + 1);
}

static void v2_register_widget(V2CompleteLayout *layout, TunefishWidget *widget, ft2_ui_widget_page_t page)
{
    if (!layout || !widget)
    {
        tf_widget_destroy(widget);
        return;
    }

    widget->page = (int)page;
    widget->visible = v2_widget_visible_on_page(widget, layout->current_page);

    if (layout->all_widget_count < V2_MAX_WIDGETS)
    {
        layout->all_widgets[layout->all_widget_count++] = widget;
    }
    else
    {
        tf_widget_destroy(widget);
    }
}

static void v2_add_combo_items_from_ctlstr(TunefishWidget *combo, const char *ctlstr)
{
    char text[128];
    size_t outLen;
    size_t i;

    if (!combo || !ctlstr) return;

    const char *p = ctlstr;
    if (*p == '!') ++p;

    const char *start = p;
    while (*p != '\0')
    {
        if (*p == '|')
        {
            if (p > start)
            {
                size_t len = (size_t)(p - start);
                if (len >= sizeof(text)) len = sizeof(text) - 1;

                outLen = 0;
                for (i = 0; i < len && outLen < sizeof(text) - 1; ++i)
                {
                    const unsigned char ch = (unsigned char)start[i];
                    if (ch >= 32 && ch <= 126)
                    {
                        text[outLen++] = (char)ch;
                    }
                    else
                    {
                        static const char replacement[] = "+/-";
                        size_t j;
                        for (j = 0; j < sizeof(replacement) - 1 && outLen < sizeof(text) - 1; ++j)
                            text[outLen++] = replacement[j];
                    }
                }
                text[outLen] = '\0';
                tf_widget_add_combo_item(combo, text);
            }
            start = p + 1;
        }
        ++p;
    }

    if (p > start)
    {
        size_t len = (size_t)(p - start);
        if (len >= sizeof(text)) len = sizeof(text) - 1;

        outLen = 0;
        for (i = 0; i < len && outLen < sizeof(text) - 1; ++i)
        {
            const unsigned char ch = (unsigned char)start[i];
            if (ch >= 32 && ch <= 126)
            {
                text[outLen++] = (char)ch;
            }
            else
            {
                static const char replacement[] = "+/-";
                size_t j;
                for (j = 0; j < sizeof(replacement) - 1 && outLen < sizeof(text) - 1; ++j)
                    text[outLen++] = replacement[j];
            }
        }
        text[outLen] = '\0';
        tf_widget_add_combo_item(combo, text);
    }
}

static const Ft2V2ParamInfo *v2_param_info_for_binding(V2BindingKind kind, int target)
{
    if (kind == V2_BIND_PATCH_PARAM)
        return ft2_v2_get_param_info(target);
    if (kind == V2_BIND_GLOBAL_PARAM)
        return ft2_v2_get_global_param_info(target);
    return NULL;
}

static void v2_populate_param_combo(TunefishWidget *widget, V2BindingKind kind, int target)
{
    const Ft2V2ParamInfo *info;

    if (!widget || widget->type != TF_WIDGET_COMBO_BOX)
        return;

    info = v2_param_info_for_binding(kind, target);
    if (!info) return;

    tf_widget_clear_combo_items(widget);
    if (info->ctltype == FT2_V2_CTL_MB)
        v2_add_combo_items_from_ctlstr(widget, info->ctlstr);
}

static void v2_populate_mod_source_combo(TunefishWidget *widget)
{
    int i;

    if (!widget) return;
    tf_widget_clear_combo_items(widget);

    for (i = 0; i < ft2_v2_get_mod_source_count(); ++i)
    {
        const char *name = ft2_v2_get_mod_source_name(i);
        tf_widget_add_combo_item(widget, name ? name : "");
    }
}

static void v2_populate_mod_dest_combo(TunefishWidget *widget)
{
    int i;

    if (!widget) return;
    tf_widget_clear_combo_items(widget);

    for (i = 0; i < ft2_v2_get_mod_dest_count(); ++i)
    {
        const char *name = ft2_v2_get_mod_dest_name(i);
        tf_widget_add_combo_item(widget, name ? name : "");
    }
}

static void v2_populate_mod_bank_combo(TunefishWidget *widget, int currentBank)
{
    int i;

    if (!widget) return;
    tf_widget_clear_combo_items(widget);

    for (i = 0; i < 32; ++i)
    {
        char text[24];
        const int start = (i * V2_MOD_ROWS) + 1;
        const int end = start + V2_MOD_ROWS - 1;
        snprintf(text, sizeof(text), "%03d-%03d", start, end);
        tf_widget_add_combo_item(widget, text);
    }

    widget->selectedIndex = v2_clampi(currentBank, 0, 31);
    widget->value = (float)widget->selectedIndex / 31.0f;
}

static void v2_populate_preset_combo(V2CompleteLayout *layout)
{
    int i;
    int count;
    int presetIndex;
    int instrID;

    if (!layout || !layout->preset_combo) return;

    tf_widget_clear_combo_items(layout->preset_combo);

    count = ft2_v2_get_factory_preset_count();
    for (i = 0; i < count; ++i)
    {
        const char *name = ft2_v2_get_preset_name_for_instrument(v2_current_instr_id(layout), i);
        char fallback[32];

        if (!name || !*name)
        {
            snprintf(fallback, sizeof(fallback), "Preset %03d", i + 1);
            name = fallback;
        }

        tf_widget_add_combo_item(layout->preset_combo, name);
    }

    instrID = v2_current_instr_id(layout);
    presetIndex = (instrID > 0) ? ft2_v2_get_current_preset_for_instrument(instrID) : 0;
    if (count <= 0)
    {
        layout->preset_combo->selectedIndex = 0;
        layout->preset_combo->value = 0.0f;
    }
    else
    {
        presetIndex = v2_clampi(presetIndex, 0, count - 1);
        layout->preset_combo->selectedIndex = presetIndex;
        layout->preset_combo->value = (count > 1) ? ((float)presetIndex / (float)(count - 1)) : 0.0f;
    }
}

static void v2_style_widget(TunefishWidget *widget)
{
    if (!widget) return;

    tf_apply_tunefish_styling(widget);

    switch (widget->type)
    {
        case TF_WIDGET_BUTTON:
            tf_style_as_main_button(widget);
            break;

        case TF_WIDGET_ROTARY_SLIDER:
            tf_style_as_parameter_knob(widget);
            break;

        case TF_WIDGET_COMBO_BOX:
            tf_apply_combo_styling(widget);
            break;

        case TF_WIDGET_LEVEL_METER:
            tf_style_as_level_indicator(widget);
            break;

        case TF_WIDGET_LABEL:
            widget->textColor = TF_COL_TEXT_NORMAL;
            break;

        default:
            break;
    }
}

void v2_style_all_widgets_authentic(V2CompleteLayout *layout)
{
    int i;

    if (!layout) return;
    for (i = 0; i < layout->all_widget_count; ++i)
        v2_style_widget(layout->all_widgets[i]);
}

static void v2_apply_page_button_state(V2CompleteLayout *layout)
{
    if (!layout) return;

    if (layout->page_voice_button) layout->page_voice_button->pressed = (layout->current_page == V2_PAGE_VOICE_OSC);
    if (layout->page_filter_button) layout->page_filter_button->pressed = (layout->current_page == V2_PAGE_FILTER);
    if (layout->page_lfo_env_button) layout->page_lfo_env_button->pressed = (layout->current_page == V2_PAGE_LFO_ENV);
    if (layout->page_fx_button) layout->page_fx_button->pressed = (layout->current_page == V2_PAGE_FX);
    if (layout->page_master_button) layout->page_master_button->pressed = (layout->current_page == V2_PAGE_MASTER);
    if (layout->page_mod_button) layout->page_mod_button->pressed = (layout->current_page == V2_PAGE_MOD);
}

static void v2_update_caption(V2CompleteLayout *layout)
{
    if (!layout || !layout->page_caption_label) return;
    tf_widget_set_label(layout->page_caption_label, k_page_names[v2_clampi(layout->current_page, 0, V2_PAGE_COUNT - 1)]);
}

static void v2_update_widget_visibility(V2CompleteLayout *layout)
{
    int i;

    if (!layout) return;

    for (i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *widget = layout->all_widgets[i];
        if (!widget) continue;
        widget->visible = v2_widget_visible_on_page(widget, layout->current_page);
    }

    if (layout->title_label) layout->title_label->visible = true;
    if (layout->page_caption_label) layout->page_caption_label->visible = true;
    if (layout->preset_combo) layout->preset_combo->visible = true;
    if (layout->voice_meter) layout->voice_meter->visible = true;
    if (layout->close_button) layout->close_button->visible = true;
    if (layout->page_voice_button) layout->page_voice_button->visible = true;
    if (layout->page_filter_button) layout->page_filter_button->visible = true;
    if (layout->page_lfo_env_button) layout->page_lfo_env_button->visible = true;
    if (layout->page_fx_button) layout->page_fx_button->visible = true;
    if (layout->page_master_button) layout->page_master_button->visible = true;
    if (layout->page_mod_button) layout->page_mod_button->visible = true;

    v2_apply_page_button_state(layout);
    v2_update_caption(layout);
}

static int v2_find_binding_index_for_widget(const V2CompleteLayout *layout, const TunefishWidget *widget)
{
    int i;

    if (!layout || !widget) return -1;

    for (i = 0; i < layout->binding_count; ++i)
    {
        if (layout->bindings[i].widget == widget)
            return i;
    }

    return -1;
}

static void v2_sync_patch_or_global_binding(V2CompleteLayout *layout, const V2WidgetBinding *binding)
{
    int instrID;
    float value;
    const Ft2V2ParamInfo *info;

    if (!layout || !binding || !binding->widget) return;

    instrID = v2_current_instr_id(layout);
    if (instrID <= 0) return;

    if (binding->kind == V2_BIND_PATCH_PARAM)
        value = ft2_v2_get_param_for_instrument(instrID, binding->target);
    else
        value = ft2_v2_get_global_param_for_instrument(instrID, binding->target);

    value = v2_clampf(value, 0.0f, 1.0f);

    if (binding->widget->type == TF_WIDGET_COMBO_BOX)
    {
        if (binding->widget->comboItemCount > 1)
        {
            int idx = (int)lroundf(value * (float)(binding->widget->comboItemCount - 1));
            binding->widget->selectedIndex = v2_clampi(idx, 0, binding->widget->comboItemCount - 1);
            binding->widget->value = (float)binding->widget->selectedIndex / (float)(binding->widget->comboItemCount - 1);
        }
        else
        {
            binding->widget->selectedIndex = 0;
            binding->widget->value = 0.0f;
        }
    }
    else
    {
        binding->widget->value = value;
    }

    info = v2_param_info_for_binding(binding->kind, binding->target);
    if (info && binding->widget->type == TF_WIDGET_ROTARY_SLIDER && binding->widget->text[0] == '\0')
        tf_widget_set_label(binding->widget, info->name ? info->name : "");
}

static void v2_sync_mod_widgets(V2CompleteLayout *layout)
{
    int row;
    int instrID;
    int baseSlot;

    if (!layout) return;

    instrID = v2_current_instr_id(layout);
    if (instrID <= 0) return;

    baseSlot = layout->current_mod_bank * V2_MOD_ROWS;
    for (row = 0; row < V2_MOD_ROWS; ++row)
    {
        int slot = baseSlot + row;
        int source = 0;
        int amount = 0;
        int dest = 0;
        int destListIndex;
        char text[24];

        ft2_v2_get_mod_slot_for_instrument(instrID, slot, &source, &amount, &dest);

        if (layout->mod_slot_labels[row])
        {
            snprintf(text, sizeof(text), "Slot %d", slot + 1);
            tf_widget_set_label(layout->mod_slot_labels[row], text);
        }

        if (layout->mod_source_widgets[row])
        {
            layout->mod_source_widgets[row]->selectedIndex =
                v2_clampi(source, 0, layout->mod_source_widgets[row]->comboItemCount > 0 ? layout->mod_source_widgets[row]->comboItemCount - 1 : 0);
            if (layout->mod_source_widgets[row]->comboItemCount > 1)
                layout->mod_source_widgets[row]->value =
                    (float)layout->mod_source_widgets[row]->selectedIndex / (float)(layout->mod_source_widgets[row]->comboItemCount - 1);
            else
                layout->mod_source_widgets[row]->value = 0.0f;
        }

        if (layout->mod_amount_widgets[row])
            layout->mod_amount_widgets[row]->value = (float)v2_clampi(amount, 0, 127) / 127.0f;

        destListIndex = ft2_v2_find_mod_dest_list_index(dest);
        if (layout->mod_dest_widgets[row])
        {
            if (destListIndex < 0) destListIndex = 0;
            layout->mod_dest_widgets[row]->selectedIndex =
                v2_clampi(destListIndex, 0, layout->mod_dest_widgets[row]->comboItemCount > 0 ? layout->mod_dest_widgets[row]->comboItemCount - 1 : 0);
            if (layout->mod_dest_widgets[row]->comboItemCount > 1)
                layout->mod_dest_widgets[row]->value =
                    (float)layout->mod_dest_widgets[row]->selectedIndex / (float)(layout->mod_dest_widgets[row]->comboItemCount - 1);
            else
                layout->mod_dest_widgets[row]->value = 0.0f;
        }
    }
}

static void v2_sync_voice_meter(V2CompleteLayout *layout)
{
    int instrID;
    int voices;
    float norm;

    if (!layout || !layout->voice_meter) return;

    instrID = v2_current_instr_id(layout);
    if (instrID <= 0)
    {
        layout->voice_meter->value = 0.0f;
        layout->voice_meter->peakLevel = 0.0f;
        return;
    }

    voices = ft2_v2_get_active_voice_count(instrID);
    norm = (voices <= 0) ? 0.0f : ((voices >= 16) ? 1.0f : ((float)voices / 16.0f));
    layout->voice_meter->value = norm;
    layout->voice_meter->peakLevel = norm;
}

static void v2_refresh_runtime_state(V2CompleteLayout *layout)
{
    int currentInstr;

    if (!layout) return;

    currentInstr = v2_current_instr_id(layout);
    if (currentInstr != layout->active_instrument_id)
    {
        layout->active_instrument_id = currentInstr;
        v2_update_all_widgets_from_synth(layout);
        return;
    }

    v2_sync_voice_meter(layout);
}

void v2_update_all_widgets_from_synth(V2CompleteLayout *layout)
{
    int i;
    int currentInstr;

    if (!layout) return;

    currentInstr = v2_current_instr_id(layout);
    if (currentInstr != layout->active_instrument_id)
        layout->active_instrument_id = currentInstr;

    v2_populate_preset_combo(layout);
    if (layout->mod_bank_combo)
        v2_populate_mod_bank_combo(layout->mod_bank_combo, layout->current_mod_bank);

    for (i = 0; i < layout->binding_count; ++i)
    {
        const V2WidgetBinding *binding = &layout->bindings[i];
        switch (binding->kind)
        {
            case V2_BIND_PATCH_PARAM:
            case V2_BIND_GLOBAL_PARAM:
                v2_sync_patch_or_global_binding(layout, binding);
                break;

            default:
                break;
        }
    }

    v2_sync_mod_widgets(layout);
    v2_sync_voice_meter(layout);
    v2_update_widget_visibility(layout);
}

void v2_sync_widgets_with_parameters(V2CompleteLayout *layout)
{
    v2_update_all_widgets_from_synth(layout);
}

static void v2_button_clicked(TunefishWidget *widget)
{
    if (!g_active_v2_layout || !widget) return;

    if (widget == g_active_v2_layout->page_voice_button)
        v2_switch_to_page(g_active_v2_layout, V2_PAGE_VOICE_OSC);
    else if (widget == g_active_v2_layout->page_filter_button)
        v2_switch_to_page(g_active_v2_layout, V2_PAGE_FILTER);
    else if (widget == g_active_v2_layout->page_lfo_env_button)
        v2_switch_to_page(g_active_v2_layout, V2_PAGE_LFO_ENV);
    else if (widget == g_active_v2_layout->page_fx_button)
        v2_switch_to_page(g_active_v2_layout, V2_PAGE_FX);
    else if (widget == g_active_v2_layout->page_master_button)
        v2_switch_to_page(g_active_v2_layout, V2_PAGE_MASTER);
    else if (widget == g_active_v2_layout->page_mod_button)
        v2_switch_to_page(g_active_v2_layout, V2_PAGE_MOD);
    else if (widget == g_active_v2_layout->close_button)
    {
        v2_hide_layout(g_active_v2_layout);
        ft2_close_synth_editor();
    }
}

static void v2_value_changed(TunefishWidget *widget, float newValue)
{
    int bindIndex;
    int instrID;
    V2WidgetBinding *binding;

    if (!g_active_v2_layout || !widget) return;

    bindIndex = v2_find_binding_index_for_widget(g_active_v2_layout, widget);
    if (bindIndex < 0) return;

    instrID = v2_current_instr_id(g_active_v2_layout);
    if (instrID <= 0) return;

    binding = &g_active_v2_layout->bindings[bindIndex];
    switch (binding->kind)
    {
        case V2_BIND_PATCH_PARAM:
            ft2_v2_set_param_for_instrument(instrID, binding->target, v2_clampf(newValue, 0.0f, 1.0f));
            break;

        case V2_BIND_GLOBAL_PARAM:
            ft2_v2_set_global_param_for_instrument(instrID, binding->target, v2_clampf(newValue, 0.0f, 1.0f));
            break;

        case V2_BIND_MOD_AMOUNT:
        {
            int slot = binding->target;
            int source = 0;
            int dest = 0;
            int amount = v2_clampi((int)lroundf(newValue * 127.0f), 0, 127);

            ft2_v2_get_mod_slot_for_instrument(instrID, slot, &source, NULL, &dest);
            ft2_v2_set_mod_slot_for_instrument(instrID, slot, source, amount, dest);
            break;
        }

        default:
            break;
    }
}

static void v2_combo_changed(TunefishWidget *widget, int selectedIndex)
{
    int bindIndex;
    int instrID;
    V2WidgetBinding *binding;

    if (!g_active_v2_layout || !widget) return;

    if (widget == g_active_v2_layout->preset_combo)
    {
        instrID = v2_current_instr_id(g_active_v2_layout);
        if (instrID > 0)
            ft2_v2_load_preset_for_instrument(instrID, selectedIndex);
        v2_update_all_widgets_from_synth(g_active_v2_layout);
        return;
    }

    if (widget == g_active_v2_layout->mod_bank_combo)
    {
        g_active_v2_layout->current_mod_bank = v2_clampi(selectedIndex, 0, 31);
        v2_update_all_widgets_from_synth(g_active_v2_layout);
        return;
    }

    bindIndex = v2_find_binding_index_for_widget(g_active_v2_layout, widget);
    if (bindIndex < 0) return;

    instrID = v2_current_instr_id(g_active_v2_layout);
    if (instrID <= 0) return;

    binding = &g_active_v2_layout->bindings[bindIndex];
    switch (binding->kind)
    {
        case V2_BIND_PATCH_PARAM:
        {
            float value = (widget->comboItemCount > 1) ?
                ((float)v2_clampi(selectedIndex, 0, widget->comboItemCount - 1) / (float)(widget->comboItemCount - 1)) : 0.0f;
            ft2_v2_set_param_for_instrument(instrID, binding->target, value);
            break;
        }

        case V2_BIND_GLOBAL_PARAM:
        {
            float value = (widget->comboItemCount > 1) ?
                ((float)v2_clampi(selectedIndex, 0, widget->comboItemCount - 1) / (float)(widget->comboItemCount - 1)) : 0.0f;
            ft2_v2_set_global_param_for_instrument(instrID, binding->target, value);
            break;
        }

        case V2_BIND_MOD_SOURCE:
        {
            int amount = 0;
            int dest = 0;
            ft2_v2_get_mod_slot_for_instrument(instrID, binding->target, NULL, &amount, &dest);
            ft2_v2_set_mod_slot_for_instrument(instrID, binding->target, selectedIndex, amount, dest);
            break;
        }

        case V2_BIND_MOD_DEST:
        {
            int source = 0;
            int amount = 0;
            ft2_v2_get_mod_slot_for_instrument(instrID, binding->target, &source, &amount, NULL);
            ft2_v2_set_mod_slot_for_instrument(instrID, binding->target, source, amount, selectedIndex);
            break;
        }

        default:
            break;
    }
}

static void v2_bind_widget(V2CompleteLayout *layout, TunefishWidget *widget, V2BindingKind kind, int target)
{
    V2WidgetBinding *binding;

    if (!layout || !widget || kind == V2_BIND_NONE) return;
    if (layout->binding_count >= V2_MAX_BINDINGS) return;

    binding = &layout->bindings[layout->binding_count++];
    binding->widget = widget;
    binding->kind = kind;
    binding->target = target;

    switch (kind)
    {
        case V2_BIND_PATCH_PARAM:
        case V2_BIND_GLOBAL_PARAM:
            if (widget->type == TF_WIDGET_COMBO_BOX)
            {
                v2_populate_param_combo(widget, kind, target);
                widget->onComboSelect = v2_combo_changed;
            }
            else
            {
                widget->onValueChange = v2_value_changed;
            }
            break;

        case V2_BIND_MOD_SOURCE:
            v2_populate_mod_source_combo(widget);
            widget->onComboSelect = v2_combo_changed;
            break;

        case V2_BIND_MOD_AMOUNT:
            widget->onValueChange = v2_value_changed;
            break;

        case V2_BIND_MOD_DEST:
            v2_populate_mod_dest_combo(widget);
            widget->onComboSelect = v2_combo_changed;
            break;

        default:
            break;
    }
}

static void v2_assign_named_widget(V2CompleteLayout *layout, TunefishWidget *widget)
{
    int index;
    int target;

    if (!layout || !widget) return;

    if (strcmp(widget->name, "title_label") == 0) { layout->title_label = widget; return; }
    if (strcmp(widget->name, "page_caption_label") == 0) { layout->page_caption_label = widget; return; }
    if (strcmp(widget->name, "preset_combo") == 0)
    {
        layout->preset_combo = widget;
        widget->onComboSelect = v2_combo_changed;
        return;
    }
    if (strcmp(widget->name, "voice_meter") == 0) { layout->voice_meter = widget; return; }
    if (strcmp(widget->name, "close_btn") == 0) { layout->close_button = widget; widget->onClick = v2_button_clicked; return; }
    if (strcmp(widget->name, "page_voice_btn") == 0) { layout->page_voice_button = widget; widget->onClick = v2_button_clicked; return; }
    if (strcmp(widget->name, "page_filter_btn") == 0) { layout->page_filter_button = widget; widget->onClick = v2_button_clicked; return; }
    if (strcmp(widget->name, "page_lfo_env_btn") == 0) { layout->page_lfo_env_button = widget; widget->onClick = v2_button_clicked; return; }
    if (strcmp(widget->name, "page_fx_btn") == 0) { layout->page_fx_button = widget; widget->onClick = v2_button_clicked; return; }
    if (strcmp(widget->name, "page_master_btn") == 0) { layout->page_master_button = widget; widget->onClick = v2_button_clicked; return; }
    if (strcmp(widget->name, "page_mod_btn") == 0) { layout->page_mod_button = widget; widget->onClick = v2_button_clicked; return; }
    if (strcmp(widget->name, "mod_bank_combo") == 0)
    {
        layout->mod_bank_combo = widget;
        v2_populate_mod_bank_combo(widget, layout->current_mod_bank);
        widget->onComboSelect = v2_combo_changed;
        return;
    }
    if (strcmp(widget->name, "amp_env_display") == 0) { layout->amp_env_display = widget; return; }
    if (strcmp(widget->name, "eg2_env_display") == 0) { layout->eg2_env_display = widget; return; }

    if (sscanf(widget->name, "p_%d", &target) == 1)
    {
        v2_bind_widget(layout, widget, V2_BIND_PATCH_PARAM, target);
        return;
    }

    if (sscanf(widget->name, "g_%d", &target) == 1)
    {
        v2_bind_widget(layout, widget, V2_BIND_GLOBAL_PARAM, target);
        return;
    }

    if (sscanf(widget->name, "m_src_%d", &index) == 1 && index >= 0 && index < V2_MOD_ROWS)
    {
        layout->mod_source_widgets[index] = widget;
        v2_bind_widget(layout, widget, V2_BIND_MOD_SOURCE, index);
        return;
    }

    if (sscanf(widget->name, "m_amt_%d", &index) == 1 && index >= 0 && index < V2_MOD_ROWS)
    {
        layout->mod_amount_widgets[index] = widget;
        v2_bind_widget(layout, widget, V2_BIND_MOD_AMOUNT, index);
        return;
    }

    if (sscanf(widget->name, "m_dst_%d", &index) == 1 && index >= 0 && index < V2_MOD_ROWS)
    {
        layout->mod_dest_widgets[index] = widget;
        v2_bind_widget(layout, widget, V2_BIND_MOD_DEST, index);
        return;
    }

    if (sscanf(widget->name, "m_lbl_%d", &index) == 1 && index >= 0 && index < V2_MOD_ROWS)
    {
        layout->mod_slot_labels[index] = widget;
        return;
    }
}

static TunefishWidget *v2_create_label_from_desc(const ft2_ui_tf_label_desc_t *desc)
{
    if (!desc) return NULL;
    return tf_create_label(desc->name ? desc->name : "label", desc->text ? desc->text : "", desc->x, desc->y, desc->w, desc->h);
}

static TunefishWidget *v2_create_button_from_desc(const ft2_ui_tf_button_desc_t *desc)
{
    if (!desc) return NULL;
    return tf_create_button(desc->name ? desc->name : "button", desc->text ? desc->text : "", desc->x, desc->y, desc->w, desc->h);
}

static TunefishWidget *v2_create_rotary_from_desc(const ft2_ui_tf_rotary_slider_desc_t *desc)
{
    TunefishWidget *widget;

    if (!desc) return NULL;

    widget = tf_create_rotary_slider(desc->name ? desc->name : "rotary",
        (int)desc->x + (int)desc->radius,
        (int)desc->y + (int)desc->radius,
        (int)desc->radius,
        desc->start_angle,
        desc->end_angle);
    if (widget && desc->label)
        tf_widget_set_label(widget, desc->label);
    if (widget)
    {
        widget->modRingMode = desc->mod_ring.mode ? desc->mod_ring.mode : FT2_UI_MOD_RING_AUTO_BY_NAME;
        widget->modMatrixSlot = desc->mod_ring.matrix_slot;
        widget->modTargetParam = desc->mod_ring.target_param;
        widget->modAmountScale = desc->mod_ring.amount_scale > 0.0f ? desc->mod_ring.amount_scale : 1.0f;
    }
    return widget;
}

static TunefishWidget *v2_create_linear_from_desc(const ft2_ui_tf_linear_slider_desc_t *desc)
{
    if (!desc) return NULL;
    return tf_create_linear_slider(desc->name ? desc->name : "linear", desc->x, desc->y, desc->w, desc->h, desc->vertical);
}

static TunefishWidget *v2_create_combo_from_desc(const ft2_ui_tf_combo_box_desc_t *desc)
{
    TunefishWidget *widget;

    if (!desc) return NULL;
    widget = tf_create_combo_box(desc->name ? desc->name : "combo", desc->x, desc->y, desc->w, desc->h, NULL, 0);
    if (widget)
        widget->selectedIndex = desc->selected_index;
    return widget;
}

static TunefishWidget *v2_create_meter_from_desc(const ft2_ui_tf_level_meter_desc_t *desc)
{
    if (!desc) return NULL;
    return tf_create_level_meter(desc->name ? desc->name : "meter", desc->x, desc->y, desc->w, desc->h,
        desc->num_leds > 0 ? desc->num_leds : 12, desc->show_peak);
}

static TunefishWidget *v2_create_group_from_desc(const ft2_ui_tf_group_box_desc_t *desc)
{
    if (!desc) return NULL;
    return tf_create_group_box(desc->name ? desc->name : "group", desc->title ? desc->title : "", desc->x, desc->y, desc->w, desc->h);
}

static TunefishWidget *v2_create_env_from_desc(const ft2_ui_tf_envelope_display_desc_t *desc)
{
    if (!desc) return NULL;
    return tf_create_envelope_display(desc->name ? desc->name : "env", desc->x, desc->y, desc->w, desc->h);
}

static TunefishWidget *v2_create_wave_from_desc(const ft2_ui_waveform_view_desc_t *desc)
{
    if (!desc) return NULL;
    return tf_create_waveform_view(desc->name ? desc->name : "waveform_view", desc->x, desc->y, desc->w, desc->h);
}

static V2CompleteLayout *v2_create_complete_layout_from_schema(const ft2_ui_layout_desc_t *desc)
{
    V2CompleteLayout *layout;
    uint16_t i;

    if (!desc) return NULL;

    layout = (V2CompleteLayout *)calloc(1, sizeof(V2CompleteLayout));
    if (!layout) return NULL;

    layout->current_page = V2_PAGE_VOICE_OSC;
    layout->current_mod_bank = 0;
    layout->active_instrument_id = v2_current_instr_id(layout);
    layout->initialized = true;
    layout->visible = false;

    if (desc->waveform_views.count > 0 && desc->waveform_view_desc)
    {
        for (i = 0; i < desc->waveform_views.count; ++i)
        {
            TunefishWidget *widget = v2_create_wave_from_desc(&desc->waveform_view_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->waveform_view_desc[i].page);
        }
    }

    if (desc->tf_buttons.count > 0 && desc->tf_button_desc)
    {
        for (i = 0; i < desc->tf_buttons.count; ++i)
        {
            TunefishWidget *widget = v2_create_button_from_desc(&desc->tf_button_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_button_desc[i].page);
        }
    }

    if (desc->tf_labels.count > 0 && desc->tf_label_desc)
    {
        for (i = 0; i < desc->tf_labels.count; ++i)
        {
            TunefishWidget *widget = v2_create_label_from_desc(&desc->tf_label_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_label_desc[i].page);
        }
    }

    if (desc->tf_rotary_sliders.count > 0 && desc->tf_rotary_slider_desc)
    {
        for (i = 0; i < desc->tf_rotary_sliders.count; ++i)
        {
            TunefishWidget *widget = v2_create_rotary_from_desc(&desc->tf_rotary_slider_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_rotary_slider_desc[i].page);
        }
    }

    if (desc->tf_linear_sliders.count > 0 && desc->tf_linear_slider_desc)
    {
        for (i = 0; i < desc->tf_linear_sliders.count; ++i)
        {
            TunefishWidget *widget = v2_create_linear_from_desc(&desc->tf_linear_slider_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_linear_slider_desc[i].page);
        }
    }

    if (desc->tf_combo_boxes.count > 0 && desc->tf_combo_box_desc)
    {
        for (i = 0; i < desc->tf_combo_boxes.count; ++i)
        {
            TunefishWidget *widget = v2_create_combo_from_desc(&desc->tf_combo_box_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_combo_box_desc[i].page);
        }
    }

    if (desc->tf_level_meters.count > 0 && desc->tf_level_meter_desc)
    {
        for (i = 0; i < desc->tf_level_meters.count; ++i)
        {
            TunefishWidget *widget = v2_create_meter_from_desc(&desc->tf_level_meter_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_level_meter_desc[i].page);
        }
    }

    if (desc->tf_envelope_displays.count > 0 && desc->tf_envelope_display_desc)
    {
        for (i = 0; i < desc->tf_envelope_displays.count; ++i)
        {
            TunefishWidget *widget = v2_create_env_from_desc(&desc->tf_envelope_display_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_envelope_display_desc[i].page);
        }
    }

    if (desc->tf_group_boxes.count > 0 && desc->tf_group_box_desc)
    {
        for (i = 0; i < desc->tf_group_boxes.count; ++i)
        {
            TunefishWidget *widget = v2_create_group_from_desc(&desc->tf_group_box_desc[i]);
            if (!widget) continue;
            v2_style_widget(widget);
            v2_register_widget(layout, widget, desc->tf_group_box_desc[i].page);
        }
    }

    for (i = 0; i < (uint16_t)layout->all_widget_count; ++i)
        v2_assign_named_widget(layout, layout->all_widgets[i]);

    v2_update_all_widgets_from_synth(layout);
    return layout;
}

static void v2_set_active_instrument(V2CompleteLayout *layout)
{
    if (!layout) return;
    layout->active_instrument_id = v2_current_instr_id(layout);
}

V2CompleteLayout *v2_create_complete_layout(void)
{
    return v2_create_complete_layout_from_schema(&ft2_v2_complete_layout_layout);
}

void v2_destroy_complete_layout(V2CompleteLayout *layout)
{
    int i;

    if (!layout) return;

    for (i = 0; i < layout->all_widget_count; ++i)
        tf_widget_destroy(layout->all_widgets[i]);

    if (g_active_v2_layout == layout)
        g_active_v2_layout = NULL;

    free(layout);
}

void v2_show_layout(V2CompleteLayout *layout)
{
    if (!layout) return;

    g_active_v2_layout = layout;
    layout->visible = true;
    v2_set_active_instrument(layout);
    v2_update_all_widgets_from_synth(layout);
}

void v2_hide_layout(V2CompleteLayout *layout)
{
    if (!layout) return;
    layout->mouse_capture = NULL;
    layout->visible = false;
    if (g_active_v2_layout == layout)
        g_active_v2_layout = NULL;
}

void v2_switch_to_page(V2CompleteLayout *layout, int page)
{
    if (!layout) return;
    layout->mouse_capture = NULL;
    layout->current_page = v2_clampi(page, 0, V2_PAGE_COUNT - 1);
    v2_update_widget_visibility(layout);
}

TunefishWidget *v2_find_widget_by_name(V2CompleteLayout *layout, const char *name)
{
    int i;

    if (!layout || !name) return NULL;

    for (i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *widget = layout->all_widgets[i];
        if (widget && strcmp(widget->name, name) == 0)
            return widget;
    }

    return NULL;
}

static void v2_draw_env_pixel(int x, int y, uint8_t color)
{
    if (video.frameBuffer == NULL || x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    video.frameBuffer[(size_t)y * SCREEN_W + (size_t)x] = video.palette[color];
}

static void v2_draw_line(int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        const int e2 = err * 2;
        v2_draw_env_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void v2_get_env_points(V2CompleteLayout *layout, int envIndex, const TunefishWidget *widget, int points[5][2])
{
    int instrID;
    int left;
    int right;
    int top;
    int bottom;
    int width;
    int height;
    float attack;
    float decay;
    float sustain;
    float sustainTime;
    float release;
    float spans[4];
    float total;
    int i;
    int x;

    if (!layout || !widget || !points) return;

    for (i = 0; i < 5; ++i)
    {
        points[i][0] = widget->x + 6;
        points[i][1] = widget->y + widget->h - 8;
    }

    instrID = v2_current_instr_id(layout);
    if (instrID <= 0)
        return;

    left = widget->x + 6;
    right = widget->x + widget->w - 7;
    top = widget->y + 8;
    bottom = widget->y + widget->h - 8;
    width = right - left;
    height = bottom - top;
    if (width <= 8 || height <= 8)
        return;

    attack = ft2_v2_get_param_for_instrument(instrID, k_env_param_ids[envIndex][0]);
    decay = ft2_v2_get_param_for_instrument(instrID, k_env_param_ids[envIndex][1]);
    sustain = ft2_v2_get_param_for_instrument(instrID, k_env_param_ids[envIndex][2]);
    sustainTime = ft2_v2_get_param_for_instrument(instrID, k_env_param_ids[envIndex][3]);
    release = ft2_v2_get_param_for_instrument(instrID, k_env_param_ids[envIndex][4]);

    spans[0] = 12.0f + attack * (width * 0.22f);
    spans[1] = 12.0f + decay * (width * 0.18f);
    spans[2] = 18.0f + sustainTime * (width * 0.26f);
    spans[3] = 12.0f + release * (width * 0.20f);
    total = spans[0] + spans[1] + spans[2] + spans[3];
    if (total > width)
    {
        float scale = (float)width / total;
        for (i = 0; i < 4; ++i)
            spans[i] *= scale;
    }

    points[0][0] = left;
    points[0][1] = bottom;

    x = left + (int)lroundf(spans[0]);
    points[1][0] = x;
    points[1][1] = top;

    x += (int)lroundf(spans[1]);
    points[2][0] = v2_clampi(x, left, right - 2);
    points[2][1] = bottom - (int)lroundf(v2_clampf(sustain, 0.0f, 1.0f) * (float)height);

    x += (int)lroundf(spans[2]);
    points[3][0] = v2_clampi(x, points[2][0], right - 1);
    points[3][1] = points[2][1];

    points[4][0] = right;
    points[4][1] = bottom;
}

static void v2_draw_env_display(V2CompleteLayout *layout, const TunefishWidget *widget, int envIndex)
{
    int points[5][2];
    int i;

    if (!layout || !widget || !widget->visible) return;
    if (envIndex < 0 || envIndex >= 2) return;
    if (widget->w < 8 || widget->h < 8) return;

    fillRect(widget->x + 1, widget->y + 1, widget->w - 2, widget->h - 2, PAL_BUTTON2);
    hLine(widget->x, widget->y, widget->w, PAL_BCKGRND);
    hLine(widget->x, widget->y + widget->h - 1, widget->w, PAL_BCKGRND);
    vLine(widget->x, widget->y, widget->h, PAL_BCKGRND);
    vLine(widget->x + widget->w - 1, widget->y, widget->h, PAL_BCKGRND);

    v2_get_env_points(layout, envIndex, widget, points);

    for (i = 0; i < 5; ++i)
        fillRect(points[i][0] - 1, points[i][1] - 1, 3, 3, PAL_FORGRND);

    for (i = 1; i < 5; ++i)
        v2_draw_line(points[i - 1][0], points[i - 1][1], points[i][0], points[i][1], PAL_PATTEXT);
}

void v2_render_complete_layout(V2CompleteLayout *layout)
{
    int i;

    if (!layout || !layout->visible) return;

    v2_refresh_runtime_state(layout);
    fillRect(0, 0, SCREEN_W, SCREEN_H, PAL_DESKTOP);

    for (i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *widget = layout->all_widgets[i];
        if (!widget || !widget->visible || widget->type != TF_WIDGET_GROUP_BOX) continue;
        tf_draw_widget(widget);
    }

    if (layout->current_page == V2_PAGE_LFO_ENV && layout->amp_env_display && layout->amp_env_display->visible)
        v2_draw_env_display(layout, layout->amp_env_display, 0);
    if (layout->current_page == V2_PAGE_LFO_ENV && layout->eg2_env_display && layout->eg2_env_display->visible)
        v2_draw_env_display(layout, layout->eg2_env_display, 1);
    for (i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *widget = layout->all_widgets[i];
        if (!widget || !widget->visible) continue;
        if (widget->type == TF_WIDGET_GROUP_BOX || widget->type == TF_WIDGET_ENVELOPE_DISPLAY ||
            widget->type == TF_WIDGET_LABEL)
            continue;
        tf_draw_widget(widget);
    }

    for (i = 0; i < layout->all_widget_count; ++i)
    {
        TunefishWidget *widget = layout->all_widgets[i];
        if (!widget || !widget->visible || widget->type != TF_WIDGET_LABEL) continue;
        tf_draw_widget(widget);
    }
}

bool v2_handle_layout_mouse_event(V2CompleteLayout *layout, int mouseX, int mouseY, bool pressed)
{
    int i;

    if (!layout || !layout->visible) return false;

    v2_set_active_instrument(layout);

    if (!pressed)
    {
        /* Release the captured control even when the pointer left its bounds. */
        if (layout->mouse_capture && layout->mouse_capture->type == TF_WIDGET_BUTTON)
            layout->mouse_capture->pressed = false;
        layout->mouse_capture = NULL;
        v2_apply_page_button_state(layout);
        return true;
    }
    layout->mouse_capture = NULL;

    for (i = layout->all_widget_count - 1; i >= 0; --i)
    {
        TunefishWidget *widget = layout->all_widgets[i];
        if (!widget || !widget->visible) continue;
        const int page = layout->current_page;
        if (tf_widget_handle_mouse_event(widget, mouseX, mouseY, true))
        {
            if (layout->visible && widget->visible && layout->current_page == page)
                layout->mouse_capture = widget;
            v2_apply_page_button_state(layout);
            return true;
        }
    }

    return true;
}

bool v2_handle_layout_mouse_drag(V2CompleteLayout *layout, int mouseX, int mouseY)
{
    if (!layout || !layout->visible) return false;
    TunefishWidget *widget = layout->mouse_capture;
    if (!widget || !widget->visible || !widget->enabled) return false;
    if (widget->type != TF_WIDGET_ROTARY_SLIDER && widget->type != TF_WIDGET_LINEAR_SLIDER)
        return false;
    mouseX = v2_clampi(mouseX, widget->x, widget->x + widget->w - 1);
    mouseY = v2_clampi(mouseY, widget->y, widget->y + widget->h - 1);
    return tf_widget_handle_mouse_event(widget, mouseX, mouseY, true);
}

bool v2_handle_layout_keyboard_test(V2CompleteLayout *layout, int key)
{
    if (!layout || !layout->visible) return false;

    switch (key)
    {
        case SDLK_ESCAPE:
            v2_hide_layout(layout);
            ft2_close_synth_editor();
            return true;

        case SDLK_TAB:
            v2_switch_to_page(layout, (layout->current_page + 1) % V2_PAGE_COUNT);
            return true;

        default:
            return false;
    }
}
