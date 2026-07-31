#include "palette.h"
#include <stdlib.h>
#include <string.h>

// Global palette array
uint32_t g_palette[256];
static int g_framebuffer_height;
static designer_palette_theme_t g_palette_theme = DESIGNER_THEME_ARCTIC;

typedef struct designer_theme_desc_t
{
    const char *name;
    uint32_t pal[16];
} designer_theme_desc_t;

#define PALRGB(r, g, b) (0xFF000000u | RGB32((r), (g), (b)))
#define PAL6(r, g, b) PALRGB(P6_TO_P8(r), P6_TO_P8(g), P6_TO_P8(b))

static const designer_theme_desc_t theme_descs[DESIGNER_THEME_COUNT] = {
    {
        "FT2 Arctic",
        {
            PAL6(0, 0, 0),   PAL6(30, 38, 63), PAL6(0, 0, 17),  PAL6(63, 63, 63),
            PAL6(27, 36, 40),PAL6(63, 63, 63), PAL6(40, 40, 40),PAL6(0, 0, 0),
            PAL6(10, 13, 14),PAL6(49, 63, 63), PAL6(15, 15, 15),PAL6(63, 63, 63),
            PAL6(63, 63, 63),PAL6(63, 63, 63), PAL6(63, 63, 63),PAL6(63, 63, 63)
        }
    },
    {
        "OP-1 Inspired",
        {
            PALRGB(18, 18, 16),   PALRGB(60, 69, 67),   PALRGB(225, 96, 74),  PALRGB(251, 246, 231),
            PALRGB(222, 216, 195),PALRGB(34, 34, 31),   PALRGB(188, 184, 166),PALRGB(24, 24, 22),
            PALRGB(165, 158, 141),PALRGB(255, 252, 241),PALRGB(238, 183, 64), PALRGB(49, 49, 44),
            PALRGB(79, 154, 172), PALRGB(104, 181, 121),PALRGB(225, 96, 74),  PALRGB(251, 246, 231)
        }
    },
    {
        "Renoise Inspired",
        {
            PALRGB(12, 13, 12),   PALRGB(172, 216, 113),PALRGB(33, 37, 35),   PALRGB(230, 235, 220),
            PALRGB(42, 46, 43),   PALRGB(226, 235, 212),PALRGB(62, 68, 64),   PALRGB(10, 11, 10),
            PALRGB(24, 28, 26),   PALRGB(119, 179, 97), PALRGB(29, 31, 30),   PALRGB(92, 105, 97),
            PALRGB(222, 176, 72), PALRGB(88, 158, 211), PALRGB(201, 91, 82),  PALRGB(235, 235, 220)
        }
    },
    {
        "VS Code Inspired",
        {
            PALRGB(10, 10, 10),   PALRGB(156, 220, 254),PALRGB(24, 24, 24),   PALRGB(220, 220, 220),
            PALRGB(37, 37, 38),   PALRGB(225, 225, 225),PALRGB(51, 51, 51),   PALRGB(0, 0, 0),
            PALRGB(30, 30, 30),   PALRGB(86, 156, 214), PALRGB(45, 45, 48),   PALRGB(63, 63, 70),
            PALRGB(197, 134, 192),PALRGB(78, 201, 176), PALRGB(206, 145, 120),PALRGB(220, 220, 220)
        }
    }
};

void init_palette(void)
{
    designer_set_palette_theme(DESIGNER_THEME_ARCTIC);
}

void designer_set_framebuffer_height(int height)
{
    g_framebuffer_height = (height > 0) ? height : 0;
}

