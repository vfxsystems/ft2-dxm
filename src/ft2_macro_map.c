#include "ft2_macro_map.h"
#include "ft2_structs.h"
#include "ft2_macromap.h"
#include "ft2_v2.h"
#include "ft2_ostirus.h"
#include <stdio.h>
#include <string.h>
// Define macroSys instance
macroSystem_t macroSys = { .showMapper = false, .currentInst = -1 };

// Flag for requesting UI sync from audio thread (thread-safe)
static volatile bool g_macroUiSyncRequested = false;
static volatile bool g_dspUiSyncRequested = false;
#ifndef MACRO_TARGET_TF4
#define MACRO_TARGET_TF4 1
#endif
#ifndef MACRO_CURVE_LINEAR
#define MACRO_CURVE_LINEAR 0
#endif

#include "ft2_gui.h"
#include "ft2_popup_list.h"
#include "ft2_pattern_ed.h"
#include "ft2_synth.h"
#include "ft2_replayer.h"
#include "ft2_mixer.h"

/* Auto-generated from TF4 enum list (keep in sync!) */
const char *const tf4_param_names[] = {
    "Global Gain",            // TF_GLOBAL_GAIN

    "Gen Bandwidth",
    "Gen NumHarm",
    "Gen Damp",
    "Gen Modulation",
    "Gen Volume",
    "Gen Panning",
    "Gen Slop",
    "Gen Octave",
    "Gen Glide",
    "Gen Detune",
    "Gen Frequency",
    "Gen Polyphony",
    "Gen Drive",
    "Gen Unisono",
    "Gen Spread",
    "Gen Scale",

    "Noise Amount",
    "Noise Freq",
    "Noise BW",

    "LP Filter On",
    "LP Cutoff",
    "LP Resonance",

    "HP Filter On",
    "HP Cutoff",
    "HP Resonance",

    "ADSR1 Attack",
    "ADSR1 Decay",
    "ADSR1 Sustain",
    "ADSR1 Release",
    "ADSR1 Slope",

    "ADSR2 Attack",
    "ADSR2 Decay",
    "ADSR2 Sustain",
    "ADSR2 Release",
    "ADSR2 Slope",

    "LFO1 Rate",
    "LFO1 Depth",
    "LFO1 Shape",
    "LFO1 Sync",

    "LFO2 Rate",
    "LFO2 Depth",
    "LFO2 Shape",
    "LFO2 Sync",

    /* Mod matrix slots 1-10 */
    "MM1 Source", "MM1 Mod", "MM1 Target",
    "MM2 Source", "MM2 Mod", "MM2 Target",
    "MM3 Source", "MM3 Mod", "MM3 Target",
    "MM4 Source", "MM4 Mod", "MM4 Target",
    "MM5 Source", "MM5 Mod", "MM5 Target",
    "MM6 Source", "MM6 Mod", "MM6 Target",
    "MM7 Source", "MM7 Mod", "MM7 Target",
    "MM8 Source", "MM8 Mod", "MM8 Target",
    "MM9 Source", "MM9 Mod", "MM9 Target",
    "MM10 Source", "MM10 Mod", "MM10 Target",

    /* Effect slots 1-10 */
    "Effect 1", "Effect 2", "Effect 3", "Effect 4", "Effect 5",
    "Effect 6", "Effect 7", "Effect 8", "Effect 9", "Effect 10",

    "Distort Amt",

    "Chorus Rate", "Chorus Depth",

    "Delay Left", "Delay Right", "Delay Decay",

    "Reverb Room", "Reverb Damp", "Reverb Wet", "Reverb Width",

    "Flanger LFO", "Flanger Freq", "Flanger Amp", "Flanger Wet",

    "Chorus Gain",

    "Formant Mode", "Formant Wet",

    "EQ Low", "EQ Mid", "EQ High",

    "PitchWheel Up", "PitchWheel Down",

    "BP Filter On", "BP Cutoff", "BP Q",
    "NT Filter On", "NT Cutoff", "NT Q"
};

