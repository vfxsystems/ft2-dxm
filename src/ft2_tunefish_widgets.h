#ifndef FT2_TUNEFISH_WIDGETS_H
#define FT2_TUNEFISH_WIDGETS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tunefish Color Scheme (from tflookandfeel.cpp)
#define TF_COL_BG_MAIN           0x282828FF     // RGB(40,40,40) - Main background
#define TF_COL_SLIDER_FILL       0xF0F0C8FF     // RGB(240,240,200) - Slider fill
#define TF_COL_SLIDER_TRACK      0x282828FF     // RGB(40,40,40) - Track color
#define TF_COL_BUTTON_NORMAL     0x808080FF     // RGB(128,128,128) - Button normal
#define TF_COL_BUTTON_PRESSED    0x404040FF     // RGB(64,64,64) - Button pressed (darker)
#define TF_COL_BUTTON_BASE       0x303030FF     // RGB(48,48,48) - Dark button base
#define TF_COL_BUTTON_HIGHLIGHT  0xA0A0A0FF     // RGB(160,160,160) - Light highlight
#define TF_COL_BUTTON_ACCENT     0xFF8000FF     // RGB(255,128,0) - Accent orange
#define TF_COL_TEXT_NORMAL       0xFFFFFFFF     // RGB(255,255,255) - White text
#define TF_COL_TEXT_HIGHLIGHT    0xC8E6C8FF     // RGB(200,230,200) - Highlighted text
#define TF_COL_COMBO_BG          0x3C3C3CFF     // RGB(60,60,60) - ComboBox background
#define TF_COL_COMBO_TEXT        0x809650FF     // RGB(128,150,128) - ComboBox text
#define TF_COL_OUTLINE           0x707070FF     // RGB(112,112,112) - Outline color

// Forward declarations
typedef struct TunefishWidget TunefishWidget;

// Widget Types
typedef enum {
    TF_WIDGET_BUTTON = 0,
    TF_WIDGET_ROTARY_SLIDER,
    TF_WIDGET_LINEAR_SLIDER,
    TF_WIDGET_COMBO_BOX,
    TF_WIDGET_LEVEL_METER,
    TF_WIDGET_LABEL,
    TF_WIDGET_TOGGLE_BUTTON,
    TF_WIDGET_PARAMETER_CONTROL,
    TF_WIDGET_ENVELOPE_DISPLAY,
    TF_WIDGET_GROUP_BOX,
    TF_WIDGET_WAVEFORM_VIEW,
    TF_WIDGET_BITMAP
} TunefishWidgetType;

// Widget Structure
struct TunefishWidget {
    // Basic properties
    char name[64];
    char text[128];
    int x, y, w, h;
    TunefishWidgetType type;
    bool visible;
    bool enabled;
    bool pressed;
    bool highlighted;
    
    // Value properties
    float value;           // Current value (0.0 - 1.0)
    float minValue;        // Minimum value
    float maxValue;        // Maximum value
    float defaultValue;    // Default/reset value
    float modValue;        // Modulation amount (for rotary sliders)
    
    // Rotary slider properties
    float rotaryStartAngle;  // Start angle in radians
    float rotaryEndAngle;    // End angle in radians
    
    // ComboBox properties
    char** comboItems;       // Array of strings for combo box
    int comboItemCount;      // Number of items
    int selectedIndex;       // Currently selected item
    
    // Level meter properties
    int numLEDs;            // Number of LED segments
    float peakLevel;        // Peak level for meter
    bool showPeak;          // Show peak indicator
    
    // Group box properties
    char groupTitle[64];    // Title for group boxes

    // Bitmap properties
    uint8_t *bitmapPixels;  // 4-bit palette indices (PAL_TRANSPR supported)
    int bitmapW;
    int bitmapH;
    bool bitmapOwned;
    
    // Styling
    uint32_t bgColor;
    uint32_t textColor;
    uint32_t accentColor;
    int page; // PAGE_BOTH, PAGE1, PAGE2
    
