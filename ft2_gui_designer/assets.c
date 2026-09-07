#include "assets.h"
#include "palette.h"
#include "shared/ft2_ui_assets.h"
#include "shared/ft2_ui_bitmap.h"
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "third_party/stb_image.h"

#ifdef _WIN32
static int designer_strcasecmp(const char *a, const char *b)
{
    unsigned char ca, cb;

    if (a == NULL || b == NULL)
        return (a == b) ? 0 : (a ? 1 : -1);

    while (*a != '\0' && *b != '\0')
    {
        ca = (unsigned char)tolower((unsigned char)*a++);
        cb = (unsigned char)tolower((unsigned char)*b++);
        if (ca != cb)
            return (int)ca - (int)cb;
    }

    return (int)(unsigned char)tolower((unsigned char)*a) -
           (int)(unsigned char)tolower((unsigned char)*b);
}
#define strcasecmp designer_strcasecmp
#else
#include <strings.h>
#endif

enum
{
    COMP_RGB = 0,
    COMP_RLE8 = 1,
    COMP_RLE4 = 2
};

#pragma pack(push, 1)
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
#pragma pack(pop)

static designer_assets_t g_assets;
static designer_bitmap_t g_bitmaps[DESIGNER_MAX_BITMAPS];
static int g_bitmap_count = 0;
static int g_next_custom_id = FT2_UI_BITMAP_COUNT;

static const uint32_t bmpCustomPalette[] =
{
    0x000000, 0x5397FF, 0x000067, 0x4BFFFF, 0xAB7787,
    0xFFFFFF, 0x7F7F7F, 0xABCDEF, 0x733747, 0xF7CBDB,
    0x434343, 0xD3D3D3, 0xFFFF00, 0xC0FFEE, 0xC0FFEE,
    0xC0FFEE, 0xFF0000
};

static int8_t getFT2PalNrFromPixel(uint32_t pixel32)
{
    for (int32_t i = 0; i < (int32_t)(sizeof(bmpCustomPalette) / sizeof(bmpCustomPalette[0])); i++)
    {
        if (pixel32 == bmpCustomPalette[i])
            return (int8_t)i;
    }

    return PAL_TRANSPR;
}

static uint8_t map_rgb_to_palette_index(uint8_t r, uint8_t g, uint8_t b)
{
    int best_idx = 0;
    int best_dist = 0x7FFFFFFF;

    for (int i = 0; i < 16; i++) {
        uint32_t color = g_palette[i];
        int dr = (int)r - (int)RGB32_R(color);
        int dg = (int)g - (int)RGB32_G(color);
        int db = (int)b - (int)RGB32_B(color);
        int dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }

    return (uint8_t)best_idx;
}

