#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "shared/ft2_ui_assets.h"

#define DESIGNER_MAX_BITMAPS 256
#define DESIGNER_BITMAP_NAME_MAX 64

typedef struct {
    int id;
    uint16_t w;
    uint16_t h;
    uint8_t *pixels;
    uint32_t *pixels32;
    ft2_ui_bmp_format_t format;
    uint8_t transparent_index;
    char name[DESIGNER_BITMAP_NAME_MAX];
} designer_bitmap_t;

typedef struct {
    uint8_t *button_gfx;     // 1-bit bitmap (values 0/1)
    uint8_t *checkbox_gfx;   // palette-indexed bitmap
    uint8_t *radiobutton_gfx;
    designer_bitmap_t *bitmaps;
    int bitmap_count;
} designer_assets_t;

bool init_designer_assets(void);
void free_designer_assets(void);
const designer_assets_t *designer_assets(void);

int designer_bitmap_count(void);
int designer_bitmap_id_at(int index);
int designer_bitmap_index_for_id(int id);
const designer_bitmap_t *designer_bitmap_at(int index);
const designer_bitmap_t *designer_bitmap_by_id(int id);
bool designer_import_bitmap(const char *path, int *out_id);
bool designer_import_bitmap_data(const uint8_t *data, size_t data_len, const char *format_hint,
                                 const char *name, int *out_id);
int designer_bitmap_transparent_index(int id);
bool designer_set_bitmap_transparent_index(int id, int index);
uint32_t designer_custom_palette_color(int index);
bool designer_bitmap_has_truecolor(int id);
