#include "shared/ft2_ui_assets.h"
#include "ft2_gfxdata.h"
#include <stddef.h>

// Minimal font width tables (copied from ft2_tables.c).
static const uint8_t ft2_font1_widths[128] =
{
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    3,2,5,6,6,9,7,2,4,4,5,6,2,4,2,6,6,4,6,6,6,6,6,6,6,6,2,2,5,5,5,6,
    9,7,6,6,7,5,5,7,7,3,5,6,5,8,7,7,6,7,6,6,6,7,7,9,6,6,6,3,6,3,5,6,
    3,6,6,5,6,6,4,6,6,2,3,5,2,8,6,6,6,6,4,5,4,6,6,8,5,6,5,4,2,4,6,8
};

static const uint8_t ft2_font2_widths[128] =
{
    16,16,16,16,14,16,14,16,16,16,16,16,16,16,16,16,
    16,16,16,16,14,16,16,16,16,16,16,16,16,16,16,16,
    10, 8,16,16,16,16,16,10,12,12,16,16,10,14, 8,16,
    16,16,16,16,16,16,16,16,16,16, 8,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16, 6,14,16,15,16,16,16,
    16,17,16,16,16,16,16,16,16,16,17,16,16,16,16,16,
    16,14,14,14,14,14,12,14,14, 6,10,14, 6,17,14,14,
    14,14,13,14,12,14,14,17,16,14,16,16,16,16,16,16
};

// Font dimensions (copied from ft2_gui.h to avoid heavy include dependencies).
#define FT2_FONT1_CHAR_W 8
#define FT2_FONT1_CHAR_H 10
#define FT2_FONT1_WIDTH 1024
#define FT2_FONT2_CHAR_W 16
#define FT2_FONT2_CHAR_H 20
#define FT2_FONT2_WIDTH 2048
#define FT2_FONT3_CHAR_W 4
#define FT2_FONT3_CHAR_H 7
#define FT2_FONT3_WIDTH 172
#define FT2_FONT4_CHAR_W 8
#define FT2_FONT4_CHAR_H 8
#define FT2_FONT4_WIDTH 624
#define FT2_FONT5_CHAR_W 16
#define FT2_FONT5_CHAR_H 8
#define FT2_FONT5_WIDTH 624
#define FT2_FONT6_CHAR_W 7
#define FT2_FONT6_CHAR_H 8
#define FT2_FONT6_WIDTH 112
#define FT2_FONT7_CHAR_W 6
#define FT2_FONT7_CHAR_H 7
#define FT2_FONT7_WIDTH 140
#define FT2_FONT8_CHAR_W 5
#define FT2_FONT8_CHAR_H 7
#define FT2_FONT8_WIDTH 80

static const ft2_ui_font_asset_t ft2_ui_font_assets[] =
{
    { FT2_UI_FONT_1, font1BMP, sizeof(font1BMP), FT2_FONT1_CHAR_W, FT2_FONT1_CHAR_H, FT2_FONT1_WIDTH, ft2_font1_widths },
    { FT2_UI_FONT_2, font2BMP, sizeof(font2BMP), FT2_FONT2_CHAR_W, FT2_FONT2_CHAR_H, FT2_FONT2_WIDTH, ft2_font2_widths },
    { FT2_UI_FONT_3, font3BMP, sizeof(font3BMP), FT2_FONT3_CHAR_W, FT2_FONT3_CHAR_H, FT2_FONT3_WIDTH, NULL },
    { FT2_UI_FONT_4, font4BMP, sizeof(font4BMP), FT2_FONT4_CHAR_W, FT2_FONT4_CHAR_H, FT2_FONT4_WIDTH, NULL },
    { FT2_UI_FONT_5, NULL, 0, FT2_FONT5_CHAR_W, FT2_FONT5_CHAR_H, FT2_FONT5_WIDTH, NULL },
    { FT2_UI_FONT_6, font6BMP, sizeof(font6BMP), FT2_FONT6_CHAR_W, FT2_FONT6_CHAR_H, FT2_FONT6_WIDTH, NULL },
    { FT2_UI_FONT_7, font7BMP, sizeof(font7BMP), FT2_FONT7_CHAR_W, FT2_FONT7_CHAR_H, FT2_FONT7_WIDTH, NULL },
    { FT2_UI_FONT_8, font8BMP, sizeof(font8BMP), FT2_FONT8_CHAR_W, FT2_FONT8_CHAR_H, FT2_FONT8_WIDTH, NULL }
};

