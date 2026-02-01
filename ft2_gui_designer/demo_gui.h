// Auto-generated GUI header file
#pragma once

#include <stdint.h>
#include <stdbool.h>

// Widget IDs
typedef enum {
    WID_main_button = 1,
    WID_settings_frame = 2,
    WID_sound_checkbox = 3,
    WID_COUNT = 3
} WidgetID;

// Widget runtime data
typedef struct {
    int id;
    int type;
    int x, y, w, h;
    bool visible;
    bool pressed;
    void (*callback)(int widget_id);
} gui_widget_t;

// Function prototypes
void init_gui(void);
void cleanup_gui(void);
void render_gui(uint32_t *framebuffer, int fb_width);
bool handle_gui_click(int x, int y);
void set_widget_callback(int widget_id, void (*callback)(int));
void set_widget_visible(int widget_id, bool visible);
bool is_widget_visible(int widget_id);

void main_button_clicked(int widget_id);  // Callback for Click Me
void settings_frame_clicked(int widget_id);  // Callback for Settings
void sound_checkbox_clicked(int widget_id);  // Callback for Enable Sound