const int tf4_param_name_count = sizeof(tf4_param_names) / sizeof(tf4_param_names[0]);

// Parameter scaling ranges (min, max) for each TF4 parameter
typedef struct {
    float min, max;
} tf4_param_range_t;

const tf4_param_range_t tf4_param_ranges[] = {
    {0.0f, 1.0f},     // TF_GLOBAL_GAIN
    
    {0.0f, 1.0f},     // Gen Bandwidth
    {0.0f, 1.0f},     // Gen NumHarm (normalized 0-1)
    {0.0f, 1.0f},     // Gen Damp
    {0.0f, 1.0f},     // Gen Modulation
    {0.0f, 1.0f},     // Gen Volume
    {0.0f, 1.0f},     // Gen Panning (normalized 0-1)
    {0.0f, 1.0f},     // Gen Slop
    {0.0f, 1.0f},     // Gen Octave (normalized 0-1, maps to 0-8)
    {0.0f, 1.0f},     // Gen Glide
    {0.0f, 1.0f},     // Gen Detune (normalized 0-1)
    {0.0f, 1.0f},     // Gen Frequency
    {0.0f, 1.0f},     // Gen Polyphony (normalized 0-1, maps to 0-15)
    {0.0f, 1.0f},     // Gen Drive
    {0.0f, 1.0f},     // Gen Unisono (normalized 0-1)
    {0.0f, 1.0f},     // Gen Spread
    {0.0f, 1.0f},     // Gen Scale
    
    {0.0f, 1.0f},     // Noise Amount
    {0.0f, 1.0f},     // Noise Freq
    {0.0f, 1.0f},     // Noise BW
    
    {0.0f, 1.0f},     // LP Filter On
    {0.0f, 1.0f},     // LP Cutoff
    {0.0f, 1.0f},     // LP Resonance
    
    {0.0f, 1.0f},     // HP Filter On
    {0.0f, 1.0f},     // HP Cutoff
    {0.0f, 1.0f},     // HP Resonance
    
    {0.0f, 1.0f},     // ADSR1 Attack
    {0.0f, 1.0f},     // ADSR1 Decay
    {0.0f, 1.0f},     // ADSR1 Sustain
    {0.0f, 1.0f},     // ADSR1 Release
    {0.0f, 1.0f},     // ADSR1 Slope
    
    {0.0f, 1.0f},     // ADSR2 Attack
    {0.0f, 1.0f},     // ADSR2 Decay
    {0.0f, 1.0f},     // ADSR2 Sustain
    {0.0f, 1.0f},     // ADSR2 Release
    {0.0f, 1.0f},     // ADSR2 Slope
    
    {0.0f, 1.0f},     // LFO1 Rate
    {0.0f, 1.0f},     // LFO1 Depth
    {0.0f, 1.0f},     // LFO1 Shape (normalized 0-1)
    {0.0f, 1.0f},     // LFO1 Sync
    
    {0.0f, 1.0f},     // LFO2 Rate
    {0.0f, 1.0f},     // LFO2 Depth
    {0.0f, 1.0f},     // LFO2 Shape (normalized 0-1)
    {0.0f, 1.0f},     // LFO2 Sync
    
    // Mod matrix slots (all normalized 0-1)
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM1
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM2
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM3
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM4
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM5
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM6
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM7
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM8
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM9
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},  // MM10
    
    // Effect slots (all normalized 0-1)
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},
    
    {0.0f, 1.0f},     // Distort Amt
    
    {0.0f, 1.0f}, {0.0f, 1.0f},     // Chorus Rate, Depth
    
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},     // Delay Left, Right, Decay
    
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},     // Reverb
    
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},     // Flanger
    
    {0.0f, 1.0f},     // Chorus Gain
    
    {0.0f, 1.0f}, {0.0f, 1.0f},     // Formant Mode, Wet
    
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},     // EQ Low, Mid, High
    
    {0.0f, 1.0f}, {0.0f, 1.0f},     // PitchWheel Up, Down (normalized 0-1, maps to 0-12)
    
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f},     // BP Filter
    {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}      // NT Filter
};

