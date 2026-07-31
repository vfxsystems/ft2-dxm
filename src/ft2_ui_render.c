#include "ft2_ui_render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_header.h"
#include "ft2_gui.h"
#include "ft2_palette.h"
#include "ft2_video.h"

#ifdef FT2_ENABLE_TTF
#include <SDL2/SDL_ttf.h>
#endif

typedef struct ft2_ui_render_state_t
{
	ft2_ui_render_backend_t backend;
	ft2_ui_render_metrics_t metrics;
#ifdef FT2_ENABLE_TTF
	bool ttfInitialized;
	TTF_Font *ttfFont;
#endif
} ft2_ui_render_state_t;

static ft2_ui_render_state_t uiRender;

static int32_t iroundf32(float v)
{
	return (int32_t)((v < 0.0f) ? (v - 0.5f) : (v + 0.5f));
}

static bool clipRectToScreen(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
	if (x == NULL || y == NULL || w == NULL || h == NULL || *w <= 0 || *h <= 0)
		return false;

	if (*x < 0)
	{
		*w += *x;
		*x = 0;
	}

	if (*y < 0)
	{
		*h += *y;
		*y = 0;
	}

	if (*x >= SCREEN_W || *y >= SCREEN_H)
		return false;

	if (*x + *w > SCREEN_W)
		*w = SCREEN_W - *x;

	if (*y + *h > SCREEN_H)
		*h = SCREEN_H - *y;

	return *w > 0 && *h > 0;
}

static void fillRectColor(uint32_t *frameBuffer, const uint32_t *palette, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
	(void)palette;

	if (frameBuffer == NULL || !clipRectToScreen(&x, &y, &w, &h))
		return;

	uint32_t *dstPtr = &frameBuffer[(y * SCREEN_W) + x];
	for (int32_t yy = 0; yy < h; yy++, dstPtr += SCREEN_W)
	{
		for (int32_t xx = 0; xx < w; xx++)
			dstPtr[xx] = color;
	}
}

static void fillRectPalette(uint32_t *frameBuffer, const uint32_t *palette, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t paletteIndex)
{
	if (palette == NULL)
		return;

	fillRectColor(frameBuffer, palette, x, y, w, h, palette[paletteIndex]);
}

static void clearRectBuffer(uint32_t *frameBuffer, int32_t x, int32_t y, int32_t w, int32_t h)
{
	if (frameBuffer == NULL || !clipRectToScreen(&x, &y, &w, &h))
		return;

	const size_t pitch = (size_t)w * sizeof (uint32_t);
	uint32_t *dstPtr = &frameBuffer[(y * SCREEN_W) + x];
	for (int32_t yy = 0; yy < h; yy++, dstPtr += SCREEN_W)
		memset(dstPtr, 0, pitch);
}

static void putPixelClipped(uint32_t *frameBuffer, const uint32_t *palette, int32_t x, int32_t y, uint8_t paletteIndex)
{
	if (frameBuffer == NULL || palette == NULL || x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H)
		return;

	frameBuffer[(y * SCREEN_W) + x] = palette[paletteIndex];
}

static void linePalette(uint32_t *frameBuffer, const uint32_t *palette, int32_t x1, int32_t x2, int32_t y1, int32_t y2, uint8_t paletteIndex)
{
	const int32_t dx = abs(x2 - x1);
	const int32_t sx = (x1 < x2) ? 1 : -1;
	const int32_t dy = -abs(y2 - y1);
	const int32_t sy = (y1 < y2) ? 1 : -1;
	int32_t err = dx + dy;

	while (true)
	{
		putPixelClipped(frameBuffer, palette, x1, y1, paletteIndex);

		if (x1 == x2 && y1 == y2)
			break;

		const int32_t e2 = err * 2;
		if (e2 >= dy)
		{
			err += dy;
			x1 += sx;
		}

		if (e2 <= dx)
		{
			err += dx;
			y1 += sy;
		}
	}
}

