#include "ft2_tunefish_widgets.h"
#include "ft2_waveform_view.h"
#include "ft2_gui.h"
#include "ft2_palette.h"
#include "ft2_video.h"
#include "ft2_structs.h"
#include "ft2_bmp.h"
#include "ft2_checkboxes.h"
#include "ft2_pushbuttons.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Forward declarations
static void tf_draw_waveform_view_ft2(const TunefishWidget* widget);

// Math constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper Functions for Tunefish-style Drawing

static bool tf_is_arp_step_widget(const TunefishWidget *widget)
{
    return widget && strncmp(widget->name, "arp_step_", 9) == 0;
}

static int tf_parse_arp_step_index(const TunefishWidget *widget)
{
    if (!tf_is_arp_step_widget(widget))
        return -1;

    int idx = -1;
    if (sscanf(widget->name, "arp_step_%d", &idx) != 1)
        return -1;

    return idx - 1;
}

// Convert Tunefish colors to FT2 palette indices
static uint8_t tf_color_to_ft2_palette(uint32_t tfColor) {
    // Map Tunefish colors to closest FT2 palette colors
    switch (tfColor) {
        case TF_COL_BG_MAIN:        return PAL_BCKGRND;
        case TF_COL_BUTTON_NORMAL:  return PAL_BUTTONS;
        case TF_COL_BUTTON_PRESSED: return PAL_BUTTON2;
        case TF_COL_BUTTON_BASE:    return PAL_BUTTONS; /* Dark base */
        case TF_COL_BUTTON_HIGHLIGHT: return PAL_FORGRND; /* Light highlight */
        case TF_COL_BUTTON_ACCENT:  return PAL_BTNTEXT; /* Accent orange */
        case TF_COL_TEXT_NORMAL:    return PAL_FORGRND;
        case TF_COL_TEXT_HIGHLIGHT: return PAL_BTNTEXT;
        case TF_COL_OUTLINE:        return PAL_FORGRND;
        case TF_COL_COMBO_BG:       return PAL_DESKTOP;
        case TF_COL_COMBO_TEXT:     return PAL_BTNTEXT;
        default:                    return PAL_FORGRND;
    }
}

// Safe drawing with bounds checking
static void tf_draw_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        video.frameBuffer[y * SCREEN_W + x] = video.palette[color];
    }
}

static void tf_draw_line(int x1, int y1, int x2, int y2, uint8_t color) {
    // Simple line drawing using Bresenham's algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        tf_draw_pixel(x1, y1, color);

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

static void tf_draw_circle(int centerX, int centerY, int radius, uint8_t color, bool filled) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            int dist = x * x + y * y;
            if (filled ? (dist <= radius * radius) : (abs(dist - radius * radius) < radius)) {
                tf_draw_pixel(centerX + x, centerY + y, color);
            }
        }
    }
}

// Tunefish-style Rotary Slider Drawing (based on tflookandfeel.cpp)
static void tf_draw_rotary_slider_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    int centerX = widget->x + widget->w / 2;
    int centerY = widget->y + widget->h / 2;
    int radius = (widget->w < widget->h ? widget->w : widget->h) / 2 - 4;

    uint8_t trackColor = tf_color_to_ft2_palette(TF_COL_SLIDER_TRACK);
    uint8_t fillColor = tf_color_to_ft2_palette(TF_COL_SLIDER_FILL);
    uint8_t outlineColor = tf_color_to_ft2_palette(TF_COL_OUTLINE);

    // Calculate angles
    float startAngle = widget->rotaryStartAngle;
    float endAngle = widget->rotaryEndAngle;
    float valueAngle = startAngle + widget->value * (endAngle - startAngle);
    float modAngle = startAngle + (widget->value * widget->modValue) * (endAngle - startAngle);

    const uint32_t tinyTextColor = video.palette[PAL_FORGRND];

    if (radius > 12) {
        // Large rotary slider with pie segments

        // Draw background track
        tf_draw_circle(centerX, centerY, radius, PAL_BUTTON2, true);
        tf_draw_circle(centerX, centerY, radius - 3.5, PAL_BUTTONS, true);

        // Draw value arc (simplified pie segment)
        int arcSteps = 1024;
        for (int i = 0; i <= arcSteps; i++) {
            float angle = startAngle + (float)i / arcSteps * (valueAngle - startAngle);
            int x1 = centerX + (radius - 4) * cos(angle);
            int y1 = centerY + (radius - 4) * sin(angle);
            int x2 = centerX + (radius + 0.5) * cos(angle);
            int y2 = centerY + (radius + 0.5) * sin(angle);
            tf_draw_line(x1, y1, x2, y2, PAL_PATTEXT);
        }

        // Draw modulation arc if present
        if (widget->modValue > 0.0f) {
            for (int i = 0; i <= arcSteps; i++) {
                float angle = startAngle + (float)i / arcSteps * (modAngle - startAngle);
                int x1 = centerX + (radius - 4) * cos(angle);
                int y1 = centerY + (radius - 4) * sin(angle);
                tf_draw_pixel(x1, y1, PAL_TEXTMRK);
            }
        }

        // Draw value text in center
        char valueText[16];
        snprintf(valueText, sizeof(valueText), "%d", (int)(widget->value * 99.0f));
        int textW = textWidth(valueText);
        textOutTiny(centerX - textW/4, centerY - 4, valueText, tinyTextColor);

    } else {
        // Small rotary slider - simple pointer style
        tf_draw_circle(centerX, centerY, radius, outlineColor, false);

        // Draw pointer
        int pointerX = centerX + (radius - 2) * cos(valueAngle);
        int pointerY = centerY + (radius - 2) * sin(valueAngle);
        tf_draw_line(centerX, centerY, pointerX, pointerY, fillColor);
        tf_draw_circle(centerX, centerY, 2, fillColor, true);
    }

    // Draw label text beneath the rotary slider (center-aligned, tiny font)
    if (widget->text[0] != '\0') {
        int textW = (int)strlen(widget->text) * FONT3_CHAR_W;
        int labelY = widget->y + widget->h + 2; // 2px gap below knob
        textOutTiny(centerX - textW / 2, labelY, widget->text, tinyTextColor);
    }
}

