/* ft2_v2_preparations.h - v2 integration preparations
 *
 * This file contains preparations for v2 integration, including:
 * - New features and improvements
 * - Enhanced UI components
 * - Better performance optimizations
 * - Future-proofing measures
 */

#pragma once

#include "ft2_header.h"

#ifdef __cplusplus
extern "C" {
#endif

/* V2 Feature Flags */
#define FT2_V2_ENHANCED_PERFORMANCE 1
#define FT2_V2_IMPROVED_UI 1
#define FT2_V2_BETTER_SYNTH_INTEGRATION 1
#define FT2_V2_ADVANCED_MACRO_MAPPING 1

/* V2 Performance Optimizations */
#if FT2_V2_ENHANCED_PERFORMANCE
#define FT2_V2_OPTIMIZED_MIXING 1
#define FT2_V2_ENHANCED_CACHING 1
#define FT2_V2_BETTER_MEMORY_MANAGEMENT 1
#endif

/* V2 UI Enhancements */
#if FT2_V2_IMPROVED_UI
#define FT2_V2_DARK_MODE_SUPPORT 1
#define FT2_V2_THEMING_SYSTEM 1
#define FT2_V2_BETTER_RENDERING 1
#endif

/* V2 Synth Integration */
#if FT2_V2_BETTER_SYNTH_INTEGRATION
#define FT2_V2_UNIFIED_SYNTH_ENGINE 1
#define FT2_V2_ENHANCED_DSP_CHAIN 1
#define FT2_V2_BETTER_PARAMETER_MAPPING 1
#endif

/* V2 Macro System */
#if FT2_V2_ADVANCED_MACRO_MAPPING
#define FT2_V2_ENHANCED_MACRO_SYSTEM 1
#define FT2_V2_MACRO_LEARNING 1
#define FT2_V2_PARAMETER_SMOOTHING 1
#endif

/* V2 Version Information */
typedef struct {
    int major;
    int minor;
    int patch;
    const char* release_date;
    const char* build_info;
} ft2_version_info_t;

extern ft2_version_info_t ft2_v2_version;

/* V2 Initialization Functions */
void ft2_v2_initialize(void);
void ft2_v2_shutdown(void);
void ft2_v2_apply_optimizations(void);

/* V2 Performance Monitoring */
#if FT2_V2_ENHANCED_PERFORMANCE
typedef struct {
    float cpu_usage;
    float memory_usage;
    float render_time;
    float audio_latency;
    int frame_count;
    int dropped_frames;
} ft2_performance_stats_t;

extern ft2_performance_stats_t ft2_perf_stats;

void ft2_v2_update_performance_stats(void);
void ft2_v2_get_performance_stats(ft2_performance_stats_t* stats);
#endif

/* V2 UI Enhancements */
#if FT2_V2_IMPROVED_UI
typedef enum {
    FT2_V2_THEME_DEFAULT,
    FT2_V2_THEME_DARK,
    FT2_V2_THEME_CONTRAST,
    FT2_V2_THEME_CUSTOM
} ft2_v2_theme_t;

extern ft2_v2_theme_t ft2_current_theme;

void ft2_v2_set_theme(ft2_v2_theme_t theme);
void ft2_v2_apply_theme(void);
const char* ft2_v2_get_theme_name(ft2_v2_theme_t theme);
#endif

/* V2 Macro System Enhancements */
#if FT2_V2_ADVANCED_MACRO_MAPPING
typedef struct {
    int source_param;
    int target_param;
    float min_value;
    float max_value;
    float curve;
    float smoothing;
    bool active;
    int learn_mode;
} ft2_v2_macro_mapping_t;

extern ft2_v2_macro_mapping_t ft2_v2_macro_mappings[16];

void ft2_v2_macro_init(void);
void ft2_v2_macro_learn(int source_param, int target_param);
void ft2_v2_macro_apply_mapping(int slot);
void ft2_v2_macro_set_smoothing(int slot, float smoothing);
#endif

/* V2 Utility Functions */
const char* ft2_v2_get_version_string(void);
int ft2_v2_get_version_major(void);
int ft2_v2_get_version_minor(void);
int ft2_v2_get_version_patch(void);
const char* ft2_v2_get_build_info(void);

/* V2 Compatibility Layer */
void ft2_v2_apply_compatibility_patches(void);
void ft2_v2_check_deprecated_features(void);

#ifdef __cplusplus
}
#endif