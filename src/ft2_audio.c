// for finding memory leaks in debug mode with Visual Studio
#if defined _DEBUG && defined _MSC_VER
#include <crtdbg.h>
#endif

 #include <stdio.h>
#include <stdint.h>
#include <math.h> // for fabsf in peak calculation
#include <string.h>
#include "ft2_header.h"
#include "ft2_config.h"
#include "scopes/ft2_scopes.h"
#include "ft2_video.h"
#include "ft2_gui.h"
#include "ft2_midi.h"
#include "ft2_wav_renderer.h"
#include "ft2_tables.h"
#include "ft2_structs.h"
#include "ft2_audioselector.h"
#include "mixer/ft2_mix.h"
#include "mixer/ft2_silence_mix.h"
#include "ft2_audio.h"
#include "ft2_mixer.h"
#include "ft2_dsp.h"
#include "ft2_synth.h"
#include "ft2_dexed.h"
#include "ft2_v2.h"
#include "ft2_ostirus.h"
#include "ft2_unified_synth.h"

static void sendSamples16BitStereo(void *stream, uint32_t sampleBlockLength);
static void sendSamples32BitFloatStereo(void *stream, uint32_t sampleBlockLength);
void lockAudio(void);
void unlockAudio(void);

// hide POSIX warnings
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

static int32_t smpShiftValue;
static uint32_t oldAudioFreq, tickTimeLenInt;
static uint64_t tickTimeLenFrac;
static float fAudioNormalizeMul = 1.0f, fSqrtPanningTable[256+1];
static voice_t voice[MAX_CHANNELS  * 2];

// globalized
audio_t audio;
pattSyncData_t *pattSyncEntry;
chSyncData_t *chSyncEntry;
chSync_t chSync;
pattSync_t pattSync;
volatile bool pattQueueClearing, chQueueClearing;
static volatile uint32_t mixerUpdateMask = 0; // bitmask of channels needing update

/* Master DSP chain */
dspEffectInstance_t masterEffects[DSP_MAX_SLOTS];

/* Per-channel intermediate mix buffers (allocated in setupAudioBuffers) */
static float *chMixBufL[MAX_CHANNELS] = { NULL };
static float *chMixBufR[MAX_CHANNELS] = { NULL };

// per-stereo-pair mix buffers (new)
static float *pairMixBufL[MAX_STEREO_PAIRS] = { NULL };
static float *pairMixBufR[MAX_STEREO_PAIRS] = { NULL };

/* Scratch buffers for per-instrument synth rendering */
static float *synthMixBufL = NULL;
static float *synthMixBufR = NULL;
static uint32_t synthMixBufSize = 0;
static bool synthRoutingDebug = false;
static float outputMonitorMono[AUDIO_OUTPUT_MONITOR_LEN];
static uint32_t outputMonitorSamples = 0;

/* Buffer sent to GUI scopes for synth-inclusive waveform (per stereo pair) */
#define SYNTH_SCOPE_LEN 512
static int16_t synthScopeBufL[MAX_CHANNELS][SYNTH_SCOPE_LEN];

/* Last tracker channel that had a TF4 instrument triggered (for keyjazz/playback) */
static int tf4CurrentChn = 0;

// Global live-meter values (shared with GUI)
volatile float g_audioOutPeak = 0.0f; // 0.0-1.0 normalized peak level
volatile float g_audioCPULoad = 0.0f; // 0.0-1.0 normalized CPU load

// Bumped every time the output monitor buffer is refreshed, so UI consumers (e.g. the
// synth editor spectrum/scope widget) can skip expensive recomputation on frames where
// no new audio has arrived.
static volatile uint32_t g_audioOutputMonitorGeneration = 0;

static void updateOutputMonitor(uint32_t sampleBlockLength)
{
    uint32_t i, samplesToCopy, srcOffset;

    samplesToCopy = sampleBlockLength;
    if (samplesToCopy > AUDIO_OUTPUT_MONITOR_LEN)
        samplesToCopy = AUDIO_OUTPUT_MONITOR_LEN;

    srcOffset = sampleBlockLength - samplesToCopy;
    for (i = 0; i < samplesToCopy; ++i)
        outputMonitorMono[i] = 0.5f * (audio.fMixBufferL[srcOffset + i] + audio.fMixBufferR[srcOffset + i]);

    outputMonitorSamples = samplesToCopy;
    g_audioOutputMonitorGeneration++;
}

void stopVoice(int32_t i)
{
	voice_t *v;

	v = &voice[i];
	memset(v, 0, sizeof (voice_t));
	v->panning = 128;

	// clear "fade out" voice too

	v = &voice[MAX_CHANNELS + i];
	memset(v, 0, sizeof (voice_t));
	v->panning = 128;
}

