#include "ft2_waveform_view.h"
#include "ft2_video.h"
#include "ft2_structs.h"
#include "ft2_synth.h"
#include "ft2_tunefish_widgets.h"
#include "ft2_tunefish_complete_layout.h"
#include "ft2_gui.h"
#include <math.h>
#include <stdio.h>
#include <stddef.h>

// Forward declarations for missing functions
extern void* getInstrumentInstance(int instrID);
extern void* tf_instrument_get_latest_voice(void* instrument);
extern float* tf_voice_get_freq_table(void* voice);
extern float* tf_voice_get_freq_mod_table(void* voice);
extern float* tf_voice_get_result_table(void* voice);
extern float tf_voice_get_modulation(void* voice);
extern int tf_voice_is_playing(void* voice);
extern float tf_voice_get_current_freq(void* voice);
extern int tf_voice_get_current_note(void* voice);
extern int tf_voice_get_current_velocity(void* voice);
extern float tf_instrument_get_drive_param(void* instrument);
extern void hLine(uint16_t x, uint16_t y, uint16_t width, uint8_t paletteIndex);
extern void textOut(uint16_t x, uint16_t y, uint8_t paletteIndex, const char* textPtr);

// Constants for waveform view
#define WAVEFORM_VIEW_WIDTH 400
#define WAVEFORM_VIEW_HEIGHT 120
#define FREQ_BARS_COUNT 32
#define TIME_SAMPLES_COUNT 200
#define TF_IFFT_FRAMESIZE 2048  // From Tunefish4 constants

// Animation counters
static int g_animation_counter = 0;
static int g_freq_animation_offset = 0;
static int g_time_animation_offset = 0;

// Real synth data access
static void* get_current_instrument(void) {
    extern struct editor_t editor;
    int currentInstr = editor.curInstr;
    if (currentInstr < 1 || currentInstr > 128) return NULL;
    
    return getInstrumentInstance(currentInstr);
}

// Get the latest triggered voice from the current instrument
static void* get_latest_voice(void) {
    // Local implementation of getCurrentTF4InstrumentID (since the original is static in ft2_synth_ed.c)
    extern struct editor_t editor;
    int currentInstrID = editor.curInstr;
//    printf("🌊 [VOICE] get_latest_voice: currentInstrID=%d\n", currentInstrID);
    
    void* instrument = getInstrumentInstance(currentInstrID);
//    printf("🌊 [VOICE] get_latest_voice: instrument=%p\n", instrument);
    
    if (!instrument) {
//        printf("🌊 [VOICE] get_latest_voice: No instrument instance found\n");
        return NULL;
    }
    
    void* voice = tf_instrument_get_latest_voice(instrument);
//    printf("🌊 [VOICE] get_latest_voice: voice=%p\n", voice);
    
    return voice;
}

// Get real frequency data from synth engine
static void get_real_frequency_data(float* freqData, int maxSamples) {
//    printf("🌊 [FREQ] get_real_frequency_data: called with maxSamples=%d\n", maxSamples);
    
    void* voice = get_latest_voice();
//    printf("🌊 [FREQ] get_real_frequency_data: voice=%p\n", voice);
    
    if (!voice) {
        // Fallback to simulated data if no voice
//        printf("🌊 [FREQ] get_real_frequency_data: No voice, using simulated data\n");
        for (int i = 0; i < maxSamples; i++) {
            freqData[i] = 0.1f + 0.3f * sinf(i * 0.1f + g_freq_animation_offset * 0.05f);
        }
        return;
    }
    
    // Get frequency table from voice
    float* freqTable = tf_voice_get_freq_table(voice);
    float* freqModTable = tf_voice_get_freq_mod_table(voice);
    
//    printf("🌊 [FREQ] get_real_frequency_data: freqTable=%p, freqModTable=%p\n", freqTable, freqModTable);
    
    if (!freqTable) {
        // Fallback to simulated data
//        printf("🌊 [FREQ] get_real_frequency_data: No freqTable, using simulated data\n");
        for (int i = 0; i < maxSamples; i++) {
            freqData[i] = 0.1f + 0.3f * sinf(i * 0.1f + g_freq_animation_offset * 0.05f);
        }
        return;
    }
    
    // Use modulated frequency table if available, otherwise use base frequency table
    float* sourceTable = freqModTable ? freqModTable : freqTable;
//    printf("🌊 [FREQ] get_real_frequency_data: Using sourceTable=%p\n", sourceTable);
    
    // Copy frequency data (limit to maxSamples)
    int samplesToCopy = maxSamples;
    if (samplesToCopy > TF_IFFT_FRAMESIZE) samplesToCopy = TF_IFFT_FRAMESIZE;
    
    for (int i = 0; i < samplesToCopy; i++) {
        freqData[i] = sourceTable[i];
    }
    
    // Fill remaining with zeros if needed
    for (int i = samplesToCopy; i < maxSamples; i++) {
        freqData[i] = 0.0f;
    }
    
//    printf("🌊 [FREQ] get_real_frequency_data: Copied %d samples\n", samplesToCopy);
}

