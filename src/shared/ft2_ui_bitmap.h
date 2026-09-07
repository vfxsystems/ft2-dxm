#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* True-color UI pixels use straight-alpha ARGB: 0xAARRGGBB. */
bool ft2_ui_bitmap_decode_bmp_argb32(const uint8_t *data, size_t data_len,
                                     uint32_t **out_pixels, int32_t *out_w, int32_t *out_h);
bool ft2_ui_bitmap_encode_bmp_argb32(const uint32_t *pixels, int32_t w, int32_t h,
                                     uint8_t **out_data, size_t *out_len);

/* Composites a straight-alpha source pixel over an RGB framebuffer pixel. */
uint32_t ft2_ui_bitmap_blend_argb32(uint32_t dst, uint32_t src, uint8_t opacity);
