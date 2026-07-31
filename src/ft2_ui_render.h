#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum ft2_ui_render_backend_t
{
	FT2_UI_RENDER_BACKEND_SOFTWARE = 0,
	FT2_UI_RENDER_BACKEND_SDL_VECTOR = 1
} ft2_ui_render_backend_t;

typedef enum ft2_ui_theme_t
{
	FT2_UI_THEME_CLASSIC = 0,
	FT2_UI_THEME_OP1,
	FT2_UI_THEME_RENOISE,
	FT2_UI_THEME_VSCODE,
	FT2_UI_THEME_COUNT
} ft2_ui_theme_t;

typedef struct ft2_ui_pointf_t
{
	float x, y;
} ft2_ui_pointf_t;

typedef struct ft2_ui_rectf_t
{
	float x, y, w, h;
} ft2_ui_rectf_t;

typedef struct ft2_ui_render_metrics_t
{
	double logicalToOutputX, logicalToOutputY;
	double dpiScaleX, dpiScaleY;
} ft2_ui_render_metrics_t;

void ft2_ui_render_reset(void);
void ft2_ui_render_shutdown(void);
void ft2_ui_render_set_backend(ft2_ui_render_backend_t backend);
ft2_ui_render_backend_t ft2_ui_render_get_backend(void);
bool ft2_ui_render_set_backend_name(const char *name);
const char *ft2_ui_render_backend_name(ft2_ui_render_backend_t backend);
void ft2_ui_render_set_metrics(double logicalToOutputX, double logicalToOutputY, double dpiScaleX, double dpiScaleY);
ft2_ui_render_metrics_t ft2_ui_render_get_metrics(void);
bool ft2_ui_render_set_theme(ft2_ui_theme_t theme, bool redrawScreen);
bool ft2_ui_render_set_theme_name(const char *name, bool redrawScreen);
ft2_ui_theme_t ft2_ui_render_get_theme(void);
const char *ft2_ui_render_theme_name(ft2_ui_theme_t theme);
void ft2_ui_render_configure_from_env(void);

void ft2_ui_render_clear_rect(int32_t x, int32_t y, int32_t w, int32_t h);
void ft2_ui_render_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t paletteIndex);
void ft2_ui_render_hline(int32_t x, int32_t y, int32_t w, uint8_t paletteIndex);
void ft2_ui_render_vline(int32_t x, int32_t y, int32_t h, uint8_t paletteIndex);
void ft2_ui_render_line(int32_t x1, int32_t x2, int32_t y1, int32_t y2, uint8_t paletteIndex);
void ft2_ui_render_framework(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t type);

void ft2_ui_render_fill_rect_f(ft2_ui_rectf_t rect, uint8_t paletteIndex);
void ft2_ui_render_stroke_rect_f(ft2_ui_rectf_t rect, float thickness, uint8_t paletteIndex);
void ft2_ui_render_line_f(ft2_ui_pointf_t a, ft2_ui_pointf_t b, float thickness, uint8_t paletteIndex);
void ft2_ui_render_arc_f(ft2_ui_pointf_t center, float radius, float startAngle, float endAngle, float thickness, uint8_t paletteIndex);

bool ft2_ui_render_ttf_available(void);
bool ft2_ui_render_load_ttf_font(const char *path, int ptSize);
void ft2_ui_render_unload_ttf_font(void);
bool ft2_ui_render_ttf_font_loaded(void);
bool ft2_ui_render_text_ttf(int32_t x, int32_t y, uint8_t paletteIndex, const char *text);

bool ft2_ui_render_self_test(char *errBuf, size_t errBufSize);
