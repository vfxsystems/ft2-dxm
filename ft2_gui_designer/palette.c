#include "palette.h"
#include <string.h>

// Global palette array
uint32_t g_palette[256];

// FT2 "Arctic" palette (palTable[0] from ft2_tables.c), 6-bit VGA RGB
static const uint8_t arctic_palette[16][3] = {
    {0, 0, 0},   {30, 38, 63}, {0, 0, 17},  {63, 63, 63},
    {27, 36, 40},{63, 63, 63}, {40, 40, 40},{0, 0, 0},
    {10, 13, 14},{49, 63, 63}, {15, 15, 15},{63, 63, 63},
    {63, 63, 63},{63, 63, 63}, {63, 63, 63},{63, 63, 63}
};

void init_palette(void)
{
    // Initialize with FT2 Arctic palette in real palette order
    for (int i = 0; i < 16; i++) {
        uint8_t r = P6_TO_P8(arctic_palette[i][0]);
        uint8_t g = P6_TO_P8(arctic_palette[i][1]);
        uint8_t b = P6_TO_P8(arctic_palette[i][2]);
        g_palette[i] = 0xFF000000 | RGB32(r, g, b);
    }

    // Custom FT2 clone palette entries
    g_palette[PAL_TEXTMRK] = 0xFF0078D7;
    g_palette[PAL_BOXSLCT] = 0xFF7F7F7F;

    // Loop pin color is derived from PAL_PATTEXT (subtract 96 from RGB)
    uint8_t r = RGB32_R(g_palette[PAL_PATTEXT]);
    uint8_t g = RGB32_G(g_palette[PAL_PATTEXT]);
    uint8_t b = RGB32_B(g_palette[PAL_PATTEXT]);
    r = (uint8_t)((r > 96) ? (r - 96) : 0);
    g = (uint8_t)((g > 96) ? (g - 96) : 0);
    b = (uint8_t)((b > 96) ? (b - 96) : 0);
    g_palette[PAL_LOOPPIN] = 0xFF000000 | RGB32(r, g, b);

    g_palette[PAL_CUSTOM] = 0xFF000000;

    // Fill remaining slots with black
    for (int i = 16; i < 256; i++) {
        if (i == PAL_TEXTMRK || i == PAL_BOXSLCT || i == PAL_LOOPPIN || i == PAL_CUSTOM)
            continue;
        g_palette[i] = 0xFF000000;
    }
}

uint32_t get_palette_color(uint8_t index)
{
    return g_palette[index & 0xFF];
}

void set_palette_color(uint8_t index, uint32_t color)
{
    g_palette[index] = color | 0xFF000000; // Ensure alpha is 255
}

void fill_rect(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0 || y < 0) return;
    
    for (int dy = 0; dy < h; dy++) {
        
        uint32_t *row = &framebuffer[(y + dy) * fb_width + x];
        for (int dx = 0; dx < w; dx++) {
            if (x + dx >= fb_width) break;
            row[dx] = color;
        }
    }
}

void h_line(uint32_t *framebuffer, int fb_width, int x, int y, int w, uint32_t color)
{
    if (x < 0 || y < 0) return;
    
    uint32_t *row = &framebuffer[y * fb_width + x];
    for (int i = 0; i < w && (x + i) < fb_width; i++) {
        row[i] = color;
    }
}

void v_line(uint32_t *framebuffer, int fb_width, int x, int y, int h, uint32_t color)
{
    if (x < 0 || y < 0 || x >= fb_width) return;
    
    for (int i = 0; i < h; i++) {
        framebuffer[(y + i) * fb_width + x] = color;
    }
}

void draw_framework(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, int type)
{
    if (w < 2 || h < 2) return;
    
    if (type == FRAMEWORK_TYPE1) {
        // Raised framework (like FT2's main panels)
        // Top-left highlight
        h_line(framebuffer, fb_width, x, y, w - 1, get_palette_color(PAL_DSKTOP1));
        v_line(framebuffer, fb_width, x, y + 1, h - 2, get_palette_color(PAL_DSKTOP1));
        
        // Bottom-right shadow  
        h_line(framebuffer, fb_width, x + 1, y + h - 1, w - 1, get_palette_color(PAL_DSKTOP2));
        v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_DSKTOP2));
        
        // Fill interior
        fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_DESKTOP));
    } else {
        // Sunken framework (like FT2's text boxes)
        // Top-left shadow
        h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_DSKTOP2));
        v_line(framebuffer, fb_width, x, y + 1, h - 1, get_palette_color(PAL_DSKTOP2));
        
        // Bottom-right highlight
        h_line(framebuffer, fb_width, x + 1, y + h - 1, w - 1, get_palette_color(PAL_DSKTOP1));
        v_line(framebuffer, fb_width, x + w - 1, y + 1, h - 2, get_palette_color(PAL_DSKTOP1));
        
        // Clear interior (black background)
        fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BCKGRND));
    }
}

void blit_fast(uint32_t *framebuffer, int fb_width, int x, int y, const uint8_t *src, int w, int h)
{
    if (!framebuffer || !src || x < 0 || y < 0) return;

    for (int dy = 0; dy < h; dy++) {
        uint32_t *dst = &framebuffer[(y + dy) * fb_width + x];
        const uint8_t *src_row = &src[dy * w];
        for (int dx = 0; dx < w; dx++) {
            dst[dx] = get_palette_color(src_row[dx]);
        }
    }
}

void blit_transparent(uint32_t *framebuffer, int fb_width, int x, int y, const uint8_t *src, int w, int h)
{
    if (!framebuffer || !src || x < 0 || y < 0) return;

    for (int dy = 0; dy < h; dy++) {
        uint32_t *dst = &framebuffer[(y + dy) * fb_width + x];
        const uint8_t *src_row = &src[dy * w];
        for (int dx = 0; dx < w; dx++) {
            uint8_t pix = src_row[dx];
            if (pix != PAL_TRANSPR)
                dst[dx] = get_palette_color(pix);
        }
    }
}