// Tunefish-style Level Meter Drawing
static void tf_draw_level_meter_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    uint8_t bgColor = tf_color_to_ft2_palette(TF_COL_BG_MAIN);
    uint8_t ledColor = tf_color_to_ft2_palette(TF_COL_SLIDER_FILL);
    uint8_t outlineColor = tf_color_to_ft2_palette(TF_COL_OUTLINE);

    // Draw background
    fillRect(widget->x, widget->y, widget->w, widget->h, bgColor);

    // Calculate LED dimensions
    int ledWidth = (widget->w - 2) / widget->numLEDs;
    int ledHeight = widget->h - 2;
    int activeLEDs = (int)(widget->value * widget->numLEDs);

    // Draw LEDs
    for (int i = 0; i < widget->numLEDs; i++) {
        int ledX = widget->x + 1 + i * ledWidth;
        int ledY = widget->y + 1;

        uint8_t color;
        if (i < activeLEDs) {
            // Active LED - color based on level
            if (i >= widget->numLEDs - 2) {
                color = PAL_TEXTMRK; // Red for high levels
            } else if (i >= widget->numLEDs - 4) {
                color = PAL_BTNTEXT; // Orange for medium levels
            } else {
                color = ledColor;    // Green for low levels
            }
        } else {
            color = PAL_DSKTOP2; // Inactive LED
        }

        fillRect(ledX, ledY, ledWidth - 1, ledHeight, color);
    }

    // Draw peak indicator if enabled
    if (widget->showPeak && widget->peakLevel > 0.0f) {
        int peakLED = (int)(widget->peakLevel * widget->numLEDs);
        if (peakLED < widget->numLEDs) {
            int peakX = widget->x + 1 + peakLED * ledWidth;
            fillRect(peakX, widget->y + 1, ledWidth - 1, ledHeight, PAL_TEXTMRK);
        }
    }

    // Draw outline
    hLine(widget->x, widget->y, widget->w, outlineColor);
    hLine(widget->x, widget->y + widget->h - 1, widget->w, outlineColor);
    vLine(widget->x, widget->y, widget->h, outlineColor);
    vLine(widget->x + widget->w - 1, widget->y, widget->h, outlineColor);
}

// LFO Shape Button Drawing - Draws waveform graphics instead of numbers
static void tf_draw_lfo_shape_button_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    // Extract shape index from widget name (lfo1_shape_X or lfo2_shape_X)
    int shapeIndex = -1;
    if (strncmp(widget->name, "lfo1_shape_", 11) == 0) {
        shapeIndex = atoi(widget->name + 11) - 1; // Convert "1" to 0, "2" to 1, etc.
    } else if (strncmp(widget->name, "lfo2_shape_", 11) == 0) {
        shapeIndex = atoi(widget->name + 11) - 1;
    }
    
    if (shapeIndex < 0 || shapeIndex >= 5) return;

    // Map to correct waveform order: 0=Sine, 1=RampUp, 2=RampDown, 3=Square, 4=Random
    int waveformIndex;
    switch (shapeIndex) {
        case 0: waveformIndex = 0; break; // Sine
        case 1: waveformIndex = 2; break; // Ramp Up (was SawUp)
        case 2: waveformIndex = 1; break; // Ramp Down (was SawDown)
        case 3: waveformIndex = 3; break; // Square
        case 4: waveformIndex = 4; break; // Random (Noise)
        default: waveformIndex = 0; break;
    }

    // Draw button background using FT2 style
    uint8_t bgColor = widget->pressed ? PAL_BUTTON2 : PAL_BUTTONS;
    fillRect(widget->x + 1, widget->y + 1, widget->w - 2, widget->h - 2, bgColor);
    hLine(widget->x, widget->y, widget->w, PAL_BCKGRND);
    hLine(widget->x, widget->y + widget->h - 1, widget->w, PAL_BCKGRND);
    vLine(widget->x, widget->y, widget->h, PAL_BCKGRND);
    vLine(widget->x + widget->w - 1, widget->y, widget->h, PAL_BCKGRND);
    
    if (!widget->pressed) {
        hLine(widget->x + 1, widget->y + 1, widget->w - 3, PAL_BUTTON1);
        vLine(widget->x + 1, widget->y + 2, widget->h - 4, PAL_BUTTON1);
    }

    // Draw waveform inside button (12x8 pixel area)
    int centerX = widget->x + widget->w / 2;
    int centerY = widget->y + widget->h / 2;
    int waveX = centerX - 6;
    int waveY = centerY - 4;
    uint8_t waveColor = PAL_FORGRND;

    switch (waveformIndex) {
        case 0: // Sine wave
            for (int x = 0; x < 12; x++) {
                float t = (float)x / 11.0f * 2.0f * M_PI;
                int y = (int)(sin(t) * 3.0f);
                tf_draw_pixel(waveX + x, waveY + 4 + y, waveColor);
            }
            break;
            
        case 1: // Ramp Down (SawDown)
            for (int x = 0; x < 12; x++) {
                int y = 4 - (x * 8 / 11);
                tf_draw_pixel(waveX + x, waveY + y, waveColor);
            }
            break;
            
        case 2: // Ramp Up (SawUp)
            for (int x = 0; x < 12; x++) {
                int y = (x * 8 / 11) - 4;
                tf_draw_pixel(waveX + x, waveY + y, waveColor);
            }
            break;
            
        case 3: // Square wave
            for (int x = 0; x < 12; x++) {
                int y = (x < 6) ? -2 : 2;
                tf_draw_pixel(waveX + x, waveY + 4 + y, waveColor);
            }
            break;
            
        case 4: // Random (Noise) - draw random dots
            for (int x = 0; x < 12; x++) {
                int y = (x * 7 + 13) % 9 - 4; // Pseudo-random pattern
                tf_draw_pixel(waveX + x, waveY + 4 + y, waveColor);
            }
            break;
    }
}

