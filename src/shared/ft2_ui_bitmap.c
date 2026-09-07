#include "shared/ft2_ui_bitmap.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum
{
    BMP_FILE_HEADER_SIZE = 14,
    BMP_INFO_HEADER_SIZE = 40,
    BMP_V4_HEADER_SIZE = 108,
    BMP_COMPRESSION_RGB = 0,
    BMP_COMPRESSION_BITFIELDS = 3,
    BMP_COMPRESSION_ALPHA_BITFIELDS = 6
};

static uint16_t read_u16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_u16le(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void write_u32le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint8_t component_from_mask(uint32_t pixel, uint32_t mask)
{
    if (mask == 0)
        return 0;

    unsigned int shift = 0;
    while (shift < 32 && ((mask >> shift) & 1u) == 0u)
        shift++;

    if (shift == 32)
        return 0;

    const uint32_t value = (pixel & mask) >> shift;
    const uint32_t maximum = mask >> shift;
    return maximum == 0 ? 0 : (uint8_t)(((uint64_t)value * 255u + maximum / 2u) / maximum);
}

static bool mask_is_contiguous(uint32_t mask)
{
    if (mask == 0)
        return true;

    while ((mask & 1u) == 0u)
        mask >>= 1;
    return (mask & (mask + 1u)) == 0u;
}

bool ft2_ui_bitmap_decode_bmp_argb32(const uint8_t *data, size_t data_len,
                                     uint32_t **out_pixels, int32_t *out_w, int32_t *out_h)
{
    if (!data || !out_pixels || !out_w || !out_h || data_len < BMP_FILE_HEADER_SIZE + BMP_INFO_HEADER_SIZE)
        return false;

    *out_pixels = NULL;
    *out_w = 0;
    *out_h = 0;

    if (data[0] != 'B' || data[1] != 'M')
        return false;

    const uint32_t pixel_offset = read_u32le(&data[10]);
    const uint32_t dib_size = read_u32le(&data[14]);
    if (dib_size < BMP_INFO_HEADER_SIZE || dib_size > data_len - BMP_FILE_HEADER_SIZE)
        return false;

    const int32_t width = (int32_t)read_u32le(&data[18]);
    const int32_t signed_height = (int32_t)read_u32le(&data[22]);
    const uint16_t planes = read_u16le(&data[26]);
    const uint16_t bits_per_pixel = read_u16le(&data[28]);
    const uint32_t compression = read_u32le(&data[30]);

    if (width <= 0 || signed_height == 0 || signed_height == INT32_MIN || planes != 1)
        return false;
    if (bits_per_pixel != 24 && bits_per_pixel != 32)
        return false;
    if (compression != BMP_COMPRESSION_RGB &&
        compression != BMP_COMPRESSION_BITFIELDS &&
        compression != BMP_COMPRESSION_ALPHA_BITFIELDS)
        return false;
    if (bits_per_pixel == 24 && compression != BMP_COMPRESSION_RGB)
        return false;

    const int32_t height = signed_height < 0 ? -signed_height : signed_height;
    const bool top_down = signed_height < 0;
    const uint64_t row_stride64 = (((uint64_t)(uint32_t)width * bits_per_pixel + 31u) / 32u) * 4u;
    const uint64_t pixel_bytes64 = row_stride64 * (uint32_t)height;
    const uint64_t pixel_count64 = (uint64_t)(uint32_t)width * (uint32_t)height;
    const uint64_t dib_end = BMP_FILE_HEADER_SIZE + (uint64_t)dib_size;
    if (pixel_offset < dib_end || pixel_offset > data_len || pixel_bytes64 > data_len - pixel_offset ||
        pixel_count64 > SIZE_MAX / sizeof(uint32_t))
        return false;

    uint32_t red_mask = 0x00FF0000u;
    uint32_t green_mask = 0x0000FF00u;
    uint32_t blue_mask = 0x000000FFu;
    uint32_t alpha_mask = 0;
    if (compression == BMP_COMPRESSION_BITFIELDS || compression == BMP_COMPRESSION_ALPHA_BITFIELDS) {
        size_t masks_offset = BMP_FILE_HEADER_SIZE + BMP_INFO_HEADER_SIZE;
        if (dib_size >= 56)
            masks_offset = BMP_FILE_HEADER_SIZE + 40;
        const size_t required_masks = compression == BMP_COMPRESSION_ALPHA_BITFIELDS || dib_size >= 56 ? 16u : 12u;
        if (masks_offset > data_len || required_masks > data_len - masks_offset)
            return false;
        if (pixel_offset < masks_offset + required_masks)
            return false;
        red_mask = read_u32le(&data[masks_offset]);
        green_mask = read_u32le(&data[masks_offset + 4]);
        blue_mask = read_u32le(&data[masks_offset + 8]);
        if (required_masks == 16)
            alpha_mask = read_u32le(&data[masks_offset + 12]);
        if (red_mask == 0 || green_mask == 0 || blue_mask == 0 ||
            !mask_is_contiguous(red_mask) || !mask_is_contiguous(green_mask) ||
            !mask_is_contiguous(blue_mask) || !mask_is_contiguous(alpha_mask) ||
            (red_mask & green_mask) || (red_mask & blue_mask) || (green_mask & blue_mask) ||
            (alpha_mask & (red_mask | green_mask | blue_mask)))
            return false;
    }

    bool rgb32_has_alpha = false;
    if (bits_per_pixel == 32 && compression == BMP_COMPRESSION_RGB) {
        for (int32_t y = 0; y < height && !rgb32_has_alpha; y++) {
            const uint8_t *row = data + pixel_offset + (size_t)y * (size_t)row_stride64;
            for (int32_t x = 0; x < width; x++) {
                if (row[x * 4 + 3] != 0) {
                    rgb32_has_alpha = true;
                    break;
                }
            }
        }
    }

    uint32_t *pixels = (uint32_t *)malloc((size_t)pixel_count64 * sizeof(uint32_t));
    if (!pixels)
        return false;

    for (int32_t y = 0; y < height; y++) {
        const int32_t src_y = top_down ? y : height - 1 - y;
        const uint8_t *row = data + pixel_offset + (size_t)src_y * (size_t)row_stride64;
        uint32_t *dst = &pixels[(size_t)y * (size_t)width];
        for (int32_t x = 0; x < width; x++) {
            uint8_t r, g, b, a = 255;
            if (bits_per_pixel == 24 || compression == BMP_COMPRESSION_RGB) {
                const uint8_t *src = &row[x * (bits_per_pixel / 8)];
                b = src[0];
                g = src[1];
                r = src[2];
                if (bits_per_pixel == 32 && rgb32_has_alpha)
                    a = src[3];
            } else {
                const uint32_t packed = read_u32le(&row[x * 4]);
                r = component_from_mask(packed, red_mask);
                g = component_from_mask(packed, green_mask);
                b = component_from_mask(packed, blue_mask);
                if (alpha_mask != 0)
                    a = component_from_mask(packed, alpha_mask);
            }
            dst[x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }

    *out_pixels = pixels;
    *out_w = width;
    *out_h = height;
    return true;
}

bool ft2_ui_bitmap_encode_bmp_argb32(const uint32_t *pixels, int32_t w, int32_t h,
                                     uint8_t **out_data, size_t *out_len)
{
    if (!pixels || w <= 0 || h <= 0 || !out_data || !out_len)
        return false;

    *out_data = NULL;
    *out_len = 0;

    const uint64_t image_size64 = (uint64_t)(uint32_t)w * (uint32_t)h * 4u;
    const uint64_t file_size64 = BMP_FILE_HEADER_SIZE + BMP_V4_HEADER_SIZE + image_size64;
    if (image_size64 > UINT32_MAX || file_size64 > UINT32_MAX || file_size64 > SIZE_MAX)
        return false;

    const uint32_t pixel_offset = BMP_FILE_HEADER_SIZE + BMP_V4_HEADER_SIZE;
    const uint32_t image_size = (uint32_t)image_size64;
    const uint32_t file_size = (uint32_t)file_size64;
    uint8_t *data = (uint8_t *)calloc(1, file_size);
    if (!data)
        return false;

    data[0] = 'B';
    data[1] = 'M';
    write_u32le(&data[2], file_size);
    write_u32le(&data[10], pixel_offset);
    write_u32le(&data[14], BMP_V4_HEADER_SIZE);
    write_u32le(&data[18], (uint32_t)w);
    write_u32le(&data[22], (uint32_t)h);
    write_u16le(&data[26], 1);
    write_u16le(&data[28], 32);
    write_u32le(&data[30], BMP_COMPRESSION_BITFIELDS);
    write_u32le(&data[34], image_size);
    write_u32le(&data[38], 2835);
    write_u32le(&data[42], 2835);
    write_u32le(&data[54], 0x00FF0000u);
    write_u32le(&data[58], 0x0000FF00u);
    write_u32le(&data[62], 0x000000FFu);
    write_u32le(&data[66], 0xFF000000u);
    write_u32le(&data[70], 0x73524742u); /* LCS_sRGB */

    uint8_t *dst = data + pixel_offset;
    for (int32_t y = h - 1; y >= 0; y--) {
        const uint32_t *row = &pixels[(size_t)y * (size_t)w];
        for (int32_t x = 0; x < w; x++) {
            const uint32_t pixel = row[x];
            *dst++ = (uint8_t)pixel;
            *dst++ = (uint8_t)(pixel >> 8);
            *dst++ = (uint8_t)(pixel >> 16);
            *dst++ = (uint8_t)(pixel >> 24);
        }
    }

    *out_data = data;
    *out_len = file_size;
    return true;
}

uint32_t ft2_ui_bitmap_blend_argb32(uint32_t dst, uint32_t src, uint8_t opacity)
{
    const uint32_t src_alpha = ((src >> 24) * opacity + 127u) / 255u;
    if (src_alpha == 0)
        return dst;
    if (src_alpha == 255)
        return 0xFF000000u | (src & 0x00FFFFFFu);

    const uint32_t inverse = 255u - src_alpha;
    const uint32_t r = ((((src >> 16) & 0xFFu) * src_alpha) + (((dst >> 16) & 0xFFu) * inverse) + 127u) / 255u;
    const uint32_t g = ((((src >> 8) & 0xFFu) * src_alpha) + (((dst >> 8) & 0xFFu) * inverse) + 127u) / 255u;
    const uint32_t b = (((src & 0xFFu) * src_alpha) + ((dst & 0xFFu) * inverse) + 127u) / 255u;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
