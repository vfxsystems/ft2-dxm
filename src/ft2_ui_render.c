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
	ft2_ui_theme_t theme;
	ft2_ui_render_metrics_t metrics;
#ifdef FT2_ENABLE_TTF
	bool ttfInitialized;
	TTF_Font *ttfFont;
#endif
} ft2_ui_render_state_t;

static ft2_ui_render_state_t uiRender;

typedef struct ft2_ui_theme_desc_t
{
	ft2_ui_theme_t theme;
	const char *id;
	const char *label;
	uint32_t pal[16];
} ft2_ui_theme_desc_t;

#define PALRGB(r, g, b) RGB32((r), (g), (b))
#define PAL6(r, g, b) PALRGB(P6_TO_P8(r), P6_TO_P8(g), P6_TO_P8(b))

static const ft2_ui_theme_desc_t uiThemeDescs[FT2_UI_THEME_COUNT] =
{
	{
		FT2_UI_THEME_CLASSIC, "classic", "FT2 Arctic",
		{
			PAL6(0, 0, 0),    PAL6(30, 38, 63), PAL6(0, 0, 17),   PAL6(63, 63, 63),
			PAL6(27, 36, 40), PAL6(63, 63, 63), PAL6(40, 40, 40), PAL6(0, 0, 0),
			PAL6(10, 13, 14), PAL6(49, 63, 63), PAL6(15, 15, 15), PAL6(63, 63, 63),
			PAL6(63, 63, 63), PAL6(63, 63, 63), PAL6(63, 63, 63), PAL6(63, 63, 63)
		}
	},
	{
		FT2_UI_THEME_OP1, "op1", "OP-1 Inspired",
		{
			PALRGB(18, 18, 16),    PALRGB(60, 69, 67),    PALRGB(225, 96, 74),   PALRGB(251, 246, 231),
			PALRGB(222, 216, 195), PALRGB(34, 34, 31),    PALRGB(188, 184, 166), PALRGB(24, 24, 22),
			PALRGB(165, 158, 141), PALRGB(255, 252, 241), PALRGB(238, 183, 64),  PALRGB(49, 49, 44),
			PALRGB(79, 154, 172),  PALRGB(104, 181, 121), PALRGB(225, 96, 74),   PALRGB(251, 246, 231)
		}
	},
	{
		FT2_UI_THEME_RENOISE, "renoise", "Renoise Inspired",
		{
			PALRGB(12, 13, 12),   PALRGB(172, 216, 113), PALRGB(33, 37, 35),   PALRGB(230, 235, 220),
			PALRGB(42, 46, 43),   PALRGB(226, 235, 212), PALRGB(62, 68, 64),   PALRGB(10, 11, 10),
			PALRGB(24, 28, 26),   PALRGB(119, 179, 97),  PALRGB(29, 31, 30),   PALRGB(92, 105, 97),
			PALRGB(222, 176, 72), PALRGB(88, 158, 211),  PALRGB(201, 91, 82),  PALRGB(235, 235, 220)
		}
	},
	{
		FT2_UI_THEME_VSCODE, "vscode", "VS Code Inspired",
		{
			PALRGB(10, 10, 10),    PALRGB(156, 220, 254), PALRGB(24, 24, 24),    PALRGB(220, 220, 220),
			PALRGB(37, 37, 38),    PALRGB(225, 225, 225), PALRGB(51, 51, 51),    PALRGB(0, 0, 0),
			PALRGB(30, 30, 30),    PALRGB(86, 156, 214),  PALRGB(45, 45, 48),    PALRGB(63, 63, 70),
			PALRGB(197, 134, 192), PALRGB(78, 201, 176),  PALRGB(206, 145, 120), PALRGB(220, 220, 220)
		}
	}
};

static int32_t iroundf32(float v)
{
	return (int32_t)((v < 0.0f) ? (v - 0.5f) : (v + 0.5f));
}