// Button Drawing - Using FT2 pushbutton drawing
static void tf_draw_button_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    // Special case for LFO shape buttons
    if (strncmp(widget->name, "lfo1_shape_", 11) == 0 || strncmp(widget->name, "lfo2_shape_", 11) == 0) {
        tf_draw_lfo_shape_button_ft2(widget);
        return;
    }

    // Create a temporary pushbutton structure to use FT2 drawing
    pushButton_t tempButton;
    tempButton.x = widget->x;
    tempButton.y = widget->y;
    tempButton.w = widget->w;
    tempButton.h = widget->h;
    tempButton.caption = (char*)widget->text;
    tempButton.caption2 = NULL;
    tempButton.state = widget->pressed ? PUSHBUTTON_PRESSED : PUSHBUTTON_UNPRESSED;
    tempButton.bitmapFlag = false;
    tempButton.visible = true;
    tempButton.bitmapUnpressed = NULL;
    tempButton.bitmapPressed = NULL;

    // Use FT2's drawPushButton logic directly
    uint16_t textX, textY, textW;

    // fill button background
    fillRect(tempButton.x + 1, tempButton.y + 1, tempButton.w - 2, tempButton.h - 2, PAL_BUTTONS);

    // draw outer border
    hLine(tempButton.x,         tempButton.y,         tempButton.w, PAL_BCKGRND);
    hLine(tempButton.x,         tempButton.y + tempButton.h - 1, tempButton.w, PAL_BCKGRND);
    vLine(tempButton.x,         tempButton.y,         tempButton.h, PAL_BCKGRND);
    vLine(tempButton.x + tempButton.w - 1, tempButton.y,         tempButton.h, PAL_BCKGRND);

    //draw inner borders
    if (tempButton.state == PUSHBUTTON_UNPRESSED)
    {
        // top left corner inner border
        hLine(tempButton.x + 1, tempButton.y + 1, tempButton.w - 3, PAL_BUTTON1);
        vLine(tempButton.x + 1, tempButton.y + 2, tempButton.h - 4, PAL_BUTTON1);

        // bottom right corner inner border
        hLine(tempButton.x + 1 - 0, tempButton.y + tempButton.h - 2, tempButton.w - 2, PAL_BUTTON2);
        vLine(tempButton.x + tempButton.w - 2, tempButton.y + 1 - 0, tempButton.h - 3, PAL_BUTTON2);
    }
    else
    {
        // top left corner inner border
        hLine(tempButton.x + 1, tempButton.y + 1, tempButton.w - 2, PAL_BUTTON2);
        vLine(tempButton.x + 1, tempButton.y + 2, tempButton.h - 3, PAL_BUTTON2);
    }

    // render button text
    if (tempButton.caption != NULL && *tempButton.caption != '\0')
    {
        textW = textWidth(tempButton.caption);
        textX = tempButton.x + ((tempButton.w - textW) / 2);
        textY = tempButton.y + ((tempButton.h - (FONT1_CHAR_H - 2)) / 2);

        if (tempButton.state == PUSHBUTTON_PRESSED)
            textOut(textX + 1, textY + 1, PAL_BTNTEXT, tempButton.caption);
        else
            textOut(textX, textY, PAL_BTNTEXT, tempButton.caption);
    }
}

// Combo Box Drawing - Using FT2 3D style with inverted colors (sunken appearance)
static void tf_draw_combo_box_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    // Draw background
    fillRect(widget->x + 1, widget->y + 1, widget->w - 2, widget->h - 2, PAL_BUTTON2);

    // Draw outer border (dark)
    hLine(widget->x, widget->y, widget->w, PAL_BCKGRND);
    hLine(widget->x, widget->y + widget->h - 1, widget->w, PAL_BCKGRND);
    vLine(widget->x, widget->y, widget->h, PAL_BCKGRND);
    vLine(widget->x + widget->w - 1, widget->y, widget->h, PAL_BCKGRND);

    // Draw inner border (light) to create sunken 3D effect
    hLine(widget->x + 1, widget->y + 1, widget->w - 3, PAL_BUTTONS);
    vLine(widget->x + 1, widget->y + 2, widget->h - 4, PAL_BUTTONS);

    // Draw arrow button area using FT2 pushbutton style
    int arrowX = widget->x + widget->w - 11;
    int buttonW = 10;
    int buttonH = (widget->h - 2) / 2;

    // Draw up button using FT2 style
    fillRect(arrowX + 1, widget->y + 1 + 1, buttonW - 2, buttonH - 2, PAL_BUTTONS);
    hLine(arrowX, widget->y + 1, buttonW, PAL_BCKGRND);
    hLine(arrowX, widget->y + 1 + buttonH - 1, buttonW, PAL_BCKGRND);
    vLine(arrowX, widget->y + 1, buttonH, PAL_BCKGRND);
    vLine(arrowX + buttonW - 1, widget->y + 1, buttonH, PAL_BCKGRND);
    hLine(arrowX + 1, widget->y + 1 + 1, buttonW - 3, PAL_BUTTON1);
    vLine(arrowX + 1, widget->y + 1 + 2, buttonH - 4, PAL_BUTTON1);

    // Draw down button using FT2 style
    fillRect(arrowX + 1, widget->y + 1 + buttonH + 1 + 1, buttonW - 2, buttonH - 3, PAL_BUTTONS);
    hLine(arrowX, widget->y + 1 + buttonH + 1, buttonW, PAL_BCKGRND);
    hLine(arrowX, widget->y + 1 + buttonH + 1 + buttonH - 1, buttonW, PAL_BCKGRND);
    vLine(arrowX, widget->y + 1 + buttonH + 1, buttonH, PAL_BCKGRND);
    vLine(arrowX + buttonW - 1, widget->y + 1 + buttonH + 1, buttonH, PAL_BCKGRND);
    hLine(arrowX + 1, widget->y + 1 + buttonH + 1 + 1, buttonW - 3, PAL_BUTTON1);
    vLine(arrowX + 1, widget->y + 1 + buttonH + 1 + 2, buttonH - 4, PAL_BUTTON1);

    // Draw up arrow
    int upCenterX = arrowX + buttonW / 2;
    int upCenterY = widget->y + 1 + buttonH / 2;
    tf_draw_line(upCenterX - 2, upCenterY + 1, upCenterX, upCenterY - 1, PAL_FORGRND);
    tf_draw_line(upCenterX, upCenterY - 1, upCenterX + 2, upCenterY + 1, PAL_FORGRND);

    // Draw down arrow
    int downCenterX = arrowX + buttonW / 2;
    int downCenterY = widget->y + 1 + buttonH + 1 + buttonH / 2;
    tf_draw_line(downCenterX - 2, downCenterY - 1, downCenterX, downCenterY + 1, PAL_FORGRND);
    tf_draw_line(downCenterX, downCenterY + 1, downCenterX + 2, downCenterY - 1, PAL_FORGRND);

    // Draw selected text
    if (widget->comboItems && widget->selectedIndex >= 0 && widget->selectedIndex < widget->comboItemCount) {
        const char* selectedText = widget->comboItems[widget->selectedIndex];
        textOut(widget->x + 7, widget->y + (widget->h - 8) / 2, PAL_FORGRND, selectedText);
    }
}

// Toggle Button Drawing - Using FT2 checkbox drawing
static void tf_draw_toggle_button_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    // Small radio-style button (<=24px wide) used for formant vowels
    if (widget->w <= 24) {
        uint8_t bg = widget->pressed ? PAL_TEXTMRK : PAL_DESKTOP;
        uint8_t outline = PAL_FORGRND;
        uint8_t textColor = PAL_FORGRND;

        fillRect(widget->x, widget->y, widget->w, widget->h, bg);
        hLine(widget->x, widget->y, widget->w, outline);
        hLine(widget->x, widget->y + widget->h - 1, widget->w, outline);
        vLine(widget->x, widget->y, widget->h, outline);
        vLine(widget->x + widget->w - 1, widget->y, widget->h, outline);

        // Centered letter label
        int textW = textWidth(widget->text);
        textOut(widget->x + (widget->w - textW) / 2, widget->y + (widget->h - 8) / 2, textColor, widget->text);
        return;
    }

    // Default checkbox-style toggle using FT2 checkbox drawing
    const uint8_t *gfxPtr;
    
    // Use FT2 checkbox graphics
    if (widget->pressed)
        gfxPtr = &bmp.checkboxGfx[2*(CHECKBOX_W*CHECKBOX_H)]; // checked state
    else
        gfxPtr = &bmp.checkboxGfx[0*(CHECKBOX_W*CHECKBOX_H)]; // unchecked state

    // Draw checkbox using FT2 blit
    blitFast(widget->x, widget->y, gfxPtr, CHECKBOX_W, CHECKBOX_H);

    // Draw label text
    if (widget->text[0] != '\0') {
        textOut(widget->x + CHECKBOX_W + 6, widget->y + (widget->h - 8) / 2, PAL_FORGRND, widget->text);
    }
}

