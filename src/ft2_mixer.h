#pragma once

#include <stdint.h>
#include "ft2_header.h"
#include "ft2_dsp.h"

// Slider range for mixer/master gain scrollbars (shared)
#ifndef GAIN_SLIDER_END
#define GAIN_SLIDER_END 200  /* maps 0.0 .. 2.0 */
#endif

#define MAX_MIXER_CHANNELS MAX_CHANNELS /* reuse global constant (32) */

typedef struct
{
    float fader; /* linear gain 0.0 .. 2.0 (0dB at 1.0) */
    float pan;   /* -1.0 = left, 0.0 = center, +1.0 = right */

    /* Per-channel DSP chain (up to DSP_MAX_SLOTS) */
    dspEffectInstance_t effects[DSP_MAX_SLOTS];
} mixerCh_t;

extern mixerCh_t mixerCh[MAX_MIXER_CHANNELS];
// new: per-stereo-pair mixer strips (16)
extern mixerCh_t stereoMixerCh[MAX_STEREO_PAIRS];
extern float mixerMasterGain; /* 0.0 .. 2.0 */

void mixerSetMasterGain(float g);

void mixerInit(void);
void mixerInitDSPEffects(uint32_t sampleRate);
void mixerShutdownDSPEffects(void);

// Persistent mixer/DSP state cache
#define NUM_MIXER_DSP_PARAMS DSP_MAX_SLOTS

typedef struct {
    float fader; /* linear gain 0.0 .. 2.0 */
    float pan;   /* -1.0 = left, +1.0 = right */
    dspEffectInstance_t effects[DSP_MAX_SLOTS];
} MixerChannelState;

typedef struct {
    float masterGain; /* 0.0 .. 2.0 */
    dspEffectInstance_t effects[DSP_MAX_SLOTS];
} MixerMasterState;

extern MixerChannelState gMixerState[MAX_STEREO_PAIRS];
extern MixerMasterState gMasterState;

void cacheMixerStateFromData(void);
void cacheMixerStateFromGUI(void);
void applyMixerStateToGUI(void); 
