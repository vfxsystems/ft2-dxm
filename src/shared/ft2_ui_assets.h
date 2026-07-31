#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "../ft2_palette.h"

#define FT2_UI_ASSET_VERSION 1

typedef enum
{
    FT2_UI_FONT_1 = 0,
    FT2_UI_FONT_2,
    FT2_UI_FONT_3,
    FT2_UI_FONT_4,
    FT2_UI_FONT_5,
    FT2_UI_FONT_6,
    FT2_UI_FONT_7,
    FT2_UI_FONT_8,
    FT2_UI_FONT_COUNT
} ft2_ui_font_id_t;

typedef enum
{
    FT2_UI_BITMAP_BUTTON_GFX = 0,
    FT2_UI_BITMAP_CHECKBOX_GFX,
    FT2_UI_BITMAP_RADIOBUTTON_GFX,
    FT2_UI_BITMAP_FT2_LOGO_BADGES,
    FT2_UI_BITMAP_FT2_BY_BADGES,
    FT2_UI_BITMAP_FT2_OLD_ABOUT_LOGO,
    FT2_UI_BITMAP_FT2_ABOUT_LOGO,
    FT2_UI_BITMAP_MIDI_LOGO,
    FT2_UI_BITMAP_NIBBLES_LOGO,
    FT2_UI_BITMAP_NIBBLES_STAGES,
    FT2_UI_BITMAP_LOOP_PINS,
    FT2_UI_BITMAP_MOUSE_CURSORS,
    FT2_UI_BITMAP_MOUSE_CURSOR_BUSY_CLOCK,
    FT2_UI_BITMAP_MOUSE_CURSOR_BUSY_GLASS,
    FT2_UI_BITMAP_WHITE_PIANO_KEYS,
    FT2_UI_BITMAP_BLACK_PIANO_KEYS,
    FT2_UI_BITMAP_VIBRATO_WAVEFORMS,
    FT2_UI_BITMAP_SCOPE_REC,
    FT2_UI_BITMAP_SCOPE_MUTE,
    FT2_UI_BITMAP_FT2_TUNEFISH_COMPLETE_LAYOUT_IMAGE_1,
    FT2_UI_BITMAP_FT2_MIXER_LAYOUT_IMAGE_1,
    FT2_UI_BITMAP_DX_COMPLETE_LAYOUT_IMAGE_1,
    FT2_UI_BITMAP_COUNT
} ft2_ui_bitmap_id_t;

typedef enum
{
    FT2_UI_BMP_FMT_RLE4 = 0,
    FT2_UI_BMP_FMT_RLE8,
    FT2_UI_BMP_FMT_RGB,
    FT2_UI_BMP_FMT_RGB32 = FT2_UI_BMP_FMT_RGB
} ft2_ui_bmp_format_t;

typedef struct
{
    ft2_ui_font_id_t id;
    const uint8_t *bmp;
    uint32_t bmp_len;
    uint16_t char_w;
    uint16_t char_h;
    uint16_t bmp_w;
    const uint8_t *widths;
} ft2_ui_font_asset_t;

typedef struct
{
    ft2_ui_bitmap_id_t id;
    const uint8_t *bmp;
    uint32_t bmp_len;
    ft2_ui_bmp_format_t fmt;
} ft2_ui_bitmap_asset_t;

typedef struct
{
    uint32_t version;
    uint16_t font_count;
    uint16_t bitmap_count;
    const ft2_ui_font_asset_t *fonts;
    const ft2_ui_bitmap_asset_t *bitmaps;
} ft2_ui_asset_registry_t;

extern const ft2_ui_asset_registry_t ft2_ui_assets;