// Group Box Drawing - Using FT2 drawFramework
static void tf_draw_group_box_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    // Transparent frame: draw only borders (no fill)
    hLine(widget->x, widget->y, widget->w, PAL_BUTTON1);
    vLine(widget->x, widget->y, widget->h, PAL_BUTTON1);
    hLine(widget->x, widget->y + widget->h - 1, widget->w, PAL_BUTTON2);
    vLine(widget->x + widget->w - 1, widget->y, widget->h, PAL_BUTTON2);

    // Draw title text if present
    if (widget->groupTitle[0] != '\0') {
        // Position title text at the top of the framework
        textOut(widget->x + 4, widget->y + 3, PAL_FORGRND, widget->groupTitle);
    }
}

static void tf_draw_bitmap_ft2(const TunefishWidget* widget)
{
    if (!widget || !widget->visible) return;

    const int w = (widget->bitmapW > 0) ? widget->bitmapW : widget->w;
    const int h = (widget->bitmapH > 0) ? widget->bitmapH : widget->h;
    if (w <= 0 || h <= 0) return;

    if (widget->bitmap32) {
        if (!widget->bitmapPixels32) return;
        blit32((uint16_t)widget->x, (uint16_t)widget->y, widget->bitmapPixels32, (uint16_t)w, (uint16_t)h);
    } else {
        if (!widget->bitmapPixels) return;
        blit((uint16_t)widget->x, (uint16_t)widget->y, widget->bitmapPixels, (uint16_t)w, (uint16_t)h);
    }
}

// Label Drawing
static void tf_draw_label_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible || widget->text[0] == '\0') return;

    uint8_t textColor = tf_color_to_ft2_palette(widget->textColor);
    if (strcmp(widget->name, "title_label") == 0 || strcmp(widget->name, "dx_title_label") == 0) {
        char titleBuf[128];
        strncpy(titleBuf, widget->text, sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';
        for (size_t i = 0; titleBuf[i] != '\0'; i++) {
            if (titleBuf[i] == '_')
                titleBuf[i] = ' ';
        }
        bigTextOut(widget->x, widget->y, textColor, titleBuf);
        return;
    }

    textOut(widget->x, widget->y, textColor, widget->text);
}

// Enhanced Parameter Control (up/down buttons + value display) - Using FT2-style drawing
static void tf_draw_parameter_control_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    // Draw parameter label above control
    if (widget->text[0] != '\0') {
        const uint32_t tinyTextColor = video.palette[PAL_FORGRND];
        textOutTiny(widget->x, widget->y - (FONT3_CHAR_H + 1), widget->text, tinyTextColor);
    }

    // Button dimensions
    int buttonW = 14;
    int buttonH = 11;
    int valueW = widget->w - (buttonW * 2) - 2;
    int valueH = buttonH;

    // Draw up button using FT2 style
    fillRect(widget->x + 1, widget->y + 1, buttonW - 2, buttonH - 2, PAL_BUTTONS);
    hLine(widget->x, widget->y, buttonW, PAL_BCKGRND);
    hLine(widget->x, widget->y + buttonH - 1, buttonW, PAL_BCKGRND);
    vLine(widget->x, widget->y, buttonH, PAL_BCKGRND);
    vLine(widget->x + buttonW - 1, widget->y, buttonH, PAL_BCKGRND);
    hLine(widget->x + 1, widget->y + 1, buttonW - 3, PAL_BUTTON1);
    vLine(widget->x + 1, widget->y + 2, buttonH - 4, PAL_BUTTON1);

    // Draw up arrow
    int upCenterX = widget->x + buttonW / 2;
    int upCenterY = widget->y + buttonH / 2;
    tf_draw_line(upCenterX - 2, upCenterY + 1, upCenterX, upCenterY - 1, PAL_FORGRND);
    tf_draw_line(upCenterX, upCenterY - 1, upCenterX + 2, upCenterY + 1, PAL_FORGRND);

    // Draw value display
    int valueX = widget->x + buttonW + 1;
    fillRect(valueX, widget->y, valueW, valueH, PAL_DESKTOP);
    hLine(valueX, widget->y, valueW, PAL_FORGRND);
    hLine(valueX, widget->y + valueH - 1, valueW, PAL_FORGRND);
    vLine(valueX, widget->y, valueH, PAL_FORGRND);
    vLine(valueX + valueW - 1, widget->y, valueH, PAL_FORGRND);

    // Draw value text (tiny font)
    char valueText[16];
    snprintf(valueText, sizeof(valueText), "%d", (int)(widget->value * (widget->maxValue - widget->minValue) + widget->minValue));
    int textW = textWidth(valueText);
    int tinyW = textW / 2;
    const uint32_t tinyTextColor = video.palette[PAL_FORGRND];
    textOutTiny(valueX + (valueW - tinyW) / 2, widget->y + 2, valueText, tinyTextColor);

    // Draw down button using FT2 style
    int downX = widget->x + buttonW + valueW + 1;
    fillRect(downX + 1, widget->y + 1, buttonW - 2, buttonH - 2, PAL_BUTTONS);
    hLine(downX, widget->y, buttonW, PAL_BCKGRND);
    hLine(downX, widget->y + buttonH - 1, buttonW, PAL_BCKGRND);
    vLine(downX, widget->y, buttonH, PAL_BCKGRND);
    vLine(downX + buttonW - 1, widget->y, buttonH, PAL_BCKGRND);
    hLine(downX + 1, widget->y + 1, buttonW - 3, PAL_BUTTON1);
    vLine(downX + 1, widget->y + 2, buttonH - 4, PAL_BUTTON1);

    // Draw down arrow
    int downCenterX = downX + buttonW / 2;
    int downCenterY = widget->y + buttonH / 2;
    tf_draw_line(downCenterX - 2, downCenterY - 1, downCenterX, downCenterY + 1, PAL_FORGRND);
    tf_draw_line(downCenterX, downCenterY + 1, downCenterX + 2, downCenterY - 1, PAL_FORGRND);
}