static void frameworkPalette(uint32_t *frameBuffer, const uint32_t *palette, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t type)
{
	if (w < 2 || h < 2)
		return;

	h--;
	w--;

	if (type == FRAMEWORK_TYPE1)
	{
		fillRectPalette(frameBuffer, palette, x,     y,     w,     1,     PAL_DSKTOP1);
		fillRectPalette(frameBuffer, palette, x,     y + 1, 1,     h - 1, PAL_DSKTOP1);
		fillRectPalette(frameBuffer, palette, x,     y + h, w,     1,     PAL_DSKTOP2);
		fillRectPalette(frameBuffer, palette, x + w, y,     1,     h + 1, PAL_DSKTOP2);
		fillRectPalette(frameBuffer, palette, x + 1, y + 1, w - 1, h - 1, PAL_DESKTOP);
	}
	else
	{
		fillRectPalette(frameBuffer, palette, x,     y,     w + 1, 1,     PAL_DSKTOP2);
		fillRectPalette(frameBuffer, palette, x,     y + 1, 1,     h,     PAL_DSKTOP2);
		fillRectPalette(frameBuffer, palette, x + 1, y + h, w,     1,     PAL_DSKTOP1);
		fillRectPalette(frameBuffer, palette, x + w, y + 1, 1,     h - 1, PAL_DSKTOP1);
		clearRectBuffer(frameBuffer, x + 1, y + 1, w - 1, h - 1);
	}
}

void ft2_ui_render_reset(void)
{
	ft2_ui_render_shutdown();
	memset(&uiRender, 0, sizeof (uiRender));
	uiRender.backend = FT2_UI_RENDER_BACKEND_SOFTWARE;
	uiRender.metrics.logicalToOutputX = 1.0;
	uiRender.metrics.logicalToOutputY = 1.0;
	uiRender.metrics.dpiScaleX = 1.0;
	uiRender.metrics.dpiScaleY = 1.0;
}

void ft2_ui_render_shutdown(void)
{
#ifdef FT2_ENABLE_TTF
	if (uiRender.ttfFont != NULL)
	{
		TTF_CloseFont(uiRender.ttfFont);
		uiRender.ttfFont = NULL;
	}

	if (uiRender.ttfInitialized)
	{
		TTF_Quit();
		uiRender.ttfInitialized = false;
	}
#endif
}

void ft2_ui_render_set_backend(ft2_ui_render_backend_t backend)
{
	if (backend != FT2_UI_RENDER_BACKEND_SOFTWARE && backend != FT2_UI_RENDER_BACKEND_SDL_VECTOR)
		backend = FT2_UI_RENDER_BACKEND_SOFTWARE;

	uiRender.backend = backend;
}

ft2_ui_render_backend_t ft2_ui_render_get_backend(void)
{
	return uiRender.backend;
}

void ft2_ui_render_set_metrics(double logicalToOutputX, double logicalToOutputY, double dpiScaleX, double dpiScaleY)
{
	uiRender.metrics.logicalToOutputX = (logicalToOutputX > 0.0) ? logicalToOutputX : 1.0;
	uiRender.metrics.logicalToOutputY = (logicalToOutputY > 0.0) ? logicalToOutputY : 1.0;
	uiRender.metrics.dpiScaleX = (dpiScaleX > 0.0) ? dpiScaleX : 1.0;
	uiRender.metrics.dpiScaleY = (dpiScaleY > 0.0) ? dpiScaleY : 1.0;
}

ft2_ui_render_metrics_t ft2_ui_render_get_metrics(void)
{
	ft2_ui_render_metrics_t metrics = uiRender.metrics;

	if (metrics.logicalToOutputX <= 0.0)
		metrics.logicalToOutputX = 1.0;
	if (metrics.logicalToOutputY <= 0.0)
		metrics.logicalToOutputY = 1.0;
	if (metrics.dpiScaleX <= 0.0)
		metrics.dpiScaleX = 1.0;
	if (metrics.dpiScaleY <= 0.0)
		metrics.dpiScaleY = 1.0;

	return metrics;
}

void ft2_ui_render_clear_rect(int32_t x, int32_t y, int32_t w, int32_t h)
{
	clearRectBuffer(video.frameBuffer, x, y, w, h);
}

void ft2_ui_render_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t paletteIndex)
{
	fillRectPalette(video.frameBuffer, video.palette, x, y, w, h, paletteIndex);
}

void ft2_ui_render_hline(int32_t x, int32_t y, int32_t w, uint8_t paletteIndex)
{
	fillRectPalette(video.frameBuffer, video.palette, x, y, w, 1, paletteIndex);
}

void ft2_ui_render_vline(int32_t x, int32_t y, int32_t h, uint8_t paletteIndex)
{
	fillRectPalette(video.frameBuffer, video.palette, x, y, 1, h, paletteIndex);
}

