#include "assets.h"
#include "palette.h"
#include "shared/ft2_ui_bitmap.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "bitmap pipeline: %s\n", message); failures++; } } while (0)

static int failures;

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

static void make_bmp24(uint8_t data[70])
{
    memset(data, 0, 70);
    data[0] = 'B';
    data[1] = 'M';
    write_u32le(&data[2], 70);
    write_u32le(&data[10], 54);
    write_u32le(&data[14], 40);
    write_u32le(&data[18], 2);
    write_u32le(&data[22], 2);
    write_u16le(&data[26], 1);
    write_u16le(&data[28], 24);
    write_u32le(&data[34], 16);

    /* BMP rows are bottom-up and BGR, with each row padded to four bytes. */
    const uint8_t pixels[16] = {
        255, 0, 0,  0, 255, 0,  0, 0,
        0, 0, 255,  255, 255, 255,  0, 0
    };
    memcpy(&data[54], pixels, sizeof(pixels));
}

static const uint8_t png_rgba_2x2[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D, 0x24, 0x00, 0x00, 0x00,
    0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
    0x1F, 0x08, 0x1B, 0x18, 0x40, 0x34, 0x88, 0x09, 0x00, 0x39, 0xDD, 0x06,
    0x7B, 0x9A, 0xCB, 0x92, 0xC5, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
    0x44, 0xAE, 0x42, 0x60, 0x82
};

static void check_import(const uint8_t *data, size_t len, const char *hint,
                         const uint32_t expected[4], const char *label)
{
    int id = -1;
    CHECK(designer_import_bitmap_data(data, len, hint, label, &id), label);
    const designer_bitmap_t *bitmap = designer_bitmap_by_id(id);
    CHECK(bitmap != NULL, "imported bitmap is registered");
    if (!bitmap)
        return;
    CHECK(bitmap->w == 2 && bitmap->h == 2, "imported bitmap dimensions are preserved");
    CHECK(bitmap->pixels32 != NULL, "imported bitmap retains true-color pixels");
    if (bitmap->pixels32)
        CHECK(memcmp(bitmap->pixels32, expected, 4 * sizeof(uint32_t)) == 0,
              "imported bitmap channel order and alpha are exact");
}

int main(void)
{
    init_palette();
    CHECK(init_designer_assets(), "designer asset registry initializes");

    const uint32_t argb[4] = {
        0xFFFF0000u, 0x8000FF00u,
        0x000000FFu, 0xFF00FF00u
    };
    uint8_t *encoded = NULL;
    size_t encoded_len = 0;
    CHECK(ft2_ui_bitmap_encode_bmp_argb32(argb, 2, 2, &encoded, &encoded_len),
          "32-bit BMP encoder accepts ARGB pixels");

    uint32_t *decoded = NULL;
    int32_t width = 0;
    int32_t height = 0;
    CHECK(ft2_ui_bitmap_decode_bmp_argb32(encoded, encoded_len, &decoded, &width, &height),
          "32-bit BMP decoder accepts exported V4 BMP");
    CHECK(width == 2 && height == 2, "32-bit BMP round-trip preserves dimensions");
    if (decoded)
        CHECK(memcmp(decoded, argb, sizeof(argb)) == 0, "32-bit BMP round-trip preserves every ARGB byte");
    free(decoded);
    check_import(encoded, encoded_len, ".bmp", argb, "32-bit BMP import");

    uint8_t *rgb32 = (uint8_t *)malloc(encoded_len);
    CHECK(rgb32 != NULL, "32-bit BI_RGB fixture allocates");
    if (rgb32) {
        memcpy(rgb32, encoded, encoded_len);
        write_u32le(&rgb32[14], 40);
        write_u32le(&rgb32[30], 0);
        check_import(rgb32, encoded_len, ".bmp", argb, "32-bit BI_RGB alpha import");
        free(rgb32);
    }
    free(encoded);

    uint8_t bmp24[70];
    make_bmp24(bmp24);
    const uint32_t expected_bmp24[4] = {
        0xFFFF0000u, 0xFFFFFFFFu,
        0xFF0000FFu, 0xFF00FF00u
    };
    check_import(bmp24, sizeof(bmp24), ".bmp", expected_bmp24, "24-bit BMP import");

    const uint32_t expected_png[4] = {
        0xFFFF0000u, 0x8000FF00u,
        0x000000FFu, 0xFF00FF00u
    };
    check_import(png_rgba_2x2, sizeof(png_rgba_2x2), ".png", expected_png, "PNG alpha import");

    CHECK(ft2_ui_bitmap_blend_argb32(0xFF0000FFu, 0x80FF0000u, 255) == 0xFF80007Fu,
          "per-pixel alpha blends with exact channel order");
    CHECK(ft2_ui_bitmap_blend_argb32(0xFF0000FFu, 0x80FF0000u, 128) == 0xFF4000BFu,
          "schema opacity multiplies per-pixel alpha");
    CHECK(ft2_ui_bitmap_blend_argb32(0xFF123456u, 0x00010203u, 255) == 0xFF123456u,
          "zero-alpha pixels leave the destination untouched");

    free_designer_assets();
    if (failures != 0)
        return 1;
    printf("bitmap pipeline: PNG, 24-bit BMP, 32-bit BMP, alpha, and opacity passed\n");
    return 0;
}
