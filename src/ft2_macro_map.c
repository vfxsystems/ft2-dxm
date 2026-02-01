#include "ft2_macro_map.h"
#include "ft2_structs.h"
#include "ft2_macromap.h"
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