const int tf4_param_ranges_count = sizeof(tf4_param_ranges) / sizeof(tf4_param_ranges[0]);

bool ft2_macro_map_target_to_engine(uint8_t target, SynthEngineType *out)
{
    SynthEngineType engine;

    switch (target)
    {
        case MACRO_TARGET_TF4:     engine = SYNTH_TYPE_TUNEFISH4; break;
        case MACRO_TARGET_DEXED:   engine = SYNTH_TYPE_DEXED;     break;
        case MACRO_TARGET_V2:      engine = SYNTH_TYPE_V2;        break;
        case MACRO_TARGET_OSTIRUS: engine = SYNTH_TYPE_OSTIRUS;   break;
        default:                   return false;
    }

    if (out != NULL)
        *out = engine;

    return true;
}

bool ft2_macro_map_target_matches_instrument(uint8_t target, const instr_t *ins)
{
    if (ins == NULL)
        return false;

    switch (target)
    {
        case MACRO_TARGET_TF4:     return ins->useTF4;
        case MACRO_TARGET_DEXED:   return ins->useDexed;
        case MACRO_TARGET_V2:      return ins->useV2;
        case MACRO_TARGET_OSTIRUS: return ins->useOsTirus;
        default:                   return false;
    }
}

int ft2_macro_map_target_param_count(uint8_t target)
{
    switch (target)
    {
        case MACRO_TARGET_TF4:     return tf4_param_count();
        case MACRO_TARGET_DEXED:   return 156;
        case MACRO_TARGET_V2:      return ft2_v2_get_param_count();
        case MACRO_TARGET_OSTIRUS: return ft2_ostirus_get_param_count();
        default:                   return 0;
    }
}

const char *ft2_macro_map_target_param_name(uint8_t target, uint16_t paramId)
{
    static char dexedName[32];
    static char unknownName[32];
    static const char *const dexedNames[] = {
        "Op1 Freq", "Op1 Detune", "Op1 Level", "Op1 Attack", "Op1 Decay", "Op1 Release", "Op1 Sustain",
        "Op2 Freq", "Op2 Detune", "Op2 Level", "Op2 Attack", "Op2 Decay", "Op2 Release", "Op2 Sustain",
        "Op3 Freq", "Op3 Detune", "Op3 Level", "Op3 Attack", "Op3 Decay", "Op3 Release", "Op3 Sustain",
        "Op4 Freq", "Op4 Detune", "Op4 Level", "Op4 Attack", "Op4 Decay", "Op4 Release", "Op4 Sustain",
        "Algorithm", "Pitch EG Attack", "Pitch EG Decay", "Pitch EG Release", "Pitch EG Sustain",
        "LFO Rate", "LFO Depth", "LFO Delay", "LFO Sync", "LFO Wave", "Mod Sens", "Key Track", "Pitch Bend", "Portamento", "Brightness"
    };

    const int count = ft2_macro_map_target_param_count(target);
    if (count <= 0 || paramId >= (uint16_t)count)
        return "Unknown";

    switch (target)
    {
        case MACRO_TARGET_TF4:
            return tf4_param_name(paramId);

        case MACRO_TARGET_DEXED:
        {
            const int nameCount = (int)(sizeof (dexedNames) / sizeof (dexedNames[0]));
            if (paramId < (uint16_t)nameCount)
                return dexedNames[paramId];
            snprintf(dexedName, sizeof (dexedName), "DX Param %u", (unsigned)paramId);
            return dexedName;
        }

        case MACRO_TARGET_V2:
        {
            const char *name = ft2_v2_get_param_name(paramId);
            return name ? name : "V2 Param";
        }

        case MACRO_TARGET_OSTIRUS:
        {
            const char *name = ft2_ostirus_get_param_name(paramId);
            if (name != NULL)
                return name;
            snprintf(unknownName, sizeof (unknownName), "OsTIrus Param %u", (unsigned)paramId);
            return unknownName;
        }

        default:
            return "Unknown";
    }
}

