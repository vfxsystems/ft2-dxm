// C++ wrapper for Tunefish4 synth engine to provide C interface
#include "tunefish4/Source/runtime/system.hpp"
#include "tunefish4/Source/synth/tf4.hpp"
#include "tunefish4/Source/factorypatches.hpp"
#include <cstdio>
#include <cstdlib>

// Debug flag for wrapper
//#define DEBUG_TF_WRAPPER 1

#if DEBUG_TF_WRAPPER
#define TF_DEBUG(fmt, ...) printf("[TF_WRAPPER] " fmt "\n", ##__VA_ARGS__)
#else
#define TF_DEBUG(fmt, ...)
#endif

// Global effect function tables - needed to avoid static linkage issues
extern "C" {

// Effect create functions
eTfEffect* tf_effect_create(int effectType) {
    TF_DEBUG("tf_effect_create() called with type=%d", effectType);
    
    switch(effectType) {
        case 1: // FX_DISTORTION
            return eTfEffectDistortionCreate();
        case 2: // FX_DELAY  
            return eTfEffectDelayCreate();
        case 3: // FX_CHORUS
            return eTfEffectChorusCreate();
        case 4: // FX_FLANGER
            return eTfEffectFlangerCreate();
        case 5: // FX_REVERB
            return eTfEffectReverbCreate();
        case 6: // FX_FORMANT
            return eTfEffectFormantCreate();
        case 7: // FX_EQ
            return eTfEffectEqCreate();
        default:
            TF_DEBUG("Unknown effect type: %d", effectType);
            return nullptr;
    }
}

// Effect delete functions
void tf_effect_delete(int effectType, eTfEffect* fx) {
    TF_DEBUG("tf_effect_delete() called with type=%d, fx=%p", effectType, fx);
    
    if (!fx) return;
    
    switch(effectType) {
        case 1: // FX_DISTORTION
            eTfEffectDistortionDelete(fx);
            break;
        case 2: // FX_DELAY
            eTfEffectDelayDelete(fx);
            break;
        case 3: // FX_CHORUS
            eTfEffectChorusDelete(fx);
            break;
        case 4: // FX_FLANGER
            eTfEffectFlangerDelete(fx);
            break;
        case 5: // FX_REVERB
            eTfEffectReverbDelete(fx);
            break;
        case 6: // FX_FORMANT
            eTfEffectFormantDelete(fx);
            break;
        case 7: // FX_EQ
            eTfEffectEqDelete(fx);
            break;
        default:
            TF_DEBUG("Unknown effect type for deletion: %d", effectType);
            break;
    }
}

// Effect process functions
void tf_effect_process(int effectType, eTfEffect* fx, eTfSynth* synth, eTfInstrument* instr, float** signal, unsigned int len) {
    if (!fx || !synth || !instr) return;
    
    switch(effectType) {
        case 1: // FX_DISTORTION
            eTfEffectDistortionProcess(fx, *synth, *instr, signal, len);
            break;
        case 2: // FX_DELAY
            eTfEffectDelayProcess(fx, *synth, *instr, signal, len);
            break;
        case 3: // FX_CHORUS
            eTfEffectChorusProcess(fx, *synth, *instr, signal, len);
            break;
        case 4: // FX_FLANGER
            eTfEffectFlangerProcess(fx, *synth, *instr, signal, len);
            break;
        case 5: // FX_REVERB
            eTfEffectReverbProcess(fx, *synth, *instr, signal, len);
            break;
        case 6: // FX_FORMANT
            eTfEffectFormantProcess(fx, *synth, *instr, signal, len);
            break;
        case 7: // FX_EQ
            eTfEffectEqProcess(fx, *synth, *instr, signal, len);
            break;
        default:
            break;
    }
}

void* tf_synth_create(void) {
    TF_DEBUG("tf_synth_create() called");
    try {
        eTfSynth* synth = new eTfSynth();
        TF_DEBUG("Created Tunefish synth at %p", synth);
        return synth;
    } catch (...) {
        TF_DEBUG("ERROR: Exception in tf_synth_create()");
        return nullptr;
    }
}

void tf_synth_destroy(void* synth) {
    TF_DEBUG("tf_synth_destroy() called with synth=%p", synth);
    if (synth) {
        try {
            delete static_cast<eTfSynth*>(synth);
            TF_DEBUG("Destroyed Tunefish synth");
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_synth_destroy()");
        }
    }
}

void tf_synth_init(void* synth, unsigned int sampleRate) {
    TF_DEBUG("tf_synth_init() called with synth=%p, sampleRate=%u", synth, sampleRate);
    if (synth) {
        try {
            eTfSynth* tf_synth = static_cast<eTfSynth*>(synth);
            tf_synth->sampleRate = sampleRate;
            eTfSynthInit(*tf_synth);
            TF_DEBUG("Initialized Tunefish synth with sample rate %u", sampleRate);
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_synth_init()");
        }
    }
}

// Global flag to control preset loading during instrument creation
static bool g_skipPresetOnCreate = false;

// Function to check if persistent parameters exist for an instrument
extern bool ft2_synth_has_persistent_state(int instrID);
extern void ft2_synth_get_all_persistent_params(int instrID, float* params, int paramCount);

void tf_set_skip_preset_on_create(bool skip) {
    g_skipPresetOnCreate = skip;
    TF_DEBUG("Set skip preset on create: %s", skip ? "true" : "false");
}

void* tf_instrument_create(void* synth, int instrID) {
    TF_DEBUG("tf_instrument_create() called with synth=%p, instrID=%d, skipPreset=%s", synth, instrID, g_skipPresetOnCreate ? "true" : "false");
    if (!synth) {
        TF_DEBUG("ERROR: null synth in tf_instrument_create()");
        return nullptr;
    }
    
    try {
        eTfInstrument* instr = new eTfInstrument();
        eTfInstrumentInit(*instr);
        
        // Check if we have persistent parameters for this instrument
        if (ft2_synth_has_persistent_state(instrID)) {
            // Apply persistent parameters instead of factory preset
            float params[TF_PARAM_COUNT];
            ft2_synth_get_all_persistent_params(instrID, params, TF_PARAM_COUNT);
            for (int p = 0; p < TF_PARAM_COUNT; p++) {
                instr->params[p] = params[p];
            }
            TF_DEBUG("Applied persistent parameters to new instrument %d", instrID);
        }
        // Only load factory preset if not skipping and no persistent parameters
        else if (!g_skipPresetOnCreate) {
            // Load a sensible default factory preset (preset 0: "Saaaaw")
            if (TF_FACTORY_PATCH_COUNT > 0) {
                const double* presetParams = TF_FACTORY_PATCHES[0];
                for (int i = 0; i < TF_PARAM_COUNT && i < TF_FACTORY_PATCH_PARAMCOUNT; i++) {
                    instr->params[i] = static_cast<float>(presetParams[i]);
                }
                TF_DEBUG("Created instrument at %p with factory preset 0 (\"%s\")", 
                         instr, TF_FACTORY_PATCH_NAMES[0]);
            } else {
                // Fallback to basic defaults if no factory presets available
                for (int i = 0; i < TF_PARAM_COUNT; i++) {
                    instr->params[i] = 0.0f;
                }
                
                // Set some basic defaults for immediate playability
                instr->params[TF_GLOBAL_GAIN] = 0.5f;          // 50% gain
                instr->params[TF_GEN_VOLUME] = 0.7f;           // 70% generator volume
                instr->params[TF_GEN_BANDWIDTH] = 0.5f;        // 50% bandwidth
                instr->params[TF_GEN_NUMHARMONICS] = 0.3f;     // ~30% harmonics
                instr->params[TF_GEN_POLYPHONY] = 0.5f;        // 8 voices
                instr->params[TF_GEN_OCTAVE] = 4.0f / 8.0f;    // middle octave
                instr->params[TF_ADSR1_ATTACK] = 0.01f;        // quick attack
                instr->params[TF_ADSR1_DECAY] = 0.3f;          // medium decay
                instr->params[TF_ADSR1_SUSTAIN] = 0.6f;        // 60% sustain
                instr->params[TF_ADSR1_RELEASE] = 0.2f;        // medium release
                TF_DEBUG("Created instrument at %p with fallback default parameters", instr);
            }
        } else {
            // Skip preset loading - leave parameters at default 0.0f
            TF_DEBUG("Created instrument at %p with skipped preset loading", instr);
        }
        
        return instr;
    } catch (...) {
        TF_DEBUG("ERROR: Exception in tf_instrument_create()");
        return nullptr;
    }
}

void tf_instrument_destroy(void* instrument) {
    TF_DEBUG("tf_instrument_destroy() called with instrument=%p", instrument);
    if (instrument) {
        try {
            eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
            eTfInstrumentFree(*instr);
            delete instr;
            TF_DEBUG("Destroyed instrument");
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_instrument_destroy()");
        }
    }
}

void tf_instrument_note_on(void* instrument, int note, int velocity) {
    if (instrument) {
        try {
            eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
            eTfInstrumentNoteOn(*instr, note, velocity);
            TF_DEBUG("Note ON: note=%d, velocity=%d", note, velocity);
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_instrument_note_on()");
        }
    }
}

void tf_instrument_note_off(void* instrument, int note) {
    if (instrument) {
        try {
            eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
            eTfInstrumentNoteOff(*instr, note);
            TF_DEBUG("Note OFF: note=%d", note);
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_instrument_note_off()");
        }
    }
}

void tf_instrument_set_param(void* instrument, int param, float value) {
    if (instrument && param >= 0 && param < TF_PARAM_COUNT) {
        try {
            eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
            instr->params[param] = value;
            TF_DEBUG("Set param %d = %f", param, value);
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_instrument_set_param()");
        }
    }
}

float tf_instrument_get_param(void* instrument, int param) {
    if (instrument && param >= 0 && param < TF_PARAM_COUNT) {
        try {
            eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
            return instr->params[param];
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_instrument_get_param()");
        }
    }
    return 0.0f;
}

void tf_instrument_process(void* synth, void* instrument, float** outputs, unsigned int frameSize) {
    if (synth && instrument) {
        try {
            eTfSynth* tf_synth = static_cast<eTfSynth*>(synth);
            eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
            
            float peak = eTfInstrumentProcess(*tf_synth, *instr, outputs, frameSize);
            // TF_DEBUG("Processed %u frames, peak=%f", frameSize, peak);
        } catch (...) {
            TF_DEBUG("ERROR: Exception in tf_instrument_process()");
        }
    }
}

void tf_instrument_send_midi(void* instrument, unsigned char status, unsigned char data1, unsigned char data2) {
    if (!instrument) return;
    
    try {
        eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
        
        unsigned char msgType = status & 0xF0;
        unsigned char channel = status & 0x0F;
        
        switch (msgType) {
            case 0x90: // Note On
                if (data2 > 0) {
                    eTfInstrumentNoteOn(*instr, data1, data2);
                    TF_DEBUG("MIDI Note On: note=%d, vel=%d", data1, data2);
                } else {
                    eTfInstrumentNoteOff(*instr, data1);
                    TF_DEBUG("MIDI Note Off (vel=0): note=%d", data1);
                }
                break;
                
            case 0x80: // Note Off
                eTfInstrumentNoteOff(*instr, data1);
                TF_DEBUG("MIDI Note Off: note=%d", data1);
                break;
                
            case 0xB0: // Control Change
                TF_DEBUG("MIDI CC: controller=%d, value=%d", data1, data2);
                // Map FT2 synth editor CCs to Tunefish parameters
                // Note: These CC numbers come from the CCParam enum in ft2_synth_ed.c
                switch (data1) {
                    // Global controls
                    case 0: // CC_POLY
                        instr->params[TF_GEN_POLYPHONY] = data2 / 127.0f;
                        break;
                    case 65: // CC_PITCH_UP (treating as pitch shift) - using higher CC numbers to avoid conflict
                    case 66: // CC_PITCH_DOWN  
                        instr->params[TF_GEN_OCTAVE] = (data2 / 127.0f) * 8.0f; // 0-8 octave range
                        break;
                        
                    // Generator controls
                    case 3: // CC_GEN_VOLUME
                        instr->params[TF_GEN_VOLUME] = data2 / 127.0f;
                        break;
                    case 4: // CC_GEN_PANNING
                        instr->params[TF_GEN_PANNING] = data2 / 127.0f;
                        break;
                    case 5: // CC_GEN_SPREAD
                        instr->params[TF_GEN_SPREAD] = data2 / 127.0f;
                        break;
                    case 6: // CC_GEN_BANDWIDTH
                        instr->params[TF_GEN_BANDWIDTH] = data2 / 127.0f;
                        break;
                    case 7: // CC_GEN_DAMP
                        instr->params[TF_GEN_DAMP] = data2 / 127.0f;
                        break;
                    case 8: // CC_GEN_HARMONICS
                        instr->params[TF_GEN_NUMHARMONICS] = data2 / 127.0f;
                        break;
                    case 9: // CC_GEN_DRIVE
                        instr->params[TF_GEN_DRIVE] = data2 / 127.0f;
                        break;
                    case 10: // CC_GEN_SCALE
                        instr->params[TF_GEN_SCALE] = data2 / 127.0f;
                        break;
                    case 11: // CC_GEN_MODULATION
                        instr->params[TF_GEN_MODULATION] = data2 / 127.0f;
                        break;
                    case 12: // CC_GEN_NOISE
                        instr->params[TF_NOISE_AMOUNT] = data2 / 127.0f;
                        break;
                    case 13: // CC_GEN_NOISE_FREQ
                        instr->params[TF_NOISE_FREQ] = data2 / 127.0f;
                        break;
                    case 14: // CC_GEN_NOISE_BW
                        instr->params[TF_NOISE_BW] = data2 / 127.0f;
                        break;
                        
                    // Filter controls
                    case 15: // CC_FILTER_LP_FREQ
                        instr->params[TF_LP_FILTER_CUTOFF] = data2 / 127.0f;
                        break;
                    case 16: // CC_FILTER_LP_RES
                        instr->params[TF_LP_FILTER_RESONANCE] = data2 / 127.0f;
                        break;
                    case 17: // CC_FILTER_HP_FREQ
                        instr->params[TF_HP_FILTER_CUTOFF] = data2 / 127.0f;
                        break;
                    case 18: // CC_FILTER_HP_RES
                        instr->params[TF_HP_FILTER_RESONANCE] = data2 / 127.0f;
                        break;
                    case 19: // CC_FILTER_BP_FREQ
                        instr->params[TF_BP_FILTER_CUTOFF] = data2 / 127.0f;
                        break;
                    case 20: // CC_FILTER_BP_RES
                        instr->params[TF_BP_FILTER_Q] = data2 / 127.0f;
                        break;
                    case 21: // CC_FILTER_NT_FREQ
                        instr->params[TF_NT_FILTER_CUTOFF] = data2 / 127.0f;
                        break;
                    case 22: // CC_FILTER_NT_RES
                        instr->params[TF_NT_FILTER_Q] = data2 / 127.0f;
                        break;
                        
                    // LFO controls
                    case 23: // CC_LFO1_RATE
                        instr->params[TF_LFO1_RATE] = data2 / 127.0f;
                        break;
                    case 24: // CC_LFO1_DEPTH
                        instr->params[TF_LFO1_DEPTH] = data2 / 127.0f;
                        break;
                    case 25: // CC_LFO1_SYNC
                        instr->params[TF_LFO1_SYNC] = (data2 > 63) ? 1.0f : 0.0f;
                        break;
                    case 26: // CC_LFO2_RATE
                        instr->params[TF_LFO2_RATE] = data2 / 127.0f;
                        break;
                    case 27: // CC_LFO2_DEPTH
                        instr->params[TF_LFO2_DEPTH] = data2 / 127.0f;
                        break;
                    case 28: // CC_LFO2_SYNC
                        instr->params[TF_LFO2_SYNC] = (data2 > 63) ? 1.0f : 0.0f;
                        break;
                        
                    // ADSR controls
                    case 29: // CC_ADSR1_A
                        instr->params[TF_ADSR1_ATTACK] = data2 / 127.0f;
                        break;
                    case 30: // CC_ADSR1_D
                        instr->params[TF_ADSR1_DECAY] = data2 / 127.0f;
                        break;
                    case 31: // CC_ADSR1_S
                        instr->params[TF_ADSR1_SUSTAIN] = data2 / 127.0f;
                        break;
                    case 32: // CC_ADSR1_R
                        instr->params[TF_ADSR1_RELEASE] = data2 / 127.0f;
                        break;
                    case 75: // CC_ADSR1_SLOPE
                        instr->params[TF_ADSR1_SLOPE] = data2 / 127.0f;
                        break;
                    case 33: // CC_ADSR2_A
                        instr->params[TF_ADSR2_ATTACK] = data2 / 127.0f;
                        break;
                    case 34: // CC_ADSR2_D
                        instr->params[TF_ADSR2_DECAY] = data2 / 127.0f;
                        break;
                    case 35: // CC_ADSR2_S
                        instr->params[TF_ADSR2_SUSTAIN] = data2 / 127.0f;
                        break;
                    case 36: // CC_ADSR2_R
                        instr->params[TF_ADSR2_RELEASE] = data2 / 127.0f;
                        break;
                    case 76: // CC_ADSR2_SLOPE
                        instr->params[TF_ADSR2_SLOPE] = data2 / 127.0f;
                        break;
                        
                    // FX controls  
                    case 37: // CC_FX_FLANGER_LFO
                        instr->params[TF_FLANGER_LFO] = data2 / 127.0f;
                        break;
                    case 38: // CC_FX_FLANGER_FREQ
                        instr->params[TF_FLANGER_FREQUENCY] = data2 / 127.0f;
                        break;
                    case 39: // CC_FX_FLANGER_AMP
                        instr->params[TF_FLANGER_AMPLITUDE] = data2 / 127.0f;
                        break;
                    case 40: // CC_FX_FLANGER_WET
                        instr->params[TF_FLANGER_WET] = data2 / 127.0f;
                        break;
                    case 41: // CC_FX_REVERB_ROOM_SZ
                        instr->params[TF_REVERB_ROOMSIZE] = data2 / 127.0f;
                        break;
                    case 42: // CC_FX_REVERB_DAMP
                        instr->params[TF_REVERB_DAMP] = data2 / 127.0f;
                        break;
                    case 43: // CC_FX_REVERB_WET
                        instr->params[TF_REVERB_WET] = data2 / 127.0f;
                        break;
                    case 44: // CC_FX_REVERB_WIDTH
                        instr->params[TF_REVERB_WIDTH] = data2 / 127.0f;
                        break;
                    case 45: // CC_FX_DELAY_LEFT
                        instr->params[TF_DELAY_LEFT] = data2 / 127.0f;
                        break;
                    case 46: // CC_FX_DELAY_RIGHT
                        instr->params[TF_DELAY_RIGHT] = data2 / 127.0f;
                        break;
                    case 47: // CC_FX_DELAY_DECAY
                        instr->params[TF_DELAY_DECAY] = data2 / 127.0f;
                        break;
                    case 48: // CC_FX_EQ_BASS
                        instr->params[TF_EQ_LOW] = data2 / 127.0f;
                        break;
                    case 49: // CC_FX_EQ_MID
                        instr->params[TF_EQ_MID] = data2 / 127.0f;
                        break;
                    case 50: // CC_FX_EQ_TREBLE
                        instr->params[TF_EQ_HIGH] = data2 / 127.0f;
                        break;
                    case 51: // CC_FX_CHORUS_FREQ
                        instr->params[TF_CHORUS_RATE] = data2 / 127.0f;
                        break;
                    case 52: // CC_FX_CHORUS_DEPTH
                        instr->params[TF_CHORUS_DEPTH] = data2 / 127.0f;
                        break;
                    case 53: // CC_FX_CHORUS_GAIN
                        instr->params[TF_CHORUS_GAIN] = data2 / 127.0f;
                        break;
                    case 54: // CC_FX_FORMANT_WET
                        instr->params[TF_FORMANT_WET] = data2 / 127.0f;
                        break;
                    case 55: // CC_FX_FORMANT_A
                        instr->params[TF_FORMANT_MODE] = 0.0f; // A vowel = mode 0
                        break;
                    case 56: // CC_FX_FORMANT_E
                        instr->params[TF_FORMANT_MODE] = 0.25f; // E vowel = mode 1
                        break;
                    case 57: // CC_FX_FORMANT_I
                        instr->params[TF_FORMANT_MODE] = 0.5f; // I vowel = mode 2
                        break;
                    case 58: // CC_FX_FORMANT_O
                        instr->params[TF_FORMANT_MODE] = 0.75f; // O vowel = mode 3
                        break;
                    case 59: // CC_FX_FORMANT_U
                        instr->params[TF_FORMANT_MODE] = 1.0f; // U vowel = mode 4
                        break;
                    case 60: // CC_FX_DISTORTION_AMOUNT
                        instr->params[TF_DISTORT_AMOUNT] = data2 / 127.0f;
                        break;
                    
                    // Standard MIDI CCs (for compatibility)
                    case 74: // Standard MIDI filter cutoff
                        instr->params[TF_LP_FILTER_CUTOFF] = data2 / 127.0f;
                        break;
                    case 71: // Standard MIDI filter resonance
                        instr->params[TF_LP_FILTER_RESONANCE] = data2 / 127.0f;
                        break;
                    case 1:  // Standard MIDI mod wheel
                        eTfInstrumentModWheel(*instr, data2 / 127.0f);
                        break;
                }
                TF_DEBUG("Mapped CC %d (value %d) to Tunefish parameter", data1, data2);
                break;
                
            case 0xC0: // Program Change
                TF_DEBUG("MIDI Program Change: program=%d", data1);
                // Could load different presets here
                break;
                
            case 0xE0: // Pitch Bend
                {
                    int pitchBend = (data2 << 7) | data1;
                    float bendAmount = (pitchBend - 8192) / 8192.0f; // -1 to +1
                    eTfInstrumentPitchBend(*instr, bendAmount * 2.0f, 0.0f); // ±2 semitones
                    TF_DEBUG("MIDI Pitch Bend: amount=%f", bendAmount);
                }
                break;
        }
    } catch (...) {
        TF_DEBUG("ERROR: Exception in tf_instrument_send_midi()");
    }
}

// Preset loading functions
int tf_get_factory_preset_count(void) {
    return TF_FACTORY_PATCH_COUNT;
}

const char* tf_get_factory_preset_name(int index) {
    if (index >= 0 && index < TF_FACTORY_PATCH_COUNT) {
        return TF_FACTORY_PATCH_NAMES[index];
    }
    return nullptr;
}

int tf_instrument_load_factory_preset(void* instrument, int index) {
    TF_DEBUG("tf_instrument_load_factory_preset() called with instrument=%p, index=%d", instrument, index);
    
    if (!instrument) {
        TF_DEBUG("ERROR: null instrument in tf_instrument_load_factory_preset()");
        return 0;
    }
    
    if (index < 0 || index >= TF_FACTORY_PATCH_COUNT) {
        TF_DEBUG("ERROR: Invalid preset index %d (must be 0-%d)", index, TF_FACTORY_PATCH_COUNT-1);
        return 0;
    }
    
    try {
        eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
        
        // Load the factory preset parameters
        const double* presetParams = TF_FACTORY_PATCHES[index];
        for (int i = 0; i < TF_PARAM_COUNT && i < TF_FACTORY_PATCH_PARAMCOUNT; i++) {
            instr->params[i] = static_cast<float>(presetParams[i]);
        }
        
        TF_DEBUG("Loaded factory preset %d (\"%s\") into instrument %p", 
                 index, TF_FACTORY_PATCH_NAMES[index], instr);
        return 1; // Success
        
    } catch (...) {
        TF_DEBUG("ERROR: Exception in tf_instrument_load_factory_preset()");
        return 0;
    }
}

// Return number of active voices currently playing on the instrument (note on & playing)
int tf_instrument_get_active_voice_count(void* instrument) {
    if (!instrument) return 0;
    try {
        eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
        int count = 0;
        for (int i = 0; i < TF_MAXVOICES; ++i) {
            if (instr->voice[i].playing)
                ++count;
        }
        return count;
    } catch (...) {
        TF_DEBUG("ERROR: Exception in tf_instrument_get_active_voice_count()");
        return 0;
    }
}

// Exposed panic to stop all voices in an instrument
void tf_instrument_panic(void* instrument) {
    if (!instrument) return;
    try {
        eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
        eTfInstrumentPanic(*instr);
    } catch (...) {
        TF_DEBUG("ERROR: Exception in tf_instrument_panic()");
    }
}

} // extern "C"

