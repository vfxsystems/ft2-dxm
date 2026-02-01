#include "ft2_mixer.h"
#include <string.h>
#include "ft2_dsp.h"
#include "ft2_scrollbars.h"
#include <math.h>
#include <string.h>  // for memcpy

// Persistent mixer/DSP state cache globals
MixerChannelState gMixerState[MAX_STEREO_PAIRS];
MixerMasterState gMasterState;

// Persistent state functions
void cacheMixerStateFromData(void)
{
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
        gMixerState[ch].fader = stereoMixerCh[ch].fader;
        gMixerState[ch].pan = stereoMixerCh[ch].pan;
        memcpy(gMixerState[ch].effects, stereoMixerCh[ch].effects, sizeof(dspEffectInstance_t) * DSP_MAX_SLOTS);
    }
    gMasterState.masterGain = mixerMasterGain;
    memcpy(gMasterState.effects, masterEffects, sizeof(dspEffectInstance_t) * DSP_MAX_SLOTS);
}

void cacheMixerStateFromGUI(void)
{
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
        uint32_t pos = getScrollBarPos(SB_MIX_GAIN_0 + ch);
        gMixerState[ch].fader = (float)(GAIN_SLIDER_END - pos) / 100.0f;
        uint32_t panPos = getScrollBarPos(SB_MIX_PAN_0 + ch);
        gMixerState[ch].pan = ((float)panPos / 100.0f) - 1.0f;
        memcpy(gMixerState[ch].effects, stereoMixerCh[ch].effects, sizeof(dspEffectInstance_t) * DSP_MAX_SLOTS);
    }
    uint32_t mPos = getScrollBarPos(SB_MIX_MASTER_GAIN);
    gMasterState.masterGain = (float)(GAIN_SLIDER_END - mPos) / 100.0f;
    memcpy(gMasterState.effects, masterEffects, sizeof(dspEffectInstance_t) * DSP_MAX_SLOTS);
}

void applyMixerStateToGUI(void)
{
    for (int ch = 0; ch < MAX_STEREO_PAIRS; ch++) {
        mixerCh[ch].fader = gMixerState[ch].fader;
        stereoMixerCh[ch].fader = gMixerState[ch].fader;
        mixerCh[ch].pan = gMixerState[ch].pan;
        stereoMixerCh[ch].pan = gMixerState[ch].pan;
        memcpy(stereoMixerCh[ch].effects, gMixerState[ch].effects, sizeof(dspEffectInstance_t) * DSP_MAX_SLOTS);

        int gainUnits = (int)lrintf(gMixerState[ch].fader * 100.0f);
        setScrollBarPos(SB_MIX_GAIN_0 + ch, GAIN_SLIDER_END - gainUnits, false);
        int panUnits = (int)lrintf((gMixerState[ch].pan + 1.0f) * 100.0f);
        setScrollBarPos(SB_MIX_PAN_0 + ch, panUnits, false);
    }
    mixerMasterGain = gMasterState.masterGain;
    memcpy(masterEffects, gMasterState.effects, sizeof(dspEffectInstance_t) * DSP_MAX_SLOTS);
    int masterUnits = (int)lrintf(gMasterState.masterGain * 100.0f);
    setScrollBarPos(SB_MIX_MASTER_GAIN, GAIN_SLIDER_END - masterUnits, false);
}

mixerCh_t mixerCh[MAX_MIXER_CHANNELS];
// new stereo pair mixer array (16 strips)
mixerCh_t stereoMixerCh[MAX_STEREO_PAIRS];
float mixerMasterGain = 1.0f;

void mixerSetMasterGain(float g)
{
    if (g < 0.0f) g = 0.0f;
    if (g > 2.0f) g = 2.0f;
    mixerMasterGain = g;
}

void mixerInit(void)
{
    for (int i = 0; i < MAX_MIXER_CHANNELS; i++)
    {
        mixerCh[i].fader = 1.0f; /* unity gain */
        mixerCh[i].pan   = 0.0f; /* center */

        /* clear DSP chain */
        memset(mixerCh[i].effects, 0, sizeof(mixerCh[i].effects));
    }

    // initialize stereo pair strips
    for (int i = 0; i < MAX_STEREO_PAIRS; i++)
    {
        stereoMixerCh[i].fader = 1.0f;
        stereoMixerCh[i].pan   = 0.0f;
        memset(stereoMixerCh[i].effects, 0, sizeof(stereoMixerCh[i].effects));
    }

    // Initialize master effects chain
    memset(masterEffects, 0, sizeof(masterEffects));

    mixerMasterGain = 1.0f;

    // Initialize cache with default mixer values
    cacheMixerStateFromData();
}

void mixerInitDSPEffects(uint32_t sampleRate)
{
    // Initialize all DSP effects with the current sample rate
    for (int i = 0; i < MAX_STEREO_PAIRS; i++)
    {
        for (int slot = 0; slot < DSP_MAX_SLOTS; slot++)
        {
            dspEffectInstance_t *eff = &stereoMixerCh[i].effects[slot];
            if (eff->type != DSP_TYPE_NONE && eff->enabled)
            {
                // Re-initialize with current sample rate
                dspFreeEffect(eff);
                dspInitEffect(eff, eff->type, sampleRate);
                dspResetEffectState(eff);
            }
        }
    }

    // Initialize master effects
    for (int slot = 0; slot < DSP_MAX_SLOTS; slot++)
    {
        dspEffectInstance_t *eff = &masterEffects[slot];
        if (eff->type != DSP_TYPE_NONE && eff->enabled)
        {
            // Re-initialize with current sample rate
            dspFreeEffect(eff);
            dspInitEffect(eff, eff->type, sampleRate);
            dspResetEffectState(eff);
        }
    }
}