static bool strEqualFold(const char *a, const char *b)
{
	if (a == NULL || b == NULL)
		return false;

	while (*a != '\0' || *b != '\0')
	{
		while (*a == '_' || *a == '-' || *a == ' ')
			a++;
		while (*b == '_' || *b == '-' || *b == ' ')
			b++;

		char ca = *a;
		char cb = *b;

		if (ca == '\0' || cb == '\0')
			return ca == cb;

		a++;
		b++;

		if (ca >= 'A' && ca <= 'Z')
			ca = (char)(ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z')
			cb = (char)(cb - 'A' + 'a');

		if (ca != cb)
			return false;
	}

	return true;
}

static void refreshFrameBufferPalette(void)
{
	if (video.frameBuffer == NULL)
		return;

	for (int32_t i = 0; i < SCREEN_W*SCREEN_H; i++)
		video.frameBuffer[i] = video.palette[(video.frameBuffer[i] >> 24) & 15];
}

static void deriveClonePaletteEntries(void)
{
#define LOOP_PIN_COL_SUB 96
#define TEXT_MARK_COLOR 0x0078D7
#define BOX_SELECT_COLOR 0x7F7F7F

	video.palette[PAL_TEXTMRK] = (PAL_TEXTMRK << 24) | TEXT_MARK_COLOR;
	video.palette[PAL_BOXSLCT] = (PAL_BOXSLCT << 24) | BOX_SELECT_COLOR;

	int32_t r = RGB32_R(video.palette[PAL_PATTEXT]);
	int32_t g = RGB32_G(video.palette[PAL_PATTEXT]);
	int32_t b = RGB32_B(video.palette[PAL_PATTEXT]);

	r = MAX(r - LOOP_PIN_COL_SUB, 0);
	g = MAX(g - LOOP_PIN_COL_SUB, 0);
	b = MAX(b - LOOP_PIN_COL_SUB, 0);

	video.palette[PAL_LOOPPIN] = (PAL_LOOPPIN << 24) | RGB32(r, g, b);
	video.palette[PAL_CUSTOM] = (PAL_CUSTOM << 24);
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
	uiRender.theme = FT2_UI_THEME_CLASSIC;
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

#ifndef FT2_ENABLE_VECTOR_UI
	if (backend == FT2_UI_RENDER_BACKEND_SDL_VECTOR)
		backend = FT2_UI_RENDER_BACKEND_SOFTWARE;
#endif

	uiRender.backend = backend;
}

ft2_ui_render_backend_t ft2_ui_render_get_backend(void)
{
	return uiRender.backend;
}

bool ft2_ui_render_set_backend_name(const char *name)
{
	if (name == NULL || name[0] == '\0' || strEqualFold(name, "software") || strEqualFold(name, "classic"))
	{
		ft2_ui_render_set_backend(FT2_UI_RENDER_BACKEND_SOFTWARE);
		return true;
	}

	if (strEqualFold(name, "vector") || strEqualFold(name, "sdlvector") || strEqualFold(name, "sdl"))
	{
#ifdef FT2_ENABLE_VECTOR_UI
		ft2_ui_render_set_backend(FT2_UI_RENDER_BACKEND_SDL_VECTOR);
		return true;
#else
		ft2_ui_render_set_backend(FT2_UI_RENDER_BACKEND_SOFTWARE);
		return false;
#endif
	}

	return false;
}

const char *ft2_ui_render_backend_name(ft2_ui_render_backend_t backend)
{
	switch (backend)
	{
		case FT2_UI_RENDER_BACKEND_SDL_VECTOR: return "vector";
		case FT2_UI_RENDER_BACKEND_SOFTWARE:
		default: return "software";
	}
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

bool ft2_ui_render_set_theme(ft2_ui_theme_t theme, bool redrawScreen)
{
	if (theme < 0 || theme >= FT2_UI_THEME_COUNT)
		return false;

	const ft2_ui_theme_desc_t *desc = &uiThemeDescs[theme];
	for (int32_t i = 0; i < 16; i++)
		video.palette[i] = (i << 24) | desc->pal[i];

	deriveClonePaletteEntries();
	uiRender.theme = theme;

	if (redrawScreen)
		refreshFrameBufferPalette();

	return true;
}

bool ft2_ui_render_set_theme_name(const char *name, bool redrawScreen)
{
	if (name == NULL || name[0] == '\0')
		return false;

	for (int32_t i = 0; i < FT2_UI_THEME_COUNT; i++)
	{
		if (strEqualFold(name, uiThemeDescs[i].id) || strEqualFold(name, uiThemeDescs[i].label))
			return ft2_ui_render_set_theme(uiThemeDescs[i].theme, redrawScreen);
	}

	if (strEqualFold(name, "op") || strEqualFold(name, "op1inspired"))
		return ft2_ui_render_set_theme(FT2_UI_THEME_OP1, redrawScreen);

	if (strEqualFold(name, "vs") || strEqualFold(name, "vscodeinspired"))
		return ft2_ui_render_set_theme(FT2_UI_THEME_VSCODE, redrawScreen);

	return false;
}

ft2_ui_theme_t ft2_ui_render_get_theme(void)
{
	return uiRender.theme;
}

const char *ft2_ui_render_theme_name(ft2_ui_theme_t theme)
{
	if (theme < 0 || theme >= FT2_UI_THEME_COUNT)
		theme = FT2_UI_THEME_CLASSIC;

	return uiThemeDescs[theme].label;
}

void ft2_ui_render_configure_from_env(void)
{
	const char *backend = getenv("FT2_UI_BACKEND");
	const char *theme = getenv("FT2_UI_THEME");

	if (backend != NULL && backend[0] != '\0')
		(void)ft2_ui_render_set_backend_name(backend);

	if (theme != NULL && theme[0] != '\0')
		(void)ft2_ui_render_set_theme_name(theme, false);
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

void ft2_ui_render_arc_f(ft2_ui_pointf_t center, float radius, float startAngle, float endAngle, float thickness, uint8_t paletteIndex)
{
	if (radius <= 0.0f || thickness <= 0.0f || startAngle == endAngle)
		return;

	const float sweep = endAngle - startAngle;
	int32_t steps = (int32_t)ceilf(fabsf(sweep) * radius * 0.75f);
	if (steps < 4)
		steps = 4;
	else if (steps > 192)
		steps = 192;

	ft2_ui_pointf_t prev = {
		center.x + cosf(startAngle) * radius,
		center.y + sinf(startAngle) * radius
	};

	for (int32_t i = 1; i <= steps; i++)
	{
		const float t = (float)i / (float)steps;
		const float angle = startAngle + sweep * t;
		const ft2_ui_pointf_t next = {
			center.x + cosf(angle) * radius,
			center.y + sinf(angle) * radius
		};

		ft2_ui_render_line_f(prev, next, thickness, paletteIndex);
		prev = next;
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

	ft2_ui_render_set_metrics(2.0, 3.0, 1.5, 2.5);
	const ft2_ui_render_metrics_t metrics = ft2_ui_render_get_metrics();
	if (metrics.logicalToOutputX != 2.0 || metrics.logicalToOutputY != 3.0 ||
	    metrics.dpiScaleX != 1.5 || metrics.dpiScaleY != 2.5)
	{
		snprintf(errBuf, errBufSize, "ui-render metrics mismatch");
		free(buffer);
		return false;
	}

	ft2_ui_render_set_backend(FT2_UI_RENDER_BACKEND_SDL_VECTOR);
#ifdef FT2_ENABLE_VECTOR_UI
	if (ft2_ui_render_get_backend() != FT2_UI_RENDER_BACKEND_SDL_VECTOR)
#else
	if (ft2_ui_render_get_backend() != FT2_UI_RENDER_BACKEND_SOFTWARE)
#endif
	{
		snprintf(errBuf, errBufSize, "ui-render backend selection mismatch");
		free(buffer);
		return false;
	}

	if (!ft2_ui_render_set_theme_name("op-1", false) || ft2_ui_render_get_theme() != FT2_UI_THEME_OP1 ||
	    video.palette[PAL_PATTEXT] != ((PAL_PATTEXT << 24) | RGB32(60, 69, 67)))
	{
		snprintf(errBuf, errBufSize, "ui-render theme selection mismatch");
		free(buffer);
		return false;
	}

	free(buffer);
	return true;
}
