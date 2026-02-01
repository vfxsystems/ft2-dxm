#pragma once

#include <stdint.h>
#include <stdbool.h>

enum // SCROLLBARS
{
	// reserved
	SB_RES_1,
	SB_RES_2,
	SB_RES_3,

	SB_POS_ED,
	SB_SAMPLE_LIST,
	SB_CHAN_SCROLL,
	SB_HELP_SCROLL,
	SB_SAMP_SCROLL,

	// Instrument Editor
	SB_INST_VOL,
	SB_INST_PAN,
	SB_INST_FTUNE,
	SB_INST_FADEOUT,
	SB_INST_VIBSPEED,
	SB_INST_VIBDEPTH,
	SB_INST_VIBSWEEP,

	// Instrument Editor Extension
	SB_INST_EXT_MIDI_CH,
	SB_INST_EXT_MIDI_PRG,
	SB_INST_EXT_MIDI_BEND,

	// Config Audio
	SB_AUDIO_OUTPUT_SCROLL,
	SB_AUDIO_INPUT_SCROLL,
	SB_AMP_SCROLL,
	SB_MASTERVOL_SCROLL,

	// Config Layout
	SB_PAL_R,
	SB_PAL_G,
	SB_PAL_B,
	SB_PAL_CONTRAST,

	// Config Miscellaneous
	SB_MIDI_SENS,

#ifdef HAS_MIDI
	// Config MIDI input
	SB_MIDI_INPUT_SCROLL,
#endif

	// Disk Op.
	SB_DISKOP_LIST,

	// Mixer Screen (32 channels)
	SB_MIX_GAIN_0, SB_MIX_GAIN_1, SB_MIX_GAIN_2, SB_MIX_GAIN_3, SB_MIX_GAIN_4, SB_MIX_GAIN_5, SB_MIX_GAIN_6, SB_MIX_GAIN_7,
	SB_MIX_GAIN_8, SB_MIX_GAIN_9, SB_MIX_GAIN_10, SB_MIX_GAIN_11, SB_MIX_GAIN_12, SB_MIX_GAIN_13, SB_MIX_GAIN_14, SB_MIX_GAIN_15,
	SB_MIX_GAIN_16, SB_MIX_GAIN_17, SB_MIX_GAIN_18, SB_MIX_GAIN_19, SB_MIX_GAIN_20, SB_MIX_GAIN_21, SB_MIX_GAIN_22, SB_MIX_GAIN_23,
	SB_MIX_GAIN_24, SB_MIX_GAIN_25, SB_MIX_GAIN_26, SB_MIX_GAIN_27, SB_MIX_GAIN_28, SB_MIX_GAIN_29, SB_MIX_GAIN_30, SB_MIX_GAIN_31,
	SB_MIX_PAN_0, SB_MIX_PAN_1, SB_MIX_PAN_2, SB_MIX_PAN_3, SB_MIX_PAN_4, SB_MIX_PAN_5, SB_MIX_PAN_6, SB_MIX_PAN_7,
	SB_MIX_PAN_8, SB_MIX_PAN_9, SB_MIX_PAN_10, SB_MIX_PAN_11, SB_MIX_PAN_12, SB_MIX_PAN_13, SB_MIX_PAN_14, SB_MIX_PAN_15,
	SB_MIX_PAN_16, SB_MIX_PAN_17, SB_MIX_PAN_18, SB_MIX_PAN_19, SB_MIX_PAN_20, SB_MIX_PAN_21, SB_MIX_PAN_22, SB_MIX_PAN_23,
	SB_MIX_PAN_24, SB_MIX_PAN_25, SB_MIX_PAN_26, SB_MIX_PAN_27, SB_MIX_PAN_28, SB_MIX_PAN_29, SB_MIX_PAN_30, SB_MIX_PAN_31,
	SB_MIX_MASTER_GAIN,

    // Inline DSP parameter scrollbars (max 16 parameters)
    SB_DSP_PARAM_0, SB_DSP_PARAM_1, SB_DSP_PARAM_2, SB_DSP_PARAM_3,
    SB_DSP_PARAM_4, SB_DSP_PARAM_5, SB_DSP_PARAM_6, SB_DSP_PARAM_7,
    SB_DSP_PARAM_8, SB_DSP_PARAM_9, SB_DSP_PARAM_10, SB_DSP_PARAM_11,
    SB_DSP_PARAM_12, SB_DSP_PARAM_13, SB_DSP_PARAM_14, SB_DSP_PARAM_15,
    SB_DSP_PARAM_SCROLL,

	NUM_SCROLLBARS
};

// Append-only ID ranges for designer-driven layouts (do not reorder existing IDs).
#define SB_TF_BASE NUM_SCROLLBARS
#define SB_TF_COUNT 0
#define SB_DX_BASE (SB_TF_BASE + SB_TF_COUNT)
#define SB_DX_COUNT 0
#define SB_MIXER_BASE (SB_DX_BASE + SB_DX_COUNT)
#define SB_MIXER_COUNT 0
#define SB_DSP_BASE (SB_MIXER_BASE + SB_MIXER_COUNT)
#define SB_DSP_COUNT 0
#define SB_MACRO_MAP_BASE (SB_DSP_BASE + SB_DSP_COUNT)
#define SB_MACRO_MAP_COUNT 0

enum
{
	SCROLLBAR_UNPRESSED = 0,
	SCROLLBAR_PRESSED = 1,
	SCROLLBAR_HORIZONTAL = 0,
	SCROLLBAR_VERTICAL = 1,
	SCROLLBAR_FIXED_THUMB_SIZE = 0,
	SCROLLBAR_DYNAMIC_THUMB_SIZE = 1
};

typedef struct scrollBar_t // DO NOT TOUCH!
{
	uint16_t x, y, w, h;
	uint8_t type, thumbType;
	void (*callbackFunc)(uint32_t pos);

	bool visible;
	uint8_t state;
	uint32_t pos, page, end;
	uint16_t thumbX, thumbY, thumbW, thumbH, originalThumbSize;
} scrollBar_t;

void drawScrollBar(uint16_t scrollBarID);
void showScrollBar(uint16_t scrollBarID);
void hideScrollBar(uint16_t scrollBarID);
void scrollBarScrollUp(uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollDown(uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollLeft(uint16_t scrollBarID, uint32_t amount);
void scrollBarScrollRight(uint16_t scrollBarID, uint32_t amount);
void setScrollBarPos(uint16_t scrollBarID, uint32_t pos, bool triggerCallBack);
uint32_t getScrollBarPos(uint16_t scrollBarID);
void setScrollBarEnd(uint16_t scrollBarID, uint32_t end);
void setScrollBarPageLength(uint16_t scrollBarID, uint32_t pageLength);
bool testScrollBarMouseDown(void);
void testScrollBarMouseRelease(void);
void handleScrollBarsWhileMouseDown(void);
void initializeScrollBars(void);