void ft2_macro_map_sanitize_slot(instr_t *ins, int slot)
{
    if (ins == NULL || slot < 0 || slot >= FT2_MACRO_MAP_NUM_SLOTS)
        return;

    uint8_t *target = &ins->macroTargetType[slot];
    uint16_t *param = &ins->macroParamID[slot];
    uint8_t *scale = &ins->macroScale[slot];

    if (*scale >= NUM_MACRO_CURVES)
        *scale = MACRO_CURVE_LINEAR;

    if (*target == MACRO_TARGET_NONE)
    {
        *param = 0;
        *scale = MACRO_CURVE_LINEAR;
        return;
    }

    if (ft2_macro_map_target_to_engine(*target, NULL))
    {
        const int count = ft2_macro_map_target_param_count(*target);
        if (count <= 0)
        {
            *target = MACRO_TARGET_NONE;
            *param = 0;
            *scale = MACRO_CURVE_LINEAR;
        }
        else if (*param >= (uint16_t)count)
        {
            *param = 0;
        }
        return;
    }

    if (*target == MACRO_TARGET_DSP)
    {
        uint8_t scope = DSP_MACRO_SCOPE(*param);
        uint8_t dspSlot = DSP_MACRO_SLOT(*param);
        uint8_t paramIdx = DSP_MACRO_PARAM(*param);

        if (scope != DSP_MACRO_SCOPE_MASTER)
            scope = DSP_MACRO_SCOPE_PAIR;
        if (dspSlot >= DSP_MAX_SLOTS)
            dspSlot = 0;

        const dspEffectInstance_t *eff = (scope == DSP_MACRO_SCOPE_MASTER)
            ? &masterEffects[dspSlot]
            : &stereoMixerCh[0].effects[dspSlot];

        int numParams = 0;
        const dspParamInfo_t *pi = dspGetParamInfo(eff->type, &numParams);
        if (pi != NULL && numParams > 0 && paramIdx >= (uint8_t)numParams)
            paramIdx = 0;

        *param = DSP_MACRO_PARAMID(scope, dspSlot, paramIdx);
        return;
    }

    *target = MACRO_TARGET_NONE;
    *param = 0;
    *scale = MACRO_CURVE_LINEAR;
}

void ft2_macro_map_sanitize_instrument(instr_t *ins)
{
    if (ins == NULL)
        return;

    for (int i = 0; i < FT2_MACRO_MAP_NUM_SLOTS; i++)
        ft2_macro_map_sanitize_slot(ins, i);
}

bool ft2_macro_map_self_test(char *errBuf, size_t errBufSize)
{
    static const uint8_t synthTargets[] = {
        MACRO_TARGET_TF4,
        MACRO_TARGET_DEXED,
        MACRO_TARGET_V2,
        MACRO_TARGET_OSTIRUS
    };

    for (size_t i = 0; i < sizeof (synthTargets) / sizeof (synthTargets[0]); i++)
    {
        SynthEngineType engine;
        const uint8_t target = synthTargets[i];
        const int count = ft2_macro_map_target_param_count(target);
        if (!ft2_macro_map_target_to_engine(target, &engine) || count <= 0)
        {
            if (errBuf != NULL && errBufSize > 0)
                snprintf(errBuf, errBufSize, "macro-map: target %u has no engine/params", (unsigned)target);
            return false;
        }

        const char *first = ft2_macro_map_target_param_name(target, 0);
        const char *last = ft2_macro_map_target_param_name(target, (uint16_t)(count - 1));
        if (first == NULL || first[0] == '\0' || last == NULL || last[0] == '\0')
        {
            if (errBuf != NULL && errBufSize > 0)
                snprintf(errBuf, errBufSize, "macro-map: target %u has blank parameter name", (unsigned)target);
            return false;
        }
    }

    instr_t testIns;
    memset(&testIns, 0, sizeof (testIns));
    testIns.macroTargetType[0] = MACRO_TARGET_OSTIRUS;
    testIns.macroParamID[0] = 4095;
    testIns.macroScale[0] = 255;
    ft2_macro_map_sanitize_slot(&testIns, 0);
    if (testIns.macroTargetType[0] != MACRO_TARGET_OSTIRUS ||
        testIns.macroParamID[0] != 0 ||
        testIns.macroScale[0] != MACRO_CURVE_LINEAR)
    {
        if (errBuf != NULL && errBufSize > 0)
            snprintf(errBuf, errBufSize, "macro-map: synth slot sanitation failed");
        return false;
    }

    testIns.macroTargetType[1] = 255;
    testIns.macroParamID[1] = 99;
    testIns.macroScale[1] = 255;
    ft2_macro_map_sanitize_slot(&testIns, 1);
    if (testIns.macroTargetType[1] != MACRO_TARGET_NONE ||
        testIns.macroParamID[1] != 0 ||
        testIns.macroScale[1] != MACRO_CURVE_LINEAR)
    {
        if (errBuf != NULL && errBufSize > 0)
            snprintf(errBuf, errBufSize, "macro-map: invalid target sanitation failed");
        return false;
    }

    return true;
}

