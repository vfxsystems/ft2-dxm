#pragma once

#include <stdint.h>

/* Opens the DSP editor for the given mixer channel (0..31) */
void showChannelDspEditor(uint8_t channelIdx);

/* Hide/close the DSP editor */
void hideDspEditor(void);

/* Per-frame renderer (call while editor shown) */
void dspEditorFrame(void); 