static const ft2_ui_bitmap_asset_t ft2_ui_bitmap_assets[] =
{
    { FT2_UI_BITMAP_BUTTON_GFX, buttonGfxBMP, sizeof(buttonGfxBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_CHECKBOX_GFX, checkboxGfxBMP, sizeof(checkboxGfxBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_RADIOBUTTON_GFX, radiobuttonGfxBMP, sizeof(radiobuttonGfxBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_FT2_LOGO_BADGES, ft2LogoBadgesBMP, sizeof(ft2LogoBadgesBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_FT2_BY_BADGES, ft2ByBadgesBMP, sizeof(ft2ByBadgesBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_FT2_OLD_ABOUT_LOGO, ft2OldAboutLogoBMP, sizeof(ft2OldAboutLogoBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_FT2_ABOUT_LOGO, ft2AboutLogoBMP, sizeof(ft2AboutLogoBMP), FT2_UI_BMP_FMT_RGB },
    { FT2_UI_BITMAP_MIDI_LOGO, midiLogoBMP, sizeof(midiLogoBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_NIBBLES_LOGO, nibblesLogoBMP, sizeof(nibblesLogoBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_NIBBLES_STAGES, nibblesStagesBMP, sizeof(nibblesStagesBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_LOOP_PINS, loopPinsBMP, sizeof(loopPinsBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_MOUSE_CURSORS, mouseCursorsBMP, sizeof(mouseCursorsBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_MOUSE_CURSOR_BUSY_CLOCK, mouseCursorBusyClockBMP, sizeof(mouseCursorBusyClockBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_MOUSE_CURSOR_BUSY_GLASS, mouseCursorBusyGlassBMP, sizeof(mouseCursorBusyGlassBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_WHITE_PIANO_KEYS, whitePianoKeysBMP, sizeof(whitePianoKeysBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_BLACK_PIANO_KEYS, blackPianoKeysBMP, sizeof(blackPianoKeysBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_VIBRATO_WAVEFORMS, vibratoWaveformsBMP, sizeof(vibratoWaveformsBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_SCOPE_REC, scopeRecBMP, sizeof(scopeRecBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_SCOPE_MUTE, scopeMuteBMP, sizeof(scopeMuteBMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_FT2_TUNEFISH_COMPLETE_LAYOUT_IMAGE_1, ft2_tunefish_complete_layout_image1BMP, sizeof(ft2_tunefish_complete_layout_image1BMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_FT2_MIXER_LAYOUT_IMAGE_1, ft2_mixer_layout_image1BMP, sizeof(ft2_mixer_layout_image1BMP), FT2_UI_BMP_FMT_RLE4 },
    { FT2_UI_BITMAP_DX_COMPLETE_LAYOUT_IMAGE_1, dx_complete_layout_image1BMP, sizeof(dx_complete_layout_image1BMP), FT2_UI_BMP_FMT_RLE4 },
};

const ft2_ui_asset_registry_t ft2_ui_assets =
{
    FT2_UI_ASSET_VERSION,
    (uint16_t)(sizeof(ft2_ui_font_assets) / sizeof(ft2_ui_font_assets[0])),
    (uint16_t)(sizeof(ft2_ui_bitmap_assets) / sizeof(ft2_ui_bitmap_assets[0])),
    ft2_ui_font_assets,
    ft2_ui_bitmap_assets
};