bool setNewAudioSettings(void) // only call this from the main input/video thread
{
	pauseAudio();

	if (!setupAudio(CONFIG_HIDE_ERRORS))
	{
		// set back old known working settings

		config.audioFreq = audio.lastWorkingAudioFreq;
		config.specialFlags &= ~(BITDEPTH_16 + BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
		config.specialFlags |= audio.lastWorkingAudioBits;

		if (audio.lastWorkingAudioDeviceName != NULL)
		{
			if (audio.currOutputDevice != NULL)
			{
				free(audio.currOutputDevice);
				audio.currOutputDevice = NULL;
			}

			audio.currOutputDevice = strdup(audio.lastWorkingAudioDeviceName);
		}

		// also update config audio radio buttons if we're on that screen at the moment
		if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
			setConfigAudioRadioButtonStates();

		// if it didn't work to use the old settings again, then something is seriously wrong...
		if (!setupAudio(CONFIG_HIDE_ERRORS))
			okBox(0, "System message", "Couldn't find a working audio mode... You'll get no sound / replayer timer!", NULL);

		resumeAudio();
		return false;
	}

	resumeAudio();

	setWavRenderFrequency(audio.freq);
	setWavRenderBitDepth((config.specialFlags & BITDEPTH_32) ? 32 : 16);
	return true;
}

// amp = 1..32, masterVol = 0..256
void setAudioAmp(int16_t amp, int16_t masterVol, bool bitDepth32Flag)
{
	amp = CLAMP(amp, 1, 32);
	masterVol = CLAMP(masterVol, 0, 256);

	double dAmp = (amp * masterVol) / (32.0 * 256.0);
	/* Float-domain gain; PCM scaling belongs only to the output converter. */
	(void)bitDepth32Flag;
	fAudioNormalizeMul = (float)dAmp;
}

void decreaseMasterVol(void)
{
	if (config.masterVol >= 16)
		config.masterVol -= 16;
	else
		config.masterVol = 0;

	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// if Config -> Audio is open, update master volume scrollbar
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		drawScrollBar(SB_MASTERVOL_SCROLL);
}

void increaseMasterVol(void)
{
	if (config.masterVol < (256-16))
		config.masterVol += 16;
	else
		config.masterVol = 256;

	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// if Config -> Audio is open, update master volume scrollbar
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		drawScrollBar(SB_MASTERVOL_SCROLL);
}

void setNewAudioFreq(uint32_t freq) // for song-to-WAV rendering
{
	if (freq == 0)
		return;

	oldAudioFreq = audio.freq;
	audio.freq = freq;

	const bool mustRecalcTables = audio.freq != oldAudioFreq;
	if (mustRecalcTables)
		calcReplayerVars(audio.freq);

	ft2_unified_synth_set_samplerate(audio.freq);
	mixerInitDSPEffects(audio.freq);
}

void setBackOldAudioFreq(void) // for song-to-WAV rendering
{
	const bool mustRecalcTables = audio.freq != oldAudioFreq;

	audio.freq = oldAudioFreq;

	if (mustRecalcTables)
		calcReplayerVars(audio.freq);

	ft2_unified_synth_set_samplerate(audio.freq);
	mixerInitDSPEffects(audio.freq);
}

void audioSetSynthRoutingDebug(bool enabled)
{
	synthRoutingDebug = enabled;
}
void setMixerBPM(int32_t bpm)
{
	if (bpm < MIN_BPM || bpm > MAX_BPM)
		return;

	int32_t i = bpm - MIN_BPM;

	audio.samplesPerTickInt = audio.samplesPerTickIntTab[i];
	audio.samplesPerTickFrac = audio.samplesPerTickFracTab[i];
	audio.fSamplesPerTickIntMul = (float)(1.0 / (double)audio.samplesPerTickInt);

	// for audio/video sync timestamp
	tickTimeLenInt = audio.tickTimeIntTab[i];
	tickTimeLenFrac = audio.tickTimeFracTab[i];
}

void audioSetVolRamp(bool volRamp)
{
	lockMixerCallback();
	audio.volumeRampingFlag = volRamp;
	unlockMixerCallback();
}

void audioSetInterpolationType(uint8_t interpolationType)
{
	lockMixerCallback();
	audio.interpolationType = interpolationType;

	audio.sincInterpolation = false;

	// set sinc LUT pointers
	if (config.interpolation == INTERPOLATION_SINC8)
	{
		fSinc_1 = fSinc8_1;
		fSinc_2 = fSinc8_2;
		fSinc_3 = fSinc8_3;

		audio.sincInterpolation = true;
	}
	else if (config.interpolation == INTERPOLATION_SINC16)
	{
		fSinc_1 = fSinc16_1;
		fSinc_2 = fSinc16_2;
		fSinc_3 = fSinc16_3;

		audio.sincInterpolation = true;
	}

	unlockMixerCallback();
}

void calcPanningTable(void)
{
	// same formula as FT2's panning table (with 0.0 .. 1.0 scale)
	for (int32_t i = 0; i <= 256; i++)
		fSqrtPanningTable[i] = (float)sqrt(i / 256.0);
}

static void voiceUpdateVolumes(int32_t i, uint8_t status)
{
	voice_t *v = &voice[i];

	/* Base volumes from note + instrument + pattern pan */
	float tgtL = v->fVolume * fSqrtPanningTable[256 - v->panning];
	float tgtR = v->fVolume * fSqrtPanningTable[      v->panning];

	/* Note: Stereo pair fader/pan is now applied in the stereo pair processing stage */
	/* This ensures proper signal flow: tracker channels -> stereo pairs -> master bus */

	v->fTargetVolumeL = tgtL;
	v->fTargetVolumeR = tgtR;

	if (!audio.volumeRampingFlag)
	{
		// volume ramping is disabled, set volume directly
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->volumeRampLength = 0;
		return;
	}

	// now we need to handle volume ramping

	const bool voiceSampleTrigger = !!(status & IS_Trigger);

	if (voiceSampleTrigger)
	{
		// sample is about to start, ramp out/in at the same time

		if (v->fCurrVolumeL > 0.0f || v->fCurrVolumeR > 0.0f)
		{
			// setup fadeout voice

			voice_t *f = &voice[MAX_CHANNELS+i];

			*f = *v; // copy current voice to new fadeout-ramp voice

			const float fVolumeLDiff = 0.0f - f->fCurrVolumeL;
			const float fVolumeRDiff = 0.0f - f->fCurrVolumeR;

			f->volumeRampLength = audio.quickVolRampSamples; // 5ms
			f->fVolumeLDelta = fVolumeLDiff * audio.fQuickVolRampSamplesMul;
			f->fVolumeRDelta = fVolumeRDiff * audio.fQuickVolRampSamplesMul;

			f->isFadeOutVoice = true;
		}

		// make current voice fade in from zero when it starts
		v->fCurrVolumeL = v->fCurrVolumeR = 0.0f;
	}

	if (!voiceSampleTrigger && v->fTargetVolumeL == v->fCurrVolumeL && v->fTargetVolumeR == v->fCurrVolumeR)
	{
		v->volumeRampLength = 0; // no ramp needed for now
	}
	else
	{
		const float fVolumeLDiff = v->fTargetVolumeL - v->fCurrVolumeL;
		const float fVolumeRDiff = v->fTargetVolumeR - v->fCurrVolumeR;

		float fRampLengthMul;
		if (status & IS_QuickVol) // duration of 5ms
		{
			v->volumeRampLength = audio.quickVolRampSamples;
			fRampLengthMul = audio.fQuickVolRampSamplesMul;
		}
		else // duration of a tick
		{
			v->volumeRampLength = audio.samplesPerTickInt;
			fRampLengthMul = audio.fSamplesPerTickIntMul;
		}

		v->fVolumeLDelta = fVolumeLDiff * fRampLengthMul;
		v->fVolumeRDelta = fVolumeRDiff * fRampLengthMul;
	}
}

static void voiceTrigger(int32_t ch, sample_t *s, int32_t position)
{
	voice_t *v = &voice[ch];

	int32_t length = s->length;
	int32_t loopStart = s->loopStart;
	int32_t loopLength = s->loopLength;
	int32_t loopEnd = s->loopStart + s->loopLength;
	uint8_t loopType = GET_LOOPTYPE(s->flags);
	bool sample16Bit = !!(s->flags & SAMPLE_16BIT);
    bool stereo = !!(s->flags & SAMPLE_STEREO);

    // Use new L/R sample pointers
    if (s->dataPtrL == NULL || length < 1) {
        v->active = false; // shut down voice (illegal parameters)
        return;
    }

    if (loopLength < 1) // disable loop if loopLength is below 1
        loopType = 0;

    if (sample16Bit) {
        v->base16 = (const int16_t *)s->dataPtrL;
        v->base16R = stereo ? (const int16_t *)s->dataPtrR : NULL;
        v->revBase16 = &((const int16_t *)s->dataPtrL)[loopStart + loopEnd]; // for pingpong loops
        v->leftEdgeTaps16 = s->leftEdgeTapSamples16 + MAX_LEFT_TAPS;
        v->base8 = NULL; v->base8R = NULL; v->revBase8 = NULL; v->leftEdgeTaps8 = NULL;


    } else {
        v->base8 = s->dataPtrL;
        v->base8R = stereo ? s->dataPtrR : NULL;
        v->revBase8 = &s->dataPtrL[loopStart + loopEnd]; // for pingpong loops
        v->leftEdgeTaps8 = s->leftEdgeTapSamples8 + MAX_LEFT_TAPS;
        v->base16 = NULL; v->base16R = NULL; v->revBase16 = NULL; v->leftEdgeTaps16 = NULL;


    }

    v->hasLooped = false; // for cubic/sinc interpolation special case
    v->samplingBackwards = false;
    v->loopType = loopType;
    v->sampleEnd = (loopType == LOOP_OFF) ? length : loopEnd;
    v->loopStart = loopStart;
    v->loopLength = loopLength;
    v->position = position;
    v->positionFrac = 0;

    // if position overflows, shut down voice (f.ex. through 9xx command)
    if (v->position >= v->sampleEnd) {
        v->active = false;
        return;
    }

    v->mixFuncOffset = ((int32_t)sample16Bit * 18) + (audio.interpolationType * 3) + loopType;
    v->active = true;
}

void resetRampVolumes(void)
{
	voice_t *v = voice;
	for (int32_t i = 0; i < song.numChannels; i++, v++)
	{
		v->fCurrVolumeL = v->fTargetVolumeL;
		v->fCurrVolumeR = v->fTargetVolumeR;
		v->volumeRampLength = 0;
	}
}

void updateVoices(void)
{
	channel_t *ch = channel;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.numChannels; i++, ch++, v++)
	{
		const uint8_t status = ch->tmpStatus = ch->status; // (tmpStatus is used for audio/video sync queue)
		if (status == 0)
			continue;

		ch->status = 0;

		if (status & IS_Vol)
		{
			v->fVolume = ch->fFinalVol; // 0.0f .. 1.0f
			v->scopeVolume = (uint8_t)((ch->fFinalVol * (SCOPE_HEIGHT*4.0f)) + 0.5f);
		}

		if (status & IS_Pan)
			v->panning = ch->finalPan;

		if (status & (IS_Vol + IS_Pan))
			voiceUpdateVolumes(i, status);

		/* Apply pending mixer gain/pan changes requested from GUI */
		if (mixerUpdateMask & (1u << i))
		{
			mixerUpdateMask &= ~(1u << i);
			voiceUpdateVolumes(i, IS_QuickVol);
		}

		if (status & IS_Period)
		{
			const double dVoiceHz = dPeriod2Hz(ch->finalPeriod);

			// set voice delta
			v->delta = (int64_t)((dVoiceHz * audio.dHz2MixDeltaMul) + 0.5); // Hz -> fixed-point delta (rounded)
			if (audio.sincInterpolation)
			{
				// decide which sinc LUT to use according to the resampling ratio
				if (v->delta <= sincRatio1)
					v->fSincLUT = fSinc_1;
				else if (v->delta <= sincRatio2)
					v->fSincLUT = fSinc_2;
				else
					v->fSincLUT = fSinc_3;
			}
		}

		if (status & IS_Trigger)
			voiceTrigger(i, ch->smpPtr, ch->smpStartPos);
	}
}

