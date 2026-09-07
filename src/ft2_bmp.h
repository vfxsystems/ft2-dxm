#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct bmp_t
{
	uint8_t *buttonGfx, *font1, *font2, *font3, *font4, *font6, *font7, *font8;
	uint8_t *ft2LogoBadges, *ft2ByBadges, *radiobuttonGfx, *checkboxGfx;
	uint8_t *midiLogo, *nibblesLogo, *nibblesStages, *loopPins, *ft2OldAboutLogo;
	uint8_t *mouseCursors, *mouseCursorBusyClock, *mouseCursorBusyGlass;
	uint8_t *whitePianoKeys, *blackPianoKeys, *vibratoWaveforms, *scopeRec, *scopeMute;
	uint32_t *ft2AboutLogo;
} bmp_t;

extern bmp_t bmp; // ft2_bmp.c

bool loadBMPs(void);
void freeBMPs(void);
uint8_t *ft2_bmp_decode_rle4_to_pal(const uint8_t *src, int32_t *out_w, int32_t *out_h);
uint32_t *ft2_bmp_decode_to_rgb32(const uint8_t *src, size_t src_len, int32_t *out_w, int32_t *out_h);