static uint8_t *loadBMPTo1Bit(const uint8_t *src)
{
    uint8_t palIdx, color, color2, *tmp8;
    int32_t len, byte, i;
    uint32_t pal[16];

    const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
    const uint8_t *pData = &src[hdr->bfOffBits];
    const int32_t colorsInBitmap = 1 << hdr->biBitCount;

    if (hdr->biCompression != COMP_RLE4 || hdr->biClrUsed > 16 || colorsInBitmap > 16)
        return NULL;

    uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof (uint8_t));
    if (outData == NULL)
        return NULL;

    const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
    memcpy(pal, &src[0x36], palEntries * sizeof (uint32_t));

    // pre-fill image with first palette color
    color = !!pal[0];
    for (i = 0; i < hdr->biWidth * hdr->biHeight; i++)
        outData[i] = color;

    const int32_t lineEnd = hdr->biWidth;
    const uint8_t *src8 = pData;
    uint8_t *dst8 = outData;
    int32_t x = 0;
    int32_t y = hdr->biHeight - 1;

    while (true)
    {
        byte = *src8++;
        if (byte == 0) // escape control
        {
            byte = *src8++;
            if (byte == 0) // end of line
            {
                x = 0;
                y--;
                if (y < 0)
                    break;
            }
            else if (byte == 1) // end of bitmap
            {
                break;
            }
            else if (byte == 2) // add to x/y position
            {
                x += *src8++;
                y -= *src8++;
                if (y < 0)
                    break;
            }
            else // absolute bytes
            {
                len = byte >> 1;
                if (y < 0 || y >= hdr->biHeight)
                    break;
                tmp8 = &dst8[y * hdr->biWidth];
                for (i = 0; i < len; i++)
                {
                    palIdx = *src8++;

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
            palIdx = *src8++;

            color = !!pal[palIdx >> 4];
            color2 = !!pal[palIdx & 0x0F];

            len = byte >> 1;
            if (y < 0 || y >= hdr->biHeight)
                break;
            tmp8 = &dst8[y * hdr->biWidth];
            for (i = 0; i < len; i++)
            {
                if (x < lineEnd)
                    tmp8[x] = color;
                x++;
                if (x < lineEnd)
                    tmp8[x] = color2;
                x++;
            }
        }
    }

    return outData;
}

static uint8_t *loadBMPTo4BitPal(const uint8_t *src)
{
    uint8_t palIdx, *tmp8, pal1, pal2;
    int32_t len, byte, i;
    uint32_t pal[16];

    const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
    const uint8_t *pData = &src[hdr->bfOffBits];
    const int32_t colorsInBitmap = 1 << hdr->biBitCount;

    if (hdr->biCompression != COMP_RLE4 || hdr->biClrUsed > 16 || colorsInBitmap > 16)
        return NULL;

    uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof (uint8_t));
    if (outData == NULL)
        return NULL;

    const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
    memcpy(pal, &src[0x36], palEntries * sizeof (uint32_t));

    // pre-fill image with first palette color
    palIdx = getFT2PalNrFromPixel(pal[0]);
    for (i = 0; i < hdr->biWidth * hdr->biHeight; i++)
        outData[i] = palIdx;

    const int32_t lineEnd = hdr->biWidth;
    const uint8_t *src8 = pData;
    uint8_t *dst8 = outData;
    int32_t x = 0;
    int32_t y = hdr->biHeight - 1;

    while (true)
    {
        byte = *src8++;
        if (byte == 0) // escape control
        {
            byte = *src8++;
            if (byte == 0) // end of line
            {
                x = 0;
                y--;
                if (y < 0)
                    break;
            }
            else if (byte == 1) // end of bitmap
            {
                break;
            }
            else if (byte == 2) // add to x/y position
            {
                x += *src8++;
                y -= *src8++;
                if (y < 0)
                    break;
            }
            else // absolute bytes
            {
                len = byte >> 1;
                if (y < 0 || y >= hdr->biHeight)
                    break;
                tmp8 = &dst8[y * hdr->biWidth];
                for (i = 0; i < len; i++)
                {
                    palIdx = *src8++;

                    if (x < lineEnd)
                        tmp8[x] = getFT2PalNrFromPixel(pal[palIdx >> 4]);
                    x++;
                    if (x < lineEnd)
                        tmp8[x] = getFT2PalNrFromPixel(pal[palIdx & 0xF]);
                    x++;
                }

                if (((byte + 1) >> 1) & 1)
                    src8++;
            }
        }
        else
        {
            palIdx = *src8++;

            pal1 = getFT2PalNrFromPixel(pal[palIdx >> 4]);
            pal2 = getFT2PalNrFromPixel(pal[palIdx & 0x0F]);

            len = byte >> 1;
            if (y < 0 || y >= hdr->biHeight)
                break;
            tmp8 = &dst8[y * hdr->biWidth];
            for (i = 0; i < len; i++)
            {
                if (x < lineEnd)
                    tmp8[x] = pal1;
                x++;
                if (x < lineEnd)
                    tmp8[x] = pal2;
                x++;
            }
        }
    }

    return outData;
}