/* helper to mix a single voice into custom buffers by temporarily redirecting global mix pointers */
static inline void mixVoiceToBuffer(voice_t *vc, int32_t bufferPos, int32_t samplesToMix, float *dstL, float *dstR)
{
	float *prevL = audio.fMixBufferL;
	float *prevR = audio.fMixBufferR;
	audio.fMixBufferL = dstL - bufferPos; /* macros add bufferPos internally */
	audio.fMixBufferR = dstR - bufferPos;

	bool volRampFlag = (vc->volumeRampLength > 0);
	const int32_t mixOffsetBias = 3 * NUM_INTERPOLATORS * 2;
	if (!volRampFlag && vc->fCurrVolumeL == 0.0f && vc->fCurrVolumeR == 0.0f)
	{
		silenceMixRoutine(vc, samplesToMix);
	}
	else
	{
		mixFuncTab[((int32_t)volRampFlag * mixOffsetBias) + vc->mixFuncOffset](vc, bufferPos, samplesToMix);
	}

	audio.fMixBufferL = prevL;
	audio.fMixBufferR = prevR;
}

static inline void mixStereoVoiceToBuffer(voice_t *v, int32_t bufferPos, int32_t samplesToMix, float *dstL, float *dstR)
{
    /* Simple nearest-neighbor mixer for true stereo samples.
       We purposely keep this independent of the legacy mono mixer macros so that
       we don’t have to duplicate 100+ specialized routines. */

    if (v->base16R == NULL && v->base8R == NULL) {
        /* fall back to mono routine if R channel missing */
        mixVoiceToBuffer(v, bufferPos, samplesToMix, dstL, dstR);
        return;
    }

    const bool sample16Bit = (v->base16 != NULL);
    const uint64_t delta = v->delta;

    int32_t position = v->position;
    uint64_t frac    = v->positionFrac;

    const int32_t loopLength = v->loopLength;
    const int32_t sampleEnd  = v->sampleEnd; // loopEnd if loop, else length
    const uint8_t loopType   = v->loopType;   // 0 = no loop, 1 = FWD, 2 = bidi

    float currVolL = v->fCurrVolumeL;
    float currVolR = v->fCurrVolumeR;

    float volDeltaL = v->fVolumeLDelta;
    float volDeltaR = v->fVolumeRDelta;
    int32_t volRampLeft = v->volumeRampLength;

    /* Pre-compute scalers */
    const float scale8  = 1.0f / 128.0f;
    const float scale16 = 1.0f / 32768.0f;

    for (int32_t s = 0; s < samplesToMix; s++)
    {
        /* Safety – if we reach end of (non-loop) sample, stop mixing */
        if (position >= sampleEnd) {
            if (loopType == LOOP_OFF) {
                v->active = false;
                break; // remaining dst samples stay zero (already cleared by caller)
            } else if (loopType == LOOP_FWD) {
                position -= loopLength;
            } else { /* ping-pong – treat as forward loop (simple) */
                position -= loopLength;
            }
        }

        float sL, sR;
        if (sample16Bit) {
            sL = v->base16[position]  * scale16;
            sR = v->base16R[position] * scale16;
        } else {
            sL = v->base8[position]  * scale8;
            sR = v->base8R[position] * scale8;
        }

        dstL[s] += sL * currVolL;
        dstR[s] += sR * currVolR;

        /* Handle volume ramp */
        if (volRampLeft > 0) {
            currVolL += volDeltaL;
            currVolR += volDeltaR;
            volRampLeft--;
        }

        /* Advance sample position */
        frac += delta;
        position += (int32_t)(frac >> MIXER_FRAC_BITS);
        frac &= MIXER_FRAC_MASK;
    }

    /* Write back voice state */
    v->position       = position;
    v->positionFrac   = frac;
    v->fCurrVolumeL   = currVolL;
    v->fCurrVolumeR   = currVolR;
    v->volumeRampLength = volRampLeft;
}

