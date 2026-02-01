#ifndef FT2_SYNTH_VARS_H
#define FT2_SYNTH_VARS_H

#ifdef __cplusplus
extern "C" {
#endif

// Global synth parameter variables (extern declarations)
extern int synthPoly;
extern int synthPitchUp;
extern int synthPitchDown;
extern int synthPatch;

// Generator parameters
extern int synthGenVolume;
extern int synthGenPanning;
extern int synthGenSpread;
extern int synthGenBandwidth;
extern int synthGenDamp;
extern int synthGenHarmonics;
extern int synthGenDrive;
extern int synthGenScale;
extern int synthGenModulation;
extern int synthGenNoise;
extern int synthGenNoiseFreq;
extern int synthGenNoiseBW;

// Filter parameters
extern int synthFilterLPFreq;
extern int synthFilterLPRes;
extern int synthFilterHPFreq;
extern int synthFilterHPRes;
extern int synthFilterBPFreq;
extern int synthFilterBPRes;
extern int synthFilterNTFreq;
extern int synthFilterNTRes;

// LFO parameters
extern int synthLFO1Rate;
extern int synthLFO1Depth;
extern int synthLFO2Rate;
extern int synthLFO2Depth;
extern bool synthLFO1Sync;
extern bool synthLFO2Sync;

// ADSR parameters
extern int adsr1A;
extern int adsr1D;
extern int adsr1S;
extern int adsr1R;
extern int adsr1Slope;
extern int adsr2A;
extern int adsr2D;
extern int adsr2S;
extern int adsr2R;
extern int adsr2Slope;

// Voice parameters
extern int synthUnisono;
extern int synthOctave;

// FX parameters
extern int fxFlangerLFO;
extern int fxFlangerFreq;
extern int fxFlangerAmp;
extern int fxFlangerWet;
extern int fxReverbRoomSz;
extern int fxReverbDamp;
extern int fxReverbWet;
extern int fxReverbWidth;
extern int fxDelayLeft;
extern int fxDelayRight;
extern int fxDelayDecay;
extern int fxEQBass;
extern int fxEQMid;
extern int fxEQTreble;
extern int fxChorusFreq;
extern int fxChorusDepth;
extern int fxChorusGain;
extern int fxFormantWet;
extern int fxFormantA;
extern int fxFormantE;
extern int fxFormantI;
extern int fxFormantO;
extern int fxFormantU;
extern int fxDistortionAmount;

#ifdef __cplusplus
}
#endif

#endif // FT2_SYNTH_VARS_H 