static uint8_t *loadBMPTo4BitPalMapped(const uint8_t *src, int *out_w, int *out_h, bool use_custom_palette)
{
    const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
    const uint8_t *pData = &src[hdr->bfOffBits];
    const int32_t colorsInBitmap = 1 << hdr->biBitCount;

    if (hdr->biCompression != COMP_RLE4 || hdr->biClrUsed > 16 || colorsInBitmap > 16)
        return NULL;

    uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof (uint8_t));
    if (outData == NULL)
        return NULL;

    uint32_t pal[16];
    uint8_t pal_map[16];
    const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
    memcpy(pal, &src[0x36], palEntries * sizeof (uint32_t));
    for (int32_t i = 0; i < palEntries; i++) {
        uint32_t color = pal[i] & 0x00FFFFFF;
        if (use_custom_palette)
            pal_map[i] = (uint8_t)getFT2PalNrFromPixel(color);
        else
            pal_map[i] = map_rgb_to_palette_index(RGB32_R(color), RGB32_G(color), RGB32_B(color));
    }

    uint8_t palIdx = pal_map[0];
    for (int32_t i = 0; i < hdr->biWidth * hdr->biHeight; i++)
        outData[i] = palIdx;

    const int32_t lineEnd = hdr->biWidth;
    const uint8_t *src8 = pData;
    uint8_t *dst8 = outData;
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
                    palIdx = *src8++;

                    if (x < lineEnd)
                        tmp8[x] = pal_map[palIdx >> 4];
                    x++;
                    if (x < lineEnd)
                        tmp8[x] = pal_map[palIdx & 0xF];
                    x++;
                }

                if (((byte + 1) >> 1) & 1)
                    src8++;
            }
        }
        else
        {
            palIdx = *src8++;

            uint8_t pal1 = pal_map[palIdx >> 4];
            uint8_t pal2 = pal_map[palIdx & 0x0F];

            int32_t len = byte >> 1;
            if (y < 0 || y >= hdr->biHeight)
                break;
            uint8_t *tmp8 = &dst8[y * hdr->biWidth];
            for (int32_t i = 0; i < len; i++)
            {
                if (x < lineEnd)
                    tmp8[x] = pal1;
                x++;
                if (x < lineEnd)
                    tmp8[x] = pal2;
                x++;
            }
        }
    }

    if (out_w) *out_w = hdr->biWidth;
    if (out_h) *out_h = hdr->biHeight;

    return outData;
}

static uint8_t *loadBMPTo8BitPalMapped(const uint8_t *src, int *out_w, int *out_h, bool use_custom_palette)
{
    const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
    const uint8_t *pData = &src[hdr->bfOffBits];
    const int32_t colorsInBitmap = 1 << hdr->biBitCount;

    if (hdr->biCompression != COMP_RLE8 || hdr->biClrUsed > 256 || colorsInBitmap > 256)
        return NULL;

    uint8_t *outData = (uint8_t *)malloc(hdr->biWidth * hdr->biHeight * sizeof (uint8_t));
    if (outData == NULL)
        return NULL;

    uint32_t pal[256];
    uint8_t pal_map[256];
    const int32_t palEntries = (hdr->biClrUsed == 0) ? colorsInBitmap : hdr->biClrUsed;
    memcpy(pal, &src[0x36], palEntries * sizeof (uint32_t));
    for (int32_t i = 0; i < palEntries; i++) {
        uint32_t color = pal[i] & 0x00FFFFFF;
        if (use_custom_palette)
            pal_map[i] = (uint8_t)getFT2PalNrFromPixel(color);
        else
            pal_map[i] = map_rgb_to_palette_index(RGB32_R(color), RGB32_G(color), RGB32_B(color));
    }

    uint8_t palIdx = pal_map[0];
    for (int32_t i = 0; i < hdr->biWidth * hdr->biHeight; i++)
        outData[i] = palIdx;

    const uint8_t *src8 = pData;
    uint8_t *dst8 = outData;
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
                if (y < 0 || y >= hdr->biHeight)
                    break;
                uint8_t *tmp8 = &dst8[y * hdr->biWidth];
                for (int32_t i = 0; i < byte; i++)
                {
                    palIdx = *src8++;
                    if (x < hdr->biWidth)
                        tmp8[x] = pal_map[palIdx];
                    x++;
                }

                if (byte & 1)
                    src8++;
            }
        }
        else
        {
            palIdx = *src8++;
            uint8_t pal1 = pal_map[palIdx];

            if (y < 0 || y >= hdr->biHeight)
                break;
            uint8_t *tmp8 = &dst8[y * hdr->biWidth];
            for (int32_t i = 0; i < byte; i++) {
                if (x < hdr->biWidth)
                    tmp8[x] = pal1;
                x++;
            }
        }
    }

    if (out_w) *out_w = hdr->biWidth;
    if (out_h) *out_h = hdr->biHeight;

    return outData;
}

