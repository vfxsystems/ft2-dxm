#include "font.h"
#include <stdlib.h>
#include <string.h>

// FT2 font character widths (from FT2 source)
static const uint8_t font1Widths[128] = {
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8, // 0-31
    3,2,5,6,6,9,7,2,4,4,5,6,2,4,2,6,6,4,6,6,6,6,6,6,6,6,2,2,5,5,5,6, // 32-63 
    9,7,6,6,7,5,5,7,7,3,5,6,5,8,7,7,6,7,6,6,6,7,7,9,6,6,6,3,6,3,5,6, // 64-95
    3,6,6,5,6,6,4,6,6,2,3,5,2,8,6,6,6,6,4,5,4,6,6,8,5,6,5,4,2,4,6,8  // 96-127
};


enum
{
    COMP_RLE4 = 2
};

typedef struct bmpHeader_t
{
    uint32_t bfSizebfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    int32_t biClrUsed;
    int32_t biClrImportant;
} bmpHeader_t;

// Proper 4-bit BMP decoding for FT2 font
static const ft2_ui_font_asset_t *get_font_asset(const font_system_t *fs, ft2_ui_font_id_t font_id)
{
    if (!fs || !fs->font_assets)
        return NULL;
    if (font_id < 0 || font_id >= FT2_UI_FONT_COUNT)
        return NULL;
    return &fs->font_assets[font_id];
}

static uint8_t *decompress_ft2_font(const ft2_ui_font_asset_t *asset, int *out_bmp_h) {
    if (out_bmp_h)
        *out_bmp_h = 0;
    if (!asset || !asset->bmp || asset->bmp_len == 0)
        return NULL;

    const bmpHeader_t *hdr = (const bmpHeader_t *)&asset->bmp[2];
    const uint8_t *pData = &asset->bmp[hdr->bfOffBits];
    const int32_t colorsInBitmap = 1 << hdr->biBitCount;

    if (hdr->biCompression != COMP_RLE4 || hdr->biClrUsed > 16 || colorsInBitmap > 16)
        return NULL;

    if (hdr->biWidth != asset->bmp_w || asset->char_h == 0 || hdr->biHeight <= 0)
        return NULL;

    if ((hdr->biHeight % asset->char_h) != 0)
        return NULL;

    if (out_bmp_h)
        *out_bmp_h = hdr->biHeight;

    uint8_t *fontData = (uint8_t *)calloc(hdr->biWidth * hdr->biHeight, sizeof(uint8_t));
    if (!fontData) return NULL;

    uint32_t pal[16];
    const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
    memcpy(pal, &asset->bmp[0x36], palEntries * sizeof (uint32_t));

    uint8_t color = !!pal[0];
    for (int32_t i = 0; i < hdr->biWidth * hdr->biHeight; i++)
        fontData[i] = color;

    const int32_t lineEnd = hdr->biWidth;
    const uint8_t *src8 = pData;
    uint8_t *dst8 = fontData;
    int32_t x = 0;
    int32_t y = hdr->biHeight - 1;

    while (true)
    {
        int32_t byte = *src8++;
        if (byte == 0)
        {
            byte = *src8++;
            if (byte == 0)
            {
                x = 0;
                y--;
                if (y < 0)
                    break;
            }
            else if (byte == 1)
            {
                break;
            }
            else if (byte == 2)
            {
                x += *src8++;
                y -= *src8++;
                if (y < 0)
                    break;
            }
            else
            {
                int32_t len = byte >> 1;
                if (y < 0 || y >= hdr->biHeight)
                    break;
                uint8_t *tmp8 = &dst8[y * hdr->biWidth];
                for (int32_t i = 0; i < len; i++)
                {
                    uint8_t palIdx = *src8++;
                    if (x < lineEnd)
                        tmp8[x] = !!pal[palIdx >> 4];
                    x++;
                    if (x < lineEnd)
                        tmp8[x] = !!pal[palIdx & 0xF];
                    x++;
                }

                if (((byte + 1) >> 1) & 1)
                    src8++;
            }
        }
        else
        {
            uint8_t palIdx = *src8++;
            uint8_t color1 = !!pal[palIdx >> 4];
            uint8_t color2 = !!pal[palIdx & 0x0F];

            int32_t len = byte >> 1;
            if (y < 0 || y >= hdr->biHeight)
                break;
            uint8_t *tmp8 = &dst8[y * hdr->biWidth];
            for (int32_t i = 0; i < len; i++)
            {
                if (x < lineEnd)
                    tmp8[x] = color1;
                x++;
                if (x < lineEnd)
                    tmp8[x] = color2;
                x++;
            }
        }
    }

    return fontData;
}


