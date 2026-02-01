// Auto-generated GUI source file
#include "demo_gui.h"
#include <stdio.h>
#include <string.h>

// Drawing function prototypes (implement these in your project)
extern void fillRect(int x, int y, int w, int h, uint32_t color);
extern void drawRect(int x, int y, int w, int h, uint32_t color);
extern void textOut(int x, int y, uint32_t color, const char *text);
extern int textWidth(const char *text);
extern uint32_t get_palette_color(int pal_index);

// Palette colors (adjust for your system)
#define PAL_BUTTONS     0
#define PAL_BUTTON1     1
#define PAL_BUTTON2     2
#define PAL_BTNTEXT     3
#define PAL_DESKTOP     4

// Widget definitions
static gui_widget_t widgets[WID_COUNT] = {
    {1, 1, 50, 50, 59, 16, true, false, NULL}, // main_button
    {2, 6, 120, 50, 150, 100, true, false, NULL}, // settings_frame
    {3, 3, 130, 80, 13, 12, true, false, NULL}, // sound_checkbox
};

void init_gui(void) {
    // Initialize all widgets
    for (int i = 0; i < WID_COUNT; i++) {
        widgets[i].visible = true;
        widgets[i].pressed = false;
        widgets[i].callback = NULL;
    }
}

void cleanup_gui(void) {
    // Nothing to cleanup for now
}

void render_gui(uint32_t *framebuffer, int fb_width) {
    (void)framebuffer; (void)fb_width; // Suppress unused warnings
    
    // Render main_button (Click Me)
    if (widgets[0].visible) {
        // Draw button
        uint32_t btn_color = widgets[0].pressed ? get_palette_color(PAL_BUTTON2) : get_palette_color(PAL_BUTTONS);
        fillRect(50, 50, 59, 16, btn_color);
        drawRect(50, 50, 59, 16, get_palette_color(PAL_BUTTON1));
        int text_x = 50 + (59 - textWidth("Click Me")) / 2;
        int text_y = 50 + (16 - 8) / 2;
        textOut(text_x, text_y, get_palette_color(PAL_BTNTEXT), "Click Me");
    }
    
    // Render settings_frame (Settings)
    if (widgets[1].visible) {
        // Draw framebox
        drawRect(120, 50, 150, 100, get_palette_color(PAL_BUTTON1));
        fillRect(126, 46, textWidth("Settings") + 4, 8, get_palette_color(PAL_DESKTOP));
        textOut(128, 46, get_palette_color(PAL_BTNTEXT), "Settings");
    }
    
    // Render sound_checkbox (Enable Sound)
    if (widgets[2].visible) {
        // Draw checkbox
        drawRect(130, 80, 12, 12, get_palette_color(PAL_BUTTON1));
        fillRect(131, 81, 10, 10, get_palette_color(PAL_BUTTONS));
        textOut(146, 82, get_palette_color(PAL_BTNTEXT), "Enable Sound");
    }
}

bool handle_gui_click(int x, int y) {
    for (int i = 0; i < WID_COUNT; i++) {
        gui_widget_t *w = &widgets[i];
        if (w->visible && x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h) {
            w->pressed = true;
            if (w->callback) w->callback(w->id);
            return true;
        }
    }
    return false;
}

void set_widget_callback(int widget_id, void (*callback)(int)) {
    for (int i = 0; i < WID_COUNT; i++) {
        if (widgets[i].id == widget_id) {
            widgets[i].callback = callback;
            break;
        }
    }
}

void set_widget_visible(int widget_id, bool visible) {
    for (int i = 0; i < WID_COUNT; i++) {
        if (widgets[i].id == widget_id) {
            widgets[i].visible = visible;
            break;
        }
    }
}

bool is_widget_visible(int widget_id) {
    for (int i = 0; i < WID_COUNT; i++) {
        if (widgets[i].id == widget_id) {
            return widgets[i].visible;
        }
    }
    return false;
}

void main_button_clicked(int widget_id) {
    printf("Click Me clicked (ID: %d)\n", widget_id);
    // TODO: Implement main_button behavior
}

void settings_frame_clicked(int widget_id) {
    printf("Settings clicked (ID: %d)\n", widget_id);
    // TODO: Implement settings_frame behavior
}

void sound_checkbox_clicked(int widget_id) {
    printf("Enable Sound clicked (ID: %d)\n", widget_id);
    // TODO: Implement sound_checkbox behavior
}