static uint8_t *loadBMPToRGBPalMapped(const uint8_t *src, int *out_w, int *out_h)
{
    const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
    if (hdr->biCompression != COMP_RGB)
        return NULL;

    int32_t width = hdr->biWidth;
    int32_t height = hdr->biHeight;
    bool top_down = false;

    if (height < 0) {
        height = -height;
        top_down = true;
    }

    int bytes_per_pixel = hdr->biBitCount / 8;
    if (bytes_per_pixel != 3 && bytes_per_pixel != 4)
        return NULL;

    const uint8_t *pData = &src[hdr->bfOffBits];
    int row_stride = (width * bytes_per_pixel + 3) & ~3;

    uint8_t *outData = (uint8_t *)malloc(width * height);
    if (!outData)
        return NULL;

    for (int y = 0; y < height; y++) {
        int src_y = top_down ? y : (height - 1 - y);
        const uint8_t *row = pData + src_y * row_stride;
        for (int x = 0; x < width; x++) {
            const uint8_t *pix = &row[x * bytes_per_pixel];
            uint8_t b = pix[0];
            uint8_t g = pix[1];
            uint8_t r = pix[2];
            uint8_t a = (bytes_per_pixel == 4) ? pix[3] : 255;

            if (a < 128)
                outData[y * width + x] = PAL_TRANSPR;
            else
                outData[y * width + x] = map_rgb_to_palette_index(r, g, b);
        }
    }

    if (out_w) *out_w = width;
    if (out_h) *out_h = height;

    return outData;
}

static uint32_t *loadBMPToRGB32(const uint8_t *src, size_t src_len, int *out_w, int *out_h)
{
    int32_t width = 0;
    int32_t height = 0;
    uint32_t *pixels = NULL;
    if (!ft2_ui_bitmap_decode_bmp_argb32(src, src_len, &pixels, &width, &height))
        return NULL;
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    return pixels;
}

static uint8_t *decode_bmp_to_pal(const uint8_t *src, int *out_w, int *out_h, bool use_custom_palette)
{
    if (!src)
        return NULL;

    const bmpHeader_t *hdr = (const bmpHeader_t *)&src[2];
    if (hdr->biCompression == COMP_RLE4)
        return loadBMPTo4BitPalMapped(src, out_w, out_h, use_custom_palette);
    if (hdr->biCompression == COMP_RLE8)
        return loadBMPTo8BitPalMapped(src, out_w, out_h, use_custom_palette);
    if (hdr->biCompression == COMP_RGB)
        return loadBMPToRGBPalMapped(src, out_w, out_h);

    return NULL;
}

static bool add_bitmap_entry(int id, const char *name, uint8_t *pixels, uint32_t *pixels32, int w, int h, ft2_ui_bmp_format_t format)
{
    if (g_bitmap_count >= DESIGNER_MAX_BITMAPS || (!pixels && !pixels32))
        return false;

    designer_bitmap_t *bmp = &g_bitmaps[g_bitmap_count++];
    bmp->id = id;
    bmp->w = (uint16_t)w;
    bmp->h = (uint16_t)h;
    bmp->pixels = pixels;
    bmp->pixels32 = pixels32;
    bmp->format = format;
    bmp->transparent_index = 0;
    if (name)
        snprintf(bmp->name, sizeof(bmp->name), "%s", name);
    else
        bmp->name[0] = '\0';

    return true;
}

static const ft2_ui_bitmap_asset_t *find_bitmap_asset(ft2_ui_bitmap_id_t id)
{
    for (uint16_t i = 0; i < ft2_ui_assets.bitmap_count; i++) {
        const ft2_ui_bitmap_asset_t *asset = &ft2_ui_assets.bitmaps[i];
        if (asset->id == id)
            return asset;
    }
    return NULL;
}

