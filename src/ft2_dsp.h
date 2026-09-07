#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of DSP effect slots per channel */
#define DSP_MAX_SLOTS 4

/* -------------------------------------------------------------------------- */
/*                             Effect type enum                               */
/* -------------------------------------------------------------------------- */

typedef enum
{
    DSP_TYPE_NONE = 0,
    DSP_TYPE_GAINER,
    DSP_TYPE_COMPRESSOR,
    DSP_TYPE_LIMITER,
    DSP_TYPE_DELAY,
    DSP_TYPE_REVERB,
    DSP_TYPE_CHORUS,
    DSP_TYPE_FLANGER,
    DSP_TYPE_PHASER,
    DSP_TYPE_DRIVE,
    DSP_TYPE_AMP_SIM,
    DSP_TYPE_EQ_5BAND,
    DSP_TYPE_FILTER,
    DSP_TYPE_COMB_FILTER,
    DSP_TYPE_BITCRUSHER,

    DSP_TYPE_COUNT /* keep last */
} dspEffectType_t;

/* -------------------------------------------------------------------------- */
/*                         Effect parameter structures                        */
/* -------------------------------------------------------------------------- */

/* Gainer (simple linear gain) */
typedef struct
{
    float gain; /* 0.0 (mute) .. 2.0 (+6dB) */
} dspGainerParams_t;

/* Serialized parameter layouts for the implemented effects. */
typedef struct { float thresholdDb; float ratio; float attackMs; float releaseMs; float makeupDb; } dspCompressorParams_t;
typedef struct { float thresholdDb; float releaseMs; } dspLimiterParams_t;
typedef struct { float timeMs; float feedback; float tone; float mix; float diffusionMs; float diffusionMix; float pingPong; float character; float sync; float width; } dspDelayParams_t;
typedef struct { float sizeMs; float feedback; float damp; float mix; float character; float convMix; float irIndex; } dspReverbParams_t;
typedef struct { float depthMs; float rateHz; float feedback; float mix; float stereoOffset; } dspChorusParams_t;
typedef struct { float depthMs; float rateHz; float feedback; float mix; } dspFlangerParams_t;
typedef struct { float depth; float rateHz; float mix; float feedback; float stereoOffset; } dspPhaserParams_t;
typedef struct { float gain; float tone; float mix; } dspDriveParams_t;
typedef struct { float drive; float tone; float mix; } dspAmpSimParams_t;
typedef struct { float gains[5]; } dspEQ5Params_t;
typedef struct { float cutoffHz; float resonance; float mode; } dspFilterParams_t;
typedef struct { float timeMs; float feedback; float mix; } dspCombParams_t;
typedef struct { float rateHz; float bitDepth; } dspBitcrusherParams_t;

/* -------------------------------------------------------------------------- */
/*                         Generic effect instance struct                     */
/* -------------------------------------------------------------------------- */

struct dspEffectInstance_t; /* forward */

typedef void (*dspProcessFunc)(struct dspEffectInstance_t *inst, float *bufL, float *bufR, uint32_t frames);

typedef struct dspEffectInstance_t
{
    dspEffectType_t type;
    bool enabled;
    uint32_t sampleRate;      /* needed for effects with time parameters */

    /* Runtime processing callback */
    dspProcessFunc process;

    /* Parameter unions (public) */
    union
    {
        dspGainerParams_t     gainer;
        dspCompressorParams_t compressor;
        dspLimiterParams_t    limiter;
        dspDelayParams_t      delay;
        dspReverbParams_t     reverb;
        dspChorusParams_t     chorus;
        dspFlangerParams_t    flanger;
        dspPhaserParams_t     phaser;
        dspDriveParams_t      drive;
        dspAmpSimParams_t     ampSim;
        dspEQ5Params_t        eq5;
        dspFilterParams_t     filter;
        dspCombParams_t       comb;
        dspBitcrusherParams_t bitcrusher;
    } params;

    /* Pointer to effect-specific internal state (allocated in init, optional) */
    void *state;
} dspEffectInstance_t;

/* -------------------------------------------------------------------------- */
/*                               API functions                                */
/* -------------------------------------------------------------------------- */

/* Initialize an effect instance with default parameters. Returns true on success. */
bool dspInitEffect(dspEffectInstance_t *inst, dspEffectType_t type, uint32_t sampleRate);

/* Release resources associated with an effect instance (if any) */
void dspFreeEffect(dspEffectInstance_t *inst);
void dspResetEffectState(dspEffectInstance_t *inst);

/* Utility: clamp parameter helpers */
static inline float dspClampf(float v, float lo, float hi) { return (v < lo) ? lo : (v > hi ? hi : v); }

/* Process a chain of effects on a stereo buffer in-place */
void dspProcessChain(dspEffectInstance_t *effects, float *bufL, float *bufR, uint32_t frames);

extern dspEffectInstance_t masterEffects[DSP_MAX_SLOTS];

/* ---------------- Parameter descriptor ---------------- */

typedef struct
{
    const char *name;
    float min;
    float max;
    float def;
    float step;
} dspParamInfo_t;

/* Retrieve parameter info list for an effect type (returns pointer and sets numParams) */
const dspParamInfo_t *dspGetParamInfo(dspEffectType_t type, int *numParams);

/* Helper: return human-readable name for an effect instance */
const char *getEffectName(dspEffectInstance_t *e);

/* Helper: return pointer to parameter value by index for an effect instance */
float *getParamPtr(dspEffectInstance_t *eff, int idx); 