bool init_font_system(font_system_t *fs, uint32_t *framebuffer, int width, int height) {
    if (!fs || !framebuffer) return false;

    memset(fs, 0, sizeof(*fs));
    fs->framebuffer = framebuffer;
    fs->screen_width = width;
    fs->screen_height = height;

    fs->font_assets = ft2_ui_assets.fonts;

    for (int i = 0; i < FT2_UI_FONT_COUNT; i++) {
        fs->font_bmp_h[i] = 0;
        fs->font_chars_per_row[i] = 0;
        fs->font_data[i] = NULL;

        const ft2_ui_font_asset_t *asset = get_font_asset(fs, (ft2_ui_font_id_t)i);
        if (!asset || !asset->bmp || asset->bmp_len == 0)
            continue;

        int bmp_h = 0;
        fs->font_data[i] = decompress_ft2_font(asset, &bmp_h);
        if (!fs->font_data[i])
            continue;

        if (bmp_h > 0)
            fs->font_bmp_h[i] = (uint16_t)bmp_h;
        if (asset->char_w > 0 && asset->bmp_w > 0)
            fs->font_chars_per_row[i] = (uint16_t)(asset->bmp_w / asset->char_w);
    }

    if (!fs->font_data[FT2_UI_FONT_1]) {
        return false;
    }

    return true;
}

void cleanup_font_system(font_system_t *fs) {
    if (!fs)
        return;

    for (int i = 0; i < FT2_UI_FONT_COUNT; i++) {
        free(fs->font_data[i]);
        fs->font_data[i] = NULL;
        fs->font_bmp_h[i] = 0;
        fs->font_chars_per_row[i] = 0;
    }
}

void font_draw_char(font_system_t *fs, int x, int y, char ch, uint32_t color) {
    font_draw_char_with_font(fs, x, y, ch, color, FT2_UI_FONT_1);
}