void ft2_ui_render_line(int32_t x1, int32_t x2, int32_t y1, int32_t y2, uint8_t paletteIndex)
{
	linePalette(video.frameBuffer, video.palette, x1, x2, y1, y2, paletteIndex);
}

void ft2_ui_render_framework(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t type)
{
	frameworkPalette(video.frameBuffer, video.palette, x, y, w, h, type);
}

void ft2_ui_render_fill_rect_f(ft2_ui_rectf_t rect, uint8_t paletteIndex)
{
	ft2_ui_render_fill_rect(iroundf32(rect.x), iroundf32(rect.y), iroundf32(rect.w), iroundf32(rect.h), paletteIndex);
}

void ft2_ui_render_stroke_rect_f(ft2_ui_rectf_t rect, float thickness, uint8_t paletteIndex)
{
	const int32_t t = MAX(1, iroundf32(thickness));
	const int32_t x = iroundf32(rect.x);
	const int32_t y = iroundf32(rect.y);
	const int32_t w = iroundf32(rect.w);
	const int32_t h = iroundf32(rect.h);

	ft2_ui_render_fill_rect(x,         y,         w, t, paletteIndex);
	ft2_ui_render_fill_rect(x,         y + h - t, w, t, paletteIndex);
	ft2_ui_render_fill_rect(x,         y,         t, h, paletteIndex);
	ft2_ui_render_fill_rect(x + w - t, y,         t, h, paletteIndex);
}

void ft2_ui_render_line_f(ft2_ui_pointf_t a, ft2_ui_pointf_t b, float thickness, uint8_t paletteIndex)
{
	const int32_t t = MAX(1, iroundf32(thickness));

	if (t == 1)
	{
		ft2_ui_render_line(iroundf32(a.x), iroundf32(b.x), iroundf32(a.y), iroundf32(b.y), paletteIndex);
		return;
	}

	for (int32_t i = -(t / 2); i <= (t / 2); i++)
	{
		ft2_ui_render_line(iroundf32(a.x) + i, iroundf32(b.x) + i, iroundf32(a.y), iroundf32(b.y), paletteIndex);
		ft2_ui_render_line(iroundf32(a.x), iroundf32(b.x), iroundf32(a.y) + i, iroundf32(b.y) + i, paletteIndex);
	}
}

bool ft2_ui_render_ttf_available(void)
{
#ifdef FT2_ENABLE_TTF
	return true;
#else
	return false;
#endif
}

bool ft2_ui_render_load_ttf_font(const char *path, int ptSize)
{
#ifdef FT2_ENABLE_TTF
	if (path == NULL || path[0] == '\0' || ptSize <= 0)
		return false;

	if (!uiRender.ttfInitialized)
	{
		if (TTF_Init() != 0)
			return false;

		uiRender.ttfInitialized = true;
	}

	ft2_ui_render_unload_ttf_font();
	uiRender.ttfFont = TTF_OpenFont(path, ptSize);
	return uiRender.ttfFont != NULL;
#else
	(void)path;
	(void)ptSize;
	return false;
#endif
}

void ft2_ui_render_unload_ttf_font(void)
{
#ifdef FT2_ENABLE_TTF
	if (uiRender.ttfFont != NULL)
	{
		TTF_CloseFont(uiRender.ttfFont);
		uiRender.ttfFont = NULL;
	}
#endif
}

bool ft2_ui_render_ttf_font_loaded(void)
{
#ifdef FT2_ENABLE_TTF
	return uiRender.ttfFont != NULL;
#else
	return false;
#endif
}

