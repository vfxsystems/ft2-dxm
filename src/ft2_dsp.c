#include "ft2_dsp.h"
#include "ft2_replayer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------- */
/*                            Forward declarations                            */
/* -------------------------------------------------------------------------- */
static void dspProcessGainer(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessDelay(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessCompressor(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessLimiter(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessDrive(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessReverb(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessChorus(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessFlanger(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessPhaser(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessAmpSim(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessEQ5(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessFilter(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessCombFilter(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);
static void dspProcessBitcrusher(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);

/* -------------------------------------------------------------------------- */
/*                              Init / Destroy                                */
/* -------------------------------------------------------------------------- */

typedef struct
{
    float *bufL,*bufR;
    uint32_t bufSize;
    uint32_t writePos;
    float filterStoreL;  // feedback tone filter state for delay/reverb
    float filterStoreR;
    float hpStoreL;
    float hpStoreR;
    float wowPhase;
    float flutterPhase;
} delayState_t;

typedef struct
{
    float env;  // envelope follower state
} compressorState_t;

typedef struct
{
    float gain;  // gain reduction state
} limiterState_t;

typedef struct
{
    float zL, zR;  // tone filter state
} driveState_t;

typedef struct
{
    float zL, zR;  // tone filter state
} ampSimState_t;

typedef struct { float *bufL,*bufR; uint32_t bufSize,writePos; float lfoPhase,lfoInc; } chorusState_t;
typedef struct { float *bufL, *bufR; uint32_t bufSize, writePos; float lfoPhase, lfoInc; float feedback; } flangerState_t;

#define PHASER_STAGES 4
#define PHASER_STATE_SLOTS (PHASER_STAGES * 2)

typedef struct { float x1[PHASER_STATE_SLOTS], y1[PHASER_STATE_SLOTS], lfoPhase, lfoInc; float lastOutL, lastOutR; } phaserState_t;

// 5-band Parametric EQ state (Direct Form II transposed)
typedef struct {
    float b0[5], b1[5], b2[5], a1[5], a2[5];
    float s1L[5], s2L[5], s1R[5], s2R[5];
} eq5State_t;

// Filter state: one biquad lowpass
typedef struct { float b0,b1,b2,a1,a2; float s1L,s2L,s1R,s2R; } filterState_t;

// Comb filter state: reuse delay state struct
typedef delayState_t combState_t;

// Bitcrusher state
typedef struct { uint32_t counter, step; float heldL, heldR; } bitcrusherState_t;

// Freeverb-style reverb
#define NUM_REVERB_COMBS 8
#define NUM_REVERB_ALLPASS 4
#define NUM_REVERB_IRS 3
#define REVERB_IR_TAPS 96
static const int revCombBase[NUM_REVERB_COMBS] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static const int revAllpassBase[NUM_REVERB_ALLPASS] = {556, 441, 341, 225};
static const int revStereoSpread = 23;

typedef struct { float *buf; uint32_t bufSize; uint32_t writePos; float filterStore; } revCombState_t;
typedef struct { float *buf; uint32_t bufSize; uint32_t writePos; } revAllpassState_t;
typedef struct {
    revCombState_t combL[NUM_REVERB_COMBS];
    revCombState_t combR[NUM_REVERB_COMBS];
    revAllpassState_t apL[NUM_REVERB_ALLPASS];
    revAllpassState_t apR[NUM_REVERB_ALLPASS];
    float *convBufL;
    float *convBufR;
    uint32_t convSize;
    uint32_t convPos;
    float irData[NUM_REVERB_IRS][REVERB_IR_TAPS];
} multiReverbState_t;

// after includes and before typedef
static inline float db2lin(float db){return powf(10.0f, db/20.0f);}
static inline float lerpf(float a, float b, float t){return a + (b - a) * t;}
static inline float softClipf(float x){return x / (1.0f + fabsf(x));}

static void initReverbIRs(multiReverbState_t *st, uint32_t sampleRate)
{
    static const float irDecay[NUM_REVERB_IRS] = {0.018f, 0.035f, 0.07f};
    static const float irFreq1[NUM_REVERB_IRS] = {2400.0f, 1400.0f, 900.0f};
    static const float irFreq2[NUM_REVERB_IRS] = {1200.0f, 700.0f, 450.0f};

    for (int ir = 0; ir < NUM_REVERB_IRS; ir++) {
        float maxAbs = 0.0f;
        for (int i = 0; i < REVERB_IR_TAPS; i++) {
            float t = (float)i / (float)sampleRate;
            float env = expf(-t / irDecay[ir]);
            float s = env * (0.6f * cosf(2.0f * M_PI * irFreq1[ir] * t) +
                             0.4f * cosf(2.0f * M_PI * irFreq2[ir] * t));
            if (i == 0) s += 1.0f;
            if (i == 3) s += 0.35f;
            if (i == 7) s -= 0.25f;
            st->irData[ir][i] = s;
            float absS = fabsf(s);
            if (absS > maxAbs) maxAbs = absS;
        }
        if (maxAbs > 0.0f) {
            float norm = 0.5f / maxAbs;
            for (int i = 0; i < REVERB_IR_TAPS; i++) {
                st->irData[ir][i] *= norm;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                             Public API                                     */
/* -------------------------------------------------------------------------- */

bool dspInitEffect(dspEffectInstance_t *inst, dspEffectType_t type, uint32_t sampleRate)
{
    if (inst == NULL)
        return false;

    memset(inst, 0, sizeof (dspEffectInstance_t));

    inst->type = type;
    inst->enabled = true;
    inst->sampleRate = sampleRate;
    inst->state = NULL; /* not used for now */

    switch (type)
    {
        case DSP_TYPE_GAINER:
        {
            int n; const dspParamInfo_t *pi = dspGetParamInfo(DSP_TYPE_GAINER,&n);
            if (pi) inst->params.gainer.gain = pi[0].def;
            inst->process = dspProcessGainer;
            break;
        }
        case DSP_TYPE_DELAY:
        {
            int n; dspGetParamInfo(DSP_TYPE_DELAY,&n); // ensure table exists
            inst->params.delay.timeMs = 500.0f;
            inst->params.delay.feedback = 0.3f;
            inst->params.delay.tone = 0.5f;
            inst->params.delay.mix = 0.5f;
            inst->params.delay.diffusionMs = 20.0f; // 20 ms diffusion
            inst->params.delay.diffusionMix = 0.3f;
            inst->params.delay.pingPong = 0.0f;    // off by default
            inst->params.delay.character = 0.0f;   // clean
            inst->params.delay.sync = 0.0f;
            inst->params.delay.width = 1.2f;

            delayState_t *st = (delayState_t *)calloc(1,sizeof(delayState_t));
            if (!st) {
                inst->enabled = false; 
                break;
            }
            uint32_t bufSize = inst->sampleRate * 4; /* 4 seconds max */
            st->bufSize = bufSize;
            st->bufL = (float *)calloc(bufSize,sizeof(float));
            st->bufR = (float *)calloc(bufSize,sizeof(float));
            if (!st->bufL || !st->bufR)
            {
                if (st->bufL) free(st->bufL);
                if (st->bufR) free(st->bufR);
                free(st);
                inst->enabled = false; 
                break;
            }
            st->writePos = 0;
            st->wowPhase = 0.0f;
            st->flutterPhase = 0.0f;
            inst->state = st;
            inst->process = dspProcessDelay;
            break;
        }
        case DSP_TYPE_COMPRESSOR:
        {
            int n; dspGetParamInfo(DSP_TYPE_COMPRESSOR,&n); // ensure table exists
            inst->params.compressor.thresholdDb = -18.0f;
            inst->params.compressor.ratio = 4.0f;
            inst->params.compressor.attackMs = 10.0f;
            inst->params.compressor.releaseMs = 200.0f;
            inst->params.compressor.makeupDb = 0.0f;
            compressorState_t *st = (compressorState_t *)calloc(1, sizeof(compressorState_t));
            if (!st) { inst->enabled = false; break; }
            st->env = 0.0f;
            inst->state = st;
            inst->process = dspProcessCompressor;
            break;
        }
        case DSP_TYPE_LIMITER:
        {
            int n; dspGetParamInfo(DSP_TYPE_LIMITER,&n); // ensure table exists
            inst->params.limiter.thresholdDb = -1.0f;
            inst->params.limiter.releaseMs = 100.0f;
            limiterState_t *st = (limiterState_t *)calloc(1, sizeof(limiterState_t));
            if (!st) { inst->enabled = false; break; }
            st->gain = 1.0f;
            inst->state = st;
            inst->process = dspProcessLimiter;
            break;
        }
        case DSP_TYPE_DRIVE:
        {
            int n; dspGetParamInfo(DSP_TYPE_DRIVE,&n); // ensure table exists
            inst->params.drive.gain = 5.0f;
            inst->params.drive.tone = 0.5f;
            inst->params.drive.mix = 0.7f;
            driveState_t *st = (driveState_t *)calloc(1, sizeof(driveState_t));
            if (!st) { inst->enabled = false; break; }
            st->zL = st->zR = 0.0f;
            inst->state = st;
            inst->process = dspProcessDrive;
            break;
        }
        case DSP_TYPE_REVERB:
        {
            int n; dspGetParamInfo(DSP_TYPE_REVERB, &n);
            // default parameters
            inst->params.reverb.sizeMs   = 600.0f;
            inst->params.reverb.feedback = 0.5f;
            inst->params.reverb.damp     = 0.5f;
            inst->params.reverb.mix      = 0.5f;
            inst->params.reverb.character = 1.0f;
            inst->params.reverb.convMix   = 0.0f;
            inst->params.reverb.irIndex   = 0.0f;
            // allocate multi-comb state
            multiReverbState_t *st = calloc(1, sizeof(multiReverbState_t));
            if (!st) { inst->enabled = false; break; }
            
            // Initialize all delay lines
            bool allocationFailed = false;
            float srScale = (float)inst->sampleRate / 44100.0f;
            float maxScale = 2000.0f / 600.0f;
            for (int c = 0; c < NUM_REVERB_COMBS; c++) {
                uint32_t lenL = (uint32_t)(revCombBase[c] * srScale * maxScale) + 1;
                uint32_t lenR = (uint32_t)((revCombBase[c] + revStereoSpread) * srScale * maxScale) + 1;
                st->combL[c].bufSize = lenL;
                st->combR[c].bufSize = lenR;
                st->combL[c].buf = calloc(lenL, sizeof(float));
                st->combR[c].buf = calloc(lenR, sizeof(float));
                st->combL[c].writePos = 0;
                st->combR[c].writePos = 0;
                st->combL[c].filterStore = 0.0f;
                st->combR[c].filterStore = 0.0f;
                if (!st->combL[c].buf || !st->combR[c].buf) {
                    allocationFailed = true;
                    break;
                }
            }

            if (!allocationFailed) {
                for (int a = 0; a < NUM_REVERB_ALLPASS; a++) {
                    uint32_t lenL = (uint32_t)(revAllpassBase[a] * srScale * maxScale) + 1;
                    uint32_t lenR = (uint32_t)((revAllpassBase[a] + revStereoSpread) * srScale * maxScale) + 1;
                    st->apL[a].bufSize = lenL;
                    st->apR[a].bufSize = lenR;
                    st->apL[a].buf = calloc(lenL, sizeof(float));
                    st->apR[a].buf = calloc(lenR, sizeof(float));
                    st->apL[a].writePos = 0;
                    st->apR[a].writePos = 0;
                    if (!st->apL[a].buf || !st->apR[a].buf) {
                        allocationFailed = true;
                        break;
                    }
                }
            }
            
            if (allocationFailed) {
                // Clean up any allocated buffers
                for (int c = 0; c < NUM_REVERB_COMBS; c++) {
                    if (st->combL[c].buf) free(st->combL[c].buf);
                    if (st->combR[c].buf) free(st->combR[c].buf);
                }
                for (int a = 0; a < NUM_REVERB_ALLPASS; a++) {
                    if (st->apL[a].buf) free(st->apL[a].buf);
                    if (st->apR[a].buf) free(st->apR[a].buf);
                }
                free(st);
                inst->enabled = false;
                break;
            }

            st->convSize = REVERB_IR_TAPS;
            st->convBufL = calloc(st->convSize, sizeof(float));
            st->convBufR = calloc(st->convSize, sizeof(float));
            st->convPos = 0;
            initReverbIRs(st, inst->sampleRate);
            if (!st->convBufL || !st->convBufR) {
                if (st->convBufL) free(st->convBufL);
                if (st->convBufR) free(st->convBufR);
                for (int c = 0; c < NUM_REVERB_COMBS; c++) {
                    if (st->combL[c].buf) free(st->combL[c].buf);
                    if (st->combR[c].buf) free(st->combR[c].buf);
                }
                for (int a = 0; a < NUM_REVERB_ALLPASS; a++) {
                    if (st->apL[a].buf) free(st->apL[a].buf);
                    if (st->apR[a].buf) free(st->apR[a].buf);
                }
                free(st);
                inst->enabled = false;
                break;
            }
            
            inst->state   = st;
            inst->process = dspProcessReverb;
            break;
        }
        case DSP_TYPE_CHORUS:
        {
            int n; dspGetParamInfo(DSP_TYPE_CHORUS,&n);
            inst->params.chorus.depthMs = 5.0f;
            inst->params.chorus.rateHz = 1.0f;
            inst->params.chorus.feedback = 0.0f;
            inst->params.chorus.mix = 0.5f;
            inst->params.chorus.stereoOffset = 0.0f;
            chorusState_t *st = (chorusState_t *)calloc(1,sizeof(chorusState_t));
            if (!st) { inst->enabled = false; break; }
            uint32_t bufSize = inst->sampleRate * 2;
            st->bufSize = bufSize;
            st->bufL = (float *)calloc(bufSize,sizeof(float));
            st->bufR = (float *)calloc(bufSize,sizeof(float));
            st->writePos = 0;
            st->lfoPhase = 0.0f;
            st->lfoInc = 2.0f * 3.14159265f * inst->params.chorus.rateHz / inst->sampleRate;
            if (!st->bufL || !st->bufR) { 
                if (st->bufL) free(st->bufL); 
                if (st->bufR) free(st->bufR); 
                free(st); 
                inst->enabled = false; 
                break; 
            }
            inst->state = st;
            inst->process = dspProcessChorus;
            break;
        }
        case DSP_TYPE_FLANGER:
        {
            int n; dspGetParamInfo(DSP_TYPE_FLANGER,&n);
            inst->params.flanger.depthMs = 2.0f;
            inst->params.flanger.rateHz = 1.0f;
            inst->params.flanger.feedback = 0.5f;
            inst->params.flanger.mix = 0.5f;
            flangerState_t *st = (flangerState_t *)calloc(1,sizeof(flangerState_t));
            if (!st) { inst->enabled = false; break; }
            uint32_t bufSize = inst->sampleRate * 2;
            st->bufSize = bufSize;
            st->bufL = (float *)calloc(bufSize,sizeof(float));
            st->bufR = (float *)calloc(bufSize,sizeof(float));
            st->writePos = 0;
            st->lfoPhase = 0.0f;
            st->lfoInc = 2.0f * 3.14159265f * inst->params.flanger.rateHz / inst->sampleRate;
            st->feedback = inst->params.flanger.feedback;
            if (!st->bufL || !st->bufR) { 
                if (st->bufL) free(st->bufL); 
                if (st->bufR) free(st->bufR); 
                free(st); 
                inst->enabled = false; 
                break; 
            }
            inst->state = st;
            inst->process = dspProcessFlanger;
            break;
        }
        case DSP_TYPE_PHASER:
        {
            int n; dspGetParamInfo(DSP_TYPE_PHASER, &n);
            inst->params.phaser.depth        = 0.5f;
            inst->params.phaser.rateHz       = 1.0f;
            inst->params.phaser.mix          = 0.5f;
            inst->params.phaser.feedback     = 0.0f;
            inst->params.phaser.stereoOffset = 0.0f;
            phaserState_t *st = (phaserState_t *)calloc(1, sizeof(phaserState_t));
            if (!st)
            {
                inst->enabled = false;
                break;
            }
            st->lfoPhase   = 0.0f;
            st->lfoInc     = 2.0f * M_PI * inst->params.phaser.rateHz / inst->sampleRate;
            st->lastOutL   = 0.0f;
            st->lastOutR   = 0.0f;
            inst->state   = st;
            inst->process = dspProcessPhaser;
            break;
        }
        case DSP_TYPE_AMP_SIM:
        {
            int n; dspGetParamInfo(DSP_TYPE_AMP_SIM, &n);
            inst->params.ampSim.drive = 1.0f;
            inst->params.ampSim.tone = 0.5f;
            inst->params.ampSim.mix = 0.5f;
            ampSimState_t *st = (ampSimState_t *)calloc(1, sizeof(ampSimState_t));
            if (!st) { inst->enabled = false; break; }
            st->zL = st->zR = 0.0f;
            inst->state = st;
            inst->process = dspProcessAmpSim;
            break;
        }
        case DSP_TYPE_EQ_5BAND:
        {
            int n; dspGetParamInfo(DSP_TYPE_EQ_5BAND, &n);
            // initialize 5-band gains to 0 dB
            for (int i = 0; i < 5; i++) inst->params.eq5.gains[i] = 0.0f;
            // allocate EQ state
            eq5State_t *st = calloc(1, sizeof(eq5State_t));
            inst->state = st;
            inst->process = dspProcessEQ5;
            break;
        }
        case DSP_TYPE_FILTER: {
            int n; dspGetParamInfo(DSP_TYPE_FILTER, &n);
            inst->params.filter.cutoffHz = 1000.0f;
            inst->params.filter.resonance = 0.707f;
            filterState_t *st = calloc(1,sizeof(filterState_t));
            inst->state = st;
            inst->process = dspProcessFilter;
            break;
        }
        case DSP_TYPE_COMB_FILTER: {
            int n; dspGetParamInfo(DSP_TYPE_COMB_FILTER,&n);
            inst->params.comb.timeMs = 50.0f;
            inst->params.comb.feedback = 0.5f;
            inst->params.comb.mix = 0.5f;
            combState_t *st = (combState_t*)calloc(1,sizeof(combState_t));
            if (!st) { inst->enabled = false; break; }
            uint32_t bufSize = inst->sampleRate * 2;
            st->bufSize = bufSize;
            st->bufL = calloc(bufSize,sizeof(float));
            st->bufR = calloc(bufSize,sizeof(float));
            st->writePos = 0;
            if (!st->bufL || !st->bufR) {
                if (st->bufL) free(st->bufL);
                if (st->bufR) free(st->bufR);
                free(st);
                inst->enabled = false;
                break;
            }
            inst->state = st;
            inst->process = dspProcessCombFilter;
            break;
        }
        case DSP_TYPE_BITCRUSHER: {
            int n; dspGetParamInfo(DSP_TYPE_BITCRUSHER,&n);
            inst->params.bitcrusher.rateHz = 1000.0f;
            inst->params.bitcrusher.bitDepth = 8.0f;
            bitcrusherState_t *st = calloc(1,sizeof(bitcrusherState_t));
            st->counter = 0; st->step = 1; st->heldL = st->heldR = 0.0f;
            inst->state = st;
            inst->process = dspProcessBitcrusher;
            break;
        }
        /* other effects will be added later */
        default:
            /* unsupported type for now */
            inst->enabled = false;
            inst->process = NULL;
            break;
    }

    return true;
}

void dspFreeEffect(dspEffectInstance_t *inst)
{
    if (inst == NULL)
        return;

    /* free internal state if allocated */
    if (inst->state != NULL)
    {
        if (inst->type == DSP_TYPE_DELAY)
        {
            delayState_t *st = (delayState_t *)inst->state;
            if (st) {
                if (st->bufL) free(st->bufL);
                if (st->bufR) free(st->bufR);
            }
        }
        else if (inst->type == DSP_TYPE_REVERB)
        {
            multiReverbState_t *st = (multiReverbState_t *)inst->state;
            if (st) {
                for (int c = 0; c < NUM_REVERB_COMBS; c++) {
                    if (st->combL[c].buf) free(st->combL[c].buf);
                    if (st->combR[c].buf) free(st->combR[c].buf);
                }
                for (int a = 0; a < NUM_REVERB_ALLPASS; a++) {
                    if (st->apL[a].buf) free(st->apL[a].buf);
                    if (st->apR[a].buf) free(st->apR[a].buf);
                }
                if (st->convBufL) free(st->convBufL);
                if (st->convBufR) free(st->convBufR);
            }
        }
        else if (inst->type == DSP_TYPE_CHORUS)
        {
            chorusState_t *st = (chorusState_t *)inst->state;
            if (st) {
                if (st->bufL) free(st->bufL);
                if (st->bufR) free(st->bufR);
            }
        }
        else if (inst->type == DSP_TYPE_FLANGER)
        {
            flangerState_t *st = (flangerState_t *)inst->state;
            if (st) {
                if (st->bufL) free(st->bufL);
                if (st->bufR) free(st->bufR);
            }
        }
        else if (inst->type == DSP_TYPE_COMB_FILTER) {
            combState_t *st = (combState_t*)inst->state;
            if (st) {
                if (st->bufL) free(st->bufL);
                if (st->bufR) free(st->bufR);
            }
        }
        free(inst->state);
    }

    memset(inst, 0, sizeof (dspEffectInstance_t));
}

void dspResetEffectState(dspEffectInstance_t *inst)
{
    if (inst == NULL || !inst->enabled)
        return;

    // Reset effect-specific state based on type
    switch (inst->type)
    {
        case DSP_TYPE_COMPRESSOR:
        {
            compressorState_t *st = (compressorState_t *)inst->state;
            if (st) st->env = 0.0f;
            break;
        }
        case DSP_TYPE_LIMITER:
        {
            limiterState_t *st = (limiterState_t *)inst->state;
            if (st) st->gain = 1.0f;
            break;
        }
        case DSP_TYPE_DRIVE:
        {
            driveState_t *st = (driveState_t *)inst->state;
            if (st) { st->zL = 0.0f; st->zR = 0.0f; }
            break;
        }
        case DSP_TYPE_AMP_SIM:
        {
            ampSimState_t *st = (ampSimState_t *)inst->state;
            if (st) { st->zL = 0.0f; st->zR = 0.0f; }
            break;
        }
        case DSP_TYPE_DELAY:
        {
            delayState_t *st = (delayState_t *)inst->state;
            if (st) 
            {
                st->writePos = 0;
                st->filterStoreL = 0.0f;
                st->filterStoreR = 0.0f;
                st->hpStoreL = 0.0f;
                st->hpStoreR = 0.0f;
                st->wowPhase = 0.0f;
                st->flutterPhase = 0.0f;
                if (st->bufL) memset(st->bufL, 0, st->bufSize * sizeof(float));
                if (st->bufR) memset(st->bufR, 0, st->bufSize * sizeof(float));
            }
            break;
        }
        case DSP_TYPE_REVERB:
        {
            multiReverbState_t *st = (multiReverbState_t *)inst->state;
            if (!st) break;
            for (int c = 0; c < NUM_REVERB_COMBS; c++) {
                st->combL[c].writePos = 0;
                st->combR[c].writePos = 0;
                st->combL[c].filterStore = 0.0f;
                st->combR[c].filterStore = 0.0f;
                if (st->combL[c].buf) memset(st->combL[c].buf, 0, st->combL[c].bufSize * sizeof(float));
                if (st->combR[c].buf) memset(st->combR[c].buf, 0, st->combR[c].bufSize * sizeof(float));
            }
            for (int a = 0; a < NUM_REVERB_ALLPASS; a++) {
                st->apL[a].writePos = 0;
                st->apR[a].writePos = 0;
                if (st->apL[a].buf) memset(st->apL[a].buf, 0, st->apL[a].bufSize * sizeof(float));
                if (st->apR[a].buf) memset(st->apR[a].buf, 0, st->apR[a].bufSize * sizeof(float));
            }
            st->convPos = 0;
            if (st->convBufL) memset(st->convBufL, 0, st->convSize * sizeof(float));
            if (st->convBufR) memset(st->convBufR, 0, st->convSize * sizeof(float));
            break;
        }
        case DSP_TYPE_CHORUS:
        case DSP_TYPE_FLANGER:
        {
            chorusState_t *st = (chorusState_t *)inst->state;
            if (st)
            {
                st->writePos = 0;
                st->lfoPhase = 0.0f;
                if (st->bufL) memset(st->bufL, 0, st->bufSize * sizeof(float));
                if (st->bufR) memset(st->bufR, 0, st->bufSize * sizeof(float));
            }
            break;
        }
        case DSP_TYPE_PHASER:
        {
            phaserState_t *st = (phaserState_t *)inst->state;
            if (st)
            {
                st->lfoPhase = 0.0f;
                st->lastOutL = 0.0f;
                st->lastOutR = 0.0f;
                for (int i = 0; i < PHASER_STATE_SLOTS; i++)
                {
                    st->x1[i] = 0.0f;
                    st->y1[i] = 0.0f;
                }
            }
            break;
        }
        case DSP_TYPE_EQ_5BAND:
        {
            eq5State_t *st = (eq5State_t *)inst->state;
            if (st)
            {
                for (int i = 0; i < 5; i++)
                {
                    st->s1L[i] = st->s2L[i] = 0.0f;
                    st->s1R[i] = st->s2R[i] = 0.0f;
                }
            }
            break;
        }
        case DSP_TYPE_FILTER:
        {
            filterState_t *st = (filterState_t *)inst->state;
            if (st)
            {
                st->s1L = st->s2L = 0.0f;
                st->s1R = st->s2R = 0.0f;
            }
            break;
        }
        case DSP_TYPE_COMB_FILTER:
        {
            delayState_t *st = (delayState_t *)inst->state;
            if (st)
            {
                st->writePos = 0;
                st->filterStoreL = 0.0f;
                st->filterStoreR = 0.0f;
                if (st->bufL) memset(st->bufL, 0, st->bufSize * sizeof(float));
                if (st->bufR) memset(st->bufR, 0, st->bufSize * sizeof(float));
            }
            break;
        }
        case DSP_TYPE_BITCRUSHER:
        {
            bitcrusherState_t *st = (bitcrusherState_t *)inst->state;
            if (st)
            {
                st->counter = 0;
                st->heldL = 0.0f;
                st->heldR = 0.0f;
            }
            break;
        }
        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/*                              Effect kernels                                */
/* -------------------------------------------------------------------------- */

static void dspProcessGainer(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled)
        return;

    const float gain = inst->params.gainer.gain;
    if (gain == 1.0f)
        return; /* unity gain, skip */

    for (uint32_t i = 0; i < frames; i++)
    {
        bufL[i] *= gain;
        bufR[i] *= gain;
    }
}

void dspProcessChain(dspEffectInstance_t *effects, float *bufL, float *bufR, uint32_t frames)
{
    if (effects == NULL || bufL == NULL || bufR == NULL || frames == 0)
        return;

    // Safety check: ensure frames is reasonable
    if (frames > 8192) // Max reasonable buffer size
        return;

    for (int i = 0; i < DSP_MAX_SLOTS; i++)
    {
        dspEffectInstance_t *e = &effects[i];
        if (e && e->enabled && e->process && e->state)
            e->process(e, bufL, bufR, frames);
    }
}

/* -------------------------------------------------------------------------- */
/*                            Parameter tables                                */
/* -------------------------------------------------------------------------- */

/* Parameter table for Gainer */
static const dspParamInfo_t gainerParamInfo[] = {
    {"Gain", 0.0f, 2.0f, 1.0f, 0.1f}
};

/* Parameter table for Delay */
static const dspParamInfo_t delayParamInfo[] = {
    {"Time",        10.0f, 2000.0f, 500.0f, 10.0f},
    {"Feedback",    0.0f,    0.95f,   0.3f, 0.05f},
    {"Tone",        0.0f,    1.0f,    0.5f, 0.05f},
    {"Mix",         0.0f,    1.0f,    0.5f, 0.05f},
    {"Diffusion",   0.0f,   100.0f,   20.0f, 1.0f},
    {"DiffMix",     0.0f,    1.0f,    0.3f, 0.05f},
    {"PingPong",    0.0f,    1.0f,    0.0f, 1.0f},
    {"Character",   0.0f,    4.0f,    0.0f, 1.0f},
    {"Sync",        0.0f,    1.0f,    0.0f, 1.0f},
    {"Width",       0.5f,    2.0f,    1.2f, 0.05f}
};

static void dspProcessDelay(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled) return;
    delayState_t *st = (delayState_t *)inst->state;
    if (!st) return;

    float timeMs        = inst->params.delay.timeMs;
    float fbParam       = inst->params.delay.feedback;
    float tone          = inst->params.delay.tone;
    float mix           = inst->params.delay.mix;
    float diffMs        = inst->params.delay.diffusionMs;
    float diffMix       = inst->params.delay.diffusionMix;
    int   pingPong      = (int)inst->params.delay.pingPong;
    int   style         = (int)floorf(inst->params.delay.character + 0.5f);
    if (style < 0) style = 0;
    if (style > 4) style = 4;
    float widthUser = dspClampf(inst->params.delay.width, 0.5f, 2.0f);
    bool sync = inst->params.delay.sync >= 0.5f;

    uint32_t delaySamples = (uint32_t)((timeMs * inst->sampleRate) / 1000.0f);
    if (sync) {
        static const float beatDivs[] = {
            4.0f, 2.0f, 1.0f, 0.5f, 0.75f, 1.0f/3.0f,
            0.25f, 0.375f, 1.0f/6.0f, 0.125f, 1.0f/12.0f, 0.0625f
        };
        const int divCount = (int)(sizeof(beatDivs) / sizeof(beatDivs[0]));
        float bpm = (song.BPM > 0) ? (float)song.BPM : 125.0f;
        float secPerBeat = 60.0f / bpm;
        float desiredSec = timeMs * 0.001f;
        float bestSec = beatDivs[0] * secPerBeat;
        float bestDiff = fabsf(desiredSec - bestSec);
        for (int d = 1; d < divCount; d++) {
            float sec = beatDivs[d] * secPerBeat;
            float diff = fabsf(desiredSec - sec);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestSec = sec;
            }
        }
        delaySamples = (uint32_t)(bestSec * inst->sampleRate);
        if (delaySamples < 1) delaySamples = 1;
    }
    if (delaySamples >= st->bufSize) delaySamples = st->bufSize - 1;
    uint32_t diffSamples = (uint32_t)((diffMs * inst->sampleRate) / 1000.0f);
    if (diffSamples >= st->bufSize) diffSamples = st->bufSize - 1;

    float sat = 0.0f;
    float wowDepth = 0.0f;
    float flutterDepth = 0.0f;
    float wowRate = 0.0f;
    float flutterRate = 0.0f;
    float extraLP = 0.0f;
    float cross = 0.0f;
    float widthFactor = widthUser;
    float hpCut = 0.0f;

    switch (style) {
        case 0: // clean
            widthFactor *= 1.1f;
            break;
        case 1: // dirt
            sat = 0.4f;
            extraLP = 0.2f;
            widthFactor *= 1.15f;
            break;
        case 2: // tape
            sat = 0.35f;
            wowDepth = 2.0f;
            flutterDepth = 1.5f;
            wowRate = 0.35f;
            flutterRate = 6.0f;
            extraLP = 0.3f;
            widthFactor *= 1.2f;
            break;
        case 3: // warped tape
            sat = 0.55f;
            wowDepth = 6.0f;
            flutterDepth = 4.0f;
            wowRate = 0.2f;
            flutterRate = 4.5f;
            extraLP = 0.45f;
            widthFactor *= 1.25f;
            break;
        case 4: // dub
            sat = 0.6f;
            wowDepth = 2.5f;
            flutterDepth = 2.0f;
            wowRate = 0.25f;
            flutterRate = 3.5f;
            extraLP = 0.65f;
            cross = 0.35f;
            widthFactor *= 1.3f;
            hpCut = 0.02f;
            break;
        default:
            break;
    }
    widthFactor = dspClampf(widthFactor, 0.5f, 2.0f);

    float toneEff = dspClampf(tone + extraLP, 0.0f, 0.98f);
    float wowInc = 2.0f * (float)M_PI * wowRate / (float)inst->sampleRate;
    float flutterInc = 2.0f * (float)M_PI * flutterRate / (float)inst->sampleRate;

    for (uint32_t i = 0; i < frames; i++)
    {
        float mod = 0.0f;
        if (wowDepth > 0.0f) mod += sinf(st->wowPhase) * wowDepth;
        if (flutterDepth > 0.0f) mod += sinf(st->flutterPhase) * flutterDepth;

        float modDelay = (float)delaySamples + mod;
        if (modDelay < 1.0f) modDelay = 1.0f;
        if (modDelay > (float)(st->bufSize - 2)) modDelay = (float)(st->bufSize - 2);

        float readPosF = (float)st->writePos - modDelay;
        while (readPosF < 0.0f) readPosF += (float)st->bufSize;
        uint32_t readPos0 = (uint32_t)readPosF;
        uint32_t readPos1 = (readPos0 + 1) % st->bufSize;
        float frac = readPosF - (float)readPos0;
        float dl = st->bufL[readPos0] * (1.0f - frac) + st->bufL[readPos1] * frac;
        float dr = st->bufR[readPos0] * (1.0f - frac) + st->bufR[readPos1] * frac;

        uint32_t diffPos = (st->writePos + st->bufSize - diffSamples) % st->bufSize;
        float d2l = st->bufL[diffPos];
        float d2r = st->bufR[diffPos];

        float inL = bufL[i];
        float inR = bufR[i];

        // compute wet signals and apply stereo widening
        float outL = inL*(1.0f - mix - diffMix) + dl*mix + d2l*diffMix;
        float outR = inR*(1.0f - mix - diffMix) + dr*mix + d2r*diffMix;
        float mid = 0.5f * (outL + outR);
        float side = 0.5f * (outL - outR);
        outL = mid + side * widthFactor;
        outR = mid - side * widthFactor;
        bufL[i] = outL;
        bufR[i] = outR;

        // feedback with tone and ping-pong
        float fbL = dl*fbParam;
        float fbR = dr*fbParam;
        if (cross > 0.0f) {
            float mixL = fbL * (1.0f - cross) + fbR * cross;
            float mixR = fbR * (1.0f - cross) + fbL * cross;
            fbL = mixL;
            fbR = mixR;
        }
        if (pingPong) { float tmp=fbL; fbL=fbR; fbR=tmp; }
        if (sat > 0.0f) {
            fbL = softClipf(fbL * (1.0f + sat)) * (1.0f / (1.0f + sat));
            fbR = softClipf(fbR * (1.0f + sat)) * (1.0f / (1.0f + sat));
        }
        // apply tone LPF in feedback path
        st->filterStoreL = fbL*(1.0f - toneEff) + st->filterStoreL * toneEff;
        st->filterStoreR = fbR*(1.0f - toneEff) + st->filterStoreR * toneEff;
        float writeL = inL + st->filterStoreL;
        float writeR = inR + st->filterStoreR;
        if (hpCut > 0.0f) {
            float hpL = writeL - st->hpStoreL + hpCut * st->hpStoreL;
            float hpR = writeR - st->hpStoreR + hpCut * st->hpStoreR;
            st->hpStoreL = writeL;
            st->hpStoreR = writeR;
            writeL = hpL;
            writeR = hpR;
        }
        st->bufL[st->writePos] = writeL;
        st->bufR[st->writePos] = writeR;

        st->writePos++;
        if (st->writePos >= st->bufSize) st->writePos = 0;

        st->wowPhase += wowInc;
        st->flutterPhase += flutterInc;
        if (st->wowPhase >= 2.0f * (float)M_PI) st->wowPhase -= 2.0f * (float)M_PI;
        if (st->flutterPhase >= 2.0f * (float)M_PI) st->flutterPhase -= 2.0f * (float)M_PI;
    }
}

/* Compressor parameter table
 *
 * Thresh: Threshold in dB
 * Ratio: Compression ratio
 * Attack: Attack time in ms
 * Release: Release time in ms
 * Makeup: Gain in dB
 */
static const dspParamInfo_t compParamInfo[] = {
    {"Thresh", -60.0f, 0.0f, -18.0f, 1.0f},
    {"Ratio", 1.0f, 20.0f, 4.0f, 0.5f},
    {"Attack", 1.0f, 200.0f, 10.0f, 1.0f},
    {"Release", 10.0f, 1000.0f, 200.0f, 10.0f},
    {"Makeup", 0.0f, 24.0f, 0.0f, 1.0f}
};

static void dspProcessCompressor(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if(!inst||!inst->enabled)return;
    compressorState_t *st = (compressorState_t *)inst->state;
    if (!st) return;
    
    float thresh = db2lin(inst->params.compressor.thresholdDb);
    float ratio = inst->params.compressor.ratio;
    float atkMs = inst->params.compressor.attackMs;
    float relMs = inst->params.compressor.releaseMs;
    float makeup = db2lin(inst->params.compressor.makeupDb);

    float atkCoeff = expf(-1.0f/(atkMs*0.001f*inst->sampleRate));
    float relCoeff = expf(-1.0f/(relMs*0.001f*inst->sampleRate));

    for(uint32_t i=0;i<frames;i++){
        float in = 0.5f*(fabsf(bufL[i])+fabsf(bufR[i]));
        if(in>st->env) st->env = atkCoeff*st->env + (1.0f-atkCoeff)*in;
        else           st->env = relCoeff*st->env + (1.0f-relCoeff)*in;

        float gain = 1.0f;
        if(st->env> thresh){
            float over = st->env/thresh;
            float comp = powf(over, (1.0f-1.0f/ratio));
            gain = 1.0f/comp;
        }
        gain*=makeup;
        bufL[i]*=gain;
        bufR[i]*=gain;
    }
}

/* Limiter parameter table
 *
 * Thresh: Threshold in dB
 * Release: Release time in ms
 */
static const dspParamInfo_t limiterParamInfo[] = {
    {"Thresh", -12.0f, 0.0f, -1.0f, 0.5f},
    {"Release", 10.0f, 500.0f, 100.0f, 10.0f}
};

static void dspProcessLimiter(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if(!inst||!inst->enabled) return;
    limiterState_t *st = (limiterState_t *)inst->state;
    if (!st) return;
    
    float thresh = db2lin(inst->params.limiter.thresholdDb);
    float relMs = inst->params.limiter.releaseMs;
    float relCoeff = expf(-1.0f/(relMs*0.001f*inst->sampleRate));

    for(uint32_t i=0;i<frames;i++){
        float peak = fmaxf(fabsf(bufL[i]), fabsf(bufR[i]));
        float targetGain = 1.0f;
        if(peak>thresh) targetGain = thresh/peak;
        if(targetGain < st->gain) st->gain = targetGain; else st->gain = st->gain*relCoeff + targetGain*(1.0f-relCoeff);
        bufL[i]*=st->gain;
        bufR[i]*=st->gain;
    }
}

/* Drive parameter table
 *
 * Gain: Gain multiplier
 * Tone: Tone control (0..1)
 * Mix: Dry/wet mix (0..1)
 */
static const dspParamInfo_t driveParamInfo[] = {
    {"Gain", 0.0f, 2.0f, 1.0f, 0.1f}
};

/* Reverb parameter table */
static const dspParamInfo_t reverbParamInfo[] = {
    {"Size",      100.0f, 2000.0f, 600.0f, 100.0f},
    {"Feedback",    0.0f,   0.95f,  0.5f,   0.05f},
    {"Damp",        0.0f,   1.0f,   0.5f,   0.05f},
    {"Mix",         0.0f,   1.0f,   0.5f,   0.05f},
    {"Character",   0.0f,   2.0f,   1.0f,   1.0f},
    {"ConvMix",     0.0f,   1.0f,   0.0f,   0.05f},
    {"IR",          0.0f,   (float)(NUM_REVERB_IRS - 1), 0.0f, 1.0f}
};

/* Chorus parameter table */
static const dspParamInfo_t chorusParamInfo[] = {
    {"Depth",        0.1f,  20.0f,  5.0f,  0.5f},  /* ms */
    {"Rate",         0.1f,   5.0f,  1.0f,  0.1f},  /* Hz */
    {"Feedback",     0.0f,   1.0f,  0.0f,  0.05f},
    {"Mix",          0.0f,   1.0f,  0.5f,  0.1f},
    {"StereoOffset", 0.0f,   1.0f,  0.0f,  0.01f} // 0..1 -> 0..2pi
};

/* Flanger parameter table */
static const dspParamInfo_t flangerParamInfo[] = {
    {"Depth", 0.1f,  20.0f,  2.0f, 0.1f},
    {"Rate",  0.1f,  5.0f,   1.0f, 0.1f},
    {"Feedb", 0.0f,  0.95f,  0.5f, 0.05f},
    {"Mix",   0.0f,  1.0f,   0.5f, 0.05f}
};

/* Phaser parameter table */
static const dspParamInfo_t phaserParamInfo[] = {
    {"Depth",        0.0f, 1.0f, 0.5f, 0.05f},
    {"Rate",         0.1f, 5.0f, 1.0f, 0.1f},
    {"Mix",          0.0f, 1.0f, 0.5f, 0.05f},
    {"Feedback",     0.0f, 1.0f, 0.0f, 0.05f},
    {"StereoOffset", 0.0f, 1.0f, 0.0f, 0.01f}
};

/* Parameter table for AmpSim */
static const dspParamInfo_t ampSimParamInfo[] = {
    {"Drive", 0.0f, 10.0f, 1.0f, 0.1f},
    {"Tone",  0.0f, 1.0f,  0.5f, 0.01f},
    {"Mix",   0.0f, 1.0f,  0.5f, 0.01f}
};

/* Parameter table for 5-band Parametric EQ */
static const dspParamInfo_t eq5ParamInfo[] = {
    {"60Hz dB",   -12.0f, 12.0f, 0.0f, 0.5f},
    {"250Hz dB",  -12.0f, 12.0f, 0.0f, 0.5f},
    {"1kHz dB",   -12.0f, 12.0f, 0.0f, 0.5f},
    {"4kHz dB",   -12.0f, 12.0f, 0.0f, 0.5f},
    {"12kHz dB",  -12.0f, 12.0f, 0.0f, 0.5f}
};

/* Filter parameter table */
static const dspParamInfo_t filterParamInfo[] = {
    {"Cutoff",    100.0f, 10000.0f, 1000.0f, 100.0f},
    {"Resonance",   0.0f,     2.0f,   0.707f, 0.1f},
    {"Mode",        0.0f,     2.0f,   0.0f,   1.0f}  // 0=LP,1=HP,2=BP
};

/* Comb filter parameter table */
static const dspParamInfo_t combFilterParamInfo[] = {
    {"Time", 10.0f, 500.0f, 50.0f, 10.0f},
    {"Feedback", 0.0f, 1.0f, 0.5f, 0.1f},
    {"Mix", 0.0f, 1.0f, 0.5f, 0.1f}
};

/* Bitcrusher parameter table */
static const dspParamInfo_t bitcrusherParamInfo[] = {
    {"Rate", 100.0f, 10000.0f, 1000.0f, 100.0f},
    {"Bit Depth", 1.0f, 16.0f, 8.0f, 1.0f}
};

/* -------------------------------------------------------------------------- */
/*                       Parameter info dispatcher                            */
/* -------------------------------------------------------------------------- */

const dspParamInfo_t *dspGetParamInfo(dspEffectType_t type, int *numParams)
{
    if (numParams) *numParams = 0;
    switch (type)
    {
        case DSP_TYPE_GAINER:
            if (numParams) *numParams = sizeof(gainerParamInfo)/sizeof(gainerParamInfo[0]);
            return gainerParamInfo;
        case DSP_TYPE_DELAY:
            if (numParams) *numParams = sizeof(delayParamInfo)/sizeof(delayParamInfo[0]);
            return delayParamInfo;
        case DSP_TYPE_COMPRESSOR:
            if (numParams) *numParams = sizeof(compParamInfo)/sizeof(compParamInfo[0]);
            return compParamInfo;
        case DSP_TYPE_LIMITER:
            if (numParams) *numParams = sizeof(limiterParamInfo)/sizeof(limiterParamInfo[0]);
            return limiterParamInfo;
        case DSP_TYPE_DRIVE:
            if (numParams) *numParams = sizeof(driveParamInfo)/sizeof(driveParamInfo[0]);
            return driveParamInfo;
        case DSP_TYPE_REVERB:
            if (numParams) *numParams = sizeof(reverbParamInfo)/sizeof(reverbParamInfo[0]);
            return reverbParamInfo;
        case DSP_TYPE_CHORUS:
            if (numParams) *numParams = sizeof(chorusParamInfo)/sizeof(chorusParamInfo[0]);
            return chorusParamInfo;
        case DSP_TYPE_FLANGER:
            if (numParams) *numParams = sizeof(flangerParamInfo)/sizeof(flangerParamInfo[0]);
            return flangerParamInfo;
        case DSP_TYPE_PHASER:
            if (numParams) *numParams = sizeof(phaserParamInfo)/sizeof(phaserParamInfo[0]);
            return phaserParamInfo;
        case DSP_TYPE_AMP_SIM:
            if (numParams) *numParams = sizeof(ampSimParamInfo)/sizeof(ampSimParamInfo[0]);
            return ampSimParamInfo;
        case DSP_TYPE_EQ_5BAND:
            if (numParams) *numParams = sizeof(eq5ParamInfo)/sizeof(eq5ParamInfo[0]);
            return eq5ParamInfo;
        case DSP_TYPE_FILTER:
            if (numParams) *numParams = sizeof(filterParamInfo)/sizeof(filterParamInfo[0]);
            return filterParamInfo;
        case DSP_TYPE_COMB_FILTER:
            if (numParams) *numParams = sizeof(combFilterParamInfo)/sizeof(combFilterParamInfo[0]);
            return combFilterParamInfo;
        case DSP_TYPE_BITCRUSHER:
            if (numParams) *numParams = sizeof(bitcrusherParamInfo)/sizeof(bitcrusherParamInfo[0]);
            return bitcrusherParamInfo;
        default:
            return NULL;
    }
}

/* Reverb state: reuse delayState_t for simplicity */

static void dspProcessReverb(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled || !bufL || !bufR) return;
    multiReverbState_t *st = (multiReverbState_t*)inst->state;
    if (!st) return;
    
    static const float charSizeMul[3] = {0.7f, 1.0f, 1.5f};
    static const float charFbMul[3] = {0.85f, 1.0f, 1.1f};
    static const float charDamp[3] = {0.35f, 0.55f, 0.75f};
    static const float charWidth[3] = {1.0f, 1.2f, 1.4f};

    float character = dspClampf(inst->params.reverb.character, 0.0f, 2.0f);
    int c0 = (int)floorf(character);
    int c1 = (c0 < 2) ? c0 + 1 : 2;
    float ct = character - (float)c0;
    float sizeMul = lerpf(charSizeMul[c0], charSizeMul[c1], ct);
    float fbMul = lerpf(charFbMul[c0], charFbMul[c1], ct);
    float dampChar = lerpf(charDamp[c0], charDamp[c1], ct);
    float width = lerpf(charWidth[c0], charWidth[c1], ct);

    float roomScale = inst->params.reverb.sizeMs / 600.0f;
    roomScale = dspClampf(roomScale * sizeMul, 0.2f, 3.5f);
    float fbParam = dspClampf(inst->params.reverb.feedback * fbMul, 0.0f, 0.98f);
    float dampUser = dspClampf(inst->params.reverb.damp, 0.0f, 0.99f);
    float damp = dspClampf(dampUser * 0.6f + dampChar * 0.4f, 0.0f, 0.99f);
    float mix     = dspClampf(inst->params.reverb.mix, 0.0f, 1.0f);
    float convMix = dspClampf(inst->params.reverb.convMix, 0.0f, 1.0f);
    int irIndex = (int)floorf(inst->params.reverb.irIndex + 0.5f);
    if (irIndex < 0) irIndex = 0;
    if (irIndex >= NUM_REVERB_IRS) irIndex = NUM_REVERB_IRS - 1;
    float srScale = (float)inst->sampleRate / 44100.0f;
    const float apFeedback = 0.5f;

    for (uint32_t i = 0; i < frames; i++) {
        float inL = bufL[i];
        float inR = bufR[i];
        float accL = 0.0f;
        float accR = 0.0f;

        for (int c = 0; c < NUM_REVERB_COMBS; c++) {
            revCombState_t *cl = &st->combL[c];
            revCombState_t *cr = &st->combR[c];
            uint32_t delayL = (uint32_t)(revCombBase[c] * srScale * roomScale);
            uint32_t delayR = (uint32_t)((revCombBase[c] + revStereoSpread) * srScale * roomScale);
            if (delayL >= cl->bufSize) delayL = cl->bufSize - 1;
            if (delayR >= cr->bufSize) delayR = cr->bufSize - 1;

            uint32_t rpL = (cl->writePos + cl->bufSize - delayL) % cl->bufSize;
            uint32_t rpR = (cr->writePos + cr->bufSize - delayR) % cr->bufSize;
            float dL = cl->buf[rpL];
            float dR = cr->buf[rpR];
            accL += dL;
            accR += dR;

            cl->filterStore = dL * (1.0f - damp) + cl->filterStore * damp;
            cr->filterStore = dR * (1.0f - damp) + cr->filterStore * damp;
            cl->buf[cl->writePos] = inL + cl->filterStore * fbParam;
            cr->buf[cr->writePos] = inR + cr->filterStore * fbParam;
            if (++cl->writePos >= cl->bufSize) cl->writePos = 0;
            if (++cr->writePos >= cr->bufSize) cr->writePos = 0;
        }

        float wetL = accL * (1.0f / NUM_REVERB_COMBS);
        float wetR = accR * (1.0f / NUM_REVERB_COMBS);

        for (int a = 0; a < NUM_REVERB_ALLPASS; a++) {
            revAllpassState_t *al = &st->apL[a];
            revAllpassState_t *ar = &st->apR[a];
            uint32_t delayL = (uint32_t)(revAllpassBase[a] * srScale * roomScale);
            uint32_t delayR = (uint32_t)((revAllpassBase[a] + revStereoSpread) * srScale * roomScale);
            if (delayL >= al->bufSize) delayL = al->bufSize - 1;
            if (delayR >= ar->bufSize) delayR = ar->bufSize - 1;

            uint32_t rpL = (al->writePos + al->bufSize - delayL) % al->bufSize;
            uint32_t rpR = (ar->writePos + ar->bufSize - delayR) % ar->bufSize;
            float bufOutL = al->buf[rpL];
            float bufOutR = ar->buf[rpR];
            float outL = -wetL + bufOutL;
            float outR = -wetR + bufOutR;
            al->buf[al->writePos] = wetL + bufOutL * apFeedback;
            ar->buf[ar->writePos] = wetR + bufOutR * apFeedback;
            if (++al->writePos >= al->bufSize) al->writePos = 0;
            if (++ar->writePos >= ar->bufSize) ar->writePos = 0;
            wetL = outL;
            wetR = outR;
        }

        float mid = 0.5f * (wetL + wetR);
        float side = 0.5f * (wetL - wetR);
        wetL = mid + side * width;
        wetR = mid - side * width;

        if (st->convBufL && st->convBufR) {
            st->convBufL[st->convPos] = wetL;
            st->convBufR[st->convPos] = wetR;

            if (convMix > 0.001f) {
                float convOutL = 0.0f;
                float convOutR = 0.0f;
                uint32_t pos = st->convPos;
                const float *ir = st->irData[irIndex];
                for (int t = 0; t < REVERB_IR_TAPS; t++) {
                    convOutL += st->convBufL[pos] * ir[t];
                    convOutR += st->convBufR[pos] * ir[t];
                    if (pos == 0) pos = st->convSize - 1; else pos--;
                }

                wetL = wetL * (1.0f - convMix) + convOutL * convMix;
                wetR = wetR * (1.0f - convMix) + convOutR * convMix;
            }

            st->convPos++;
            if (st->convPos >= st->convSize) st->convPos = 0;
        }

        bufL[i] = inL * (1.0f - mix) + wetL * mix;
        bufR[i] = inR * (1.0f - mix) + wetR * mix;
    }
}

/* Chorus state */
static void dspProcessChorus(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled || !bufL || !bufR) return;
    chorusState_t *st = (chorusState_t*)inst->state;
    if (!st || !st->bufL || !st->bufR) return;
    
    // fetch parameters
    float depthMs     = inst->params.chorus.depthMs;
    float rateHz      = inst->params.chorus.rateHz;
    float fbParam     = inst->params.chorus.feedback;
    float mix         = inst->params.chorus.mix;
    float stereoOff   = inst->params.chorus.stereoOffset * 2.0f * 3.14159265f;
    uint32_t maxDelay = (uint32_t)(depthMs*inst->sampleRate/1000.0f);
    
    for(uint32_t i=0;i<frames;i++){
        // update LFO and compute fractional delays
        float phaseL = st->lfoPhase;
        float phaseR = st->lfoPhase + stereoOff;
        st->lfoInc = 2.0f * 3.14159265f * rateHz / inst->sampleRate;
        
        // left channel delay
        float dL = (sinf(phaseL)*0.5f + 0.5f) * maxDelay;
        uint32_t iDL = (uint32_t)dL; float fracL = dL - iDL;
        uint32_t rpL1 = (st->writePos + st->bufSize - iDL) % st->bufSize;
        uint32_t rpL2 = (rpL1 + 1) % st->bufSize;
        
        if (rpL1 < st->bufSize && rpL2 < st->bufSize) {
            float wetL1 = st->bufL[rpL1]; float wetL2 = st->bufL[rpL2];
            float wetL = wetL1 * (1.0f - fracL) + wetL2 * fracL;
            
            // right channel delay
            float dR = (sinf(phaseR)*0.5f + 0.5f) * maxDelay;
            uint32_t iDR = (uint32_t)dR; float fracR = dR - iDR;
            uint32_t rpR1 = (st->writePos + st->bufSize - iDR) % st->bufSize;
            uint32_t rpR2 = (rpR1 + 1) % st->bufSize;
            
            if (rpR1 < st->bufSize && rpR2 < st->bufSize) {
                float wetR1 = st->bufR[rpR1]; float wetR2 = st->bufR[rpR2];
                float wetR = wetR1 * (1.0f - fracR) + wetR2 * fracR;
                
                // mix
                float inL = bufL[i], inR = bufR[i];
                bufL[i] = inL*(1.0f - mix) + wetL*mix;
                bufR[i] = inR*(1.0f - mix) + wetR*mix;
                
                // feedback
                if (st->writePos < st->bufSize) {
                    st->bufL[st->writePos] = inL + wetL * fbParam;
                    st->bufR[st->writePos] = inR + wetR * fbParam;
                }
            }
        }
        
        st->writePos++; if(st->writePos>=st->bufSize)st->writePos=0;
        st->lfoPhase += st->lfoInc; if(st->lfoPhase>=2*3.14159265f)st->lfoPhase-=2*3.14159265f;
    }
}

/* Flanger state */
static void dspProcessFlanger(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled || !bufL || !bufR) return;
    flangerState_t *st = (flangerState_t*)inst->state;
    if (!st || !st->bufL || !st->bufR) return;
    
    // fetch and apply current parameters
    float depthMs = inst->params.flanger.depthMs;
    float rateHz  = inst->params.flanger.rateHz;
    // update LFO increment dynamically
    st->lfoInc = 2.0f * M_PI * rateHz / inst->sampleRate;
    float fb      = inst->params.flanger.feedback;
    float mix     = inst->params.flanger.mix;
    uint32_t maxDelay = (uint32_t)(depthMs * inst->sampleRate / 1000.0f);
    
    for (uint32_t i=0;i<frames;i++) {
        float lfo = (sinf(st->lfoPhase)*0.5f + 0.5f);
        uint32_t dpos = (uint32_t)(lfo * maxDelay);
        uint32_t rp = (st->writePos + st->bufSize - dpos) % st->bufSize;
        
        if (rp < st->bufSize) {
            float dl = st->bufL[rp], dr = st->bufR[rp];
            float inL = bufL[i], inR = bufR[i];
            // mix dry/wet for flanger
            bufL[i] = inL*(1.0f - mix) + dl*mix;
            bufR[i] = inR*(1.0f - mix) + dr*mix;
            
            if (st->writePos < st->bufSize) {
                st->bufL[st->writePos] = inL + dl*fb;
                st->bufR[st->writePos] = inR + dr*fb;
            }
        }
        
        if (++st->writePos >= st->bufSize) st->writePos = 0;
        st->lfoPhase += st->lfoInc; if (st->lfoPhase >= 2.0f * M_PI) st->lfoPhase -= 2.0f * M_PI;
    }
}

/* Phaser state */
static void dspProcessPhaser(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst||!inst->enabled) return;
    phaserState_t *st = (phaserState_t*)inst->state;
    if (!st) return;
    // fetch parameters
    float depth        = inst->params.phaser.depth;
    float rateHz       = inst->params.phaser.rateHz;
    float mix          = inst->params.phaser.mix;
    float feedback     = inst->params.phaser.feedback;
    float stereoOff    = inst->params.phaser.stereoOffset * 2.0f * M_PI;
    // update LFO increment
    st->lfoInc = 2.0f * M_PI * rateHz / inst->sampleRate;
    for (uint32_t i = 0; i < frames; i++) {
        // compute per-sample LFO phases
        float phaseL = st->lfoPhase;
        float phaseR = phaseL + stereoOff;
        if (phaseR >= 2.0f * M_PI) phaseR -= 2.0f * M_PI;
        // input + feedback
        float inL = bufL[i] + st->lastOutL * feedback;
        float inR = bufR[i] + st->lastOutR * feedback;
        float xpL = inL;
        float xpR = inR;
        // compute filter coefficients
        float lfoL = sinf(phaseL) * 0.5f + 0.5f;
        float acoefL = depth * lfoL;
        float lfoR = sinf(phaseR) * 0.5f + 0.5f;
        float acoefR = depth * lfoR;
        // cascade stages
        for (int s = 0; s < PHASER_STAGES; s++) {
            float yL = acoefL * xpL + st->x1[s] - acoefL * st->y1[s];
            float yR = acoefR * xpR + st->x1[s+PHASER_STAGES] - acoefR * st->y1[s+PHASER_STAGES];
            st->x1[s] = xpL; st->y1[s] = yL;
            st->x1[s+PHASER_STAGES] = xpR; st->y1[s+PHASER_STAGES] = yR;
            xpL = yL; xpR = yR;
        }
        // store output for feedback
        st->lastOutL = xpL;
        st->lastOutR = xpR;
        // mix dry/wet
        bufL[i] = inL*(1.0f - mix) + xpL*mix;
        bufR[i] = inR*(1.0f - mix) + xpR*mix;
        // advance LFO phase
        st->lfoPhase += st->lfoInc;
        if (st->lfoPhase >= 2.0f * M_PI) st->lfoPhase -= 2.0f * M_PI;
    }
}

static void dspProcessDrive(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled) return;
    driveState_t *st = (driveState_t *)inst->state;
    if (!st) return;
    
    float gain = inst->params.drive.gain;
    float tone = inst->params.drive.tone;
    float mix  = inst->params.drive.mix;
    float a = 1.0f - tone; // tone control
    if (gain == 1.0f && tone == 0.0f) return;

    for (uint32_t i = 0; i < frames; i++)
    {
        float inL = bufL[i] * gain;
        float inR = bufR[i] * gain;
        float outL = tanhf(inL);
        float outR = tanhf(inR);
        // apply tone LPF (simple one-pole)
        st->zL = st->zL + a*(outL - st->zL);
        st->zR = st->zR + a*(outR - st->zR);
        // mix dry/wet
        bufL[i] = bufL[i]*(1.0f-mix) + st->zL*mix;
        bufR[i] = bufR[i]*(1.0f-mix) + st->zR*mix;
    }
}

// AmpSim effect: soft saturation with tone and mix controls
static void dspProcessAmpSim(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled) return;
    ampSimState_t *st = (ampSimState_t *)inst->state;
    if (!st) return;
    
    // fetch parameters
    float drive = inst->params.ampSim.drive;
    float tone  = inst->params.ampSim.tone;
    float mix   = inst->params.ampSim.mix;
    float a = 1.0f - tone; // tone control via simple one-pole LPF
    
    for (uint32_t i = 0; i < frames; i++)
    {
        float inL = bufL[i] * drive;
        float inR = bufR[i] * drive;
        // soft clipping
        float satL = tanhf(inL);
        float satR = tanhf(inR);
        // tone filtering
        st->zL = st->zL + a * (satL - st->zL);
        st->zR = st->zR + a * (satR - st->zR);
        // dry/wet mix
        bufL[i] = bufL[i] * (1.0f - mix) + st->zL * mix;
        bufR[i] = bufR[i] * (1.0f - mix) + st->zR * mix;
    }
}

// 5-band Parametric EQ processing
static void dspProcessEQ5(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst || !inst->enabled) return;
    eq5State_t *st = (eq5State_t*)inst->state;
    if (!st) return;
    const float centerFreqs[5] = {60.0f, 250.0f, 1000.0f, 4000.0f, 12000.0f};
    const float Q = 1.0f;
    float fs = (float)inst->sampleRate;
    // recalc biquad coefficients per band
    for (int b = 0; b < 5; b++) {
        float gainDB = inst->params.eq5.gains[b];
        float A = powf(10.0f, gainDB / 40.0f);
        float w0 = 2.0f * M_PI * centerFreqs[b] / fs;
        float alpha = sinf(w0) / (2.0f * Q);
        float cosw0 = cosf(w0);
        float b0 = 1.0f + alpha * A;
        float b1 = -2.0f * cosw0;
        float b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        float a1 = -2.0f * cosw0;
        float a2 = 1.0f - alpha / A;
        // normalize
        st->b0[b] = b0 / a0;
        st->b1[b] = b1 / a0;
        st->b2[b] = b2 / a0;
        st->a1[b] = a1 / a0;
        st->a2[b] = a2 / a0;
    }
    // process cascade
    for (uint32_t i = 0; i < frames; i++) {
        float l = bufL[i];
        float r = bufR[i];
        for (int b = 0; b < 5; b++) {
            // left channel
            float outL = st->b0[b] * l + st->s1L[b];
            st->s1L[b] = st->b1[b] * l - st->a1[b] * outL + st->s2L[b];
            st->s2L[b] = st->b2[b] * l - st->a2[b] * outL;
            l = outL;
            // right channel
            float outR = st->b0[b] * r + st->s1R[b];
            st->s1R[b] = st->b1[b] * r - st->a1[b] * outR + st->s2R[b];
            st->s2R[b] = st->b2[b] * r - st->a2[b] * outR;
            r = outR;
        }
        bufL[i] = l;
        bufR[i] = r;
    }
}

// Resonant filter
static void dspProcessFilter(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst||!inst->enabled) return;
    filterState_t *st = (filterState_t*)inst->state;
    if (!st) return;
    float cutoff = inst->params.filter.cutoffHz;
    float Q      = inst->params.filter.resonance;
    int mode     = (int)inst->params.filter.mode;
    float fs = (float)inst->sampleRate;
    float w0 = 2.0f*M_PI*cutoff/fs;
    float alpha = sinf(w0)/(2.0f*Q);
    float cosw0 = cosf(w0);
    float b0, b1, b2;
    switch (mode) {
    case 1: // high-pass
        b0 =  (1.0f+cosw0)/2.0f;
        b1 = -(1.0f+cosw0);
        b2 =  (1.0f+cosw0)/2.0f;
        break;
    case 2: // band-pass
        b0 =  alpha;
        b1 =  0.0f;
        b2 = -alpha;
        break;
    default: // low-pass
        b0 = (1.0f-cosw0)/2.0f;
        b1 = 1.0f-cosw0;
        b2 = (1.0f-cosw0)/2.0f;
        break;
    }
    float a0=1.0f+alpha, a1=-2.0f*cosw0, a2=1.0f-alpha;
    st->b0=b0/a0; st->b1=b1/a0; st->b2=b2/a0;
    st->a1=a1/a0; st->a2=a2/a0;
    for (uint32_t i=0;i<frames;i++) {
        float xL=bufL[i]; float yL = st->b0*xL + st->s1L;
        st->s1L = st->b1*xL - st->a1*yL + st->s2L;
        st->s2L = st->b2*xL - st->a2*yL;
        bufL[i]=yL;
        float xR=bufR[i]; float yR = st->b0*xR + st->s1R;
        st->s1R = st->b1*xR - st->a1*yR + st->s2R;
        st->s2R = st->b2*xR - st->a2*yR;
        bufR[i]=yR;
    }
}

// Comb filter
static void dspProcessCombFilter(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst||!inst->enabled) return;
    combState_t *st = (combState_t*)inst->state;
    if (!st) return;
    float timeMs = inst->params.comb.timeMs;
    uint32_t delaySamples=(uint32_t)(timeMs*inst->sampleRate/1000.0f);
    if (delaySamples>=st->bufSize) delaySamples=st->bufSize-1;
    float fb=inst->params.comb.feedback;
    float mix=inst->params.comb.mix;
    for (uint32_t i=0;i<frames;i++) {
        uint32_t rp=(st->writePos+st->bufSize-delaySamples)%st->bufSize;
        float dL=st->bufL[rp], dR=st->bufR[rp];
        float inL=bufL[i], inR=bufR[i];
        // compute wet signals and apply stereo widening
        float outL = inL*(1-mix)+dL*mix;
        float outR = inR*(1-mix)+dR*mix;
        float mid = 0.5f * (outL + outR);
        float side = 0.5f * (outL - outR);
        const float widthFactor = 1.5f; // stereo width multiplier
        outL = mid + side * widthFactor;
        outR = mid - side * widthFactor;
        bufL[i]=outL;
        bufR[i]=outR;
        st->bufL[st->writePos]=inL + dL*fb;
        st->bufR[st->writePos]=inR + dR*fb;
        if (++st->writePos>=st->bufSize) st->writePos=0;
    }
}

// Bitcrusher
static void dspProcessBitcrusher(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames)
{
    if (!inst||!inst->enabled) return;
    bitcrusherState_t *st = (bitcrusherState_t*)inst->state;
    if (!st) return;
    float rateHz = inst->params.bitcrusher.rateHz;
    uint32_t step = (uint32_t)(inst->sampleRate / (rateHz>0?rateHz:1));
    if (step<1) step=1;
    float levels = powf(2.0f, inst->params.bitcrusher.bitDepth) - 1.0f;
    for (uint32_t i=0;i<frames;i++) {
        if (st->counter==0) {
            st->counter = step;
            st->heldL = floorf(bufL[i]*levels + 0.5f)/levels;
            st->heldR = floorf(bufR[i]*levels + 0.5f)/levels;
        }
        st->counter--;
        bufL[i] = st->heldL;
        bufR[i] = st->heldR;
    }
} 