void font_draw_char_with_font(font_system_t *fs, int x, int y, char ch, uint32_t color, ft2_ui_font_id_t font_id) {
    if (!fs || !fs->framebuffer) return;
    if (x >= fs->screen_width || y >= fs->screen_height) return;

    if (font_id < 0 || font_id >= FT2_UI_FONT_COUNT || !fs->font_data[font_id])
        font_id = FT2_UI_FONT_1;

    if (font_id < 0 || font_id >= FT2_UI_FONT_COUNT || !fs->font_data[font_id])
        return;

    const ft2_ui_font_asset_t *asset = get_font_asset(fs, font_id);
    if (!asset)
        return;

    const int char_w = asset->char_w;
    const int char_h = asset->char_h;
    const int bmp_w = asset->bmp_w;
    if (char_w <= 0 || char_h <= 0 || bmp_w <= 0)
        return;

    int bmp_h = fs->font_bmp_h[font_id] > 0 ? fs->font_bmp_h[font_id] : char_h;
    int chars_per_row = fs->font_chars_per_row[font_id];
    if (chars_per_row <= 0)
        chars_per_row = bmp_w / char_w;

    int rows = bmp_h / char_h;
    int max_chars = chars_per_row * rows;
    if (max_chars <= 0)
        return;

    // Handle extended characters
    if ((uint8_t)ch > 127 + 31) ch = ' ';
    ch &= 0x7F; // Nordic glyphs

    if (ch == ' ') return; // Don't draw spaces

    uint8_t glyph = (uint8_t)ch;
    if (max_chars <= 64) {
        if (ch >= '0' && ch <= '9')
            glyph = (uint8_t)(ch - '0');
        else if (ch >= 'a' && ch <= 'z')
            glyph = (uint8_t)(10 + (ch - 'a'));
        else if (ch >= 'A' && ch <= 'Z')
            glyph = (uint8_t)(10 + (ch - 'A'));
        else
            glyph = max_chars;

        if (glyph >= max_chars)
            return;
    } else if (glyph >= max_chars) {
        if (glyph >= 'a' && glyph <= 'z')
            glyph = (uint8_t)(glyph - 32);
        if (glyph >= max_chars)
            return;
    }
    const int row = glyph / chars_per_row;
    const int col = glyph % chars_per_row;

    // Get font bitmap for this character (like original FT2)
    const uint8_t *srcPtr = &fs->font_data[font_id][(row * char_h * bmp_w) + (col * char_w)];
    uint32_t *dstPtr = &fs->framebuffer[(y * fs->screen_width) + x];

    // Render character bitmap
    for (int r = 0; r < char_h; r++) {
        if (y + r >= fs->screen_height) break;

        for (int c = 0; c < char_w; c++) {
            if (x + c >= fs->screen_width) break;

            if (srcPtr[c] != 0) {
                dstPtr[c] = color;
            }
        }

        srcPtr += bmp_w;  // Move to next row (like FT2)
        dstPtr += fs->screen_width;
    }
}


void font_draw_text(font_system_t *fs, int x, int y, const char *text, uint32_t color) {
    font_draw_text_with_font(fs, x, y, text, color, FT2_UI_FONT_1);
}

void font_draw_text_with_font(font_system_t *fs, int x, int y, const char *text, uint32_t color, ft2_ui_font_id_t font_id) {
    if (!fs || !text) return;

    int currX = x;
    while (*text) {
        char ch = *text++;
        if (ch == '\0') break;

        if (currX < fs->screen_width) {
            font_draw_char_with_font(fs, currX, y, ch, color, font_id);
        }
        currX += font_get_char_width_with_font(font_id, ch);
    }
}

int font_get_char_width(char ch) {
    return font_get_char_width_with_font(FT2_UI_FONT_1, ch);
}

int font_get_text_width(const char *text) {
    return font_get_text_width_with_font(FT2_UI_FONT_1, text);
}

int font_get_char_width_with_font(ft2_ui_font_id_t font_id, char ch)
{
    if (font_id < 0 || font_id >= FT2_UI_FONT_COUNT)
        return font1Widths[ch & 0x7F];

    const ft2_ui_font_asset_t *asset = &ft2_ui_assets.fonts[font_id];
    if (asset->widths)
        return asset->widths[ch & 0x7F];
    if (asset->char_w > 0)
        return asset->char_w;

    return font1Widths[ch & 0x7F];
}

int font_get_text_width_with_font(ft2_ui_font_id_t font_id, const char *text) {
    if (!text) return 0;

    int width = 0;
    while (*text) {
        width += font_get_char_width_with_font(font_id, *text++);
    }

    // Remove trailing pixel spacer
    if (width > 0) width--;

    return width;
}

int font_get_char_height_with_font(ft2_ui_font_id_t font_id)
{
    if (font_id < 0 || font_id >= FT2_UI_FONT_COUNT)
        return FONT1_CHAR_H;

    const ft2_ui_font_asset_t *asset = &ft2_ui_assets.fonts[font_id];
    if (!asset || asset->char_h == 0)
        return FONT1_CHAR_H;

    return asset->char_h;
}