void designer_set_palette_theme(designer_palette_theme_t theme)
{
    if (theme < 0 || theme >= DESIGNER_THEME_COUNT)
        theme = DESIGNER_THEME_ARCTIC;

    g_palette_theme = theme;

    for (int i = 0; i < 16; i++)
        g_palette[i] = theme_descs[theme].pal[i];

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

void designer_cycle_palette_theme(void)
{
    designer_set_palette_theme((designer_palette_theme_t)(((int)g_palette_theme + 1) % DESIGNER_THEME_COUNT));
}

designer_palette_theme_t designer_get_palette_theme(void)
{
    return g_palette_theme;
}

const char *designer_palette_theme_name(designer_palette_theme_t theme)
{
    if (theme < 0 || theme >= DESIGNER_THEME_COUNT)
        theme = DESIGNER_THEME_ARCTIC;

    return theme_descs[theme].name;
}

uint32_t get_palette_color(uint8_t index)
{
    return g_palette[index & 0xFF];
}

void set_palette_color(uint8_t index, uint32_t color)
{
    g_palette[index] = color | 0xFF000000; // Ensure alpha is 255
}

static int iroundf(float v)
{
    return (int)((v < 0.0f) ? (v - 0.5f) : (v + 0.5f));
}

static bool clip_rect(int fb_width, int *x, int *y, int *w, int *h)
{
    if (!x || !y || !w || !h || fb_width <= 0 || *w <= 0 || *h <= 0)
        return false;

    if (*x < 0) {
        *w += *x;
        *x = 0;
    }
    if (*y < 0) {
        *h += *y;
        *y = 0;
    }
    if (*x >= fb_width)
        return false;
    if (*x + *w > fb_width)
        *w = fb_width - *x;

    if (g_framebuffer_height > 0) {
        if (*y >= g_framebuffer_height)
            return false;
        if (*y + *h > g_framebuffer_height)
            *h = g_framebuffer_height - *y;
    }

    return *w > 0 && *h > 0;
}

void fill_rect(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, uint32_t color)
{
    if (!framebuffer || !clip_rect(fb_width, &x, &y, &w, &h))
        return;
    
    for (int dy = 0; dy < h; dy++) {
        uint32_t *row = &framebuffer[(y + dy) * fb_width + x];
        for (int dx = 0; dx < w; dx++)
            row[dx] = color;
    }
}

void h_line(uint32_t *framebuffer, int fb_width, int x, int y, int w, uint32_t color)
{
    fill_rect(framebuffer, fb_width, x, y, w, 1, color);
}

void v_line(uint32_t *framebuffer, int fb_width, int x, int y, int h, uint32_t color)
{
    fill_rect(framebuffer, fb_width, x, y, 1, h, color);
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

void designer_vector_fill_rect(uint32_t *framebuffer, int fb_width, float x, float y, float w, float h, uint32_t color)
{
    fill_rect(framebuffer, fb_width, iroundf(x), iroundf(y), iroundf(w), iroundf(h), color);
}

void designer_vector_stroke_rect(uint32_t *framebuffer, int fb_width, float x, float y, float w, float h, float thickness, uint32_t color)
{
    int t = iroundf(thickness);
    if (t < 1) t = 1;

    fill_rect(framebuffer, fb_width, iroundf(x),             iroundf(y),             iroundf(w), t,          color);
    fill_rect(framebuffer, fb_width, iroundf(x),             iroundf(y + h) - t,     iroundf(w), t,          color);
    fill_rect(framebuffer, fb_width, iroundf(x),             iroundf(y),             t,          iroundf(h), color);
    fill_rect(framebuffer, fb_width, iroundf(x + w) - t,     iroundf(y),             t,          iroundf(h), color);
}

void designer_vector_line(uint32_t *framebuffer, int fb_width, float x1, float y1, float x2, float y2, float thickness, uint32_t color)
{
    int ix1 = iroundf(x1), iy1 = iroundf(y1);
    int ix2 = iroundf(x2), iy2 = iroundf(y2);
    int t = iroundf(thickness);
    if (t < 1) t = 1;

    int dx = abs(ix2 - ix1);
    int sx = (ix1 < ix2) ? 1 : -1;
    int dy = -abs(iy2 - iy1);
    int sy = (iy1 < iy2) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        fill_rect(framebuffer, fb_width, ix1 - (t / 2), iy1 - (t / 2), t, t, color);
        if (ix1 == ix2 && iy1 == iy2)
            break;

        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            ix1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            iy1 += sy;
        }
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