    // Callbacks
    void (*onClick)(TunefishWidget* widget);
    void (*onValueChange)(TunefishWidget* widget, float newValue);
    void (*onComboSelect)(TunefishWidget* widget, int selectedIndex);
};

// Widget Creation Functions
TunefishWidget* tf_create_button(const char* name, const char* text, int x, int y, int w, int h);
TunefishWidget* tf_create_rotary_slider(const char* name, int x, int y, int radius, float startAngle, float endAngle);
TunefishWidget* tf_create_linear_slider(const char* name, int x, int y, int w, int h, bool vertical);
TunefishWidget* tf_create_combo_box(const char* name, int x, int y, int w, int h, const char* const* items, int itemCount);
TunefishWidget* tf_create_level_meter(const char* name, int x, int y, int w, int h, int numLEDs, bool showPeak);
TunefishWidget* tf_create_label(const char* name, const char* text, int x, int y, int w, int h);
TunefishWidget* tf_create_toggle_button(const char* name, const char* text, int x, int y, int w, int h);
TunefishWidget* tf_create_parameter_control(const char* name, const char* label, int x, int y, int w, int h);
TunefishWidget* tf_create_envelope_display(const char* name, int x, int y, int w, int h);
TunefishWidget* tf_create_group_box(const char* name, const char* title, int x, int y, int w, int h);
TunefishWidget* tf_create_pushbutton(const char* name, const char* text, int x, int y, int w, int h, const char* label);
TunefishWidget* tf_create_waveform_view(const char* name, int x, int y, int w, int h);
TunefishWidget* tf_create_bitmap(const char* name, int x, int y, int w, int h, uint8_t *pixels, int bmp_w, int bmp_h, bool take_ownership);

// Widget Property Functions
void tf_widget_set_value(TunefishWidget* widget, float value);
float tf_widget_get_value(TunefishWidget* widget);
void tf_widget_set_mod_value(TunefishWidget* widget, float modValue);
void tf_widget_set_position(TunefishWidget* widget, int x, int y);
void tf_widget_set_size(TunefishWidget* widget, int w, int h);
void tf_widget_set_label(TunefishWidget* widget, const char* label);

// Combo Box Management Functions
void tf_widget_clear_combo_items(TunefishWidget* widget);
void tf_widget_add_combo_item(TunefishWidget* widget, const char* item);

// Widget Drawing Functions (Modular Renderer)
typedef struct {
    void (*drawButton)(const TunefishWidget* widget);
    void (*drawRotarySlider)(const TunefishWidget* widget);
    void (*drawLinearSlider)(const TunefishWidget* widget);
    void (*drawComboBox)(const TunefishWidget* widget);
    void (*drawLevelMeter)(const TunefishWidget* widget);
    void (*drawLabel)(const TunefishWidget* widget);
    void (*drawToggleButton)(const TunefishWidget* widget);
    void (*drawParameterControl)(const TunefishWidget* widget);
    void (*drawEnvelopeDisplay)(const TunefishWidget* widget);
    void (*drawGroupBox)(const TunefishWidget* widget);
    void (*drawWaveformView)(const TunefishWidget* widget);
} TunefishWidgetRenderer;

// Styling Functions
void tf_apply_tunefish_styling(TunefishWidget* widget);
void tf_style_as_main_button(TunefishWidget* widget);
void tf_style_as_parameter_knob(TunefishWidget* widget);
void tf_style_as_level_indicator(TunefishWidget* widget);

// Event Handling
bool tf_widget_handle_mouse_event(TunefishWidget* widget, int mouseX, int mouseY, bool pressed);
bool tf_widget_handle_mouse_drag(TunefishWidget* widget, int mouseX, int mouseY, int deltaX, int deltaY);
bool tf_widget_is_point_inside(const TunefishWidget* widget, int x, int y);

// Core Functions
void tf_draw_widget(const TunefishWidget* widget);
void tf_widget_destroy(TunefishWidget* widget);
TunefishWidget* tf_widget_copy(const TunefishWidget* source);

#ifdef __cplusplus
}
#endif

#endif // FT2_TUNEFISH_WIDGETS_H 