// Linear Slider Drawing - Using FT2 drawFramework
static void tf_draw_linear_slider_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    if (tf_is_arp_step_widget(widget)) {
        const int stepIndex = tf_parse_arp_step_index(widget);
        const bool active = widget->pressed || widget->value > 0.001f || widget->modValue > 0.001f;
        const uint8_t bgColor = active ? PAL_BUTTON2 : PAL_DSKTOP2;
        const uint8_t barColor = active ? PAL_PATTEXT : PAL_BUTTON1;
        const uint8_t frameColor = active ? PAL_FORGRND : PAL_BCKGRND;

        fillRect(widget->x, widget->y, widget->w, widget->h, PAL_BUTTONS);
        hLine(widget->x, widget->y, widget->w, frameColor);
        hLine(widget->x, widget->y + widget->h - 1, widget->w, frameColor);
        vLine(widget->x, widget->y, widget->h, frameColor);
        vLine(widget->x + widget->w - 1, widget->y, widget->h, frameColor);

        const int innerX = widget->x + 1;
        const int innerY = widget->y + 1;
        const int innerW = widget->w - 2;
        const int innerH = widget->h - 2;
        if (innerW <= 0 || innerH <= 0)
            return;

        int barW = 2 + (int)lroundf(widget->modValue * (float)(innerW - 2));
        int barH = 2 + (int)lroundf(widget->value * (float)(innerH - 2));
        if (barW < 2) barW = 2;
        if (barH < 2) barH = 2;
        if (barW > innerW) barW = innerW;
        if (barH > innerH) barH = innerH;

        const int barX = innerX + (innerW - barW) / 2;
        const int barY = innerY + innerH - barH;
        fillRect(barX, barY, barW, barH, bgColor);
        hLine(barX, barY, barW, barColor);
        hLine(barX, barY + barH - 1, barW, barColor);
        vLine(barX, barY, barH, barColor);
        vLine(barX + barW - 1, barY, barH, barColor);

        if (stepIndex >= 0) {
            char stepText[4];
            snprintf(stepText, sizeof(stepText), "%d", stepIndex + 1);
            const uint32_t tinyTextColor = video.palette[PAL_FORGRND];
            textOutTiny(widget->x + 1, widget->y + 1, stepText, tinyTextColor);
        }
        return;
    }

    // Determine orientation based on dimensions – assume vertical if height > width
    bool vertical = (widget->h > widget->w);

    // Draw framework using FT2's drawFramework function
    // Use FRAMEWORK_TYPE1 for the slider border (raised appearance)
    drawFramework(widget->x, widget->y, widget->w, widget->h, FRAMEWORK_TYPE1);

    if (vertical) {
        // Track rectangle (leave 2px border)
        int trackX = widget->x + widget->w/2 - 2;
        int trackY = widget->y + 2;
        int trackH = widget->h - 4;
        fillRect(trackX, trackY, 3, trackH, PAL_DSKTOP2);

        // Handle position
        int handleH = 6;
        int range   = trackH - handleH;
        int handleOffset = (int)((1.0f - widget->value) * range);
        int handleY = trackY + handleOffset;

        // Draw handle using FT2 button style
        fillRect(trackX - 3 + 1, handleY + 1, 9 - 2, handleH - 2, PAL_BUTTONS);
        hLine(trackX - 3, handleY, 9, PAL_BUTTON2);
        hLine(trackX - 3, handleY + handleH - 1, 9, PAL_BUTTON2);
        vLine(trackX - 3, handleY, handleH, PAL_BUTTON2);
        vLine(trackX + 5, handleY, handleH, PAL_BUTTON2);
        hLine(trackX - 3 + 1, handleY + 1, 9 - 3, PAL_BUTTON1);
        vLine(trackX - 3 + 1, handleY + 2, handleH - 4, PAL_BUTTON1);

        // Label under slider if present
        if (widget->text[0] != '\0') {
            int textW = textWidth(widget->text);
            textOut(widget->x + (widget->w - textW)/2, widget->y + widget->h + 2, PAL_FORGRND, widget->text);
        }
    } else {
        int trackX = widget->x + 2;
        int trackY = widget->y + widget->h/2 - 1;
        int trackW = widget->w - 4;
        int handleW = 8;
        int range = trackW - handleW;
        int handleOffset = (int)(widget->value * range);
        int handleX = trackX + handleOffset;

        fillRect(trackX, trackY, trackW, 3, PAL_DSKTOP2);
        fillRect(handleX + 1, trackY - 2, handleW - 2, 7, PAL_BUTTONS);
        hLine(handleX, trackY - 3, handleW, PAL_BUTTON2);
        hLine(handleX, trackY + 5, handleW, PAL_BUTTON2);
        vLine(handleX, trackY - 3, 9, PAL_BUTTON2);
        vLine(handleX + handleW - 1, trackY - 3, 9, PAL_BUTTON2);
        hLine(handleX + 1, trackY - 2, handleW - 3, PAL_BUTTON1);
        vLine(handleX + 1, trackY - 1, 5, PAL_BUTTON1);

        if (widget->text[0] != '\0')
            textOut(widget->x, widget->y - 10, PAL_FORGRND, widget->text);
    }
}

// Modular Widget Renderer (FT2 Implementation)
// FT2 Widget Renderer Implementation
static TunefishWidgetRenderer g_ft2WidgetRenderer = {
    .drawButton = tf_draw_button_ft2,
    .drawRotarySlider = tf_draw_rotary_slider_ft2,
    .drawLinearSlider = tf_draw_linear_slider_ft2,
    .drawComboBox = tf_draw_combo_box_ft2,
    .drawLevelMeter = tf_draw_level_meter_ft2,
    .drawLabel = tf_draw_label_ft2,
    .drawToggleButton = tf_draw_toggle_button_ft2,
    .drawParameterControl = tf_draw_parameter_control_ft2,
    .drawEnvelopeDisplay = NULL, // TODO: Implement if needed
    .drawGroupBox = tf_draw_group_box_ft2,
    .drawWaveformView = tf_draw_waveform_view_ft2
};

// Widget Creation Functions

