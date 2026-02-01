// File: src/ft2_macromap.h
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_structs.h"

// Target types for macro bindings
typedef enum
{
    MACRO_TARGET_NONE = 0,
    MACRO_TARGET_TF4,    // TF4 synth parameter
    MACRO_TARGET_DSP,    // DSP effect parameter
    MACRO_TARGET_DEXED,  // Dexed synth parameter
    MACRO_TARGET_GLOBAL  // Future: global params
} macroTargetType_t;

// Curve types for parameter scaling
typedef enum
{
    MACRO_CURVE_LINEAR = 0,
    MACRO_CURVE_LOG,
    MACRO_CURVE_EXP,
    MACRO_CURVE_SQUARE,
    MACRO_CURVE_SQRT,
    NUM_MACRO_CURVES
} macroCurve_t;

// Single macro mapping
typedef struct
{
    macroTargetType_t targetType;  // Type of parameter
    uint16_t          paramID;     // TF4 param or (DSP_slot << 8 | DSP_param)
    float             min, max;    // Output range
    macroCurve_t      curve;       // Scaling curve
    char              name[24];    // Display name
} macroMapping_t;

// Per-instrument macro system
#define NUM_MACROS_PER_INSTR 16  // M0-MF per instrument
typedef struct
{
    macroMapping_t macros[NUM_MACROS_PER_INSTR];
    int8_t learnMacro;  // -1 = not learning, 0-15 = learning for macro N
} instrumentMacro_t;

/* DSP macro param packing: (scope << 12) | (slot << 8) | paramIdx */
#define DSP_MACRO_SCOPE_PAIR   0
#define DSP_MACRO_SCOPE_MASTER 1
#define DSP_MACRO_PARAMID(scope, slot, param) \
    ((((uint16_t)(scope)) & 0x1) << 12 | (((uint16_t)(slot)) & 0xF) << 8 | ((uint16_t)(param)))
#define DSP_MACRO_SCOPE(paramID) (((paramID) >> 12) & 0x1)
#define DSP_MACRO_SLOT(paramID)  (((paramID) >> 8) & 0xF)
#define DSP_MACRO_PARAM(paramID) ((paramID) & 0xFF)

// Global macro state
typedef struct
{
    bool showMapper;     // Modal visibility
    int8_t currentInst;  // Currently edited instrument
} macroSystem_t;

// Global instance
extern macroSystem_t macroSys;

// Initialize macro system
void initMacroSystem(void);

// Get macros for an instrument
instrumentMacro_t *getInstrumentMacros(int8_t instIdx);

// Save/load mappings with instrument
bool saveMacroMappings(FILE *f, int8_t instIdx);
bool loadMacroMappings(FILE *f, int8_t instIdx, uint32_t version);

// Handle parameter learning
void macroLearnEvent(macroTargetType_t type, uint16_t paramID, const char *name);

// UI functions
void showMacroMapper(void);
bool macroMapperMouseDown(void);
void drawMacroMapper(void);

// UI sync function (legacy compatibility)
void ui_sync_from_instrument(void);