static void load_builtin_bitmaps(void)
{
    for (uint16_t i = 0; i < ft2_ui_assets.bitmap_count; i++) {
        const ft2_ui_bitmap_asset_t *asset = &ft2_ui_assets.bitmaps[i];
        if (!asset || !asset->bmp || asset->bmp_len == 0)
            continue;

        int w = 0;
        int h = 0;
        bool use_custom_palette = (asset->fmt != FT2_UI_BMP_FMT_RGB);
        uint8_t *pixels = decode_bmp_to_pal(asset->bmp, &w, &h, use_custom_palette);
        uint32_t *pixels32 = NULL;
        if (asset->fmt == FT2_UI_BMP_FMT_RGB)
            pixels32 = loadBMPToRGB32(asset->bmp, asset->bmp_len, &w, &h);

        if (!pixels && !pixels32)
            continue;

        char name[DESIGNER_BITMAP_NAME_MAX];
        snprintf(name, sizeof(name), "builtin_%d", asset->id);
        if (!add_bitmap_entry(asset->id, name, pixels, pixels32, w, h, asset->fmt)) {
            free(pixels);
            free(pixels32);
        }
    }
}

static uint8_t *load_file_bytes(const char *path, size_t *out_len)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long len = ftell(file);
    if (len <= 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    uint8_t *buffer = (uint8_t *)malloc((size_t)len);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)len, file) != (size_t)len) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);

    if (out_len) *out_len = (size_t)len;
    return buffer;
}

static uint8_t *decode_rgba_to_pal(const uint8_t *rgba, int w, int h)
{
    if (!rgba || w <= 0 || h <= 0)
        return NULL;

    uint8_t *outData = (uint8_t *)malloc((size_t)w * (size_t)h);
    if (!outData)
        return NULL;

    for (int i = 0; i < w * h; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];

        if (a < 128)
            outData[i] = PAL_TRANSPR;
        else
            outData[i] = map_rgb_to_palette_index(r, g, b);
    }

    return outData;
}

static uint32_t *decode_rgba_to_rgb32(const uint8_t *rgba, int w, int h)
{
    if (!rgba || w <= 0 || h <= 0)
        return NULL;

    uint32_t *outData = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!outData)
        return NULL;

    for (int i = 0; i < w * h; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];
        outData[i] = ((uint32_t)a << 24) | RGB32(r, g, b);
    }

    return outData;
}

bool init_designer_assets(void)
{
    memset(&g_assets, 0, sizeof (g_assets));
    memset(g_bitmaps, 0, sizeof (g_bitmaps));
    g_bitmap_count = 0;
    g_next_custom_id = FT2_UI_BITMAP_COUNT;

    g_assets.bitmaps = g_bitmaps;
    g_assets.bitmap_count = 0;

    const ft2_ui_bitmap_asset_t *button_asset = find_bitmap_asset(FT2_UI_BITMAP_BUTTON_GFX);
    const ft2_ui_bitmap_asset_t *checkbox_asset = find_bitmap_asset(FT2_UI_BITMAP_CHECKBOX_GFX);
    const ft2_ui_bitmap_asset_t *radio_asset = find_bitmap_asset(FT2_UI_BITMAP_RADIOBUTTON_GFX);

    if (button_asset)
        g_assets.button_gfx = loadBMPTo1Bit(button_asset->bmp);
    if (checkbox_asset)
        g_assets.checkbox_gfx = loadBMPTo4BitPal(checkbox_asset->bmp);
    if (radio_asset)
        g_assets.radiobutton_gfx = loadBMPTo4BitPal(radio_asset->bmp);

    load_builtin_bitmaps();
    g_assets.bitmap_count = g_bitmap_count;

    return g_assets.button_gfx != NULL &&
           g_assets.checkbox_gfx != NULL &&
           g_assets.radiobutton_gfx != NULL;
}

void free_designer_assets(void)
{
    free(g_assets.button_gfx);
    free(g_assets.checkbox_gfx);
    free(g_assets.radiobutton_gfx);
    for (int i = 0; i < g_bitmap_count; i++) {
        free(g_bitmaps[i].pixels);
        free(g_bitmaps[i].pixels32);
        g_bitmaps[i].pixels = NULL;
        g_bitmaps[i].pixels32 = NULL;
    }
    memset(&g_assets, 0, sizeof (g_assets));
    memset(g_bitmaps, 0, sizeof (g_bitmaps));
    g_bitmap_count = 0;
}