bool ft2_ui_render_text_ttf(int32_t x, int32_t y, uint8_t paletteIndex, const char *text)
{
#ifdef FT2_ENABLE_TTF
	if (uiRender.ttfFont == NULL || video.frameBuffer == NULL || text == NULL || text[0] == '\0')
		return false;

	const uint32_t pal = video.palette[paletteIndex];
	SDL_Color color = { RGB32_R(pal), RGB32_G(pal), RGB32_B(pal), SDL_ALPHA_OPAQUE };
	SDL_Surface *surface = TTF_RenderUTF8_Blended(uiRender.ttfFont, text, color);
	if (surface == NULL)
		return false;

	SDL_Surface *argb = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
	SDL_FreeSurface(surface);
	if (argb == NULL)
		return false;

	const uint32_t *srcRows = (const uint32_t *)argb->pixels;
	for (int32_t yy = 0; yy < argb->h; yy++)
	{
		const int32_t dstY = y + yy;
		if (dstY < 0 || dstY >= SCREEN_H)
			continue;

		const uint32_t *src = (const uint32_t *)((const uint8_t *)srcRows + ((size_t)yy * argb->pitch));
		for (int32_t xx = 0; xx < argb->w; xx++)
		{
			const int32_t dstX = x + xx;
			if (dstX < 0 || dstX >= SCREEN_W)
				continue;

			const uint32_t sp = src[xx];
			const uint32_t a = sp >> 24;
			if (a == 0)
				continue;

			uint32_t *dst = &video.frameBuffer[(dstY * SCREEN_W) + dstX];
			if (a == 255)
			{
				*dst = 0xFF000000 | (sp & 0x00FFFFFF);
			}
			else
			{
				const uint32_t invA = 255 - a;
				const uint32_t sr = (sp >> 16) & 0xFF;
				const uint32_t sg = (sp >>  8) & 0xFF;
				const uint32_t sb =  sp        & 0xFF;
				const uint32_t dr = (*dst >> 16) & 0xFF;
				const uint32_t dg = (*dst >>  8) & 0xFF;
				const uint32_t db =  *dst        & 0xFF;
				*dst = 0xFF000000 |
					(((sr * a) + (dr * invA) + 127) / 255) << 16 |
					(((sg * a) + (dg * invA) + 127) / 255) << 8 |
					 ((sb * a) + (db * invA) + 127) / 255;
			}
		}
	}

	SDL_FreeSurface(argb);
	return true;
#else
	(void)x;
	(void)y;
	(void)paletteIndex;
	(void)text;
	return false;
#endif
}

static bool expectPixel(const uint32_t *buffer, int32_t x, int32_t y, uint32_t expected, char *errBuf, size_t errBufSize, const char *label)
{
	const uint32_t actual = buffer[(y * SCREEN_W) + x];
	if (actual == expected)
		return true;

	snprintf(errBuf, errBufSize, "ui-render %s pixel mismatch at %d,%d: got 0x%08X expected 0x%08X",
		label, x, y, actual, expected);
	return false;
}

bool ft2_ui_render_self_test(char *errBuf, size_t errBufSize)
{
	uint32_t *buffer = (uint32_t *)calloc((size_t)SCREEN_W * SCREEN_H, sizeof (uint32_t));
	uint32_t palette[PAL_NUM];

	if (buffer == NULL)
	{
		snprintf(errBuf, errBufSize, "ui-render test allocation failed");
		return false;
	}

	for (int32_t i = 0; i < PAL_NUM; i++)
		palette[i] = 0xFF000000 | (uint32_t)(i * 0x00010101);

	fillRectPalette(buffer, palette, -2, -3, 6, 7, PAL_FORGRND);
	if (!expectPixel(buffer, 0, 0, palette[PAL_FORGRND], errBuf, errBufSize, "clipped-fill") ||
	    !expectPixel(buffer, 3, 3, palette[PAL_FORGRND], errBuf, errBufSize, "clipped-fill") ||
	    !expectPixel(buffer, 4, 4, 0, errBuf, errBufSize, "clipped-fill"))
	{
		free(buffer);
		return false;
	}

	linePalette(buffer, palette, 10, 14, 20, 20, PAL_DSKTOP1);
	if (!expectPixel(buffer, 10, 20, palette[PAL_DSKTOP1], errBuf, errBufSize, "hline") ||
	    !expectPixel(buffer, 14, 20, palette[PAL_DSKTOP1], errBuf, errBufSize, "hline"))
	{
		free(buffer);
		return false;
	}

	frameworkPalette(buffer, palette, 30, 40, 6, 5, FRAMEWORK_TYPE1);
	if (!expectPixel(buffer, 30, 40, palette[PAL_DSKTOP1], errBuf, errBufSize, "framework") ||
	    !expectPixel(buffer, 35, 44, palette[PAL_DSKTOP2], errBuf, errBufSize, "framework") ||
	    !expectPixel(buffer, 31, 41, palette[PAL_DESKTOP], errBuf, errBufSize, "framework"))
	{
		free(buffer);
		return false;
	}

	free(buffer);
	return true;
}