// Voice data access functions for waveform view
extern "C" {

// Get the latest triggered voice from an instrument
eTfVoice* tf_instrument_get_latest_voice(void* instrument) {
    if (!instrument) {
        TF_DEBUG("tf_instrument_get_latest_voice: null instrument");
        return NULL;
    }
    
    eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
    eTfVoice* voice = instr->latestTriggeredVoice;
    
    TF_DEBUG("tf_instrument_get_latest_voice: instrument=%p, voice=%p", instrument, voice);
    return voice;
}

// Get frequency table from a voice's generator
eF32* tf_voice_get_freq_table(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_freq_table: null voice");
        return NULL;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eF32* freqTable = v->generator.freqTable;
    
    TF_DEBUG("tf_voice_get_freq_table: voice=%p, freqTable=%p", voice, freqTable);
    return freqTable;
}

// Get modulated frequency table from a voice's generator
eF32* tf_voice_get_freq_mod_table(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_freq_mod_table: null voice");
        return NULL;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eF32* freqModTable = v->generator.freqModTable;
    
    TF_DEBUG("tf_voice_get_freq_mod_table: voice=%p, freqModTable=%p", voice, freqModTable);
    return freqModTable;
}

// Get result table from a voice's generator
eF32* tf_voice_get_result_table(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_result_table: null voice");
        return NULL;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eF32* resultTable = v->generator.resultTable;
    
    TF_DEBUG("tf_voice_get_result_table: voice=%p, resultTable=%p", voice, resultTable);
    return resultTable;
}

// Get modulation value from a voice's generator
eF32 tf_voice_get_modulation(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_modulation: null voice");
        return 0.0f;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eF32 modulation = v->generator.modulation;
    
    TF_DEBUG("tf_voice_get_modulation: voice=%p, modulation=%.3f", voice, modulation);
    return modulation;
}

// Get mod matrix from a voice
eTfModMatrix* tf_voice_get_mod_matrix(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_mod_matrix: null voice");
        return NULL;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eTfModMatrix* modMatrix = &v->modMatrix;
    
    TF_DEBUG("tf_voice_get_mod_matrix: voice=%p, modMatrix=%p", voice, modMatrix);
    return modMatrix;
}

// Get generator from a voice
eTfGenerator* tf_voice_get_generator(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_generator: null voice");
        return NULL;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eTfGenerator* generator = &v->generator;
    
    TF_DEBUG("tf_voice_get_generator: voice=%p, generator=%p", voice, generator);
    return generator;
}

// Get drive parameter from an instrument
eF32 tf_instrument_get_drive_param(void* instrument) {
    if (!instrument) {
        TF_DEBUG("tf_instrument_get_drive_param: null instrument");
        return 0.0f;
    }
    
    eTfInstrument* instr = static_cast<eTfInstrument*>(instrument);
    eF32 drive = instr->params[TF_GEN_DRIVE];
    
    TF_DEBUG("tf_instrument_get_drive_param: instrument=%p, drive=%.3f", instrument, drive);
    return drive;
}

// Check if a voice is currently playing
eBool tf_voice_is_playing(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_is_playing: null voice");
        return eFALSE;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eBool playing = v->playing;
    
    TF_DEBUG("tf_voice_is_playing: voice=%p, playing=%s", voice, playing ? "true" : "false");
    return playing;
}

// Get current frequency from a voice
eF32 tf_voice_get_current_freq(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_current_freq: null voice");
        return 0.0f;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eF32 freq = v->currentFreq;
    
    TF_DEBUG("tf_voice_get_current_freq: voice=%p, freq=%.3f", voice, freq);
    return freq;
}

// Get current note from a voice
eS32 tf_voice_get_current_note(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_current_note: null voice");
        return 0;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eS32 note = v->currentNote;
    
    TF_DEBUG("tf_voice_get_current_note: voice=%p, note=%d", voice, note);
    return note;
}

// Get current velocity from a voice
eS32 tf_voice_get_current_velocity(void* voice) {
    if (!voice) {
        TF_DEBUG("tf_voice_get_current_velocity: null voice");
        return 0;
    }
    
    eTfVoice* v = static_cast<eTfVoice*>(voice);
    eS32 velocity = v->currentVelocity;
    
    TF_DEBUG("tf_voice_get_current_velocity: voice=%p, velocity=%d", voice, velocity);
    return velocity;
}

} // extern "C"
