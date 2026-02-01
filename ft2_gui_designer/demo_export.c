#include "widgets.h"
#include "palette.h"
#include <stdio.h>
#include <string.h>

int main() {
    widget_manager_t manager;
    init_widget_manager(&manager);
    
    // Create a few demo widgets
    printf("Creating demo widgets...\n");
    
    // Add a button
    int btn_id = add_widget(&manager, WIDGET_PUSHBUTTON, 50, 50);
    widget_t *btn = get_widget(&manager, btn_id);
    if (btn) {
        strcpy(btn->caption, "Click Me");
        strcpy(btn->name, "main_button");
    }
    
    // Add a framebox
    int frame_id = add_widget(&manager, WIDGET_FRAMEBOX, 120, 50);
    widget_t *frame = get_widget(&manager, frame_id);
    if (frame) {
        strcpy(frame->caption, "Settings");
        strcpy(frame->name, "settings_frame");
        frame->w = 150;
        frame->h = 100;
    }
    
    // Add a checkbox
    int cb_id = add_widget(&manager, WIDGET_CHECKBOX, 130, 80);
    widget_t *cb = get_widget(&manager, cb_id);
    if (cb) {
        strcpy(cb->caption, "Enable Sound");
        strcpy(cb->name, "sound_checkbox");
    }
    
    printf("Created %d widgets\n", manager.widget_count);
    
    // Export C code
    printf("Exporting to C code...\n");
    if (!export_gui_code(&manager, "demo_gui"))
        return 1;
    
    printf("Demo complete! Check demo_gui.h and demo_gui.c\n");
    
    cleanup_widget_manager(&manager);
    return 0;
} 
