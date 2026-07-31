#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ft2_palette.h"

// Framework types
enum {
    FRAMEWORK_TYPE1 = 0,
    FRAMEWORK_TYPE2 = 1
};

extern uint32_t g_palette[256];

typedef enum designer_palette_theme_t
{
    DESIGNER_THEME_ARCTIC = 0,
    DESIGNER_THEME_OP1,
    DESIGNER_THEME_RENOISE,
    DESIGNER_THEME_VSCODE,
    DESIGNER_THEME_COUNT
} designer_palette_theme_t;

void init_palette(void);
void designer_set_framebuffer_height(int height);
void designer_set_palette_theme(designer_palette_theme_t theme);
void designer_cycle_palette_theme(void);
designer_palette_theme_t designer_get_palette_theme(void);
const char *designer_palette_theme_name(designer_palette_theme_t theme);
uint32_t get_palette_color(uint8_t index);
void set_palette_color(uint8_t index, uint32_t color);

// Drawing functions aligned with the engine renderer's clipped software path.
void fill_rect(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, uint32_t color);
void h_line(uint32_t *framebuffer, int fb_width, int x, int y, int w, uint32_t color);
void v_line(uint32_t *framebuffer, int fb_width, int x, int y, int h, uint32_t color);
void draw_framework(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, int type);
void designer_vector_fill_rect(uint32_t *framebuffer, int fb_width, float x, float y, float w, float h, uint32_t color);
void designer_vector_stroke_rect(uint32_t *framebuffer, int fb_width, float x, float y, float w, float h, float thickness, uint32_t color);
void designer_vector_line(uint32_t *framebuffer, int fb_width, float x1, float y1, float x2, float y2, float thickness, uint32_t color);
void designer_vector_arc(uint32_t *framebuffer, int fb_width, float cx, float cy, float radius, float start_angle, float end_angle, float thickness, uint32_t color);
void blit_fast(uint32_t *framebuffer, int fb_width, int x, int y, const uint8_t *src, int w, int h);
void blit_transparent(uint32_t *framebuffer, int fb_width, int x, int y, const uint8_t *src, int w, int h);
