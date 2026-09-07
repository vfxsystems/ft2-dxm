#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "font.h"
#include "shared/ft2_ui_assets.h"
#include "shared/ft2_ui_schema.h"

#define MAX_WIDGETS 512
#define MAX_CAPTION_LEN 64

// Enhanced widget types - added more FT2 widgets
typedef enum {
    WIDGET_NONE = 0,
    WIDGET_PUSHBUTTON,
    WIDGET_RADIOBUTTON,
    WIDGET_CHECKBOX,
    WIDGET_SCROLLBAR,
    WIDGET_TEXTBOX,        // Text input field
    WIDGET_FRAMEBOX,       // Frame/border container
    WIDGET_LOGO,           // Logo/bitmap display
    WIDGET_CUSTOM_BUTTON,  // Custom graphics button
    WIDGET_COMBOBOX,       // New: Dropdown combo box
    WIDGET_DROPDOWN,       // New: Simple dropdown menu
    WIDGET_LISTBOX,        // New: List selection box
    WIDGET_SLIDER,         // New: Value slider
    WIDGET_PROGRESS_BAR,   // New: Progress indicator
    WIDGET_WAVEFORM_VIEW,
    WIDGET_TF_BUTTON,
    WIDGET_TF_TOGGLE,
    WIDGET_TF_LABEL,
    WIDGET_TF_ROTARY,
    WIDGET_TF_LINEAR,
    WIDGET_TF_COMBO,
    WIDGET_TF_METER,
    WIDGET_TF_PARAMETER,
    WIDGET_TF_ENVELOPE,
    WIDGET_TF_GROUP,
    WIDGET_TF_ARP_STEP,
    WIDGET_MIXER_STRIP,
    WIDGET_MIXER_GAIN,
    WIDGET_MIXER_PAN,
    WIDGET_MIXER_MUTE,
    WIDGET_MIXER_SCOPE,
    WIDGET_MIXER_MASTER,
    WIDGET_DSP_WINDOW,
    WIDGET_DSP_SLOT,
    WIDGET_DSP_MENU,
    WIDGET_DSP_PARAM
} WidgetType;

typedef enum {
    WIDGET_UNPRESSED = 0,
    WIDGET_PRESSED,
    WIDGET_HOVER
} WidgetState;

typedef enum {
    SCROLLBAR_HORIZONTAL = 0,
    SCROLLBAR_VERTICAL
} ScrollbarOrientation;

// Widget-specific data union
typedef union {
    struct {
        ScrollbarOrientation orientation;
        int min_value, max_value, current_value;
        int thumb_size;
        bool has_nudge_buttons;
    } scrollbar;
    
    struct {
        bool checked;
    } checkbox;
    
    struct {
        bool selected;
        int group_id;
    } radiobutton;
    
    struct {
        char text[MAX_CAPTION_LEN];
        int cursor_pos;
        bool focused;
    } textbox;
    
    struct {
        int border_type;  // Different frame styles
        bool filled;
    } framebox;
    
    struct {
        int bitmap_id;    // Which bitmap to display
        int scale;        // Scaling factor
        ft2_ui_bitmap_layer_t layer;
        ft2_ui_skin_part_t skin_part;
        uint8_t flags;
        uint8_t opacity;
    } logo;
    
    struct {
        char items[8][MAX_CAPTION_LEN];  // Up to 8 items
        int item_count;
        int selected_item;
        bool expanded;
    } combobox;
    
    struct {
        char items[16][MAX_CAPTION_LEN]; // Up to 16 items  
        int item_count;
        int selected_item;
        bool visible;
    } dropdown;
    
    struct {
        char items[20][MAX_CAPTION_LEN]; // Up to 20 items
        int item_count;
        int selected_item;
        int scroll_offset;
        int visible_items;               // How many items to show at once
    } listbox;
    
    struct {
        int min_value, max_value, current_value;
        bool horizontal;
        int step_size;
    } slider;
    
    struct {
        int min_value, max_value, current_value;
        bool show_percentage;
    } progress_bar;

    struct {
        float start_angle;
        float end_angle;
    } tf_rotary;

    struct {
        bool vertical;
    } tf_linear;

    struct {
        int num_leds;
        bool show_peak;
    } tf_meter;
} WidgetData;