// Get real time-domain data from synth engine
static void get_real_time_data(float* timeData, int maxSamples) {
//    printf("🌊 [TIME] get_real_time_data: called with maxSamples=%d\n", maxSamples);
    
    void* voice = get_latest_voice();
//    printf("🌊 [TIME] get_real_time_data: voice=%p\n", voice);
    
    if (!voice) {
        // Fallback to simulated data if no voice
//        printf("🌊 [TIME] get_real_time_data: No voice, using simulated data\n");
        for (int i = 0; i < maxSamples; i++) {
            timeData[i] = 0.5f * sinf(i * 0.2f + g_time_animation_offset * 0.03f);
        }
        return;
    }
    
    // Get result table from voice
    float* resultTable = tf_voice_get_result_table(voice);
//    printf("🌊 [TIME] get_real_time_data: resultTable=%p\n", resultTable);
    
    if (!resultTable) {
        // Fallback to simulated data
//        printf("🌊 [TIME] get_real_time_data: No resultTable, using simulated data\n");
        for (int i = 0; i < maxSamples; i++) {
            timeData[i] = 0.5f * sinf(i * 0.2f + g_time_animation_offset * 0.03f);
        }
        return;
    }
    
    // Get drive parameter for distortion effect
    void* instrument = get_current_instrument();
    float drive = 1.0f;
    if (instrument) {
        drive = tf_instrument_get_drive_param(instrument);
        drive *= 32.0f;
        drive += 1.0f;
//        printf("🌊 [TIME] get_real_time_data: drive=%.3f\n", drive);
    }
    
    // Copy time-domain data with drive effect
    int samplesToCopy = maxSamples;
    if (samplesToCopy > TF_IFFT_FRAMESIZE) samplesToCopy = TF_IFFT_FRAMESIZE;
    
    for (int i = 0; i < samplesToCopy; i++) {
        float value = resultTable[i * 2]; // Use every other sample for display
        float valueDrv = value * drive;
        
        // Clamp values
        value = (value < -1.0f) ? -1.0f : (value > 1.0f) ? 1.0f : value;
        valueDrv = (valueDrv < -1.0f) ? -1.0f : (valueDrv > 1.0f) ? 1.0f : valueDrv;
        
        timeData[i] = valueDrv;
    }
    
    // Fill remaining with zeros if needed
    for (int i = samplesToCopy; i < maxSamples; i++) {
        timeData[i] = 0.0f;
    }
    
//    printf("🌊 [TIME] get_real_time_data: Copied %d samples\n", samplesToCopy);
}

// Get voice status information
static void get_voice_status(int* isPlaying, float* currentFreq, int* currentNote, int* currentVelocity) {
    void* voice = get_latest_voice();
    if (!voice) {
        *isPlaying = 0;
        *currentFreq = 0.0f;
        *currentNote = 0;
        *currentVelocity = 0;
//        printf("🌊 [VOICE] No voice available\n");
        return;
    }
    
    *isPlaying = tf_voice_is_playing(voice);
    *currentFreq = tf_voice_get_current_freq(voice);
    *currentNote = tf_voice_get_current_note(voice);
    *currentVelocity = tf_voice_get_current_velocity(voice);
    
//    printf("🌊 [VOICE] Status: playing=%d, freq=%.1f, note=%d, vel=%d\n", 
//           *isPlaying, *currentFreq, *currentNote, *currentVelocity);
}