// Correct signal flow: tracker channels -> stereo pairs -> master bus
static void doChannelMixing(int32_t bufferPosition, int32_t samplesToMix)
{
    /* Clear destination range in master buffer first */
    memset(&audio.fMixBufferL[bufferPosition], 0, sizeof(float) * samplesToMix);
    memset(&audio.fMixBufferR[bufferPosition], 0, sizeof(float) * samplesToMix);

    /* Clear stereo pair buffers */
    for (int pair = 0; pair < MAX_STEREO_PAIRS; pair++)
    {
        memset(&pairMixBufL[pair][bufferPosition], 0, sizeof(float) * samplesToMix);
        memset(&pairMixBufR[pair][bufferPosition], 0, sizeof(float) * samplesToMix);
    }

    /* Step 1: Mix individual tracker channels into stereo pairs */
    const bool canRenderSynth = (synthMixBufL != NULL && synthMixBufR != NULL && samplesToMix <= synthMixBufSize);
    bool synthNeedsMix[MAX_INST] = { false };
    uint8_t pairCounts[MAX_INST][MAX_STEREO_PAIRS] = { { 0 } };
    uint8_t totalCounts[MAX_INST] = { 0 };
    int firstChForInstr[MAX_INST];
    static uint8_t lastPairCounts[MAX_INST][MAX_STEREO_PAIRS];
    static uint8_t lastTotalCounts[MAX_INST];
    static int lastChForInstr[MAX_INST];
    static bool lastPairInit = false;
    static uint64_t lastLogTickTime = UINT64_MAX;
    if (!lastPairInit) {
        memset(lastPairCounts, 0, sizeof(lastPairCounts));
        memset(lastTotalCounts, 0, sizeof(lastTotalCounts));
        for (int i = 0; i < MAX_INST; i++)
            lastChForInstr[i] = -1;
        lastPairInit = true;
    }
    for (int i = 0; i < MAX_INST; i++)
        firstChForInstr[i] = -1;

    for (int32_t ch = 0; ch < song.numChannels; ch++)
    {
        int pairIdx = CHANNEL_TO_PAIR_IDX(ch);
        float *pairL = &pairMixBufL[pairIdx][bufferPosition];
        float *pairR = &pairMixBufR[pairIdx][bufferPosition];

        // 1.1. Mix sample (if active)
        voice_t *vc = &voice[ch];
        if (vc->active && (vc->base16 != NULL || vc->base8 != NULL))
        {
            mixStereoVoiceToBuffer(vc, bufferPosition, samplesToMix, pairL, pairR);
        }

        // 1.2. Mix synth (TF4, Dexed or V2 instrument)
        bool chHasSynth = false;
        int instrID = -1;
        if (channel[ch].instrPtr && !channel[ch].keyOff && channel[ch].noteNum > 0) {
            instrID = channel[ch].instrNum;
            chHasSynth = (ft2_unified_synth_get_active_engine(instrID) != SYNTH_TYPE_COUNT);
        }

        if (canRenderSynth && chHasSynth && instrID >= 1 && instrID <= MAX_INST)
        {
            int instrSlot = instrID - 1;
            synthNeedsMix[instrSlot] = true;
            if (pairCounts[instrSlot][pairIdx] < 255)
                pairCounts[instrSlot][pairIdx]++;
            if (totalCounts[instrSlot] < 255)
                totalCounts[instrSlot]++;
            if (firstChForInstr[instrSlot] < 0)
                firstChForInstr[instrSlot] = ch;
        }

        // 1.3. Fill scope buffer (prefer synth if present, else sample)
        int scLen = (samplesToMix < SYNTH_SCOPE_LEN) ? samplesToMix : SYNTH_SCOPE_LEN;
        if (vc->active && (vc->base16 != NULL || vc->base8 != NULL))
        {
            // Sample scope fallback (legacy logic)
        }
    }

    const bool logRouting = synthRoutingDebug && (audio.tickTime64 != lastLogTickTime);
    if (logRouting)
        lastLogTickTime = audio.tickTime64;

    if (canRenderSynth) {
        for (int instrSlot = 0; instrSlot < MAX_INST; instrSlot++) {
            int instrID = instrSlot + 1;
            bool mixNow = synthNeedsMix[instrSlot];

            if (!mixNow) {
                instr_t *ins = instr[instrID];
                if (ins) {
                    switch (ft2_unified_synth_get_active_engine(instrID)) {
                        case SYNTH_TYPE_V2:
                            mixNow = (ft2_v2_get_active_voice_count(instrID) > 0);
                            break;
                        case SYNTH_TYPE_DEXED:
                            mixNow = (ft2_dx_get_active_voice_count(instrID) > 0);
                            break;
                        case SYNTH_TYPE_TUNEFISH4:
                            mixNow = (ft2_synth_get_active_voice_count(instrID) > 0);
                            break;
                        case SYNTH_TYPE_OSTIRUS:
                            mixNow = ft2_ostirus_has_pending_audio(instrID);
                            break;
                        default:
                            break;
                    }
                }
            }

            if (!mixNow) continue;

            uint8_t totalCount = totalCounts[instrSlot];
            uint8_t *pairs = pairCounts[instrSlot];
            if (totalCount > 0) {
                memcpy(lastPairCounts[instrSlot], pairs, sizeof(lastPairCounts[instrSlot]));
                lastTotalCounts[instrSlot] = totalCount;
                if (firstChForInstr[instrSlot] >= 0)
                    lastChForInstr[instrSlot] = firstChForInstr[instrSlot];
            } else {
                totalCount = lastTotalCounts[instrSlot];
                pairs = lastPairCounts[instrSlot];
            }

            if (totalCount == 0)
                continue;

            if (logRouting) {
                printf("[SYNTH-ROUTE] instr=%d total=%u pairs:", instrID, totalCount);
                for (int pairIdx = 0; pairIdx < MAX_STEREO_PAIRS; pairIdx++) {
                    if (pairs[pairIdx] == 0)
                        continue;
                    printf(" %d:%u", pairIdx, pairs[pairIdx]);
                }
                printf("\n");
            }

            float *synthL = synthMixBufL;
            float *synthR = synthMixBufR;
            memset(synthL, 0, sizeof(float) * samplesToMix);
            memset(synthR, 0, sizeof(float) * samplesToMix);
            ft2_unified_synth_render_channel(instrID, synthL, synthR, samplesToMix, 0);

            for (int pairIdx = 0; pairIdx < MAX_STEREO_PAIRS; pairIdx++)
            {
                if (pairs[pairIdx] == 0)
                    continue;
                float *pairL = &pairMixBufL[pairIdx][bufferPosition];
                float *pairR = &pairMixBufR[pairIdx][bufferPosition];
                int routedChannels = 0, audibleChannels = 0;
                for (int ch = 0; ch < song.numChannels; ch++)
                {
                    if (channel[ch].instrNum != instrID || channel[ch].noteNum == 0 ||
                        CHANNEL_TO_PAIR_IDX(ch) != pairIdx) continue;
                    routedChannels++;
                    if (!channel[ch].channelOff) audibleChannels++;
                }
                const float muteScale = routedChannels ? (float)audibleChannels / routedChannels : 1.0f;
                const float scale = ((float)pairs[pairIdx] / (float)totalCount) * muteScale;
                for (int32_t s = 0; s < samplesToMix; s++) {
                    pairL[s] += synthL[s] * scale;
                    pairR[s] += synthR[s] * scale;
                }
            }

            int ch = lastChForInstr[instrSlot];
            if (ch >= 0 && ch < song.numChannels) {
                int scLen = (samplesToMix < SYNTH_SCOPE_LEN) ? samplesToMix : SYNTH_SCOPE_LEN;
                for (int32_t sIdx = 0; sIdx < scLen; sIdx++)
                {
                    float mono = 0.5f * (synthL[sIdx] + synthR[sIdx]);
                    int32_t v = (int32_t)lrintf(mono * 32767.0f);
                    if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
                    synthScopeBufL[ch][sIdx] = (int16_t)v;
                }
                if (scLen < SYNTH_SCOPE_LEN)
                    memset(&synthScopeBufL[ch][scLen], 0, (SYNTH_SCOPE_LEN - scLen) * sizeof(int16_t));

                volatile scope_t *sc = &scope[ch];
                if (!sc->active)
                {
                    sc->active        = true;
                    sc->sample16Bit   = true;
                    sc->base16        = synthScopeBufL[ch];
                    sc->leftEdgeTaps16= synthScopeBufL[ch];
                    sc->loopType      = LOOP_FORWARD;
                    sc->loopStart     = 0;
                    sc->loopLength    = scLen;
                    sc->sampleEnd     = scLen;
                    sc->position      = 0;
                    sc->positionFrac  = 0;
                }
                sc->delta  = ((uint64_t)scLen << SCOPE_FRAC_BITS) / SCOPE_HZ;
                /* The captured PCM already contains the synth's amplitude. Applying
                 * its peak as scope volume would scale it twice; in-phase stereo
                 * signals could then exceed the 36-pixel scope bounds. */
                sc->volume = SCOPE_HEIGHT * 4;
            }
        }
    }

    /* Step 2: Process stereo pairs through DSP chains and apply fader/pan */
    for (int pair = 0; pair < MAX_STEREO_PAIRS; pair++)
    {
        float *pairL = &pairMixBufL[pair][bufferPosition];
        float *pairR = &pairMixBufR[pair][bufferPosition];

        // 2.1. Run stereo pair DSP chain (4 slots)
        dspProcessChain(stereoMixerCh[pair].effects, pairL, pairR, samplesToMix);

        // 2.2. Apply stereo pair fader and pan
        float fader = stereoMixerCh[pair].fader;           /* 0.0 .. 2.0 */
        float pan = stereoMixerCh[pair].pan;               /* -1.0 .. +1.0 */
        float panL = (pan <= 0.0f) ? 1.0f : 1.0f - pan;
        float panR = (pan >= 0.0f) ? 1.0f : 1.0f + pan;

        /* Pair-level debug suppressed to keep logs concise. */

        for (int32_t s = 0; s < samplesToMix; s++)
        {
            pairL[s] *= fader * panL;
            pairR[s] *= fader * panR;
        }

        // 2.3. Soft limiter per stereo pair
        for (int32_t s = 0; s < samplesToMix; s++)
        {
            pairL[s] = tanhf(pairL[s]);
            pairR[s] = tanhf(pairR[s]);
        }

        // 2.4. Accumulate into master bus
        for (int32_t s = 0; s < samplesToMix; s++)
        {
            audio.fMixBufferL[bufferPosition + s] += pairL[s];
            audio.fMixBufferR[bufferPosition + s] += pairR[s];
        }
    }
}
/* Shared by device playback, WAV export and render-to-slot. */
static void processMasterBus(uint32_t frames)
{
    dspProcessChain(masterEffects, audio.fMixBufferL, audio.fMixBufferR, frames);
    const float gain = mixerMasterGain * fAudioNormalizeMul;
    for (uint32_t i = 0; i < frames; i++)
    {
        audio.fMixBufferL[i] *= gain;
        audio.fMixBufferR[i] *= gain;
    }
}

