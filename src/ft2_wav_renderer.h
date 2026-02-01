#pragma once

#include <stdint.h>
#include "ft2_header.h"

#define MIN_WAV_RENDER_FREQ 44100
#define MAX_WAV_RENDER_FREQ 384000

/* internal chunk size used for incremental rendering (both file+slot) */
#define TICKS_PER_RENDER_CHUNK 64

void cbToggleWavRenderBPMMode(void);
void setWavRenderFrequency(int32_t freq);
void setWavRenderBitDepth(uint8_t bitDepth);
void updateWavRendererSettings(void);
void drawWavRenderer(void);
void showWavRenderer(void);
void hideWavRenderer(void);
void exitWavRenderer(void);
void pbWavRender(void);
void pbWavExit(void);
void pbWavSettings(void);
void pbWavFreqUp(void);
void pbWavFreqDown(void);
void pbWavAmpUp(void);
void pbWavAmpDown(void);
void pbWavSongStartUp(void);
void pbWavSongStartDown(void);
void pbWavSongEndUp(void);
void pbWavSongEndDown(void);
void resetWavRenderer(void);
void rbWavRenderBitDepth16(void);
void rbWavRenderBitDepth32(void);

// new render target radio callbacks
void rbWavRenderTargetFile(void);
void rbWavRenderTargetSlot(void);

/* Shared state needed by slot renderer */
extern bool useLegacyBPM;
extern uint8_t WDBitDepth;
extern uint32_t WDFrequency;
extern int16_t  WDAmp;
extern uint8_t *wavRenderBuffer;
extern bool WDRenderToSlot;

/* Rendering helper implemented in ft2_slot_renderer.c */
extern bool renderSelectionToSlot(uint32_t channelMask, uint16_t startPos, uint16_t stopPos, uint8_t bitDepth);