TunefishWidget* tf_create_button(const char* name, const char* text, int x, int y, int w, int h) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    strncpy(widget->text, text, sizeof(widget->text) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_BUTTON;
    widget->visible = true;
    widget->enabled = true;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_rotary_slider(const char* name, int x, int y, int radius, float startAngle, float endAngle) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    widget->x = x - radius; widget->y = y - radius;
    widget->w = radius * 2; widget->h = radius * 2;
    widget->type = TF_WIDGET_ROTARY_SLIDER;
    widget->visible = true;
    widget->enabled = true;
    widget->value = 0.5f;
    widget->minValue = 0.0f;
    widget->maxValue = 1.0f;
    widget->rotaryStartAngle = startAngle;
    widget->rotaryEndAngle = endAngle;
    widget->modValue = 0.0f;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_combo_box(const char* name, int x, int y, int w, int h, const char* const* items, int itemCount) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_COMBO_BOX;
    widget->visible = true;
    widget->enabled = true;
    widget->selectedIndex = 0;

    // Copy items
    if (items && itemCount > 0) {
        widget->comboItems = (char**)malloc(itemCount * sizeof(char*));
        widget->comboItemCount = itemCount;
        for (int i = 0; i < itemCount; i++) {
            int len = strlen(items[i]) + 1;
            widget->comboItems[i] = (char*)malloc(len);
            strcpy(widget->comboItems[i], items[i]);
        }
    }

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_level_meter(const char* name, int x, int y, int w, int h, int numLEDs, bool showPeak) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_LEVEL_METER;
    widget->visible = true;
    widget->enabled = true;
    widget->numLEDs = numLEDs;
    widget->showPeak = showPeak;
    widget->value = 0.0f;
    widget->peakLevel = 0.0f;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_parameter_control(const char* name, const char* label, int x, int y, int w, int h) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    strncpy(widget->text, label, sizeof(widget->text) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_PARAMETER_CONTROL;
    widget->visible = true;
    widget->enabled = true;
    widget->value = 0.5f;
    widget->minValue = 0.0f;
    widget->maxValue = 127.0f;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_label(const char* name, const char* text, int x, int y, int w, int h) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    strncpy(widget->text, text, sizeof(widget->text) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_LABEL;
    widget->visible = true;
    widget->enabled = true;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_toggle_button(const char* name, const char* text, int x, int y, int w, int h) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    strncpy(widget->text, text, sizeof(widget->text) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_TOGGLE_BUTTON;
    widget->visible = true;
    widget->enabled = true;
    widget->pressed = false;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_group_box(const char* name, const char* title, int x, int y, int w, int h) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    strncpy(widget->groupTitle, title, sizeof(widget->groupTitle) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_GROUP_BOX;
    widget->visible = true;
    widget->enabled = true;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_linear_slider(const char* name, int x, int y, int w, int h, bool vertical) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_LINEAR_SLIDER;
    widget->visible = true;
    widget->enabled = true;
    widget->value = 0.5f;
    widget->minValue = 0.0f;
    widget->maxValue = 1.0f;
    widget->modValue = 0.0f;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_envelope_display(const char* name, int x, int y, int w, int h) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name, sizeof(widget->name) - 1);
    widget->x = x; widget->y = y; widget->w = w; widget->h = h;
    widget->type = TF_WIDGET_ENVELOPE_DISPLAY;
    widget->visible = true;
    widget->enabled = true;

    tf_apply_tunefish_styling(widget);
    return widget;
}

TunefishWidget* tf_create_pushbutton(const char* name, const char* text, int x, int y, int w, int h, const char* label) {
    TunefishWidget* btn = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!btn) return NULL;
    btn->type = TF_WIDGET_BUTTON;
    strncpy(btn->name, name, sizeof(btn->name)-1);
    strncpy(btn->text, text, sizeof(btn->text)-1);
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
    btn->visible = true;
    btn->enabled = true;
    btn->pressed = false;
    if (label) strncpy(btn->text, label, sizeof(btn->text)-1);
    tf_style_as_main_button(btn);
    return btn;
}

// Styling Functions

void tf_apply_tunefish_styling(TunefishWidget* widget) {
    if (!widget) return;

    // Apply Tunefish color scheme
    widget->bgColor = TF_COL_BG_MAIN;
    widget->textColor = TF_COL_TEXT_NORMAL;
    widget->accentColor = TF_COL_SLIDER_FILL;
}

void tf_style_as_main_button(TunefishWidget* widget) {
    if (!widget) return;
    widget->bgColor = TF_COL_BUTTON_BASE;
    widget->textColor = TF_COL_TEXT_NORMAL;
}

void tf_style_as_parameter_knob(TunefishWidget* widget) {
    if (!widget) return;
    widget->bgColor = TF_COL_SLIDER_TRACK;
    widget->accentColor = TF_COL_SLIDER_FILL;
    widget->rotaryStartAngle = -2.35f; // ~-135 degrees
    widget->rotaryEndAngle = 2.35f;    // ~135 degrees
}

void tf_style_as_level_indicator(TunefishWidget* widget) {
    if (!widget) return;
    widget->bgColor = TF_COL_BG_MAIN;
    widget->accentColor = TF_COL_SLIDER_FILL;
    widget->textColor = TF_COL_TEXT_NORMAL;
}

// Widget Property Functions

void tf_widget_set_value(TunefishWidget* widget, float value) {
    if (!widget) return;

    // Clamp value to range
    if (value < widget->minValue) value = widget->minValue;
    if (value > widget->maxValue) value = widget->maxValue;

    widget->value = value;

    // Call value change callback if set
    if (widget->onValueChange) {
        widget->onValueChange(widget, value);
    }
}

float tf_widget_get_value(TunefishWidget* widget) {
    return widget ? widget->value : 0.0f;
}

void tf_widget_set_label(TunefishWidget* widget, const char* label) {
    if (widget && label) {
        strncpy(widget->text, label, sizeof(widget->text) - 1);
        widget->text[sizeof(widget->text) - 1] = '\0';
    }
}

void tf_widget_set_position(TunefishWidget* widget, int x, int y) {
    if (widget) {
        widget->x = x;
        widget->y = y;
    }
}

void tf_widget_set_size(TunefishWidget* widget, int w, int h) {
    if (widget) {
        widget->w = w;
        widget->h = h;
    }
}

void tf_widget_set_mod_value(TunefishWidget* widget, float modValue) {
    if (widget) {
        widget->modValue = modValue;
    }
}

// Event Handling

bool tf_widget_handle_mouse_event(TunefishWidget* widget, int mouseX, int mouseY, bool pressed) {
    if (!widget || !widget->visible || !widget->enabled) return false;
    
    // Check if point is inside widget
    if (!tf_widget_is_point_inside(widget, mouseX, mouseY)) {
        return false;
    }

    switch (widget->type) {
        case TF_WIDGET_BUTTON:
            if (pressed) {
                const bool is_radio_like =
                    (strncmp(widget->name, "unisono_", 8) == 0) ||
                    (strncmp(widget->name, "octave_", 7) == 0) ||
                    (strncmp(widget->name, "lfo1_shape_", 10) == 0) ||
                    (strncmp(widget->name, "lfo2_shape_", 10) == 0) ||
                    (strncmp(widget->name, "formant_", 8) == 0) ||
                    (strncmp(widget->name, "dx_op", 5) == 0 && strstr(widget->name, "_select") != NULL) ||
                    (strcmp(widget->name, "dx_page_toggle_btn") == 0) ||
                    (strcmp(widget->name, "page_toggle_btn") == 0);

                if (is_radio_like) {
                    widget->pressed = !widget->pressed;
                } else {
                    widget->pressed = true;
                }
                if (widget->onClick) widget->onClick(widget);
                if (widget->onValueChange) widget->onValueChange(widget, 1.0f);
            } else {
                const bool is_radio_like =
                    (strncmp(widget->name, "unisono_", 8) == 0) ||
                    (strncmp(widget->name, "octave_", 7) == 0) ||
                    (strncmp(widget->name, "lfo1_shape_", 10) == 0) ||
                    (strncmp(widget->name, "lfo2_shape_", 10) == 0) ||
                    (strncmp(widget->name, "formant_", 8) == 0) ||
                    (strncmp(widget->name, "dx_op", 5) == 0 && strstr(widget->name, "_select") != NULL) ||
                    (strcmp(widget->name, "dx_page_toggle_btn") == 0) ||
                    (strcmp(widget->name, "page_toggle_btn") == 0);

                if (!is_radio_like) {
                    widget->pressed = false;
                }
            }
            return true;

        case TF_WIDGET_TOGGLE_BUTTON:
            if (pressed) {
                // For formant radio buttons we don't toggle here – handled in exclusivity logic
                if (strncmp(widget->name, "formant_", 8) != 0) {
                    widget->pressed = !widget->pressed;
                }

                widget->value = widget->pressed ? 1.0f : 0.0f;

                // Prefer onValueChange for param linkage; fall back to onClick if provided
                if (widget->onValueChange) {
                    widget->onValueChange(widget, widget->value);
                } else if (widget->onClick) {
                    widget->onClick(widget);
                }
            }
            return true;

        case TF_WIDGET_ROTARY_SLIDER:
            if (pressed) {
                // Calculate new value based on mouse position relative to center
                int centerX = widget->x + widget->w / 2;
                int centerY = widget->y + widget->h / 2;
                int deltaX = mouseX - centerX;
                int deltaY = mouseY - centerY;

                // Convert to angle
                float angle = atan2(deltaY, deltaX);

                // Normalize to widget's angle range
                float range = widget->rotaryEndAngle - widget->rotaryStartAngle;
                float normalizedAngle = (angle - widget->rotaryStartAngle) / range;

                // Clamp and set value
                if (normalizedAngle < 0.0f) normalizedAngle = 0.0f;
                if (normalizedAngle > 1.0f) normalizedAngle = 1.0f;

                widget->value = normalizedAngle;

                // Call value change callback
                if (widget->onValueChange) {
                    widget->onValueChange(widget, widget->value);
                }
            }
            return true;

        case TF_WIDGET_COMBO_BOX:
            if (pressed) {
                // Check if up/down button was clicked
                int buttonX = widget->x + widget->w - 11;
                int buttonW = 10;
                int buttonH = (widget->h - 2) / 2;

                if (mouseX >= buttonX && mouseX < buttonX + buttonW) {
                    // Inside button area - check which button
                    if (mouseY >= widget->y + 1 && mouseY < widget->y + 1 + buttonH) {
                        // Up button clicked - go to previous preset
                        if (widget->comboItems && widget->comboItemCount > 0) {
                            widget->selectedIndex--;
                            if (widget->selectedIndex < 0) {
                                widget->selectedIndex = widget->comboItemCount - 1;
                            }
                            // Normalize value and propagate to parameter system
                            if (widget->comboItemCount > 1) {
                                widget->value = (float)widget->selectedIndex / (float)(widget->comboItemCount - 1);
                            } else {
                                widget->value = 0.0f;
                            }

                            if (widget->onValueChange) {
                                widget->onValueChange(widget, widget->value);
                            }

                            // Call combo change callback
                            if (widget->onComboSelect) {
                                widget->onComboSelect(widget, widget->selectedIndex);
                            }
                        }
                    } else if (mouseY >= widget->y + 1 + buttonH + 1 &&
                               mouseY < widget->y + widget->h - 1) {
                        // Down button clicked - go to next preset
                        if (widget->comboItems && widget->comboItemCount > 0) {
                            widget->selectedIndex = (widget->selectedIndex + 1) % widget->comboItemCount;
                            if (widget->comboItemCount > 1) {
                                widget->value = (float)widget->selectedIndex / (float)(widget->comboItemCount - 1);
                            } else {
                                widget->value = 0.0f;
                            }

                            if (widget->onValueChange) {
                                widget->onValueChange(widget, widget->value);
                            }

                            // Call combo change callback
                            if (widget->onComboSelect) {
                                widget->onComboSelect(widget, widget->selectedIndex);
                            }
                        }
                    }
                } else {
                    // Main combo box area clicked - could open dropdown or cycle through items
                    if (widget->comboItems && widget->comboItemCount > 0) {
                        widget->selectedIndex = (widget->selectedIndex + 1) % widget->comboItemCount;
                        if (widget->comboItemCount > 1) {
                            widget->value = (float)widget->selectedIndex / (float)(widget->comboItemCount - 1);
                        } else {
                            widget->value = 0.0f;
                        }

                        if (widget->onValueChange) {
                            widget->onValueChange(widget, widget->value);
                        }

                        // Call combo change callback
                        if (widget->onComboSelect) {
                            widget->onComboSelect(widget, widget->selectedIndex);
                        }
                    }
                }
            }
            return true;

        case TF_WIDGET_PARAMETER_CONTROL:
            if (pressed) {
                // Check which part was clicked (up button, down button, or value display)
                int buttonW = 14;
                int relativeX = mouseX - widget->x;

                if (relativeX < buttonW) {
                    // Up button clicked
                    float step = 1.0f / (widget->maxValue - widget->minValue);
                    widget->value = fminf(widget->value + step, 1.0f);
                } else if (relativeX > widget->w - buttonW) {
                    // Down button clicked
                    float step = 1.0f / (widget->maxValue - widget->minValue);
                    widget->value = fmaxf(widget->value - step, 0.0f);
                }

                // Call value change callback
                if (widget->onValueChange) {
                    widget->onValueChange(widget, widget->value);
                }
            }
            return true;

        case TF_WIDGET_LINEAR_SLIDER: {
            if (!pressed) return true;

            bool vertical = (widget->h > widget->w);
            float norm;

            if (vertical) {
                if (widget->h <= 0) return true;
                norm = 1.0f - (float)(mouseY - widget->y) / (float)widget->h;
            } else {
                if (widget->w <= 0) return true;
                norm = (float)(mouseX - widget->x) / (float)widget->w;
            }

            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;

            widget->value = norm;

            if (widget->onValueChange) widget->onValueChange(widget, widget->value);
            return true;
        }

        default:
            return false;
    }
}

bool tf_widget_is_point_inside(const TunefishWidget* widget, int x, int y) {
    if (!widget) return false;
    return (x >= widget->x && x < widget->x + widget->w &&
            y >= widget->y && y < widget->y + widget->h);
}

// Utility Functions

void tf_widget_destroy(TunefishWidget* widget) {
    if (!widget) return;

    // Free combo box items
    if (widget->comboItems) {
        for (int i = 0; i < widget->comboItemCount; i++) {
            free(widget->comboItems[i]);
        }
        free(widget->comboItems);
    }

    if (widget->bitmapOwned && widget->bitmapPixels) {
        free(widget->bitmapPixels);
        widget->bitmapPixels = NULL;
    }
    if (widget->bitmapOwned && widget->bitmapPixels32) {
        free(widget->bitmapPixels32);
        widget->bitmapPixels32 = NULL;
    }

    free(widget);
}

// Drawing dispatch function
void tf_draw_widget(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;

    switch (widget->type) {
        case TF_WIDGET_BUTTON:
            g_ft2WidgetRenderer.drawButton(widget);
            break;
        case TF_WIDGET_ROTARY_SLIDER:
            g_ft2WidgetRenderer.drawRotarySlider(widget);
            break;
        case TF_WIDGET_LINEAR_SLIDER:
            if (g_ft2WidgetRenderer.drawLinearSlider)
                g_ft2WidgetRenderer.drawLinearSlider(widget);
            break;
        case TF_WIDGET_COMBO_BOX:
            g_ft2WidgetRenderer.drawComboBox(widget);
            break;
        case TF_WIDGET_LEVEL_METER:
            g_ft2WidgetRenderer.drawLevelMeter(widget);
            break;
        case TF_WIDGET_LABEL:
            g_ft2WidgetRenderer.drawLabel(widget);
            break;
        case TF_WIDGET_TOGGLE_BUTTON:
            g_ft2WidgetRenderer.drawToggleButton(widget);
            break;
        case TF_WIDGET_PARAMETER_CONTROL:
            g_ft2WidgetRenderer.drawParameterControl(widget);
            break;
        case TF_WIDGET_GROUP_BOX:
            g_ft2WidgetRenderer.drawGroupBox(widget);
            break;
        case TF_WIDGET_WAVEFORM_VIEW:
            //printf("🌊 [WIDGET] Drawing waveform view widget at (%d,%d) %dx%d\n", 
            //       widget->x, widget->y, widget->w, widget->h);
            tf_draw_waveform_view_ft2(widget);
            break;
        case TF_WIDGET_BITMAP:
            tf_draw_bitmap_ft2(widget);
            break;
        default:
            break;
    }
}

// Waveform View Widget Implementation
static void tf_draw_waveform_view_ft2(const TunefishWidget* widget) {
    if (!widget || !widget->visible) return;
    // Use the latest waveform engine to render the view
    ft2_waveform_view_draw(widget->x, widget->y, widget->w, widget->h);
}


TunefishWidget* tf_create_waveform_view(const char* name, int x, int y, int w, int h) {
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;
    
    strncpy(widget->name, name, sizeof(widget->name) - 1);
    widget->x = x;
    widget->y = y;
    widget->w = w;
    widget->h = h;
    widget->type = TF_WIDGET_WAVEFORM_VIEW;
    widget->visible = true;
    widget->enabled = true;
    widget->value = 0.0f;
    widget->minValue = 0.0f;
    widget->maxValue = 1.0f;
    widget->page = 0; // PAGE_BOTH
    
    // Apply styling
    widget->bgColor = TF_COL_BG_MAIN;
    widget->textColor = TF_COL_TEXT_NORMAL;
    widget->accentColor = TF_COL_BUTTON_ACCENT;
    
    return widget;
}

TunefishWidget* tf_create_bitmap(const char* name, int x, int y, int w, int h, uint8_t *pixels, int bmp_w, int bmp_h, bool take_ownership)
{
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name ? name : "bitmap", sizeof(widget->name) - 1);
    widget->x = x;
    widget->y = y;
    widget->w = (w > 0) ? w : bmp_w;
    widget->h = (h > 0) ? h : bmp_h;
    widget->type = TF_WIDGET_BITMAP;
    widget->visible = true;
    widget->enabled = false; // static widget, no interaction
    widget->bitmapPixels = pixels;
    widget->bitmapW = bmp_w;
    widget->bitmapH = bmp_h;
    widget->bitmapOwned = take_ownership;

    return widget;
}

TunefishWidget* tf_create_bitmap32(const char* name, int x, int y, int w, int h, uint32_t *pixels, int bmp_w, int bmp_h, bool take_ownership)
{
    TunefishWidget* widget = (TunefishWidget*)calloc(1, sizeof(TunefishWidget));
    if (!widget) return NULL;

    strncpy(widget->name, name ? name : "bitmap", sizeof(widget->name) - 1);
    widget->x = x;
    widget->y = y;
    widget->w = (w > 0) ? w : bmp_w;
    widget->h = (h > 0) ? h : bmp_h;
    widget->type = TF_WIDGET_BITMAP;
    widget->visible = true;
    widget->enabled = false;
    widget->bitmapPixels32 = pixels;
    widget->bitmapW = bmp_w;
    widget->bitmapH = bmp_h;
    widget->bitmapOwned = take_ownership;
    widget->bitmap32 = true;

    return widget;
}

// Combo Box Management Functions
void tf_widget_clear_combo_items(TunefishWidget* widget)
{
    if (!widget || widget->type != TF_WIDGET_COMBO_BOX) return;
    
    // Free existing items
    if (widget->comboItems) {
        for (int i = 0; i < widget->comboItemCount; i++) {
            free(widget->comboItems[i]);
        }
        free(widget->comboItems);
        widget->comboItems = NULL;
    }
    widget->comboItemCount = 0;
    widget->selectedIndex = 0;
}

void tf_widget_add_combo_item(TunefishWidget* widget, const char* item)
{
    if (!widget || widget->type != TF_WIDGET_COMBO_BOX || !item) return;
    
    // Allocate space for new item
    char** newItems = (char**)realloc(widget->comboItems, (widget->comboItemCount + 1) * sizeof(char*));
    if (!newItems) return;
    
    widget->comboItems = newItems;
    
    // Copy the new item
    int len = strlen(item) + 1;
    widget->comboItems[widget->comboItemCount] = (char*)malloc(len);
    if (widget->comboItems[widget->comboItemCount]) {
        strcpy(widget->comboItems[widget->comboItemCount], item);
    } else {
        // Allocation failed, clean up
        free(widget->comboItems[widget->comboItemCount]);
        widget->comboItems[widget->comboItemCount] = NULL;
        return;
    }
    
    widget->comboItemCount++;
    
    // If this is the first item, select it
    if (widget->comboItemCount == 1) {
        widget->selectedIndex = 0;
    }
}