// used for song-to-WAV renderer
void mixReplayerTickToBuffer(uint32_t samplesToMix, void *stream, uint8_t bitDepth)
{
	doChannelMixing(0, samplesToMix);
	processMasterBus(samplesToMix);

	// normalize mix buffer and send to audio stream
	if (bitDepth == 16)
		sendSamples16BitStereo(stream, samplesToMix);
	else
		sendSamples32BitFloatStereo(stream, samplesToMix);
}

int32_t pattQueueReadSize(void)
{
	while (pattQueueClearing);

	if (pattSync.writePos > pattSync.readPos)
		return pattSync.writePos - pattSync.readPos;
	else if (pattSync.writePos < pattSync.readPos)
		return pattSync.writePos - pattSync.readPos + SYNC_QUEUE_LEN + 1;
	else
		return 0;
}

int32_t pattQueueWriteSize(void)
{
	int32_t size;

	if (pattSync.writePos > pattSync.readPos)
	{
		size = pattSync.readPos - pattSync.writePos + SYNC_QUEUE_LEN;
	}
	else if (pattSync.writePos < pattSync.readPos)
	{
		pattQueueClearing = true;

		/* Buffer is full, reset the read/write pos. This is actually really nasty since
		** read/write are two different threads, but because of timestamp validation it
		** shouldn't be that dangerous.
		** It will also create a small visual stutter while the buffer is getting filled,
		** though that is barely noticable on normal buffer sizes, and it takes a minute
		** or two at max BPM between each time (when queue size is default, 4095)
		*/
		pattSync.data[0].timestamp = 0;
		pattSync.readPos = 0;
		pattSync.writePos = 0;

		size = SYNC_QUEUE_LEN;

		pattQueueClearing = false;
	}
	else
	{
		size = SYNC_QUEUE_LEN;
	}

	return size;
}

bool pattQueuePush(pattSyncData_t t)
{
	if (!pattQueueWriteSize())
		return false;

	assert(pattSync.writePos <= SYNC_QUEUE_LEN);
	pattSync.data[pattSync.writePos] = t;
	pattSync.writePos = (pattSync.writePos + 1) & SYNC_QUEUE_LEN;

	return true;
}

bool pattQueuePop(void)
{
	if (!pattQueueReadSize())
		return false;

	pattSync.readPos = (pattSync.readPos + 1) & SYNC_QUEUE_LEN;
	assert(pattSync.readPos <= SYNC_QUEUE_LEN);

	return true;
}

pattSyncData_t *pattQueuePeek(void)
{
	if (!pattQueueReadSize())
		return NULL;

	assert(pattSync.readPos <= SYNC_QUEUE_LEN);
	return &pattSync.data[pattSync.readPos];
}

uint64_t getPattQueueTimestamp(void)
{
	if (!pattQueueReadSize())
		return 0;

	assert(pattSync.readPos <= SYNC_QUEUE_LEN);
	return pattSync.data[pattSync.readPos].timestamp;
}

int32_t chQueueReadSize(void)
{
	while (chQueueClearing);

	if (chSync.writePos > chSync.readPos)
		return chSync.writePos - chSync.readPos;
	else if (chSync.writePos < chSync.readPos)
		return chSync.writePos - chSync.readPos + SYNC_QUEUE_LEN + 1;
	else
		return 0;
}

int32_t chQueueWriteSize(void)
{
	int32_t size;

	if (chSync.writePos > chSync.readPos)
	{
		size = chSync.readPos - chSync.writePos + SYNC_QUEUE_LEN;
	}
	else if (chSync.writePos < chSync.readPos)
	{
		chQueueClearing = true;

		/* Buffer is full, reset the read/write pos. This is actually really nasty since
		** read/write are two different threads, but because of timestamp validation it
		** shouldn't be that dangerous.
		** It will also create a small visual stutter while the buffer is getting filled,
		** though that is barely noticable on normal buffer sizes, and it takes several
		** minutes between each time (when queue size is default, 16384)
		*/
		chSync.data[0].timestamp = 0;
		chSync.readPos = 0;
		chSync.writePos = 0;

		size = SYNC_QUEUE_LEN;

		chQueueClearing = false;
	}
	else
	{
		size = SYNC_QUEUE_LEN;
	}

	return size;
}