const designer_assets_t *designer_assets(void)
{
    return &g_assets;
}

int designer_bitmap_count(void)
{
    return g_bitmap_count;
}

int designer_bitmap_id_at(int index)
{
    if (index < 0 || index >= g_bitmap_count)
        return -1;
    return g_bitmaps[index].id;
}

int designer_bitmap_index_for_id(int id)
{
    for (int i = 0; i < g_bitmap_count; i++) {
        if (g_bitmaps[i].id == id)
            return i;
    }
    return -1;
}

const designer_bitmap_t *designer_bitmap_at(int index)
{
    if (index < 0 || index >= g_bitmap_count)
        return NULL;
    return &g_bitmaps[index];
}

const designer_bitmap_t *designer_bitmap_by_id(int id)
{
    int index = designer_bitmap_index_for_id(id);
    if (index < 0)
        return NULL;
    return &g_bitmaps[index];
}

bool designer_import_bitmap(const char *path, int *out_id)
{
    if (!path || path[0] == '\0')
        return false;

    size_t data_len = 0;
    uint8_t *data = load_file_bytes(path, &data_len);
    if (!data)
        return false;

    const char *format_hint = strrchr(path, '.');
    const bool imported = designer_import_bitmap_data(data, data_len, format_hint, path, out_id);
    free(data);
    return imported;
}

bool designer_import_bitmap_data(const uint8_t *data, size_t data_len, const char *format_hint,
                                 const char *name, int *out_id)
{
    if (!data || data_len == 0 || data_len > INT_MAX)
        return false;

    uint8_t *pixels = NULL;
    uint32_t *pixels32 = NULL;
    int w = 0;
    int h = 0;
    ft2_ui_bmp_format_t format = FT2_UI_BMP_FMT_RLE4;

    const bool is_bmp = (format_hint && strcasecmp(format_hint, ".bmp") == 0) ||
                        (data_len >= 2 && data[0] == 'B' && data[1] == 'M');
    if (is_bmp) {
        pixels32 = loadBMPToRGB32(data, data_len, &w, &h);
        if (pixels32)
            format = FT2_UI_BMP_FMT_RGB;
        else if (data_len >= 54 && data[0] == 'B' && data[1] == 'M')
            pixels = decode_bmp_to_pal(data, &w, &h, false);
    } else {
        int comp = 0;
        stbi_uc *rgba = stbi_load_from_memory(data, (int)data_len, &w, &h, &comp, 4);
        if (rgba) {
            pixels = decode_rgba_to_pal(rgba, w, h);
            pixels32 = decode_rgba_to_rgb32(rgba, w, h);
            if (pixels32)
                format = FT2_UI_BMP_FMT_RGB;
            stbi_image_free(rgba);
        }
    }

    if (!pixels && !pixels32)
        return false;

    int id = g_next_custom_id++;
    if (!add_bitmap_entry(id, name, pixels, pixels32, w, h, format)) {
        free(pixels);
        free(pixels32);
        return false;
    }

    g_assets.bitmap_count = g_bitmap_count;

    if (out_id)
        *out_id = id;

    return true;
}

int designer_bitmap_transparent_index(int id)
{
    const designer_bitmap_t *bmp = designer_bitmap_by_id(id);
    if (!bmp)
        return 0;
    return (int)bmp->transparent_index;
}

bool designer_set_bitmap_transparent_index(int id, int index)
{
    designer_bitmap_t *bmp = NULL;
    int idx = designer_bitmap_index_for_id(id);
    if (idx >= 0)
        bmp = &g_bitmaps[idx];
    if (!bmp)
        return false;
    if (index < 0) index = 0;
    if (index > 15) index = 15;
    bmp->transparent_index = (uint8_t)index;
    return true;
}

bool designer_bitmap_has_truecolor(int id)
{
    const designer_bitmap_t *bmp = designer_bitmap_by_id(id);
    return bmp && bmp->pixels32 != NULL;
}

uint32_t designer_custom_palette_color(int index)
{
    if (index < 0 || index >= 16)
        return 0;
    return bmpCustomPalette[index] & 0x00FFFFFF;
}
