#pragma once

#include <stdint.h>
#include "ft2_palette.h"

// Framework types
enum {
    FRAMEWORK_TYPE1 = 0,
    FRAMEWORK_TYPE2 = 1
};

extern uint32_t g_palette[256];

void init_palette(void);
uint32_t get_palette_color(uint8_t index);
void set_palette_color(uint8_t index, uint32_t color);

// Drawing functions (extracted from FT2's ft2_gui.c)
void fill_rect(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, uint32_t color);
void h_line(uint32_t *framebuffer, int fb_width, int x, int y, int w, uint32_t color);
void v_line(uint32_t *framebuffer, int fb_width, int x, int y, int h, uint32_t color);
void draw_framework(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, int type);
void blit_fast(uint32_t *framebuffer, int fb_width, int x, int y, const uint8_t *src, int w, int h);
void blit_transparent(uint32_t *framebuffer, int fb_width, int x, int y, const uint8_t *src, int w, int h);