// Request UI sync from audio thread (thread-safe)
void requestMacroUiSync(void)
{
    g_macroUiSyncRequested = true;
}

// Request DSP UI sync from audio thread (thread-safe)
void requestDspUiSync(void)
{
    g_dspUiSyncRequested = true;
}

// Check and handle UI sync from main thread
void handleMacroUiSync(void)
{
    if (g_macroUiSyncRequested) {
        g_macroUiSyncRequested = false;
        
        // Safety checks before calling UI sync
        extern bool ft2_guisan_is_enabled(void);
        extern bool ft2_guisan_is_synth_editor_shown(void);
        
        if (ft2_guisan_is_enabled() && ft2_guisan_is_synth_editor_shown()) {
            ui_sync_from_instrument();
        }
    }
}

// Check and handle DSP UI sync from main thread
void handleDspUiSync(void)
{
    if (g_dspUiSyncRequested) {
        g_dspUiSyncRequested = false;
        if (ui.dspEditorShown || ui.mixerScreenShown) {
            cacheMixerStateFromData();
        }
    }
}

static void macroMapPopupCallback(int32_t index, void *ctx)
{
    // Hide popup and editor
    hideMacroMapEditor();
    if (index < 0)
        return;
    // Directly control Tunefish4 synth parameter
    int instID = macroSys.currentInst;
    if (instID < 0)
        return;
    // Install macro mapping for slot 0
    instr_t *ins = instr[instID];
    const int slot = 0;
    ins->macroTargetType[slot] = MACRO_TARGET_TF4;
    ins->macroParamID[slot]    = index;
    ins->macroScale[slot]      = MACRO_CURVE_LINEAR;
    // Get current parameter value
    float currentVal = ft2_synth_get_param(instID, index);
    // Increment value
    float newVal = currentVal + 1.0f;
    ft2_synth_set_param(instID, index, newVal);
    // Refresh UI and show instrument editor
    ui_sync_from_instrument();
}



void hideMacroMapEditor(void)
{
    // Clear mapper state and hide
    macroSys.showMapper = false;
    ui.macroMapEditorShown = false;
    // Hide popup list
    popupListHide();
    drawGUIOnRunTime();
}

// UI functions for macro mapper (legacy compatibility)
void showMacroMapper(void)
{
    if (editor.curInstr == 0)
    {
        // Optional: notify user that instr 0 cannot be a synth
        return;
    }

    if (instr[editor.curInstr] == NULL)
    {
        if (!allocateInstr(editor.curInstr)) return;
    }

    instr_t *ins = instr[editor.curInstr];

    macroSys.showMapper = false;
    macroSys.currentInst = editor.curInstr;
    showMacroMapEditor();
}

bool macroMapperMouseDown(void)
{
    return false;
}

void drawMacroMapper(void)
{
    return;
}