// Enhanced widget structure
typedef struct {
    int id;
    WidgetType type;
    int x, y, w, h;
    bool visible;
    bool selected;
    WidgetState state;
    char caption[MAX_CAPTION_LEN];
    char caption2[MAX_CAPTION_LEN];  // Second caption line for some widgets
    char name[32];                   // C identifier name for code export
    ft2_ui_font_id_t font_type;      // Font to use for this widget
    ft2_ui_widget_page_t page;       // Tunefish/Dexed page assignment
    WidgetData data;
} widget_t;

typedef struct {
    widget_t widgets[MAX_WIDGETS];
    int widget_count;
    int selected_widget_id;
    int next_id;
} widget_manager_t;

// Function declarations
void init_widget_manager(widget_manager_t *manager);
void cleanup_widget_manager(widget_manager_t *manager);
void widgets_set_font_system(font_system_t *fs);
int add_widget(widget_manager_t *manager, int type, int x, int y);
void delete_widget(widget_manager_t *manager, int widget_id);
void delete_selected_widget(widget_manager_t *manager);
widget_t *get_widget(widget_manager_t *manager, int widget_id);
widget_t *get_selected_widget(widget_manager_t *manager);
void select_widget(widget_manager_t *manager, int widget_id);
void select_widget_at_point(widget_manager_t *manager, int x, int y);
void select_widget_at_point_on_page(widget_manager_t *manager, int x, int y, ft2_ui_widget_page_t current_page);
void clear_selection(widget_manager_t *manager);
void drag_selected_widget(widget_manager_t *manager, int new_x, int new_y);

// Rendering functions
void render_widgets(widget_manager_t *manager, uint32_t *framebuffer, int fb_width, int offset_x, int offset_y, ft2_ui_widget_page_t current_page);
void render_widget(widget_t *widget, uint32_t *framebuffer, int fb_width, int offset_x, int offset_y);
void draw_selection_handles(widget_t *widget, uint32_t *framebuffer, int fb_width, int offset_x, int offset_y);

// Enhanced widget drawing functions
void draw_pushbutton(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_radiobutton(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_checkbox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_scrollbar(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_textbox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_framebox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_logo(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_custom_button(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
void draw_combobox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);     // New
void draw_dropdown(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);    // New
void draw_listbox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);     // New
void draw_slider(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);      // New
void draw_progress_bar(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y); // New

// Text rendering with font selection
void draw_text(uint32_t *framebuffer, int fb_width, int x, int y, const char *text, uint32_t color);
void draw_text_ft2(font_system_t *fs, int x, int y, const char *text, uint32_t color);
void draw_text_with_font(uint32_t *framebuffer, int fb_width, int x, int y, const char *text, 
                        uint32_t color, ft2_ui_font_id_t font_type);  // New: font-aware text rendering
int get_text_width(const char *text);
int get_text_width_with_font(const char *text, ft2_ui_font_id_t font_type);  // New

// Widget placement and management
void place_widget(widget_manager_t *manager, int widget_kind, int x, int y, int scrollbar_orientation);
void draw_widget_properties(widget_manager_t *manager, uint32_t *framebuffer, int fb_width, int x, int y);

// File I/O
bool save_design(widget_manager_t *manager, const char *filename);
bool load_design(widget_manager_t *manager, const char *filename);

// Bitmap font system (new)
bool init_bitmap_fonts(void);
void cleanup_bitmap_fonts(void);
void draw_bitmap_text(uint32_t *framebuffer, int fb_width, int x, int y, const char *text, 
                     uint32_t color, ft2_ui_font_id_t font_type);

// Font metrics (new)
typedef struct {
    int char_width;
    int char_height;
    int line_spacing;
} FontMetrics;

FontMetrics get_font_metrics(ft2_ui_font_id_t font_type); 

// C code export functions
bool export_gui_code(widget_manager_t *manager, const char *base_filename);
bool export_gui_schema_code(widget_manager_t *manager, const char *base_filename);
bool is_valid_c_identifier(const char *name);
void generate_unique_widget_name(widget_t *widget);
void sanitize_widget_name(char *name); 
