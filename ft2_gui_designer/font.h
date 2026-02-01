#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "shared/ft2_ui_assets.h"

// Font dimensions
#define FONT1_CHAR_W 8
#define FONT1_CHAR_H 10
#define FONT1_WIDTH 1024

// Font system
typedef struct {
    uint8_t *font_data[FT2_UI_FONT_COUNT];
    uint16_t font_bmp_h[FT2_UI_FONT_COUNT];
    uint16_t font_chars_per_row[FT2_UI_FONT_COUNT];
    const ft2_ui_font_asset_t *font_assets;
    uint32_t *framebuffer;
    int screen_width;
    int screen_height;
} font_system_t;

// Function declarations
bool init_font_system(font_system_t *fs, uint32_t *framebuffer, int width, int height);
void cleanup_font_system(font_system_t *fs);
void font_draw_char(font_system_t *fs, int x, int y, char ch, uint32_t color);
void font_draw_char_with_font(font_system_t *fs, int x, int y, char ch, uint32_t color, ft2_ui_font_id_t font_id);
void font_draw_text(font_system_t *fs, int x, int y, const char *text, uint32_t color);
void font_draw_text_with_font(font_system_t *fs, int x, int y, const char *text, uint32_t color, ft2_ui_font_id_t font_id);
int font_get_char_width(char ch);
int font_get_text_width(const char *text);
int font_get_char_width_with_font(ft2_ui_font_id_t font_id, char ch);
int font_get_text_width_with_font(ft2_ui_font_id_t font_id, const char *text);
int font_get_char_height_with_font(ft2_ui_font_id_t font_id);