bool chQueuePush(chSyncData_t t)
{
	if (!chQueueWriteSize())
		return false;

	assert(chSync.writePos <= SYNC_QUEUE_LEN);
	chSync.data[chSync.writePos] = t;
	chSync.writePos = (chSync.writePos + 1) & SYNC_QUEUE_LEN;

	return true;
}

bool chQueuePop(void)
{
	if (!chQueueReadSize())
		return false;

	chSync.readPos = (chSync.readPos + 1) & SYNC_QUEUE_LEN;
	assert(chSync.readPos <= SYNC_QUEUE_LEN);

	return true;
}

chSyncData_t *chQueuePeek(void)
{
	if (!chQueueReadSize())
		return NULL;

	assert(chSync.readPos <= SYNC_QUEUE_LEN);
	return &chSync.data[chSync.readPos];
}

uint64_t getChQueueTimestamp(void)
{
	if (!chQueueReadSize())
		return 0;

	assert(chSync.readPos <= SYNC_QUEUE_LEN);
	return chSync.data[chSync.readPos].timestamp;
}

void lockAudio(void)
{
	if (audio.dev != 0)
		SDL_LockAudioDevice(audio.dev);

	audio.locked = true;
}

void unlockAudio(void)
{
	if (audio.dev != 0)
		SDL_UnlockAudioDevice(audio.dev);

	audio.locked = false;
}

void resetSyncQueues(void)
{
	pattSync.data[0].timestamp = 0;
	pattSync.readPos = 0;
	pattSync.writePos = 0;

	chSync.data[0].timestamp = 0;
	chSync.writePos = 0;
	chSync.readPos = 0;
}

void lockMixerCallback(void) // lock audio + clear voices/scopes (for short operations)
{
	if (!audio.locked)
		lockAudio();

	audio.resetSyncTickTimeFlag = true;

	stopVoices(); // VERY important! prevents potential crashes by purging pointers

	// scopes, mixer and replayer are guaranteed to not be active at this point

	resetSyncQueues();
}

void unlockMixerCallback(void)
{
	stopVoices(); // VERY important! prevents potential crashes by purging pointers

	if (audio.locked)
		unlockAudio();
}

void pauseAudio(void) // lock audio + clear voices/scopes + render silence (for long operations)
{
	if (audioPaused)
	{
		stopVoices(); // VERY important! prevents potential crashes by purging pointers
		return;
	}

	if (audio.dev > 0)
		SDL_PauseAudioDevice(audio.dev, true);

	audio.resetSyncTickTimeFlag = true;

	stopVoices(); // VERY important! prevents potential crashes by purging pointers

	// scopes, mixer and replayer are guaranteed to not be active at this point

	resetSyncQueues();
	audioPaused = true;
}

void resumeAudio(void) // unlock audio
{
	if (!audioPaused)
		return;

	if (audio.dev > 0)
		SDL_PauseAudioDevice(audio.dev, false);

	// Reset all DSP effect states to prevent timing issues after pause
	for (int i = 0; i < MAX_STEREO_PAIRS; i++)
	{
		for (int slot = 0; slot < DSP_MAX_SLOTS; slot++)
		{
			dspResetEffectState(&stereoMixerCh[i].effects[slot]);
		}
	}
	for (int slot = 0; slot < DSP_MAX_SLOTS; slot++)
	{
		dspResetEffectState(&masterEffects[slot]);
	}

	audioPaused = false;
}

static void fillVisualsSyncBuffer(void)
{
	pattSyncData_t pattSyncData;
	chSyncData_t chSyncData;

	if (audio.resetSyncTickTimeFlag)
	{
		audio.resetSyncTickTimeFlag = false;

		audio.tickTime64 = SDL_GetPerformanceCounter() + audio.audLatencyPerfValInt;
		audio.tickTime64Frac = audio.audLatencyPerfValFrac;
	}

	if (songPlaying)
	{
		// push pattern variables to sync queue
		pattSyncData.tick = song.curReplayerTick;
		pattSyncData.row = song.curReplayerRow;
		pattSyncData.pattNum = song.curReplayerPattNum;
		pattSyncData.songPos = song.curReplayerSongPos;
		pattSyncData.BPM = (uint8_t)song.BPM;
		pattSyncData.speed = (uint8_t)song.speed;
		pattSyncData.globalVolume = (uint8_t)song.globalVolume;
		pattSyncData.timestamp = audio.tickTime64;
		pattQueuePush(pattSyncData);
	}

	// push channel variables to sync queue

	syncedChannel_t *c = chSyncData.channels;
	channel_t *s = channel;
	voice_t *v = voice;

	for (int32_t i = 0; i < song.numChannels; i++, c++, s++, v++)
	{
		c->scopeVolume = v->scopeVolume;
		c->period = s->finalPeriod;
		c->instrNum = s->instrNum;
		c->smpNum = s->smpNum;
		c->status = s->tmpStatus;
		c->smpStartPos = s->smpStartPos;

		c->pianoNoteNum = 255; // no piano key
		if (songPlaying && ui.instEditorShown && (c->status & IS_Period) && !s->keyOff)
		{
			const int32_t note = getPianoKey(s->finalPeriod, s->finetune, s->relativeNote);
			if (note >= 0 && note <= 95)
				c->pianoNoteNum = (uint8_t)note;
		}
	}

	chSyncData.timestamp = audio.tickTime64;
	chQueuePush(chSyncData);

	audio.tickTime64 += tickTimeLenInt;

	audio.tickTime64Frac += tickTimeLenFrac;
	if (audio.tickTime64Frac >= TICK_TIME_FRAC_SCALE)
	{
		audio.tickTime64Frac &= TICK_TIME_FRAC_MASK;
		audio.tickTime64++;
	}
}