// Simple color interpolation function
static int interpolate_color(int color1, int color2, int pos, int min, int max) {
    if (max == min) return color1;
    float t = (float)(pos - min) / (float)(max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    
    int r1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int b1 = color1 & 0xFF;
    
    int r2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int b2 = color2 & 0xFF;
    
    int r = (int)(r1 + (r2 - r1) * t);
    int g = (int)(g1 + (g2 - g1) * t);
    int b = (int)(b1 + (b2 - b1) * t);
    
    return (r << 16) | (g << 8) | b;
}

// Simple line drawing function
static void draw_line(int x1, int y1, int x2, int y2, int color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        if (x1 >= 0 && x1 < SCREEN_W && y1 >= 0 && y1 < SCREEN_H) {
            // Convert color to palette index (simplified)
            uint8_t paletteIndex = PAL_PATTEXT;
            video.frameBuffer[y1 * SCREEN_W + x1] = video.palette[paletteIndex];
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void ft2_waveform_view_draw(int x, int y, int width, int height) {
    // Update animation counters
    g_animation_counter++;
    g_freq_animation_offset++;
    g_time_animation_offset++;
    
    // Get voice status
    int isPlaying;
    float currentFreq;
    int currentNote;
    int currentVelocity;
    get_voice_status(&isPlaying, &currentFreq, &currentNote, &currentVelocity);
    
    // Draw background gradient (similar to PluginEditor.cpp)
    drawFramework((uint16_t)x - 1, (uint16_t)y - 1, (uint16_t)width + 2, (uint16_t)height + 2, FRAMEWORK_TYPE2);
    for (int py = 0; py < height/2; py++) {
        int color1 = 0x3C1E1E; // Dark red-brown (COL_FREQVIEW_BG_GRADIENT1)
        int color2 = 0x140000; // Very dark red (COL_FREQVIEW_BG_GRADIENT2)
        int gradientColor = interpolate_color(color1, color2, py, 0, height/2);
        // Use hLine for background
        hLine((uint16_t)x, (uint16_t)(y + py), (uint16_t)width, PAL_BUTTONS);
    }
    
    for (int py = height/2; py < height; py++) {
        int color1 = 0x140000; // Very dark red (COL_FREQVIEW_BG_GRADIENT2)
        int color2 = 0x3C1E1E; // Dark red-brown (COL_FREQVIEW_BG_GRADIENT1)
        int gradientColor = interpolate_color(color1, color2, py, height/2, height);
        // Use hLine for background
        hLine((uint16_t)x, (uint16_t)(y + py), (uint16_t)width, PAL_BUTTON2);
    }
    
    // Draw frequency domain bars (top half)
    float freqData[FREQ_BARS_COUNT];
    get_real_frequency_data(freqData, FREQ_BARS_COUNT);
    
    int barWidth = width / FREQ_BARS_COUNT;
    if (barWidth < 1)
        barWidth = 1;
    int freqHeight = height / 2;
    
    for (int i = 0; i < FREQ_BARS_COUNT; i++) {
        int barX = x + i * barWidth;
        float value = freqData[i];
        int barHeight = (int)(value * freqHeight * 0.8f);
        if (barHeight > freqHeight - 1)
            barHeight = freqHeight - 1;
        
        if (barHeight > 0) {
            int maxBarWidth = (x + width) - barX;
            int drawW = barWidth - 1;
            if (drawW > maxBarWidth)
                drawW = maxBarWidth;
            if (drawW <= 0)
                continue;

            // Use hLine for bars
            for (int h = 0; h < barHeight; h++) {
                hLine((uint16_t)barX, (uint16_t)(y + freqHeight - h - 1), (uint16_t)drawW, PAL_BUTTON1);
            }
        }
    }
    
    // Draw time domain waveform (bottom half)
    float timeData[TIME_SAMPLES_COUNT];
    get_real_time_data(timeData, TIME_SAMPLES_COUNT);
    
    int timeHeight = height / 2;
    int timeY = y + height / 2;
    
    // Draw waveform line
    for (int i = 1; i < TIME_SAMPLES_COUNT - 1; i++) {
        int x1 = x + (i - 1) * width / TIME_SAMPLES_COUNT;
        int x2 = x + i * width / TIME_SAMPLES_COUNT;
        int y1 = timeY + timeHeight/2 - (int)(timeData[i-1] * timeHeight * 0.4f);
        int y2 = timeY + timeHeight/2 - (int)(timeData[i] * timeHeight * 0.4f);
        
        // Draw line segment
        draw_line(x1, y1, x2, y2, PAL_PATTEXT);
    }
    
    // Draw voice status text
    char statusText[128];
    if (isPlaying) {
        snprintf(statusText, sizeof(statusText), "VOICE || Note %d (%.1f Hz) Vel %d", 
                currentNote, currentFreq, currentVelocity);
    } else {
        snprintf(statusText, sizeof(statusText), "VOICE || No active voice");
    }
    
    // Draw status text using textOut
    textOutTiny((uint16_t)(x + 5), (uint16_t)(y + 5), statusText, PAL_TEXTMRK);
    
//    printf("🌊 [WIDGET] Drawing waveform view widget at (%d,%d) %dx%d - Voice: %s\n", 
//           x, y, width, height, statusText);
} 
