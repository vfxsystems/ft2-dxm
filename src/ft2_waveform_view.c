#include "ft2_waveform_view.h"
#include "ft2_audio.h"
#include "ft2_gui.h"
#include "ft2_video.h"
#include "ft2_palette.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WAVEFORM_BAR_COUNT 32

static float clampf01(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static void drawPixelClamped(int x, int y, uint8_t pal)
{
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
        return;

    video.frameBuffer[(y * SCREEN_W) + x] = video.palette[pal];
}

static void drawLineClamped(int x0, int y0, int x1, int y1, uint8_t pal)
{
    int dx = abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        drawPixelClamped(x0, y0, pal);
        if (x0 == x1 && y0 == y1)
            break;

        const int e2 = err << 1;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

/* Hann window is expensive to compute (one cosf per sample) but depends only on the
   sample count, not on which bar is being analyzed - cache it instead of recomputing
   it redundantly for every one of the 32 bars. */
static float s_hannWindow[AUDIO_OUTPUT_MONITOR_LEN];
static uint32_t s_hannWindowLen = 0;
static float s_windowedSamples[AUDIO_OUTPUT_MONITOR_LEN];

static void buildHannWindow(uint32_t n)
{
    uint32_t i;

    if (n == s_hannWindowLen || n < 2 || n > AUDIO_OUTPUT_MONITOR_LEN)
        return;

    for (i = 0; i < n; ++i)
        s_hannWindow[i] = 0.5f - 0.5f * cosf((2.0f * (float)M_PI * (float)i) / (float)(n - 1));

    s_hannWindowLen = n;
}

/* Per-bar magnitude via the Goertzel algorithm: an O(N) single-bin DFT recurrence that
   needs only two trig calls per bar (to derive the recurrence coefficient), instead of
   the two-per-sample trig calls a direct DFT would require. Mathematically equivalent
   to (and visually identical to) a direct single-frequency DFT of the windowed signal. */
static void computeSpectrumBars(const float *samples, uint32_t sampleCount, float *bars, int barCount)
{
    const uint32_t halfCount = sampleCount >> 1;
    int bar;
    uint32_t n;

    if (bars == NULL || barCount <= 0)
        return;

    memset(bars, 0, (size_t)barCount * sizeof (float));
    if (samples == NULL || sampleCount < 32 || halfCount < 4)
        return;

    if (sampleCount > AUDIO_OUTPUT_MONITOR_LEN)
        sampleCount = AUDIO_OUTPUT_MONITOR_LEN;

    buildHannWindow(sampleCount);
    for (n = 0; n < sampleCount; ++n)
        s_windowedSamples[n] = samples[n] * s_hannWindow[n];

    for (bar = 0; bar < barCount; ++bar)
    {
        const float t = (float)(bar + 1) / (float)barCount;
        const float curve = t * t;
        int bin = 1 + (int)lroundf(curve * (float)(halfCount - 2));
        float real, imag, mag;
        float coeff, cosW, sinW;
        float sPrev = 0.0f, sPrev2 = 0.0f;

        if (bin >= (int)halfCount)
            bin = (int)halfCount - 1;

        cosW = cosf((2.0f * (float)M_PI * (float)bin) / (float)sampleCount);
        sinW = sinf((2.0f * (float)M_PI * (float)bin) / (float)sampleCount);
        coeff = 2.0f * cosW;

        for (n = 0; n < sampleCount; ++n)
        {
            const float s = s_windowedSamples[n] + (coeff * sPrev) - sPrev2;
            sPrev2 = sPrev;
            sPrev = s;
        }

        real = sPrev - (sPrev2 * cosW);
        imag = sPrev2 * sinW;

        mag = sqrtf((real * real) + (imag * imag)) / (float)sampleCount;
        bars[bar] = clampf01(powf(mag * 8.0f, 0.65f));
    }
}

static void drawSpectrum(int x, int y, int w, int h, const float *bars, int barCount)
{
    int i;
    const int innerX = x + 2;
    const int innerY = y + 2;
    const int innerW = w - 4;
    const int innerH = h - 4;

    if (innerW <= 0 || innerH <= 0)
        return;

    fillRect((uint16_t)innerX, (uint16_t)innerY, (uint16_t)innerW, (uint16_t)innerH, PAL_BUTTON2);

    for (i = 0; i < barCount; ++i)
    {
        int barX0 = innerX + (i * innerW) / barCount;
        int barX1 = innerX + ((i + 1) * innerW) / barCount;
        int barW = barX1 - barX0 - 1;
        int barH;
        int py;

        if (barW < 1)
            barW = 1;

        barH = (int)lroundf(clampf01(bars[i]) * (float)(innerH - 1));
        for (py = 0; py < barH; ++py)
        {
            const int yPos = innerY + innerH - 1 - py;
            const uint8_t pal = (py > (innerH * 2) / 3) ? PAL_TEXTMRK :
                                (py > innerH / 3) ? PAL_PATTEXT : PAL_BUTTON1;
            hLine((uint16_t)barX0, (uint16_t)yPos, (uint16_t)barW, pal);
        }
    }

    hLine((uint16_t)innerX, (uint16_t)(innerY + innerH - 1), (uint16_t)innerW, PAL_BUTTON1);
}

static void drawScope(int x, int y, int w, int h, const float *samples, uint32_t sampleCount)
{
    int px;
    const int innerX = x + 2;
    const int innerY = y + 2;
    const int innerW = w - 4;
    const int innerH = h - 4;
    const int centerY = innerY + (innerH / 2);

    if (innerW <= 1 || innerH <= 2)
        return;

    fillRect((uint16_t)innerX, (uint16_t)innerY, (uint16_t)innerW, (uint16_t)innerH, PAL_DESKTOP);
    hLine((uint16_t)innerX, (uint16_t)centerY, (uint16_t)innerW, PAL_BUTTON1);

    for (px = 1; px < innerW; ++px)
    {
        uint32_t idx0, idx1;
        float s0, s1;
        int y0, y1;

        if (sampleCount == 0)
            break;

        idx0 = (uint32_t)(((uint64_t)(px - 1) * (uint64_t)(sampleCount - 1)) / (uint64_t)(innerW - 1));
        idx1 = (uint32_t)(((uint64_t)px * (uint64_t)(sampleCount - 1)) / (uint64_t)(innerW - 1));
        s0 = samples[idx0];
        s1 = samples[idx1];
        if (s0 < -1.0f) s0 = -1.0f; else if (s0 > 1.0f) s0 = 1.0f;
        if (s1 < -1.0f) s1 = -1.0f; else if (s1 > 1.0f) s1 = 1.0f;

        y0 = centerY - (int)lroundf(s0 * (float)((innerH - 2) / 2));
        y1 = centerY - (int)lroundf(s1 * (float)((innerH - 2) / 2));
        drawLineClamped(innerX + px - 1, y0, innerX + px, y1, PAL_PATTEXT);
    }
}

void waveformViewInit(void) { }
void waveformViewShow(bool show) { (void)show; }
bool waveformViewIsVisible(void) { return false; }
bool waveformViewHandleMouse(int mouseX, int mouseY, bool mouseButtonDown)
{
    (void)mouseX;
    (void)mouseY;
    (void)mouseButtonDown;
    return false;
}
void waveformViewUpdate(void) { }
void waveformViewRender(void) { }
void waveformViewSetActive(bool active) { (void)active; }
void waveformViewGetBounds(int *x, int *y, int *w, int *h)
{
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = 0;
    if (h) *h = 0;
}
void waveformViewToggle(void) { }

void ft2_waveform_view_draw(int x, int y, int width, int height)
{
    static float s_cachedBars[WAVEFORM_BAR_COUNT];
    static float s_cachedSamples[AUDIO_OUTPUT_MONITOR_LEN];
    static uint32_t s_cachedSampleCount = 0;
    static uint32_t s_lastGeneration = 0xFFFFFFFFu;

    uint32_t generation;
    int topH, bottomH;

    if (width < 16 || height < 16)
        return;

    s_cachedSampleCount = audioGetOutputMonitor(s_cachedSamples, AUDIO_OUTPUT_MONITOR_LEN);
    if (s_cachedSampleCount == 0)
        memset(s_cachedSamples, 0, sizeof (s_cachedSamples));

    /* Keep the oscilloscope repainting at the video-loop rate, but only run the
       heavier spectrum analysis when the audio callback publishes a fresh block. */
    generation = audioGetOutputMonitorGeneration();
    if (generation != s_lastGeneration)
    {
        computeSpectrumBars(s_cachedSamples, s_cachedSampleCount, s_cachedBars, WAVEFORM_BAR_COUNT);
        s_lastGeneration = generation;
    }

    drawFramework((uint16_t)(x - 1), (uint16_t)(y - 1), (uint16_t)(width + 2), (uint16_t)(height + 2), FRAMEWORK_TYPE2);

    topH = height / 2;
    bottomH = height - topH;

    drawSpectrum(x, y, width, topH, s_cachedBars, WAVEFORM_BAR_COUNT);
    drawScope(x, y + topH, width, bottomH, s_cachedSamples, s_cachedSampleCount);
}