static void SDLCALL audioCallback(void *userdata, Uint8 *stream, int len)
{
    uint64_t cpuStart = SDL_GetPerformanceCounter();
	if (editor.wavIsRendering)
		return;

	len >>= smpShiftValue; // bytes -> samples
	if (len <= 0)
		return;

	int totalSamples = len; // store total frames for CPU load calc

			// Tunefish4 synth rendering will be integrated in channel mixing

	int32_t bufferPosition = 0;

	uint32_t samplesLeft = len;
	while (samplesLeft > 0)
	{
		if (audio.tickSampleCounter == 0) // new replayer tick
		{
			replayerBusy = true;
			if (!musicPaused) // important, don't remove this check! (also used for safety)
			{
				if (audio.volumeRampingFlag)
					resetRampVolumes();

				tickReplayer();
				updateVoices();
				fillVisualsSyncBuffer();
			}
			replayerBusy = false;

			// Use the more precise timing values
			audio.tickSampleCounter = audio.samplesPerTickInt;
			audio.tickSampleCounterFrac = audio.samplesPerTickFrac;
			
			// Handle overflow with proper bounds checking
			if (audio.tickSampleCounterFrac >= BPM_FRAC_SCALE)
			{
				audio.tickSampleCounterFrac &= BPM_FRAC_MASK;
				audio.tickSampleCounter++;
			}
		}

		uint32_t samplesToMix = samplesLeft;
		if (samplesToMix > audio.tickSampleCounter)
			samplesToMix = audio.tickSampleCounter;

		// Ensure we don't exceed buffer bounds
		if (bufferPosition + samplesToMix > totalSamples) {
			samplesToMix = totalSamples - bufferPosition;
		}

		doChannelMixing(bufferPosition, samplesToMix);

		// Process mixer updates for channels that need it
		if (mixerUpdateMask != 0) {
			for (int32_t ch = 0; ch < MAX_CHANNELS; ch++) {
				if (mixerUpdateMask & (1u << ch)) {
					// Update channel mixer state from cached data
					if (ch < MAX_STEREO_PAIRS) {
						mixerCh[ch].fader = stereoMixerCh[ch].fader;
						mixerCh[ch].pan = stereoMixerCh[ch].pan;
						memcpy(mixerCh[ch].effects, stereoMixerCh[ch].effects, sizeof(mixerCh[ch].effects));
					}
					mixerUpdateMask &= ~(1u << ch); // Clear the bit
				}
			}
			// If all channels are updated, clear the mask completely
			if (mixerUpdateMask == 0) {
				mixerUpdateMask = 0;
			}
		}

		bufferPosition += samplesToMix;

		audio.tickSampleCounter -= samplesToMix;
		samplesLeft -= samplesToMix;
	}

	processMasterBus((uint32_t)len);

	if (config.specialFlags & BITDEPTH_16)
		sendSamples16BitStereo(stream, len);
	else
		sendSamples32BitFloatStereo(stream, len);

    // CPU load calculation
    uint64_t cpuEnd = SDL_GetPerformanceCounter();
    double elapsedSec = (double)(cpuEnd - cpuStart) / editor.dPerfFreq;
    double bufferSec = (double)totalSamples / audio.freq;
    if (bufferSec > 0.0)
    {
        float load = (float)(elapsedSec / bufferSec);
        if (load < 0.0f) load = 0.0f;
        if (load > 1.0f) load = 1.0f;
        g_audioCPULoad = load;
    }

	(void)userdata;
}

static bool setupAudioBuffers(void)
{
	const int32_t maxAudioFreq = MAX(MAX_AUDIO_FREQ, MAX_WAV_RENDER_FREQ);
	// Calculate samples per tick dynamically for current sample rate to avoid precision issues
	int32_t maxSamplesPerTick = (int32_t)ceil(maxAudioFreq / (MIN_BPM / 2.5)) + 1;

	// Allocate additional buffer space for lower sample rates to handle rounding errors
	if (maxAudioFreq <= 48000) {
		maxSamplesPerTick += 8; // Extra margin for 48kHz and below
	} else if (maxAudioFreq <= 44100) {
		maxSamplesPerTick += 12; // Extra margin for 44.1kHz
	}

	audio.fMixBufferL = (float *)calloc(maxSamplesPerTick, sizeof (float));
	audio.fMixBufferR = (float *)calloc(maxSamplesPerTick, sizeof (float));

	if (audio.fMixBufferL == NULL || audio.fMixBufferR == NULL)
		return false;

	/* Clear/init master effects chain */
	memset(masterEffects, 0, sizeof(masterEffects));

	/* Allocate per-channel buffers */
	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		chMixBufL[i] = (float *)calloc(maxSamplesPerTick, sizeof(float));
		chMixBufR[i] = (float *)calloc(maxSamplesPerTick, sizeof(float));
		if (chMixBufL[i] == NULL || chMixBufR[i] == NULL)
			return false;
	}

    /* Allocate per-stereo-pair buffers */
    for (int i = 0; i < MAX_STEREO_PAIRS; i++)
    {
        pairMixBufL[i] = (float *)calloc(maxSamplesPerTick, sizeof(float));
        pairMixBufR[i] = (float *)calloc(maxSamplesPerTick, sizeof(float));
        if (pairMixBufL[i] == NULL || pairMixBufR[i] == NULL)
            return false;
    }

    synthMixBufSize = (uint32_t)maxSamplesPerTick;
    synthMixBufL = (float *)calloc(maxSamplesPerTick, sizeof(float));
    synthMixBufR = (float *)calloc(maxSamplesPerTick, sizeof(float));
    if (synthMixBufL == NULL || synthMixBufR == NULL)
        return false;

 	return true;
}

static void freeAudioBuffers(void)
{
	if (audio.fMixBufferL != NULL)
	{
		free(audio.fMixBufferL);
		audio.fMixBufferL = NULL;
	}

	if (audio.fMixBufferR != NULL)
	{
		free(audio.fMixBufferR);
		audio.fMixBufferR = NULL;
	}



	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		free(chMixBufL[i]);
		free(chMixBufR[i]);
		chMixBufL[i] = chMixBufR[i] = NULL;
	}

    for (int i = 0; i < MAX_STEREO_PAIRS; i++)
    {
        free(pairMixBufL[i]);
        free(pairMixBufR[i]);
        pairMixBufL[i] = pairMixBufR[i] = NULL;
    }

    if (synthMixBufL != NULL)
    {
        free(synthMixBufL);
        synthMixBufL = NULL;
    }

    if (synthMixBufR != NULL)
    {
        free(synthMixBufR);
        synthMixBufR = NULL;
    }

    synthMixBufSize = 0;
}

static void calcAudioLatencyVars(int32_t audioBufferSize, int32_t audioFreq)
{
	double dInt;

	if (audioFreq == 0)
		return;

	const double dAudioLatencySecs = audioBufferSize / (double)audioFreq;

	double dFrac = modf(dAudioLatencySecs * editor.dPerfFreq, &dInt);

	audio.audLatencyPerfValInt = (uint32_t)dInt;
	audio.audLatencyPerfValFrac = (uint64_t)((dFrac * TICK_TIME_FRAC_SCALE) + 0.5); // rounded
}

static void setLastWorkingAudioDevName(void)
{
	if (audio.lastWorkingAudioDeviceName != NULL)
	{
		free(audio.lastWorkingAudioDeviceName);
		audio.lastWorkingAudioDeviceName = NULL;
	}

	if (audio.currOutputDevice != NULL)
		audio.lastWorkingAudioDeviceName = strdup(audio.currOutputDevice);
}

bool setupAudio(bool showErrorMsg)
{
	SDL_AudioSpec want, have;

	closeAudio();

	if (config.audioFreq < MIN_AUDIO_FREQ || config.audioFreq > MAX_AUDIO_FREQ)
		config.audioFreq = DEFAULT_AUDIO_FREQ;

	// get audio buffer size from config special flags

	uint16_t configAudioBufSize = 1024;
	if (config.specialFlags & BUFFSIZE_512)
		configAudioBufSize = 512;
	else if (config.specialFlags & BUFFSIZE_2048)
		configAudioBufSize = 2048;

	audio.wantFreq = config.audioFreq;
	audio.wantSamples = configAudioBufSize;

	// set up audio device
	memset(&want, 0, sizeof (want));
	want.freq = config.audioFreq;
	want.format = (config.specialFlags & BITDEPTH_32) ? AUDIO_F32 : AUDIO_S16;
	want.channels = 2;
	want.callback = audioCallback;
	want.samples  = configAudioBufSize;

	char *device = audio.currOutputDevice;
	if (device != NULL && strcmp(device, DEFAULT_AUDIO_DEV_STR) == 0)
		device = NULL; // force default device

	audio.dev = SDL_OpenAudioDevice(device, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (audio.dev == 0)
	{
		audio.dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
		if (audio.currOutputDevice != NULL)
		{
			free(audio.currOutputDevice);
			audio.currOutputDevice = NULL;
		}
		audio.currOutputDevice = strdup(DEFAULT_AUDIO_DEV_STR);

		if (audio.dev == 0)
		{
			if (showErrorMsg)
				showErrorMsgBox("Couldn't open audio device:\n\"%s\"\n\nDo you have an audio device enabled and plugged in?", SDL_GetError());

			return false;
		}
	}

	// test if the received audio format is compatible
	if (have.format != AUDIO_S16 && have.format != AUDIO_F32)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program only supports 16-bit or 32-bit float audio streams. Sorry!");

		closeAudio();
		return false;
	}

	// test if the received audio stream is compatible

	if (have.channels != 2)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program only supports stereo audio streams. Sorry!");

		closeAudio();
		return false;
	}

	/*
	if (have.freq != 44100 && have.freq != 48000 && have.freq != 96000)
	{
		if (showErrorMsg)
			showErrorMsgBox("Couldn't open audio device:\nThis program doesn't support an audio output rate of %dHz. Sorry!", have.freq);

		closeAudio();
		return false;
	}
	*/

	if (!setupAudioBuffers())
	{
		if (showErrorMsg)
			showErrorMsgBox("Not enough memory!");

		closeAudio();
		return false;
	}

	// set new bit depth flag

	int8_t newBitDepth = 16;
	config.specialFlags &= ~BITDEPTH_32;
	config.specialFlags |=  BITDEPTH_16;

	if (have.format == AUDIO_F32)
	{
		newBitDepth = 24;
		config.specialFlags &= ~BITDEPTH_16;
		config.specialFlags |=  BITDEPTH_32;
	}

	audio.haveFreq = have.freq;
	audio.haveSamples = have.samples;
	config.audioFreq = audio.freq = have.freq;

	calcAudioLatencyVars(have.samples, have.freq);
	smpShiftValue = (newBitDepth == 16) ? 2 : 3;

	// make a copy of the new known working audio settings

	audio.lastWorkingAudioFreq = config.audioFreq;
	audio.lastWorkingAudioBits = config.specialFlags & (BITDEPTH_16 + BITDEPTH_32 + BUFFSIZE_512 + BUFFSIZE_1024 + BUFFSIZE_2048);
	setLastWorkingAudioDevName();

	// update config audio radio buttons if we're on that screen at the moment
	if (ui.configScreenShown && editor.currConfigScreen == CONFIG_SCREEN_AUDIO)
		showConfigScreen();

	updateWavRendererSettings();
	setAudioAmp(config.boostLevel, config.masterVol, !!(config.specialFlags & BITDEPTH_32));

	// don't call stopVoices() in this routine
	for (int32_t i = 0; i < MAX_CHANNELS; i++)
		stopVoice(i);

	stopAllScopes();

	// zero tick sample counter so that it will instantly initiate a tick
	audio.tickSampleCounterFrac  = audio.tickSampleCounter = 0;

	calcReplayerVars(audio.freq);

	if (song.BPM == 0)
		song.BPM = 125;

	setMixerBPM(song.BPM); // this is important

	audio.resetSyncTickTimeFlag = true;

	setWavRenderFrequency(audio.freq);
	setWavRenderBitDepth((config.specialFlags & BITDEPTH_32) ? 32 : 16);

	// Initialize the unified synth system with the current sample rate
	ft2_unified_synth_init(audio.freq);

	// Initialize DSP effects with current sample rate
	mixerInitDSPEffects(audio.freq);

	return true;
}

void closeAudio(void)
{
	if (audio.dev > 0)
	{
		SDL_PauseAudioDevice(audio.dev, true);
		SDL_CloseAudioDevice(audio.dev);
		audio.dev = 0;
	}

	freeAudioBuffers();

	// Shutdown unified synth system
	ft2_unified_synth_shutdown();
	mixerShutdownDSPEffects();
}

void audioRequestMixerUpdate(int32_t ch)
{
	if (ch < 0 || ch >= MAX_CHANNELS)
		return;
	mixerUpdateMask |= (1u << ch);
}

uint32_t audioGetOutputMonitor(float *dstMono, uint32_t maxSamples)
{
    uint32_t count;

    if (dstMono == NULL || maxSamples == 0)
        return 0;

    lockAudio();

    count = outputMonitorSamples;
    if (count > maxSamples)
        count = maxSamples;

    if (count > 0)
        memcpy(dstMono, outputMonitorMono, count * sizeof (float));

    unlockAudio();
    return count;
}

uint32_t audioGetOutputMonitorGeneration(void)
{
    return g_audioOutputMonitorGeneration;
}

/* Dummy to keep ABI if any */
void audioMixerUpdateChannel(int32_t ch) { audioRequestMixerUpdate(ch);} // backward compat

/* These helpers normalize and interleave mixed floats into the output stream, clearing mix buffers afterwards */
static void sendSamples16BitStereo(void *stream, uint32_t sampleBlockLength)
{
    // Final stage limiting: transparent soft clip (tanh) on master output
    for (uint32_t i = 0; i < sampleBlockLength; i++) {
        audio.fMixBufferL[i] = tanhf(audio.fMixBufferL[i]);
        audio.fMixBufferR[i] = tanhf(audio.fMixBufferR[i]);
    }
    updateOutputMonitor(sampleBlockLength);
    int16_t *out = (int16_t *)stream;
    for (uint32_t i = 0; i < sampleBlockLength; i++)
    {
        float l = audio.fMixBufferL[i];
        float r = audio.fMixBufferR[i];
        out[(i << 1) + 0] = (int16_t)CLAMP((int32_t)lrintf(l * 32767.0f), -32768, 32767);
        out[(i << 1) + 1] = (int16_t)CLAMP((int32_t)lrintf(r * 32767.0f), -32768, 32767);
    }
}

static void sendSamples32BitFloatStereo(void *stream, uint32_t sampleBlockLength)
{
    // Final stage limiting: transparent soft clip (tanh) on master output
    for (uint32_t i = 0; i < sampleBlockLength; i++) {
        audio.fMixBufferL[i] = tanhf(audio.fMixBufferL[i]);
        audio.fMixBufferR[i] = tanhf(audio.fMixBufferR[i]);
    }
    updateOutputMonitor(sampleBlockLength);
    float *out = (float *)stream;
    for (uint32_t i = 0; i < sampleBlockLength; i++)
    {
        out[(i << 1) + 0] = audio.fMixBufferL[i];
        out[(i << 1) + 1] = audio.fMixBufferR[i];
    }
}

#ifdef FT2_STABILITY_TESTS
#include "../tests/audio_tests.inc"
#endif
