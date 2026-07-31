#include "widgets.h"
#include "palette.h"
#include "font.h"
#include "assets.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "shared/ft2_ui_schema.h"

static font_system_t *g_widget_font_system = NULL;
static int g_widget_screen_height = 0;

#define BUTTON_GFX_BMP_WIDTH 90
#define ARROW_UP_GFX_CHAR 0x01
#define ARROW_DOWN_GFX_CHAR 0x02
#define ARROW_LEFT_GFX_CHAR 0x03
#define ARROW_RIGHT_GFX_CHAR 0x04
#define SMALL_1_GFX_CHAR 0x05
#define SMALL_6_GFX_CHAR 0x0A
#define DISKOP_PARENT_GFX_CHAR 0x0B

#define CHECKBOX_W 13
#define CHECKBOX_H 12
#define RADIOBUTTON_W 11
#define RADIOBUTTON_H 11

static void draw_tf_label(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_rotary(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_linear(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_combo(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_meter(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_parameter(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_group(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_envelope(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_tf_arp_step(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);
static void draw_waveform_view(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y);

static void designer_put_pixel(uint32_t *framebuffer, int fb_width, int x, int y, uint32_t color);
static void designer_draw_line(uint32_t *framebuffer, int fb_width, int x1, int y1, int x2, int y2, uint32_t color);
static void designer_draw_circle(uint32_t *framebuffer, int fb_width, int center_x, int center_y, int radius, uint32_t color, bool filled);
static void draw_scrollbar_button(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, bool pressed);
static void draw_scrollbar_arrow(const designer_assets_t *assets, uint32_t *framebuffer, int fb_width,
                                 int x, int y, int w, int h, uint8_t glyph);

void widgets_set_font_system(font_system_t *fs)
{
    g_widget_font_system = fs;
    g_widget_screen_height = fs ? fs->screen_height : 0;
}

void init_widget_manager(widget_manager_t *manager)
{
    memset(manager, 0, sizeof(widget_manager_t));
    manager->selected_widget_id = -1;
    manager->next_id = 1;
}

void cleanup_widget_manager(widget_manager_t *manager)
{
    (void)manager;
}

int add_widget(widget_manager_t *manager, int type, int x, int y)
{
    if (manager->widget_count >= MAX_WIDGETS) {
        return -1;
    }
    
    widget_t *widget = &manager->widgets[manager->widget_count];
    memset(widget, 0, sizeof(widget_t));
    
    widget->type = type;
    widget->id = manager->next_id++;
    widget->x = x;
    widget->y = y;
    widget->visible = true;
    widget->selected = false;
    widget->state = WIDGET_UNPRESSED;
    widget->font_type = FT2_UI_FONT_1;
    widget->page = FT2_UI_WIDGET_PAGE_BOTH;
    
    // Generate default name
    generate_unique_widget_name(widget);
    if (type == WIDGET_TF_ARP_STEP) {
        int next_step = 1;
        for (int i = 0; i < manager->widget_count; i++) {
            const widget_t *existing = &manager->widgets[i];
            if (existing->type != WIDGET_TF_ARP_STEP && strncmp(existing->name, "arp_step_", 9) != 0)
                continue;

            int idx = 0;
            if (sscanf(existing->name, "arp_step_%d", &idx) == 1 && idx >= next_step)
                next_step = idx + 1;
        }
        snprintf(widget->name, sizeof(widget->name), "arp_step_%02d", next_step);
    }
    
    // Set default sizes based on widget type (from FT2)
    switch (type) {
        case WIDGET_PUSHBUTTON:
            widget->w = 59;
            widget->h = 16;
            strcpy(widget->caption, "Button");
            break;
            
        case WIDGET_RADIOBUTTON:
            widget->w = 11;
            widget->h = 11;
            break;
            
        case WIDGET_CHECKBOX:
            widget->w = 13;
            widget->h = 12;
            break;
            
        case WIDGET_SCROLLBAR:
            widget->data.scrollbar.orientation = SCROLLBAR_HORIZONTAL;
            widget->w = 75;
            widget->h = 13;
            widget->data.scrollbar.min_value = 0;
            widget->data.scrollbar.max_value = 100;
            widget->data.scrollbar.current_value = 50;
            widget->data.scrollbar.thumb_size = 15;
            widget->data.scrollbar.has_nudge_buttons = false;
            break;
            
        case WIDGET_FRAMEBOX:
            widget->w = 100;
            widget->h = 80;
            strcpy(widget->caption, "Frame");
            widget->data.framebox.filled = false;
            widget->data.framebox.border_type = 0;
            break;
            
        case WIDGET_TEXTBOX:
            widget->w = 120;
            widget->h = 16;
            strcpy(widget->caption, "Text");
            break;
            
        case WIDGET_LOGO:
            widget->w = 64;
            widget->h = 32;
            strcpy(widget->caption, "Logo");
            widget->data.logo.scale = 1;
            widget->data.logo.bitmap_id = 0;
            widget->data.logo.layer = FT2_UI_BITMAP_LAYER_WIDGET;
            widget->data.logo.skin_part = FT2_UI_SKIN_PART_NONE;
            widget->data.logo.flags = FT2_UI_BITMAP_FLAG_NONE;
            break;
            
        case WIDGET_CUSTOM_BUTTON:
            widget->w = 60;
            widget->h = 16;
            strcpy(widget->caption, "Custom");
            break;
            
        case WIDGET_COMBOBOX:
            widget->w = 100;
            widget->h = 16;
            strcpy(widget->caption, "ComboBox");
            strcpy(widget->data.combobox.items[0], "Item 1");
            strcpy(widget->data.combobox.items[1], "Item 2");
            strcpy(widget->data.combobox.items[2], "Item 3");
            widget->data.combobox.item_count = 3;
            widget->data.combobox.selected_item = 0;
            widget->data.combobox.expanded = false;
            break;
            
        case WIDGET_DROPDOWN:
            widget->w = 80;
            widget->h = 16;
            strcpy(widget->caption, "Dropdown");
            break;
            
        case WIDGET_LISTBOX:
            widget->w = 120;
            widget->h = 80;
            strcpy(widget->caption, "ListBox");
            break;
            
        case WIDGET_SLIDER:
            widget->w = 100;
            widget->h = 16;
            widget->data.slider.horizontal = true;
            widget->data.slider.min_value = 0;
            widget->data.slider.max_value = 100;
            widget->data.slider.current_value = 50;
            widget->data.slider.step_size = 1;
            break;
            
        case WIDGET_PROGRESS_BAR:
            widget->w = 120;
            widget->h = 16;
            widget->data.progress_bar.min_value = 0;
            widget->data.progress_bar.max_value = 100;
            widget->data.progress_bar.current_value = 50;
            widget->data.progress_bar.show_percentage = true;
            break;

        case WIDGET_WAVEFORM_VIEW:
            widget->w = 284;
            widget->h = 72;
            strcpy(widget->caption, "Waveform");
            break;

        case WIDGET_TF_BUTTON:
            widget->w = 48;
            widget->h = 16;
            strcpy(widget->caption, "Button");
            break;

        case WIDGET_TF_TOGGLE:
            widget->w = 40;
            widget->h = 14;
            strcpy(widget->caption, "Toggle");
            break;

        case WIDGET_TF_LABEL:
            widget->w = 80;
            widget->h = 12;
            strcpy(widget->caption, "Label");
            break;

        case WIDGET_TF_ROTARY:
            widget->w = 36;
            widget->h = 36;
            widget->data.tf_rotary.start_angle = -2.35f;
            widget->data.tf_rotary.end_angle = 2.35f;
            strcpy(widget->caption, "Knob");
            break;

        case WIDGET_TF_LINEAR:
            widget->w = 20;
            widget->h = 52;
            widget->data.tf_linear.vertical = true;
            break;

        case WIDGET_TF_COMBO:
            widget->w = 110;
            widget->h = 20;
            strcpy(widget->caption, "Combo");
            break;

        case WIDGET_TF_METER:
            widget->w = 40;
            widget->h = 18;
            widget->data.tf_meter.num_leds = 12;
            widget->data.tf_meter.show_peak = true;
            break;

        case WIDGET_TF_PARAMETER:
            widget->w = 40;
            widget->h = 11;
            strcpy(widget->caption, "Param");
            break;

        case WIDGET_TF_ENVELOPE:
            widget->w = 284;
            widget->h = 72;
            strcpy(widget->caption, "Env");
            break;

        case WIDGET_TF_GROUP:
            widget->w = 120;
            widget->h = 80;
            strcpy(widget->caption, "Group");
            break;

        case WIDGET_TF_ARP_STEP:
            widget->w = 18;
            widget->h = 96;
            widget->data.tf_linear.vertical = true;
            break;

        case WIDGET_MIXER_STRIP:
            widget->w = 25;
            widget->h = 172;
            strcpy(widget->caption, "Strip");
            widget->data.framebox.filled = false;
            widget->data.framebox.border_type = 0;
            break;

        case WIDGET_MIXER_GAIN:
            widget->w = 20;
            widget->h = 80;
            widget->data.scrollbar.orientation = SCROLLBAR_VERTICAL;
            widget->data.scrollbar.min_value = 0;
            widget->data.scrollbar.max_value = 100;
            widget->data.scrollbar.current_value = 50;
            widget->data.scrollbar.thumb_size = 15;
            widget->data.scrollbar.has_nudge_buttons = false;
            strcpy(widget->caption, "Gain");
            break;

        case WIDGET_MIXER_PAN:
            widget->w = 20;
            widget->h = 7;
            widget->data.scrollbar.orientation = SCROLLBAR_HORIZONTAL;
            widget->data.scrollbar.min_value = 0;
            widget->data.scrollbar.max_value = 100;
            widget->data.scrollbar.current_value = 50;
            widget->data.scrollbar.thumb_size = 10;
            widget->data.scrollbar.has_nudge_buttons = false;
            strcpy(widget->caption, "Pan");
            break;

        case WIDGET_MIXER_MUTE:
            widget->w = 23;
            widget->h = 12;
            strcpy(widget->caption, "M");
            break;

        case WIDGET_MIXER_SCOPE:
            widget->w = 20;
            widget->h = 20;
            strcpy(widget->caption, "Scope");
            widget->data.framebox.filled = false;
            widget->data.framebox.border_type = 0;
            break;

        case WIDGET_MIXER_MASTER:
            widget->w = 40;
            widget->h = 80;
            widget->data.scrollbar.orientation = SCROLLBAR_VERTICAL;
            widget->data.scrollbar.min_value = 0;
            widget->data.scrollbar.max_value = 100;
            widget->data.scrollbar.current_value = 50;
            widget->data.scrollbar.thumb_size = 15;
            widget->data.scrollbar.has_nudge_buttons = false;
            strcpy(widget->caption, "Master");
            break;

        case WIDGET_DSP_WINDOW:
            widget->w = 192;
            widget->h = 120;
            strcpy(widget->caption, "DSP");
            widget->data.framebox.filled = false;
            widget->data.framebox.border_type = 0;
            break;

        case WIDGET_DSP_SLOT:
            widget->w = 72;
            widget->h = 14;
            strcpy(widget->caption, "Slot");
            break;

        case WIDGET_DSP_MENU:
            widget->w = 72;
            widget->h = 112;
            strcpy(widget->caption, "Menu");
            widget->data.framebox.filled = false;
            widget->data.framebox.border_type = 0;
            break;

        case WIDGET_DSP_PARAM:
            widget->w = 360;
            widget->h = 11;
            widget->data.scrollbar.orientation = SCROLLBAR_HORIZONTAL;
            widget->data.scrollbar.min_value = 0;
            widget->data.scrollbar.max_value = 100;
            widget->data.scrollbar.current_value = 50;
            widget->data.scrollbar.thumb_size = 10;
            widget->data.scrollbar.has_nudge_buttons = false;
            strcpy(widget->caption, "Param");
            break;
    }
    
    manager->widget_count++;
    return widget->id;
}

void delete_widget(widget_manager_t *manager, int widget_id)
{
    for (int i = 0; i < manager->widget_count; i++) {
        if (manager->widgets[i].id == widget_id) {
            // Shift remaining widgets down
            for (int j = i; j < manager->widget_count - 1; j++) {
                manager->widgets[j] = manager->widgets[j + 1];
            }
            manager->widget_count--;
            if (manager->selected_widget_id == widget_id) {
                manager->selected_widget_id = -1;
            }
            break;
        }
    }
}

void delete_selected_widget(widget_manager_t *manager)
{
    if (manager->selected_widget_id != -1) {
        delete_widget(manager, manager->selected_widget_id);
    }
}

widget_t *get_widget(widget_manager_t *manager, int widget_id)
{
    for (int i = 0; i < manager->widget_count; i++) {
        if (manager->widgets[i].id == widget_id) {
            return &manager->widgets[i];
        }
    }
    return NULL;
}

widget_t *get_selected_widget(widget_manager_t *manager)
{
    return get_widget(manager, manager->selected_widget_id);
}

void select_widget(widget_manager_t *manager, int widget_id)
{
    // Clear previous selection
    for (int i = 0; i < manager->widget_count; i++) {
        manager->widgets[i].selected = false;
    }
    
    // Set new selection
    widget_t *widget = get_widget(manager, widget_id);
    if (widget) {
        widget->selected = true;
        manager->selected_widget_id = widget_id;
    } else {
        manager->selected_widget_id = -1;
    }
}

void select_widget_at_point(widget_manager_t *manager, int x, int y)
{
    // Search widgets in reverse order (last drawn = topmost)
    for (int i = manager->widget_count - 1; i >= 0; i--) {
        widget_t *widget = &manager->widgets[i];
        if (widget->visible && 
            x >= widget->x && x < widget->x + widget->w &&
            y >= widget->y && y < widget->y + widget->h) {
            select_widget(manager, widget->id);
            return;
        }
    }
    
    // No widget found, clear selection
    clear_selection(manager);
}

void select_widget_at_point_on_page(widget_manager_t *manager, int x, int y, ft2_ui_widget_page_t current_page)
{
    // Search widgets in reverse order (last drawn = topmost)
    for (int i = manager->widget_count - 1; i >= 0; i--) {
        widget_t *widget = &manager->widgets[i];
        if (!widget->visible)
            continue;

        bool selectable = (current_page == FT2_UI_WIDGET_PAGE_BOTH) ||
                          (widget->page == FT2_UI_WIDGET_PAGE_BOTH) ||
                          (widget->page == current_page);
        if (!selectable)
            continue;

        if (x >= widget->x && x < widget->x + widget->w &&
            y >= widget->y && y < widget->y + widget->h) {
            select_widget(manager, widget->id);
            return;
        }
    }

    // No widget found, clear selection
    clear_selection(manager);
}

void clear_selection(widget_manager_t *manager)
{
    for (int i = 0; i < manager->widget_count; i++) {
        manager->widgets[i].selected = false;
    }
    manager->selected_widget_id = -1;
}

void drag_selected_widget(widget_manager_t *manager, int new_x, int new_y)
{
    widget_t *widget = get_selected_widget(manager);
    if (widget) {
        widget->x = new_x;
        widget->y = new_y;
        
        // Keep within bounds (simple constraint)
        if (widget->x < 0) widget->x = 0;
        if (widget->y < 0) widget->y = 0;
    }
}

static bool widget_is_background_bitmap(const widget_t *widget)
{
    return widget && widget->type == WIDGET_LOGO &&
           widget->data.logo.layer != FT2_UI_BITMAP_LAYER_WIDGET;
}

void render_widgets(widget_manager_t *manager, uint32_t *framebuffer, int fb_width, int offset_x, int offset_y, ft2_ui_widget_page_t current_page)
{
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < manager->widget_count; i++) {
        widget_t *widget = &manager->widgets[i];
        if (widget->visible) {
            bool should_render = (current_page == FT2_UI_WIDGET_PAGE_BOTH) ||
                                 (widget->page == FT2_UI_WIDGET_PAGE_BOTH) ||
                                 (widget->page == current_page);
            bool background_bitmap = widget_is_background_bitmap(widget);

            if (should_render && ((pass == 0) == background_bitmap))
            {
                render_widget(widget, framebuffer, fb_width, offset_x, offset_y);
            
                if (widget->selected) {
                    draw_selection_handles(widget, framebuffer, fb_width, offset_x, offset_y);
                }
            }
        }
        }
    }
}

void render_widget(widget_t *widget, uint32_t *framebuffer, int fb_width, int offset_x, int offset_y)
{
    int x = widget->x + offset_x;
    int y = widget->y + offset_y;
    
    switch (widget->type) {
        case WIDGET_PUSHBUTTON:
            draw_pushbutton(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_RADIOBUTTON:
            draw_radiobutton(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_CHECKBOX:
            draw_checkbox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_SCROLLBAR:
            draw_scrollbar(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TEXTBOX:
            draw_textbox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_FRAMEBOX:
            draw_framebox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_LOGO:
            draw_logo(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_CUSTOM_BUTTON:
            draw_custom_button(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_COMBOBOX:
            draw_combobox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_DROPDOWN:
            draw_dropdown(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_LISTBOX:
            draw_listbox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_SLIDER:
            draw_slider(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_PROGRESS_BAR:
            draw_progress_bar(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_WAVEFORM_VIEW:
            draw_waveform_view(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_BUTTON:
            draw_pushbutton(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_TOGGLE:
            draw_pushbutton(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_LABEL:
            draw_tf_label(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_ROTARY:
            draw_tf_rotary(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_LINEAR:
            draw_tf_linear(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_COMBO:
            draw_tf_combo(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_METER:
            draw_tf_meter(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_PARAMETER:
            draw_tf_parameter(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_ENVELOPE:
            draw_tf_envelope(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_GROUP:
            draw_tf_group(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_TF_ARP_STEP:
            draw_tf_arp_step(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_MIXER_STRIP:
            draw_framebox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_MIXER_GAIN:
            draw_scrollbar(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_MIXER_PAN:
            draw_scrollbar(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_MIXER_MUTE:
            draw_pushbutton(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_MIXER_SCOPE:
            draw_framebox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_MIXER_MASTER:
            draw_scrollbar(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_DSP_WINDOW:
            draw_framebox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_DSP_SLOT:
            draw_pushbutton(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_DSP_MENU:
            draw_framebox(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_DSP_PARAM:
            draw_scrollbar(widget, framebuffer, fb_width, x, y);
            break;
        case WIDGET_NONE:
        default:
            // Nothing to draw
            break;
    }
}

void draw_selection_handles(widget_t *widget, uint32_t *framebuffer, int fb_width, int offset_x, int offset_y)
{
    int x = widget->x + offset_x;
    int y = widget->y + offset_y;
    int w = widget->w;
    int h = widget->h;
    
    uint32_t handle_color = get_palette_color(PAL_MOUSEPT);
    
    // Draw corner handles
    fill_rect(framebuffer, fb_width, x - 2, y - 2, 4, 4, handle_color);
    fill_rect(framebuffer, fb_width, x + w - 2, y - 2, 4, 4, handle_color);
    fill_rect(framebuffer, fb_width, x - 2, y + h - 2, 4, 4, handle_color);
    fill_rect(framebuffer, fb_width, x + w - 2, y + h - 2, 4, 4, handle_color);
}

// Widget-specific drawing functions (extracted and simplified from FT2)

void draw_pushbutton(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    const designer_assets_t *assets = designer_assets();
    const uint8_t state = (widget->state == WIDGET_PRESSED) ? 1 : 0;
    int w = widget->w;
    int h = widget->h;
    int font_h = font_get_char_height_with_font(widget->font_type);

    fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BUTTONS));

    h_line(framebuffer, fb_width, x,         y,         w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x,         y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x,         y,         h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y,         h, get_palette_color(PAL_BCKGRND));

    if (state == 0)
    {
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 3, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 4, get_palette_color(PAL_BUTTON1));

        h_line(framebuffer, fb_width, x + 1,     y + h - 2, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 2, y + 1,     h - 3, get_palette_color(PAL_BUTTON2));
    }
    else
    {
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
    }

    if (widget->caption[0] != '\0')
    {
        const char *caption1 = widget->caption;
        const char *caption2 = widget->caption2;

        if ((uint8_t)caption1[0] < 32 && caption1[1] == '\0' && assets && assets->button_gfx)
        {
            const uint8_t *src8 = &assets->button_gfx[(caption1[0]-1) * 8];
            const char ch = caption1[0];
            int textW = 8;

            if (ch == ARROW_UP_GFX_CHAR || ch == ARROW_DOWN_GFX_CHAR)
                textW = 6;
            else if (ch == ARROW_LEFT_GFX_CHAR || ch == ARROW_RIGHT_GFX_CHAR)
                textW = 7;
            else if (ch >= SMALL_1_GFX_CHAR && ch <= SMALL_6_GFX_CHAR)
                textW = 5;
            else if (ch == DISKOP_PARENT_GFX_CHAR)
                textW = 10;

            int textX = x + ((w - textW) / 2);
            int textY = y + ((h - 8) / 2);

            if (state != 0)
            {
                textX++;
                textY++;
            }

            uint32_t *dst32 = &framebuffer[(textY * fb_width) + textX];
            for (int yy = 0; yy < 8; yy++, src8 += BUTTON_GFX_BMP_WIDTH, dst32 += fb_width)
            {
                for (int xx = 0; xx < textW; xx++)
                {
                    if (src8[xx] != 0)
                        dst32[xx] = get_palette_color(PAL_BTNTEXT);
                }
            }
        }
        else
        {
            int textX, textY, textW;

            if (caption2[0] != '\0')
            {
                textW = get_text_width_with_font(caption2, widget->font_type);
                textX = x + ((w - textW) / 2);
                textY = y + 6 + ((h - (font_h - 2)) / 2);

                if (state != 0)
                    draw_text_with_font(framebuffer, fb_width, textX + 1, textY + 1, caption2, get_palette_color(PAL_BTNTEXT), widget->font_type);
                else
                    draw_text_with_font(framebuffer, fb_width, textX, textY, caption2, get_palette_color(PAL_BTNTEXT), widget->font_type);

                y -= 5;
            }

            textW = get_text_width_with_font(caption1, widget->font_type);
            textX = x + ((w - textW) / 2);
            textY = y + ((h - (font_h - 2)) / 2);

            if (state != 0)
                draw_text_with_font(framebuffer, fb_width, textX + 1, textY + 1, caption1, get_palette_color(PAL_BTNTEXT), widget->font_type);
            else
                draw_text_with_font(framebuffer, fb_width, textX, textY, caption1, get_palette_color(PAL_BTNTEXT), widget->font_type);
        }
    }
}

void draw_radiobutton(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    const designer_assets_t *assets = designer_assets();
    if (!assets || !assets->radiobutton_gfx) return;

    uint8_t state = 0;
    if (widget->state == WIDGET_PRESSED)
        state = 2;
    else if (widget->data.radiobutton.selected)
        state = 1;

    const uint8_t *gfxPtr = &assets->radiobutton_gfx[state * (RADIOBUTTON_W * RADIOBUTTON_H)];
    blit_fast(framebuffer, fb_width, x, y, gfxPtr, RADIOBUTTON_W, RADIOBUTTON_H);
}

void draw_checkbox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    const designer_assets_t *assets = designer_assets();
    if (!assets || !assets->checkbox_gfx) return;

    int state = 0;
    if (widget->data.checkbox.checked)
        state += 2;
    if (widget->state == WIDGET_PRESSED)
        state += 1;

    const uint8_t *gfxPtr = &assets->checkbox_gfx[state * (CHECKBOX_W * CHECKBOX_H)];
    blit_fast(framebuffer, fb_width, x, y, gfxPtr, CHECKBOX_W, CHECKBOX_H);
}

void draw_scrollbar(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    fill_rect(framebuffer, fb_width, x, y, widget->w, widget->h, get_palette_color(PAL_BCKGRND));

    const designer_assets_t *assets = designer_assets();
    const bool pressed = (widget->state == WIDGET_PRESSED);
    const int range = widget->data.scrollbar.max_value - widget->data.scrollbar.min_value;
    const int thumb_size = (widget->data.scrollbar.thumb_size > 0) ? widget->data.scrollbar.thumb_size : 15;
    int track_x = x;
    int track_y = y;
    int track_w = widget->w;
    int track_h = widget->h;

    if (widget->data.scrollbar.has_nudge_buttons) {
        if (widget->data.scrollbar.orientation == SCROLLBAR_HORIZONTAL) {
            int button_size = widget->h;
            if (button_size > 0) {
                int left_x = x - button_size;
                int right_x = x + widget->w;
                int btn_y = y;

                draw_scrollbar_button(framebuffer, fb_width, left_x, btn_y, button_size, button_size, pressed);
                draw_scrollbar_button(framebuffer, fb_width, right_x, btn_y, button_size, button_size, pressed);
                draw_scrollbar_arrow(assets, framebuffer, fb_width, left_x, btn_y, button_size, button_size, ARROW_LEFT_GFX_CHAR);
                draw_scrollbar_arrow(assets, framebuffer, fb_width, right_x, btn_y, button_size, button_size, ARROW_RIGHT_GFX_CHAR);
            }
        } else {
            int button_size = widget->w;
            if (button_size > 0) {
                int top_y = y - button_size;
                int bottom_y = y + widget->h;
                int btn_x = x;

                draw_scrollbar_button(framebuffer, fb_width, btn_x, top_y, button_size, button_size, pressed);
                draw_scrollbar_button(framebuffer, fb_width, btn_x, bottom_y, button_size, button_size, pressed);
                draw_scrollbar_arrow(assets, framebuffer, fb_width, btn_x, top_y, button_size, button_size, ARROW_UP_GFX_CHAR);
                draw_scrollbar_arrow(assets, framebuffer, fb_width, btn_x, bottom_y, button_size, button_size, ARROW_DOWN_GFX_CHAR);
            }
        }
    }

    if (track_w < 3 || track_h < 3)
        return;

    if (range <= 0)
        return;

    if (widget->data.scrollbar.orientation == SCROLLBAR_HORIZONTAL)
    {
        const int available = track_w - thumb_size;
        const int thumb_pos = (widget->data.scrollbar.current_value * available) / range;
        const int thumbX = track_x + thumb_pos;
        const int thumbY = track_y + 1;
        const int thumbW = thumb_size;
        const int thumbH = track_h - 2;

        fill_rect(framebuffer, fb_width, thumbX, thumbY, thumbW, thumbH, get_palette_color(PAL_BUTTONS));

        if (!pressed)
        {
            h_line(framebuffer, fb_width, thumbX, thumbY,     thumbW - 1, get_palette_color(PAL_BUTTON1));
            v_line(framebuffer, fb_width, thumbX, thumbY + 1, thumbH - 2, get_palette_color(PAL_BUTTON1));
            h_line(framebuffer, fb_width, thumbX,              thumbY + thumbH - 1, thumbW - 1, get_palette_color(PAL_BUTTON2));
            v_line(framebuffer, fb_width, thumbX + thumbW - 1, thumbY,              thumbH,     get_palette_color(PAL_BUTTON2));
        }
        else
        {
            h_line(framebuffer, fb_width, thumbX, thumbY,     thumbW,     get_palette_color(PAL_BUTTON2));
            v_line(framebuffer, fb_width, thumbX, thumbY + 1, thumbH - 1, get_palette_color(PAL_BUTTON2));
        }
    }
    else
    {
        const int available = track_h - thumb_size;
        const int thumb_pos = (widget->data.scrollbar.current_value * available) / range;
        const int thumbX = track_x + 1;
        const int thumbY = track_y + thumb_pos;
        const int thumbW = track_w - 2;
        const int thumbH = thumb_size;

        fill_rect(framebuffer, fb_width, thumbX, thumbY, thumbW, thumbH, get_palette_color(PAL_BUTTONS));

        if (!pressed)
        {
            h_line(framebuffer, fb_width, thumbX, thumbY,     thumbW - 1, get_palette_color(PAL_BUTTON1));
            v_line(framebuffer, fb_width, thumbX, thumbY + 1, thumbH - 2, get_palette_color(PAL_BUTTON1));
            h_line(framebuffer, fb_width, thumbX,              thumbY + thumbH - 1, thumbW - 1, get_palette_color(PAL_BUTTON2));
            v_line(framebuffer, fb_width, thumbX + thumbW - 1, thumbY,              thumbH,     get_palette_color(PAL_BUTTON2));
        }
        else
        {
            h_line(framebuffer, fb_width, thumbX, thumbY,     thumbW,     get_palette_color(PAL_BUTTON2));
            v_line(framebuffer, fb_width, thumbX, thumbY + 1, thumbH - 1, get_palette_color(PAL_BUTTON2));
        }
    }
}

static void draw_scrollbar_button(uint32_t *framebuffer, int fb_width, int x, int y, int w, int h, bool pressed)
{
    if (x < 0 || y < 0 || x + w > fb_width)
        return;
    if (g_widget_screen_height > 0 && y + h > g_widget_screen_height)
        return;

    fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BUTTONS));

    h_line(framebuffer, fb_width, x,         y,         w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x,         y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x,         y,         h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y,         h, get_palette_color(PAL_BCKGRND));

    if (!pressed)
    {
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 3, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 4, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, x + 1,     y + h - 2, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 2, y + 1,     h - 3, get_palette_color(PAL_BUTTON2));
    }
    else
    {
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
    }
}

static void draw_scrollbar_arrow(const designer_assets_t *assets, uint32_t *framebuffer, int fb_width,
                                 int x, int y, int w, int h, uint8_t glyph)
{
    if (!assets || !assets->button_gfx)
        return;
    if (x < 0 || y < 0 || x + w > fb_width)
        return;
    if (g_widget_screen_height > 0 && y + h > g_widget_screen_height)
        return;

    int textW = 8;
    if (glyph == ARROW_UP_GFX_CHAR || glyph == ARROW_DOWN_GFX_CHAR)
        textW = 6;
    else if (glyph == ARROW_LEFT_GFX_CHAR || glyph == ARROW_RIGHT_GFX_CHAR)
        textW = 7;

    const uint8_t *src8 = &assets->button_gfx[(glyph - 1) * 8];
    int textX = x + (w - textW) / 2;
    int textY = y + (h - 8) / 2;
    uint32_t *dst32 = &framebuffer[(textY * fb_width) + textX];

    for (int yy = 0; yy < 8; yy++, src8 += BUTTON_GFX_BMP_WIDTH, dst32 += fb_width) {
        for (int xx = 0; xx < textW; xx++) {
            if (src8[xx] != 0)
                dst32[xx] = get_palette_color(PAL_BTNTEXT);
        }
    }
}

// FT2 font text rendering
void draw_text(uint32_t *framebuffer, int fb_width, int x, int y, const char *text, uint32_t color)
{
    if (!text || !g_widget_font_system) return;

    g_widget_font_system->framebuffer = framebuffer;
    g_widget_font_system->screen_width = fb_width;
    font_draw_text_with_font(g_widget_font_system, x, y, text, color, FT2_UI_FONT_1);
}

// New function that uses actual FT2 font system
void draw_text_ft2(font_system_t *fs, int x, int y, const char *text, uint32_t color)
{
    if (fs) {
        font_draw_text_with_font(fs, x, y, text, color, FT2_UI_FONT_1);
    }
}

void draw_text_with_font(uint32_t *framebuffer, int fb_width, int x, int y, const char *text, uint32_t color, ft2_ui_font_id_t font_type)
{
    if (!text || !g_widget_font_system) return;

    g_widget_font_system->framebuffer = framebuffer;
    g_widget_font_system->screen_width = fb_width;
    font_draw_text_with_font(g_widget_font_system, x, y, text, color, font_type);
}

int get_text_width(const char *text)
{
    return font_get_text_width_with_font(FT2_UI_FONT_1, text);
}

int get_text_width_with_font(const char *text, ft2_ui_font_id_t font_type)
{
    return font_get_text_width_with_font(font_type, text);
}

// Tool and interaction functions
void place_widget(widget_manager_t *manager, int widget_kind, int x, int y, int scrollbar_orientation)
{
    int widget_type = WIDGET_NONE;

    switch (widget_kind) {
        case FT2_UI_WIDGET_PUSHBUTTON: widget_type = WIDGET_PUSHBUTTON; break;
        case FT2_UI_WIDGET_RADIOBUTTON: widget_type = WIDGET_RADIOBUTTON; break;
        case FT2_UI_WIDGET_CHECKBOX: widget_type = WIDGET_CHECKBOX; break;
        case FT2_UI_WIDGET_SCROLLBAR: widget_type = WIDGET_SCROLLBAR; break;
        case FT2_UI_WIDGET_TEXTBOX: widget_type = WIDGET_TEXTBOX; break;
        case FT2_UI_WIDGET_FRAMEBOX: widget_type = WIDGET_FRAMEBOX; break;
        case FT2_UI_WIDGET_BITMAP: widget_type = WIDGET_LOGO; break;
        case FT2_UI_WIDGET_WAVEFORM_VIEW: widget_type = WIDGET_WAVEFORM_VIEW; break;
        case FT2_UI_WIDGET_TF_BUTTON: widget_type = WIDGET_TF_BUTTON; break;
        case FT2_UI_WIDGET_TF_TOGGLE_BUTTON: widget_type = WIDGET_TF_TOGGLE; break;
        case FT2_UI_WIDGET_TF_LABEL: widget_type = WIDGET_TF_LABEL; break;
        case FT2_UI_WIDGET_TF_ROTARY_SLIDER: widget_type = WIDGET_TF_ROTARY; break;
        case FT2_UI_WIDGET_TF_LINEAR_SLIDER: widget_type = WIDGET_TF_LINEAR; break;
        case FT2_UI_WIDGET_TF_ARP_STEP: widget_type = WIDGET_TF_ARP_STEP; break;
        case FT2_UI_WIDGET_TF_COMBO_BOX: widget_type = WIDGET_TF_COMBO; break;
        case FT2_UI_WIDGET_TF_LEVEL_METER: widget_type = WIDGET_TF_METER; break;
        case FT2_UI_WIDGET_TF_PARAMETER_CONTROL: widget_type = WIDGET_TF_PARAMETER; break;
        case FT2_UI_WIDGET_TF_ENVELOPE_DISPLAY: widget_type = WIDGET_TF_ENVELOPE; break;
        case FT2_UI_WIDGET_TF_GROUP_BOX: widget_type = WIDGET_TF_GROUP; break;
        case FT2_UI_WIDGET_MIXER_STRIP: widget_type = WIDGET_MIXER_STRIP; break;
        case FT2_UI_WIDGET_MIXER_GAIN: widget_type = WIDGET_MIXER_GAIN; break;
        case FT2_UI_WIDGET_MIXER_PAN: widget_type = WIDGET_MIXER_PAN; break;
        case FT2_UI_WIDGET_MIXER_MUTE: widget_type = WIDGET_MIXER_MUTE; break;
        case FT2_UI_WIDGET_MIXER_SCOPE: widget_type = WIDGET_MIXER_SCOPE; break;
        case FT2_UI_WIDGET_MIXER_MASTER: widget_type = WIDGET_MIXER_MASTER; break;
        case FT2_UI_WIDGET_DSP_WINDOW: widget_type = WIDGET_DSP_WINDOW; break;
        case FT2_UI_WIDGET_DSP_SLOT: widget_type = WIDGET_DSP_SLOT; break;
        case FT2_UI_WIDGET_DSP_MENU: widget_type = WIDGET_DSP_MENU; break;
        case FT2_UI_WIDGET_DSP_PARAM: widget_type = WIDGET_DSP_PARAM; break;
        default: break;
    }

    if (widget_type == WIDGET_NONE)
        return;

    int widget_id = add_widget(manager, widget_type, x, y);
    widget_t *widget = get_widget(manager, widget_id);

    if (widget && widget_type == WIDGET_SCROLLBAR) {
        if (scrollbar_orientation == SCROLLBAR_VERTICAL) {
            widget->data.scrollbar.orientation = SCROLLBAR_VERTICAL;
            widget->w = 13;
            widget->h = 75;
        } else {
            widget->data.scrollbar.orientation = SCROLLBAR_HORIZONTAL;
        }
    }
    if (widget) {
        if (widget_type == WIDGET_MIXER_GAIN || widget_type == WIDGET_MIXER_MASTER) {
            widget->data.scrollbar.orientation = SCROLLBAR_VERTICAL;
        } else if (widget_type == WIDGET_MIXER_PAN || widget_type == WIDGET_DSP_PARAM) {
            widget->data.scrollbar.orientation = SCROLLBAR_HORIZONTAL;
        }
    }

    select_widget(manager, widget_id);
}



void draw_widget_properties(widget_manager_t *manager, uint32_t *framebuffer, int fb_width, int x, int y)
{
    widget_t *widget = get_selected_widget(manager);
    if (!widget) {
        draw_text(framebuffer, fb_width, x, y, "No selection", get_palette_color(PAL_FORGRND));
        return;
    }
    
    char buffer[64];
    int line_height = 10;
    int current_y = y;
    
    // Draw widget info
    const char *type_names[] = {
        "None",           // WIDGET_NONE = 0
        "Button",         // WIDGET_PUSHBUTTON = 1
        "Radio",          // WIDGET_RADIOBUTTON = 2
        "Checkbox",       // WIDGET_CHECKBOX = 3
        "Scrollbar",      // WIDGET_SCROLLBAR = 4
        "Textbox",        // WIDGET_TEXTBOX = 5
        "Framebox",       // WIDGET_FRAMEBOX = 6
        "Logo",           // WIDGET_LOGO = 7
        "CustomButton",   // WIDGET_CUSTOM_BUTTON = 8
        "ComboBox",       // WIDGET_COMBOBOX = 9
        "Dropdown",       // WIDGET_DROPDOWN = 10
        "ListBox",        // WIDGET_LISTBOX = 11
        "Slider",         // WIDGET_SLIDER = 12
        "ProgressBar",    // WIDGET_PROGRESS_BAR = 13
        "Waveform",       // WIDGET_WAVEFORM_VIEW = 14
        "TF Button",      // WIDGET_TF_BUTTON = 15
        "TF Toggle",      // WIDGET_TF_TOGGLE = 16
        "TF Label",       // WIDGET_TF_LABEL = 17
        "TF Knob",        // WIDGET_TF_ROTARY = 18
        "TF Slider",      // WIDGET_TF_LINEAR = 19
        "TF Combo",       // WIDGET_TF_COMBO = 20
        "TF Meter",       // WIDGET_TF_METER = 21
        "TF Param",       // WIDGET_TF_PARAMETER = 22
        "TF Envelope",    // WIDGET_TF_ENVELOPE = 23
        "TF Group",       // WIDGET_TF_GROUP = 24
        "Mix Strip",      // WIDGET_MIXER_STRIP = 25
        "Mix Gain",       // WIDGET_MIXER_GAIN = 26
        "Mix Pan",        // WIDGET_MIXER_PAN = 27
        "Mix Mute",       // WIDGET_MIXER_MUTE = 28
        "Mix Scope",      // WIDGET_MIXER_SCOPE = 29
        "Mix Master",     // WIDGET_MIXER_MASTER = 30
        "DSP Window",     // WIDGET_DSP_WINDOW = 31
        "DSP Slot",       // WIDGET_DSP_SLOT = 32
        "DSP Menu",       // WIDGET_DSP_MENU = 33
        "DSP Param",      // WIDGET_DSP_PARAM = 34
        "TF Arp Step"     // WIDGET_TF_ARP_STEP = 35
    };
    
    // Bounds check to prevent crashes
    const char *type_name = "Unknown";
    if (widget->type >= 0 && widget->type < (int)(sizeof(type_names)/sizeof(type_names[0]))) {
        type_name = type_names[widget->type];
    }
    sprintf(buffer, "Type: %s", type_name);
    draw_text(framebuffer, fb_width, x, current_y, buffer, get_palette_color(PAL_FORGRND));
    current_y += line_height;
    
    sprintf(buffer, "Pos: %d,%d", widget->x, widget->y);
    draw_text(framebuffer, fb_width, x, current_y, buffer, get_palette_color(PAL_FORGRND));
    current_y += line_height;
    
    sprintf(buffer, "Size: %dx%d", widget->w, widget->h);
    draw_text(framebuffer, fb_width, x, current_y, buffer, get_palette_color(PAL_FORGRND));
    current_y += line_height;
    
    if (widget->caption[0] != '\0') {
        sprintf(buffer, "Text: %s", widget->caption);
        draw_text(framebuffer, fb_width, x, current_y, buffer, get_palette_color(PAL_FORGRND));
    }
}

static void write_quoted_string(FILE *file, const char *text)
{
    const char *src = text ? text : "";
    fputc('"', file);
    for (const char *p = src; *p != '\0'; p++) {
        switch (*p) {
            case '\\':
            case '"':
                fputc('\\', file);
                fputc(*p, file);
                break;
            case '\n':
                fputc('\\', file);
                fputc('n', file);
                break;
            case '\r':
                fputc('\\', file);
                fputc('r', file);
                break;
            case '\t':
                fputc('\\', file);
                fputc('t', file);
                break;
            default:
                fputc(*p, file);
                break;
        }
    }
    fputc('"', file);
}

static bool parse_int_field(char **cursor, int *out)
{
    char *p = *cursor;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p == '\0')
        return false;

    char *end = NULL;
    long value = strtol(p, &end, 10);
    if (end == p)
        return false;

    *out = (int)value;
    *cursor = end;
    return true;
}

static bool parse_string_field(char **cursor, char *out, size_t out_len)
{
    char *p = *cursor;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p == '\0')
        return false;

    size_t idx = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            char ch = *p;
            if (ch == '\\' && p[1] != '\0') {
                p++;
                ch = *p;
                if (ch == 'n') ch = '\n';
                else if (ch == 'r') ch = '\r';
                else if (ch == 't') ch = '\t';
            }
            if (idx + 1 < out_len)
                out[idx++] = ch;
            p++;
        }
        if (*p == '"')
            p++;
    } else {
        while (*p && !isspace((unsigned char)*p)) {
            if (idx + 1 < out_len)
                out[idx++] = *p;
            p++;
        }
    }

    out[idx] = '\0';
    *cursor = p;
    return true;
}

static bool designer_strnicmp_prefix(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca == '\0' || cb == '\0')
            return ca == cb;
        if (tolower(ca) != tolower(cb))
            return false;
    }
    return true;
}



bool save_design(widget_manager_t *manager, const char *filename)
{
    FILE *file = fopen(filename, "wb");
    if (!file) return false;

    // Write header
    fprintf(file, "FT2GUI_V7\n");
    fprintf(file, "%d\n", manager->widget_count);

    // Write widgets
    for (int i = 0; i < manager->widget_count; i++) {
        widget_t *w = &manager->widgets[i];
        const char *caption = (w->caption[0] != '\0') ? w->caption : "";
        const char *caption2 = (w->caption2[0] != '\0') ? w->caption2 : "";
        const char *name = (w->name[0] != '\0') ? w->name : "";
        fprintf(file, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
                w->type, w->x, w->y, w->w, w->h, w->state, w->visible ? 1 : 0,
                (int)w->page, (int)w->font_type, w->data.logo.bitmap_id,
                w->data.scrollbar.has_nudge_buttons ? 1 : 0,
                (int)w->data.logo.layer, (int)w->data.logo.flags, (int)w->data.logo.skin_part);
        write_quoted_string(file, caption);
        fputc(' ', file);
        write_quoted_string(file, caption2);
        fputc(' ', file);
        write_quoted_string(file, name);
        fputc('\n', file);
    }

    fclose(file);
    return true;
}



// New widget drawing functions

void draw_combobox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    
    // Fill background
    fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BUTTONS));
    
    // Draw outer border
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));
    
    // Draw inner borders (3D inset effect)
    h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON2));
    v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
    
    // Draw dropdown arrow on right side
    int arrow_x = x + w - 15;
    int arrow_y = y + h / 2;
    fill_rect(framebuffer, fb_width, arrow_x, arrow_y - 1, 8, 1, get_palette_color(PAL_BTNTEXT));
    fill_rect(framebuffer, fb_width, arrow_x + 1, arrow_y, 6, 1, get_palette_color(PAL_BTNTEXT));
    fill_rect(framebuffer, fb_width, arrow_x + 2, arrow_y + 1, 4, 1, get_palette_color(PAL_BTNTEXT));
    fill_rect(framebuffer, fb_width, arrow_x + 3, arrow_y + 2, 2, 1, get_palette_color(PAL_BTNTEXT));
    
    // Draw selected item text
    if (widget->data.combobox.item_count > 0 && widget->data.combobox.selected_item >= 0) {
        int text_x = x + 4;
        int text_y = y + (h - 8) / 2;
        draw_text_with_font(framebuffer, fb_width, text_x, text_y,
                 widget->data.combobox.items[widget->data.combobox.selected_item], 
                 get_palette_color(PAL_BTNTEXT), widget->font_type);
    }
    
    // Draw dropdown list if expanded
    if (widget->data.combobox.expanded && widget->data.combobox.item_count > 0) {
        int list_y = y + h;
        int list_h = widget->data.combobox.item_count * 12 + 2;
        
        // Background
        fill_rect(framebuffer, fb_width, x, list_y, w, list_h, get_palette_color(PAL_BUTTONS));
        
        // Border
        h_line(framebuffer, fb_width, x, list_y, w, get_palette_color(PAL_BCKGRND));
        h_line(framebuffer, fb_width, x, list_y + list_h - 1, w, get_palette_color(PAL_BCKGRND));
        v_line(framebuffer, fb_width, x, list_y, list_h, get_palette_color(PAL_BCKGRND));
        v_line(framebuffer, fb_width, x + w - 1, list_y, list_h, get_palette_color(PAL_BCKGRND));
        
        // Items
        for (int i = 0; i < widget->data.combobox.item_count; i++) {
            int item_y = list_y + 1 + i * 12;
            if (i == widget->data.combobox.selected_item) {
                fill_rect(framebuffer, fb_width, x + 1, item_y, w - 2, 12, get_palette_color(PAL_BUTTON1));
            }
            draw_text_with_font(framebuffer, fb_width, x + 4, item_y + 2,
                                widget->data.combobox.items[i],
                                get_palette_color(PAL_BTNTEXT), widget->font_type);
        }
    }
}

void draw_dropdown(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    // Similar to combobox but simpler - just a dropdown menu
    int w = widget->w;
    int h = widget->h;
    
    // Fill background  
    fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BUTTONS));
    
    // Draw border
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));
    
    // Draw 3D effect
    h_line(framebuffer, fb_width, x + 1, y + 1, w - 3, get_palette_color(PAL_BUTTON1));
    v_line(framebuffer, fb_width, x + 1, y + 2, h - 4, get_palette_color(PAL_BUTTON1));
    h_line(framebuffer, fb_width, x + 1, y + h - 2, w - 2, get_palette_color(PAL_BUTTON2));
    v_line(framebuffer, fb_width, x + w - 2, y + 1, h - 3, get_palette_color(PAL_BUTTON2));
    
    // Draw caption
    if (widget->caption[0] != '\0') {
        int text_x = x + (w - get_text_width_with_font(widget->caption, widget->font_type)) / 2;
        int text_y = y + (h - 8) / 2;
        draw_text_with_font(framebuffer, fb_width, text_x, text_y, widget->caption, get_palette_color(PAL_BTNTEXT), widget->font_type);
    }
}

void draw_listbox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    
    // Fill background with inset look
    fill_rect(framebuffer, fb_width, x + 2, y + 2, w - 4, h - 4, get_palette_color(PAL_BUTTONS));
    
    // Draw outer border
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));
    
    // Draw inner inset border
    h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON2));
    v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
    
    // Draw list items
    int visible_items = widget->data.listbox.visible_items;
    if (visible_items <= 0) visible_items = (h - 4) / 12; // Auto-calculate
    
    for (int i = 0; i < visible_items && (i + widget->data.listbox.scroll_offset) < widget->data.listbox.item_count; i++) {
        int item_index = i + widget->data.listbox.scroll_offset;
        int item_y = y + 2 + i * 12;
        
        // Highlight selected item
        if (item_index == widget->data.listbox.selected_item) {
            fill_rect(framebuffer, fb_width, x + 2, item_y, w - 4, 12, get_palette_color(PAL_BUTTON1));
        }
        
        // Draw item text
        draw_text_with_font(framebuffer, fb_width, x + 4, item_y + 2,
                            widget->data.listbox.items[item_index],
                            get_palette_color(PAL_BTNTEXT), widget->font_type);
    }
}

void draw_slider(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    
    if (widget->data.slider.horizontal) {
        // Horizontal slider
        int track_y = y + h / 2 - 2;
        
        // Draw track (inset)
        fill_rect(framebuffer, fb_width, x + 2, track_y, w - 4, 4, get_palette_color(PAL_BUTTON2));
        h_line(framebuffer, fb_width, x + 2, track_y, w - 4, get_palette_color(PAL_BCKGRND));
        h_line(framebuffer, fb_width, x + 2, track_y + 3, w - 4, get_palette_color(PAL_BUTTON1));
        
        // Calculate thumb position
        int range = widget->data.slider.max_value - widget->data.slider.min_value;
        int thumb_pos = 0;
        if (range > 0) {
            thumb_pos = ((widget->data.slider.current_value - widget->data.slider.min_value) * (w - 20)) / range;
        }
        
        // Draw thumb
        int thumb_x = x + 10 + thumb_pos;
        fill_rect(framebuffer, fb_width, thumb_x - 3, y, 6, h, get_palette_color(PAL_BUTTONS));
        h_line(framebuffer, fb_width, thumb_x - 3, y, 6, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, thumb_x - 3, y, h, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, thumb_x - 3, y + h - 1, 6, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, thumb_x + 2, y, h, get_palette_color(PAL_BUTTON2));
    } else {
        // Vertical slider
        int track_x = x + w / 2 - 2;
        
        // Draw track (inset)
        fill_rect(framebuffer, fb_width, track_x, y + 2, 4, h - 4, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, track_x, y + 2, h - 4, get_palette_color(PAL_BCKGRND));
        v_line(framebuffer, fb_width, track_x + 3, y + 2, h - 4, get_palette_color(PAL_BUTTON1));
        
        // Calculate thumb position
        int range = widget->data.slider.max_value - widget->data.slider.min_value;
        int thumb_pos = 0;
        if (range > 0) {
            thumb_pos = ((widget->data.slider.current_value - widget->data.slider.min_value) * (h - 20)) / range;
        }
        
        // Draw thumb
        int thumb_y = y + 10 + thumb_pos;
        fill_rect(framebuffer, fb_width, x, thumb_y - 3, w, 6, get_palette_color(PAL_BUTTONS));
        h_line(framebuffer, fb_width, x, thumb_y - 3, w, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x, thumb_y - 3, 6, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, x, thumb_y + 2, w, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 1, thumb_y - 3, 6, get_palette_color(PAL_BUTTON2));
    }
}

void draw_progress_bar(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    
    // Fill background (inset look)
    fill_rect(framebuffer, fb_width, x + 2, y + 2, w - 4, h - 4, get_palette_color(PAL_BUTTONS));
    
    // Draw outer border
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));
    
    // Draw inner inset border
    h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON2));
    v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
    
    // Calculate progress width
    int range = widget->data.progress_bar.max_value - widget->data.progress_bar.min_value;
    int progress_w = 0;
    if (range > 0) {
        progress_w = ((widget->data.progress_bar.current_value - widget->data.progress_bar.min_value) * (w - 4)) / range;
    }
    
    // Draw progress fill
    if (progress_w > 0) {
        fill_rect(framebuffer, fb_width, x + 2, y + 2, progress_w, h - 4, get_palette_color(PAL_BUTTON1));
    }
    
    // Draw percentage text if enabled
    if (widget->data.progress_bar.show_percentage) {
        char percent_text[16];
        int percent = (range > 0) ? ((widget->data.progress_bar.current_value - widget->data.progress_bar.min_value) * 100) / range : 0;
        snprintf(percent_text, sizeof(percent_text), "%d%%", percent);
        
        int text_x = x + (w - get_text_width_with_font(percent_text, widget->font_type)) / 2;
        int text_y = y + (h - 8) / 2;
        draw_text_with_font(framebuffer, fb_width, text_x, text_y, percent_text, get_palette_color(PAL_BTNTEXT), widget->font_type);
    }
}

static void draw_tf_label(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    if (widget->caption[0] == '\0')
        return;

    int font_h = font_get_char_height_with_font(widget->font_type);
    int text_y = y + (widget->h - font_h) / 2;
    draw_text_with_font(framebuffer, fb_width, x, text_y, widget->caption,
                        get_palette_color(PAL_FORGRND), widget->font_type);
}

static void draw_tf_rotary(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    int cx = x + w / 2;
    int cy = y + h / 2;
    int radius = (w < h ? w : h) / 2 - 4;
    if (radius < 2)
        return;

    float start_angle = widget->data.tf_rotary.start_angle;
    float end_angle = widget->data.tf_rotary.end_angle;
    if (start_angle == 0.0f && end_angle == 0.0f) {
        start_angle = -2.35f;
        end_angle = 2.35f;
    }

    const float value = 0.5f;
    const float mod_value = 0.0f;
    float value_angle = start_angle + value * (end_angle - start_angle);
    float mod_angle = start_angle + (value * mod_value) * (end_angle - start_angle);

    uint32_t track_color = get_palette_color(PAL_BUTTON2);
    uint32_t fill_color = get_palette_color(PAL_BUTTONS);
    uint32_t arc_color = get_palette_color(PAL_PATTEXT);
    uint32_t mod_color = get_palette_color(PAL_TEXTMRK);

    designer_draw_circle(framebuffer, fb_width, cx, cy, radius, track_color, true);
    designer_draw_circle(framebuffer, fb_width, cx, cy, radius - 3, fill_color, true);

    int arc_steps = 256;
    int r_inner = radius - 4;
    int r_outer = radius + 1;
    for (int i = 0; i <= arc_steps; i++) {
        float angle = start_angle + (float)i / (float)arc_steps * (value_angle - start_angle);
        int x1 = cx + (int)(r_inner * cosf(angle));
        int y1 = cy + (int)(r_inner * sinf(angle));
        int x2 = cx + (int)(r_outer * cosf(angle));
        int y2 = cy + (int)(r_outer * sinf(angle));
        designer_draw_line(framebuffer, fb_width, x1, y1, x2, y2, arc_color);
    }

    if (mod_value > 0.0f) {
        for (int i = 0; i <= arc_steps; i++) {
            float angle = start_angle + (float)i / (float)arc_steps * (mod_angle - start_angle);
            int px = cx + (int)(r_inner * cosf(angle));
            int py = cy + (int)(r_inner * sinf(angle));
            designer_put_pixel(framebuffer, fb_width, px, py, mod_color);
        }
    }

    char value_text[8];
    snprintf(value_text, sizeof(value_text), "%d", (int)(value * 99.0f));
    int text_w = get_text_width_with_font(value_text, FT2_UI_FONT_1);
    draw_text_with_font(framebuffer, fb_width, cx - (text_w / 4), cy - 4,
                        value_text, get_palette_color(PAL_FORGRND), FT2_UI_FONT_3);

    if (widget->caption[0] != '\0') {
        int label_w = get_text_width_with_font(widget->caption, widget->font_type);
        draw_text_with_font(framebuffer, fb_width, cx - label_w / 2, y + h + 2,
                            widget->caption, get_palette_color(PAL_FORGRND), widget->font_type);
    }
}

static void draw_tf_linear(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    bool vertical = h >= w;

    if (vertical) {
        int track_x = x + w / 2 - 1;
        fill_rect(framebuffer, fb_width, track_x, y + 2, 3, h - 4, get_palette_color(PAL_BUTTON2));

        int handle_h = 6;
        int handle_y = y + (h - handle_h) / 2;
        fill_rect(framebuffer, fb_width, track_x - 3, handle_y, 9, handle_h, get_palette_color(PAL_BUTTONS));
        h_line(framebuffer, fb_width, track_x - 3, handle_y, 9, get_palette_color(PAL_BCKGRND));
        h_line(framebuffer, fb_width, track_x - 3, handle_y + handle_h - 1, 9, get_palette_color(PAL_BCKGRND));
    } else {
        int track_y = y + h / 2 - 1;
        fill_rect(framebuffer, fb_width, x + 2, track_y, w - 4, 3, get_palette_color(PAL_BUTTON2));

        int handle_w = 8;
        int handle_x = x + (w - handle_w) / 2;
        fill_rect(framebuffer, fb_width, handle_x, track_y - 3, handle_w, 9, get_palette_color(PAL_BUTTONS));
        v_line(framebuffer, fb_width, handle_x, track_y - 3, 9, get_palette_color(PAL_BCKGRND));
        v_line(framebuffer, fb_width, handle_x + handle_w - 1, track_y - 3, 9, get_palette_color(PAL_BCKGRND));
    }
}

static void draw_tf_combo(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;

    fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BUTTON2));

    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));

    h_line(framebuffer, fb_width, x + 1, y + 1, w - 3, get_palette_color(PAL_BUTTONS));
    v_line(framebuffer, fb_width, x + 1, y + 2, h - 4, get_palette_color(PAL_BUTTONS));

    int arrow_x = x + w - 11;
    int button_w = 10;
    int button_h = (h - 2) / 2;

    fill_rect(framebuffer, fb_width, arrow_x + 1, y + 2, button_w - 2, button_h - 2, get_palette_color(PAL_BUTTONS));
    h_line(framebuffer, fb_width, arrow_x, y + 1, button_w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, arrow_x, y + button_h, button_w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, arrow_x, y + 1, button_h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, arrow_x + button_w - 1, y + 1, button_h, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, arrow_x + 1, y + 2, button_w - 3, get_palette_color(PAL_BUTTON1));
    v_line(framebuffer, fb_width, arrow_x + 1, y + 3, button_h - 4, get_palette_color(PAL_BUTTON1));

    fill_rect(framebuffer, fb_width, arrow_x + 1, y + button_h + 3, button_w - 2, button_h - 3, get_palette_color(PAL_BUTTONS));
    h_line(framebuffer, fb_width, arrow_x, y + button_h + 2, button_w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, arrow_x, y + button_h + 2 + button_h - 1, button_w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, arrow_x, y + button_h + 2, button_h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, arrow_x + button_w - 1, y + button_h + 2, button_h, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, arrow_x + 1, y + button_h + 3, button_w - 3, get_palette_color(PAL_BUTTON1));
    v_line(framebuffer, fb_width, arrow_x + 1, y + button_h + 4, button_h - 4, get_palette_color(PAL_BUTTON1));

    int up_cx = arrow_x + button_w / 2;
    int up_cy = y + 1 + button_h / 2;
    designer_draw_line(framebuffer, fb_width, up_cx - 2, up_cy + 1, up_cx, up_cy - 1,
                       get_palette_color(PAL_FORGRND));
    designer_draw_line(framebuffer, fb_width, up_cx, up_cy - 1, up_cx + 2, up_cy + 1,
                       get_palette_color(PAL_FORGRND));

    int down_cx = arrow_x + button_w / 2;
    int down_cy = y + 1 + button_h + 1 + button_h / 2;
    designer_draw_line(framebuffer, fb_width, down_cx - 2, down_cy - 1, down_cx, down_cy + 1,
                       get_palette_color(PAL_FORGRND));
    designer_draw_line(framebuffer, fb_width, down_cx, down_cy + 1, down_cx + 2, down_cy - 1,
                       get_palette_color(PAL_FORGRND));

    if (widget->caption[0] != '\0') {
        int text_y = y + (h - 8) / 2;
        draw_text_with_font(framebuffer, fb_width, x + 7, text_y, widget->caption,
                            get_palette_color(PAL_FORGRND), FT2_UI_FONT_1);
    }
}

static void draw_tf_meter(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    int segments = widget->data.tf_meter.num_leds > 0 ? widget->data.tf_meter.num_leds : 12;
    if (segments < 1)
        segments = 1;
    int led_w = (w - 2) / segments;
    int led_h = h - 2;

    float value = 0.5f;
    float peak_level = 0.0f;

    fill_rect(framebuffer, fb_width, x, y, w, h, get_palette_color(PAL_BCKGRND));

    int active_leds = (int)(value * segments);
    if (active_leds < 0)
        active_leds = 0;
    if (active_leds > segments)
        active_leds = segments;
    for (int i = 0; i < segments; i++) {
        int led_x = x + 1 + i * led_w;
        int led_y = y + 1;

        uint32_t color;
        if (i < active_leds) {
            if (i >= segments - 2) {
                color = get_palette_color(PAL_TEXTMRK);
            } else if (i >= segments - 4) {
                color = get_palette_color(PAL_BTNTEXT);
            } else {
                color = get_palette_color(PAL_FORGRND);
            }
        } else {
            color = get_palette_color(PAL_DSKTOP2);
        }

        fill_rect(framebuffer, fb_width, led_x, led_y, led_w - 1, led_h, color);
    }

    if (widget->data.tf_meter.show_peak && peak_level > 0.0f) {
        int peak_led = (int)(peak_level * segments);
        if (peak_led < segments) {
            int peak_x = x + 1 + peak_led * led_w;
            fill_rect(framebuffer, fb_width, peak_x, y + 1, led_w - 1, led_h, get_palette_color(PAL_TEXTMRK));
        }
    }

    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_FORGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_FORGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_FORGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_FORGRND));
}

static void draw_tf_parameter(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;

    fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BUTTONS));
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));

    if (widget->caption[0] != '\0') {
        int text_y = y + (h - 8) / 2;
        draw_text_with_font(framebuffer, fb_width, x + 2, text_y, widget->caption,
                            get_palette_color(PAL_BTNTEXT), widget->font_type);
    }
}

static void draw_tf_group(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    draw_framebox(widget, framebuffer, fb_width, x, y);
}

static void draw_tf_arp_step(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    if (w < 6 || h < 12)
        return;

    fill_rect(framebuffer, fb_width, x + 1, y + 1, w - 2, h - 2, get_palette_color(PAL_BUTTONS));
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));

    int inner_x = x + 2;
    int inner_y = y + 2;
    int inner_w = w - 4;
    int inner_h = h - 4;
    int bar_w = inner_w >= 5 ? 5 : inner_w;
    int bar_x = inner_x + (inner_w - bar_w) / 2;
    int bar_h = inner_h - 10;
    if (bar_h < 8)
        bar_h = inner_h;
    int bar_y = inner_y + inner_h - bar_h;

    fill_rect(framebuffer, fb_width, bar_x, bar_y, bar_w, bar_h, get_palette_color(PAL_BUTTON1));
    h_line(framebuffer, fb_width, bar_x, bar_y, bar_w, get_palette_color(PAL_FORGRND));
    h_line(framebuffer, fb_width, bar_x, bar_y + bar_h - 1, bar_w, get_palette_color(PAL_FORGRND));
    v_line(framebuffer, fb_width, bar_x, bar_y, bar_h, get_palette_color(PAL_FORGRND));
    v_line(framebuffer, fb_width, bar_x + bar_w - 1, bar_y, bar_h, get_palette_color(PAL_FORGRND));

    int cap = 0;
    if (widget->name[0] != '\0' && sscanf(widget->name, "arp_step_%d", &cap) == 1 && cap > 0) {
        char step_text[8];
        snprintf(step_text, sizeof(step_text), "%d", cap);
        ft2_ui_font_id_t font_type = widget->font_type;
        if (font_type < 0 || font_type >= FT2_UI_FONT_COUNT)
            font_type = FT2_UI_FONT_1;
        int text_w = get_text_width_with_font(step_text, font_type);
        draw_text_with_font(framebuffer, fb_width, x + (w - text_w) / 2, y + 1,
                            step_text, get_palette_color(PAL_FORGRND), font_type);
    }
}

static void draw_tf_envelope(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    draw_framebox(widget, framebuffer, fb_width, x, y);

    int x0 = x + 4;
    int y0 = y + h - 6;
    int x1 = x + w / 4;
    int y1 = y + 6;
    int x2 = x + w / 2;
    int y2 = y + h / 2;
    int x3 = x + w - 6;

    h_line(framebuffer, fb_width, x0, y0, x1 - x0, get_palette_color(PAL_BUTTON1));
    v_line(framebuffer, fb_width, x1, y1, y0 - y1, get_palette_color(PAL_BUTTON1));
    h_line(framebuffer, fb_width, x1, y1, x2 - x1, get_palette_color(PAL_BUTTON1));
    h_line(framebuffer, fb_width, x2, y2, x3 - x2, get_palette_color(PAL_BUTTON1));
}

static void draw_waveform_view(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    draw_framebox(widget, framebuffer, fb_width, x, y);

    int mid_y = y + h / 2;
    for (int i = 4; i < w - 4; i += 8) {
        h_line(framebuffer, fb_width, x + i, mid_y - 4, 4, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, x + i + 4, mid_y + 4, 4, get_palette_color(PAL_BUTTON1));
    }
}

static void designer_put_pixel(uint32_t *framebuffer, int fb_width, int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= fb_width)
        return;
    if (g_widget_screen_height > 0 && y >= g_widget_screen_height)
        return;
    framebuffer[y * fb_width + x] = color;
}

static void designer_draw_line(uint32_t *framebuffer, int fb_width, int x1, int y1, int x2, int y2, uint32_t color)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        designer_put_pixel(framebuffer, fb_width, x1, y1, color);
        if (x1 == x2 && y1 == y2)
            break;
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

static void designer_draw_circle(uint32_t *framebuffer, int fb_width, int center_x, int center_y, int radius, uint32_t color, bool filled)
{
    int r2 = radius * radius;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            int dist = x * x + y * y;
            if (filled) {
                if (dist <= r2)
                    designer_put_pixel(framebuffer, fb_width, center_x + x, center_y + y, color);
            } else {
                if (abs(dist - r2) < radius)
                    designer_put_pixel(framebuffer, fb_width, center_x + x, center_y + y, color);
            }
        }
    }
}

// Missing widget drawing functions

void draw_textbox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    
    // Fill background (inset look)
    fill_rect(framebuffer, fb_width, x + 2, y + 2, w - 4, h - 4, get_palette_color(PAL_BUTTONS));
    
    // Draw outer border
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));
    
    // Draw inner inset border
    h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON2));
    v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
    
    // Draw text content
    if (widget->data.textbox.text[0] != '\0') {
        int text_x = x + 4;
        int text_y = y + (h - 8) / 2;
        draw_text_with_font(framebuffer, fb_width, text_x, text_y, widget->data.textbox.text, get_palette_color(PAL_BTNTEXT), widget->font_type);
        
        // Draw cursor if focused
        if (widget->data.textbox.focused) {
            int cursor_x = text_x + get_text_width_with_font(widget->data.textbox.text, widget->font_type);
            v_line(framebuffer, fb_width, cursor_x, text_y, 8, get_palette_color(PAL_BTNTEXT));
        }
    } else if (widget->caption[0] != '\0') {
        // Show caption as placeholder
        int text_x = x + 4;
        int text_y = y + (h - 8) / 2;
        draw_text_with_font(framebuffer, fb_width, text_x, text_y, widget->caption, get_palette_color(PAL_BUTTON2), widget->font_type);
    }
}

void draw_framebox(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    
    // Fill background if specified
    if (widget->data.framebox.filled) {
        fill_rect(framebuffer, fb_width, x + 2, y + 2, w - 4, h - 4, get_palette_color(PAL_BUTTONS));
    }
    
    // Draw outer border
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));
    
    // Draw inner borders based on border type
    if (widget->data.framebox.border_type == 0) {
        // Simple border
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, x + 1, y + h - 2, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 2, y + 1, h - 3, get_palette_color(PAL_BUTTON2));
    } else {
        // Thick border - draw additional lines
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, x + 2, y + 2, w - 4, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x + 2, y + 3, h - 5, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, x + 2, y + h - 2, w - 3, get_palette_color(PAL_BUTTON2));
        h_line(framebuffer, fb_width, x + 1, y + h - 3, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 2, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 3, y + 3, h - 5, get_palette_color(PAL_BUTTON2));
    }
    
    // Draw title if present
    if (widget->caption[0] != '\0') {
        int text_x = x + 8;
        int text_y = y - 4;
        
        // Comprehensive bounds check for title drawing
        if (text_y >= 0 && text_x >= 2) {  // text_x >= 2 because we draw at text_x - 2
            // Draw background for title
            int title_w = get_text_width_with_font(widget->caption, widget->font_type) + 4;
            
            // Additional bounds check: ensure title doesn't extend beyond framebuffer width
            int max_width = fb_width - (text_x - 2);
            if (title_w > max_width) {
                title_w = max_width;
            }
            
            // Only draw if there's actually space for the title
            if (title_w > 0) {
                fill_rect(framebuffer, fb_width, text_x - 2, text_y, title_w, 8, get_palette_color(PAL_DESKTOP));
                draw_text_with_font(framebuffer, fb_width, text_x, text_y, widget->caption, get_palette_color(PAL_BTNTEXT), widget->font_type);
            }
        }
    }
}

void draw_logo(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    bool framed = widget->data.logo.layer == FT2_UI_BITMAP_LAYER_WIDGET;

    if (framed) {
        h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_DSKTOP1));
        h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_DSKTOP2));
        v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_DSKTOP1));
        v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_DSKTOP2));
    }
    
    const designer_bitmap_t *bmp = designer_bitmap_by_id(widget->data.logo.bitmap_id);
    if (bmp && (bmp->pixels || bmp->pixels32) && bmp->w > 0 && bmp->h > 0) {
        uint8_t trans_idx = PAL_TRANSPR;
        if (bmp->id >= FT2_UI_BITMAP_COUNT)
            trans_idx = (uint8_t)designer_bitmap_transparent_index(bmp->id);
        int scale = widget->data.logo.scale > 0 ? widget->data.logo.scale : 1;
        int draw_w = bmp->w * scale;
        int draw_h = bmp->h * scale;
        int start_x = x + (w - draw_w) / 2;
        int start_y = y + (h - draw_h) / 2;
        int clip_left = framed ? x + 1 : x;
        int clip_top = framed ? y + 1 : y;
        int clip_right = framed ? x + w - 1 : x + w;
        int clip_bottom = framed ? y + h - 1 : y + h;
        bool draw_truecolor = bmp->pixels32 && (widget->data.logo.layer != FT2_UI_BITMAP_LAYER_WIDGET || !bmp->pixels);

        if (draw_w > 0 && draw_h > 0) {
            for (int dy = 0; dy < draw_h; dy++) {
                int dst_y = start_y + dy;
                if (dst_y < clip_top || dst_y >= clip_bottom)
                    continue;

                int src_y = dy / scale;
                const uint8_t *src_row = bmp->pixels ? &bmp->pixels[src_y * bmp->w] : NULL;
                const uint32_t *src32_row = bmp->pixels32 ? &bmp->pixels32[src_y * bmp->w] : NULL;

                for (int dx = 0; dx < draw_w; dx++) {
                    int dst_x = start_x + dx;
                    if (dst_x < clip_left || dst_x >= clip_right)
                        continue;

                    int src_x = dx / scale;
                    if (draw_truecolor) {
                        uint32_t pix = src32_row[src_x];
                        if ((pix & 0x00FFFFFF) != 0x00FF00)
                            framebuffer[dst_y * fb_width + dst_x] = pix | 0xFF000000;
                    } else if (src_row) {
                        uint8_t pix = src_row[src_x];
                        if (pix != PAL_TRANSPR && pix != trans_idx)
                            framebuffer[dst_y * fb_width + dst_x] = get_palette_color(pix);
                    }
                }
            }
        }
    }
    
    // Draw caption below
    if (framed && widget->caption[0] != '\0') {
        int text_x = x + (w - get_text_width_with_font(widget->caption, widget->font_type)) / 2;
        int text_y = y + h - 12;
        draw_text_with_font(framebuffer, fb_width, text_x, text_y, widget->caption, get_palette_color(PAL_BTNTEXT), widget->font_type);
    }
}

void draw_custom_button(widget_t *widget, uint32_t *framebuffer, int fb_width, int x, int y)
{
    int w = widget->w;
    int h = widget->h;
    
    // Fill background with gradient effect
    for (int row = 0; row < h - 2; row++) {
        uint32_t color = (row < h / 2) ? get_palette_color(PAL_BUTTON1) : get_palette_color(PAL_BUTTONS);
        h_line(framebuffer, fb_width, x + 1, y + 1 + row, w - 2, color);
    }
    
    // Draw outer border
    h_line(framebuffer, fb_width, x, y, w, get_palette_color(PAL_BCKGRND));
    h_line(framebuffer, fb_width, x, y + h - 1, w, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x, y, h, get_palette_color(PAL_BCKGRND));
    v_line(framebuffer, fb_width, x + w - 1, y, h, get_palette_color(PAL_BCKGRND));
    
    // Draw 3D effect borders
    if (widget->state == WIDGET_UNPRESSED) {
        // Raised appearance with enhanced 3D effect
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 3, get_palette_color(PAL_BUTTON1));
        h_line(framebuffer, fb_width, x + 2, y + 2, w - 5, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 4, get_palette_color(PAL_BUTTON1));
        v_line(framebuffer, fb_width, x + 2, y + 3, h - 6, get_palette_color(PAL_BUTTON1));
        
        h_line(framebuffer, fb_width, x + 2, y + h - 2, w - 3, get_palette_color(PAL_BUTTON2));
        h_line(framebuffer, fb_width, x + 1, y + h - 3, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 2, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + w - 3, y + 3, h - 5, get_palette_color(PAL_BUTTON2));
    } else {
        // Pressed appearance
        h_line(framebuffer, fb_width, x + 1, y + 1, w - 2, get_palette_color(PAL_BUTTON2));
        v_line(framebuffer, fb_width, x + 1, y + 2, h - 3, get_palette_color(PAL_BUTTON2));
    }
    
    // Draw text with custom styling
    if (widget->caption[0] != '\0') {
        int text_x = x + (w - get_text_width_with_font(widget->caption, widget->font_type)) / 2;
        int text_y = y + (h - 8) / 2;
        
        if (widget->state == WIDGET_PRESSED) {
            text_x++;
            text_y++;
        }
        
        // Draw text with shadow effect
        draw_text_with_font(framebuffer, fb_width, text_x + 1, text_y + 1, widget->caption, get_palette_color(PAL_BUTTON2), widget->font_type);
        draw_text_with_font(framebuffer, fb_width, text_x, text_y, widget->caption, get_palette_color(PAL_BTNTEXT), widget->font_type);
    }
}

bool load_design(widget_manager_t *manager, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) return false;

    char first_line[32];
    if (!fgets(first_line, sizeof(first_line), file)) {
        fclose(file);
        return false;
    }

    int count;
    bool is_v7 = designer_strnicmp_prefix(first_line, "FT2GUI_V7", 9);
    bool is_v6 = designer_strnicmp_prefix(first_line, "FT2GUI_V6", 9);
    bool is_v5 = designer_strnicmp_prefix(first_line, "FT2GUI_V5", 9);
    bool is_v4 = designer_strnicmp_prefix(first_line, "FT2GUI_V4", 9);
    bool is_v3 = designer_strnicmp_prefix(first_line, "FT2GUI_V3", 9);
    bool is_v2 = designer_strnicmp_prefix(first_line, "FT2GUI_V2", 9);

    if (is_v7 || is_v6 || is_v5 || is_v4 || is_v3 || is_v2) {
        if (fscanf(file, "%d\n", &count) != 1) {
            fclose(file);
            return false;
        }
    } else {
        char *cursor = first_line;
        if (!parse_int_field(&cursor, &count)) {
            fclose(file);
            return false;
        }
    }

    // Clear existing widgets
    init_widget_manager(manager);

    // Load widgets
    for (int i = 0; i < count && i < MAX_WIDGETS; i++) {
        char line[512];
        do {
            if (!fgets(line, sizeof(line), file)) {
                fclose(file);
                return false;
            }
        } while (line[0] == '\n' || line[0] == '\r');

        int type = 0;
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        int state = 0;
        int visible = 0;
        int page = FT2_UI_WIDGET_PAGE_BOTH;
        int font_type = 0;
        int bitmap_id = 0;
        int nudge_buttons = 0;
        int bitmap_layer = FT2_UI_BITMAP_LAYER_WIDGET;
        int bitmap_flags = FT2_UI_BITMAP_FLAG_NONE;
        int skin_part = FT2_UI_SKIN_PART_NONE;
        char caption[MAX_CAPTION_LEN];
        char caption2[MAX_CAPTION_LEN];
        char name[32];

        char *cursor = line;
        if (!parse_int_field(&cursor, &type) ||
            !parse_int_field(&cursor, &x) ||
            !parse_int_field(&cursor, &y) ||
            !parse_int_field(&cursor, &w) ||
            !parse_int_field(&cursor, &h) ||
            !parse_int_field(&cursor, &state) ||
            !parse_int_field(&cursor, &visible)) {
            break;
        }

        if (is_v7) {
            if (!parse_int_field(&cursor, &page) ||
                !parse_int_field(&cursor, &font_type) ||
                !parse_int_field(&cursor, &bitmap_id) ||
                !parse_int_field(&cursor, &nudge_buttons) ||
                !parse_int_field(&cursor, &bitmap_layer) ||
                !parse_int_field(&cursor, &bitmap_flags) ||
                !parse_int_field(&cursor, &skin_part)) {
                break;
            }
        } else if (is_v6 || is_v5 || is_v4) {
            if (!parse_int_field(&cursor, &page) ||
                !parse_int_field(&cursor, &font_type) ||
                !parse_int_field(&cursor, &bitmap_id) ||
                !parse_int_field(&cursor, &nudge_buttons)) {
                break;
            }
        } else if (is_v3) {
            if (!parse_int_field(&cursor, &page) ||
                !parse_int_field(&cursor, &font_type) ||
                !parse_int_field(&cursor, &bitmap_id)) {
                break;
            }
        } else {
            char *probe = cursor;
            int legacy_page = FT2_UI_WIDGET_PAGE_BOTH;
            int legacy_font_type = 0;
            int legacy_bitmap_id = 0;
            int legacy_nudge_buttons = 0;

            if (parse_int_field(&probe, &legacy_page) &&
                parse_int_field(&probe, &legacy_font_type) &&
                parse_int_field(&probe, &legacy_bitmap_id) &&
                parse_int_field(&probe, &legacy_nudge_buttons)) {
                cursor = probe;
                page = legacy_page;
                font_type = legacy_font_type;
                bitmap_id = legacy_bitmap_id;
                nudge_buttons = legacy_nudge_buttons;
            } else {
                probe = cursor;
                if (!parse_int_field(&probe, &legacy_font_type) ||
                    !parse_int_field(&probe, &legacy_bitmap_id)) {
                    break;
                }
                cursor = probe;
                page = FT2_UI_WIDGET_PAGE_BOTH;
                font_type = legacy_font_type;
                bitmap_id = legacy_bitmap_id;
            }
        }

        if (!parse_string_field(&cursor, caption, sizeof(caption)) ||
            !parse_string_field(&cursor, caption2, sizeof(caption2)) ||
            !parse_string_field(&cursor, name, sizeof(name))) {
            break;
        }

        int widget_id = add_widget(manager, type, x, y);
        widget_t *widget = get_widget(manager, widget_id);
        if (!widget)
            break;

        widget->x = x;
        widget->y = y;
        widget->w = w;
        widget->h = h;
        widget->state = (WidgetState)state;
        widget->visible = visible != 0;
        if (page < 0) page = 0;
        if (page > 6) page = 6;
        widget->page = (ft2_ui_widget_page_t)page;
        widget->font_type = (font_type >= 0 && font_type < FT2_UI_FONT_COUNT)
            ? (ft2_ui_font_id_t)font_type
            : FT2_UI_FONT_1;
        widget->data.scrollbar.has_nudge_buttons = (nudge_buttons != 0);
        widget->data.logo.bitmap_id = bitmap_id;
        if (bitmap_layer < FT2_UI_BITMAP_LAYER_WIDGET) bitmap_layer = FT2_UI_BITMAP_LAYER_WIDGET;
        if (bitmap_layer > FT2_UI_BITMAP_LAYER_SKIN) bitmap_layer = FT2_UI_BITMAP_LAYER_SKIN;
        if (skin_part < FT2_UI_SKIN_PART_NONE) skin_part = FT2_UI_SKIN_PART_NONE;
        if (skin_part > FT2_UI_SKIN_PART_METER) skin_part = FT2_UI_SKIN_PART_METER;
        widget->data.logo.layer = (ft2_ui_bitmap_layer_t)bitmap_layer;
        widget->data.logo.flags = (uint8_t)(bitmap_flags & 0xFF);
        widget->data.logo.skin_part = (ft2_ui_skin_part_t)skin_part;

        if (strcmp(caption, "-") == 0)
            widget->caption[0] = '\0';
        else
            snprintf(widget->caption, sizeof(widget->caption), "%s", caption);

        if (strcmp(caption2, "-") == 0)
            widget->caption2[0] = '\0';
        else
            snprintf(widget->caption2, sizeof(widget->caption2), "%s", caption2);

        if (strcmp(name, "-") == 0)
            widget->name[0] = '\0';
        else
            snprintf(widget->name, sizeof(widget->name), "%s", name);

        if (type == WIDGET_TF_LINEAR && strncmp(widget->name, "arp_step_", 9) == 0) {
            widget->type = WIDGET_TF_ARP_STEP;
            widget->data.tf_linear.vertical = true;
        }

        sanitize_widget_name(widget->name);
    }

    fclose(file);
    return true;
}


 
// New widget drawing functions

// C code export functions
bool is_valid_c_identifier(const char *name) {
    if (!name || strlen(name) == 0) return false;
    
    // Must start with letter or underscore
    if (!isalpha(name[0]) && name[0] != '_') return false;
    
    // Rest must be alphanumeric or underscore
    for (int i = 1; name[i]; i++) {
        if (!isalnum(name[i]) && name[i] != '_') return false;
    }
    
    return true;
}

void sanitize_widget_name(char *name) {
    if (!name) return;
    
    // Replace invalid characters with underscores
    for (int i = 0; name[i]; i++) {
        if (!isalnum(name[i]) && name[i] != '_') {
            name[i] = '_';
        }
    }
    
    // Ensure first character is valid
    if (name[0] && !isalpha(name[0]) && name[0] != '_') {
        name[0] = '_';
    }
}

void generate_unique_widget_name(widget_t *widget) {
    const char *type_names[] = {
        "none", "button", "radio", "checkbox", "scrollbar", "textbox",
        "framebox", "logo", "custom_btn", "combobox", "dropdown",
        "listbox", "slider", "progress", "waveform", "tf_button",
        "tf_toggle", "tf_label", "tf_knob", "tf_slider", "tf_combo",
        "tf_meter", "tf_param", "tf_env", "tf_group",
        "mix_strip", "mix_gain", "mix_pan", "mix_mute", "mix_scope",
        "mix_master", "dsp_window", "dsp_slot", "dsp_menu", "dsp_param",
        "arp_step"
    };
    
    if (widget->type < sizeof(type_names)/sizeof(type_names[0])) {
        snprintf(widget->name, sizeof(widget->name), "%s_%d", 
                type_names[widget->type], widget->id);
    } else {
        snprintf(widget->name, sizeof(widget->name), "widget_%d", widget->id);
    }
}

typedef struct
{
    int indices[MAX_WIDGETS];
    int count;
} widget_index_list_t;

static void get_base_name(const char *path, char *out, size_t out_len)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    if (len >= out_len)
        len = out_len - 1;

    memcpy(out, base, len);
    out[len] = '\0';
}

static void strip_schema_suffix(char *base)
{
    const char *suffix = "_schema";
    size_t base_len = strlen(base);
    size_t suffix_len = strlen(suffix);

    if (base_len > suffix_len && strcmp(base + base_len - suffix_len, suffix) == 0)
        base[base_len - suffix_len] = '\0';
}

static void make_macro_prefix(const char *base, char *out, size_t out_len)
{
    size_t j = 0;
    for (size_t i = 0; base[i] != '\0' && j + 1 < out_len; i++) {
        char c = base[i];
        if (isalnum((unsigned char)c)) {
            if (j == 0 && isdigit((unsigned char)c) && j + 1 < out_len) {
                out[j++] = '_';
            }
            out[j++] = (char)toupper((unsigned char)c);
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
}

static void make_symbol_name(const char *base, char *out, size_t out_len)
{
    size_t j = 0;
    for (size_t i = 0; base[i] != '\0' && j + 1 < out_len; i++) {
        char c = base[i];
        if (isalnum((unsigned char)c)) {
            if (j == 0 && isdigit((unsigned char)c) && j + 1 < out_len) {
                out[j++] = '_';
            }
            out[j++] = (char)tolower((unsigned char)c);
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';
}

static void write_c_string(FILE *file, const char *text)
{
    fputc('"', file);
    for (const char *p = text; *p != '\0'; p++) {
        switch (*p) {
            case '\\': fputs("\\\\", file); break;
            case '\"': fputs("\\\"", file); break;
            case '\n': fputs("\\n", file); break;
            case '\r': fputs("\\r", file); break;
            case '\t': fputs("\\t", file); break;
            default: fputc(*p, file); break;
        }
    }
    fputc('"', file);
}

#define DESIGNER_TRANSPARENT_KEY_COLOR 0x00FF00
#define DESIGNER_BMP_BYTES_PER_LINE 24

typedef struct
{
    uint8_t *data;
    size_t size;
    size_t capacity;
} byte_buffer_t;

static bool buffer_reserve(byte_buffer_t *buf, size_t needed)
{
    if (needed <= buf->capacity)
        return true;
    size_t new_cap = buf->capacity ? buf->capacity : 256;
    while (new_cap < needed)
        new_cap *= 2;
    uint8_t *new_data = (uint8_t *)realloc(buf->data, new_cap);
    if (!new_data)
        return false;
    buf->data = new_data;
    buf->capacity = new_cap;
    return true;
}

static bool buffer_append(byte_buffer_t *buf, uint8_t value)
{
    if (!buffer_reserve(buf, buf->size + 1))
        return false;
    buf->data[buf->size++] = value;
    return true;
}

static void buffer_free(byte_buffer_t *buf)
{
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
    buf->capacity = 0;
}

static bool build_rle4_bmp(const designer_bitmap_t *bmp, uint8_t transparent_index,
                           uint8_t **out_data, size_t *out_len, int *out_w, int *out_h)
{
    if (!bmp || !bmp->pixels || bmp->w <= 0 || bmp->h <= 0)
        return false;

    int width = bmp->w;
    int height = bmp->h;
    if (width & 1)
        width++;

    uint8_t *row = (uint8_t *)malloc((size_t)width);
    if (!row)
        return false;

    byte_buffer_t rle = {0};
    for (int y = height - 1; y >= 0; y--) {
        const uint8_t *src_row = &bmp->pixels[y * bmp->w];
        for (int x = 0; x < bmp->w; x++) {
            uint8_t pix = src_row[x];
            if (pix == PAL_TRANSPR || pix > 15)
                row[x] = transparent_index;
            else
                row[x] = pix;
        }
        if (width > bmp->w)
            row[width - 1] = transparent_index;

        int x = 0;
        while (x < width) {
            int count = width - x;
            if (count > 255)
                count = 255;

            if (!buffer_append(&rle, 0) || !buffer_append(&rle, (uint8_t)count)) {
                free(row);
                buffer_free(&rle);
                return false;
            }

            int byte_count = (count + 1) / 2;
            for (int i = 0; i < byte_count; i++) {
                int idx = x + i * 2;
                uint8_t hi = row[idx] & 0x0F;
                uint8_t lo = 0;
                if (idx + 1 < x + count)
                    lo = row[idx + 1] & 0x0F;
                if (!buffer_append(&rle, (uint8_t)((hi << 4) | lo))) {
                    free(row);
                    buffer_free(&rle);
                    return false;
                }
            }

            if (byte_count & 1) {
                if (!buffer_append(&rle, 0)) {
                    free(row);
                    buffer_free(&rle);
                    return false;
                }
            }

            x += count;
        }

        if (!buffer_append(&rle, 0) || !buffer_append(&rle, 0)) {
            free(row);
            buffer_free(&rle);
            return false;
        }
    }

    if (!buffer_append(&rle, 0) || !buffer_append(&rle, 1)) {
        free(row);
        buffer_free(&rle);
        return false;
    }

    free(row);

    const uint32_t palette_entries = 16;
    const uint32_t header_size = 14 + 40 + palette_entries * 4;
    const uint32_t image_size = (uint32_t)rle.size;
    const uint32_t file_size = header_size + image_size;

    uint8_t *bmp_data = (uint8_t *)malloc(file_size);
    if (!bmp_data) {
        buffer_free(&rle);
        return false;
    }

    memset(bmp_data, 0, file_size);
    bmp_data[0] = 'B';
    bmp_data[1] = 'M';

    bmp_data[2] = (uint8_t)(file_size & 0xFF);
    bmp_data[3] = (uint8_t)((file_size >> 8) & 0xFF);
    bmp_data[4] = (uint8_t)((file_size >> 16) & 0xFF);
    bmp_data[5] = (uint8_t)((file_size >> 24) & 0xFF);

    bmp_data[10] = (uint8_t)(header_size & 0xFF);
    bmp_data[11] = (uint8_t)((header_size >> 8) & 0xFF);
    bmp_data[12] = (uint8_t)((header_size >> 16) & 0xFF);
    bmp_data[13] = (uint8_t)((header_size >> 24) & 0xFF);

    bmp_data[14] = 40;
    bmp_data[18] = (uint8_t)(width & 0xFF);
    bmp_data[19] = (uint8_t)((width >> 8) & 0xFF);
    bmp_data[20] = (uint8_t)((width >> 16) & 0xFF);
    bmp_data[21] = (uint8_t)((width >> 24) & 0xFF);
    bmp_data[22] = (uint8_t)(height & 0xFF);
    bmp_data[23] = (uint8_t)((height >> 8) & 0xFF);
    bmp_data[24] = (uint8_t)((height >> 16) & 0xFF);
    bmp_data[25] = (uint8_t)((height >> 24) & 0xFF);
    bmp_data[26] = 1;
    bmp_data[28] = 4;
    bmp_data[30] = 2;
    bmp_data[34] = (uint8_t)(image_size & 0xFF);
    bmp_data[35] = (uint8_t)((image_size >> 8) & 0xFF);
    bmp_data[36] = (uint8_t)((image_size >> 16) & 0xFF);
    bmp_data[37] = (uint8_t)((image_size >> 24) & 0xFF);
    bmp_data[38] = 0x12;
    bmp_data[39] = 0x0B;
    bmp_data[42] = 0x12;
    bmp_data[43] = 0x0B;
    bmp_data[46] = (uint8_t)(palette_entries & 0xFF);
    bmp_data[47] = (uint8_t)((palette_entries >> 8) & 0xFF);
    bmp_data[50] = (uint8_t)(palette_entries & 0xFF);
    bmp_data[51] = (uint8_t)((palette_entries >> 8) & 0xFF);

    uint8_t *pal = bmp_data + 54;
    for (int i = 0; i < 16; i++) {
        uint32_t color = designer_custom_palette_color(i) & 0x00FFFFFF;
        if (i == (int)transparent_index)
            color = DESIGNER_TRANSPARENT_KEY_COLOR;
        pal[i * 4 + 0] = (uint8_t)(color & 0xFF);
        pal[i * 4 + 1] = (uint8_t)((color >> 8) & 0xFF);
        pal[i * 4 + 2] = (uint8_t)((color >> 16) & 0xFF);
        pal[i * 4 + 3] = 0x00;
    }

    memcpy(bmp_data + header_size, rle.data, rle.size);
    buffer_free(&rle);

    if (out_data) *out_data = bmp_data;
    if (out_len) *out_len = file_size;
    if (out_w) *out_w = width;
    if (out_h) *out_h = height;
    return true;
}

static bool build_rgb32_bmp(const designer_bitmap_t *bmp, uint8_t **out_data, size_t *out_len)
{
    if (!bmp || !bmp->pixels32 || bmp->w <= 0 || bmp->h <= 0 || !out_data || !out_len)
        return false;

    const uint32_t width = bmp->w;
    const uint32_t height = bmp->h;
    const uint32_t header_size = 14 + 40;
    const uint32_t row_stride = width * 4;
    const uint32_t image_size = row_stride * height;
    const uint32_t file_size = header_size + image_size;

    uint8_t *bmp_data = (uint8_t *)malloc(file_size);
    if (!bmp_data)
        return false;

    memset(bmp_data, 0, file_size);
    bmp_data[0] = 'B';
    bmp_data[1] = 'M';
    bmp_data[2] = (uint8_t)(file_size & 0xFF);
    bmp_data[3] = (uint8_t)((file_size >> 8) & 0xFF);
    bmp_data[4] = (uint8_t)((file_size >> 16) & 0xFF);
    bmp_data[5] = (uint8_t)((file_size >> 24) & 0xFF);
    bmp_data[10] = (uint8_t)(header_size & 0xFF);
    bmp_data[14] = 40;
    bmp_data[18] = (uint8_t)(width & 0xFF);
    bmp_data[19] = (uint8_t)((width >> 8) & 0xFF);
    bmp_data[20] = (uint8_t)((width >> 16) & 0xFF);
    bmp_data[21] = (uint8_t)((width >> 24) & 0xFF);
    bmp_data[22] = (uint8_t)(height & 0xFF);
    bmp_data[23] = (uint8_t)((height >> 8) & 0xFF);
    bmp_data[24] = (uint8_t)((height >> 16) & 0xFF);
    bmp_data[25] = (uint8_t)((height >> 24) & 0xFF);
    bmp_data[26] = 1;
    bmp_data[28] = 32;
    bmp_data[34] = (uint8_t)(image_size & 0xFF);
    bmp_data[35] = (uint8_t)((image_size >> 8) & 0xFF);
    bmp_data[36] = (uint8_t)((image_size >> 16) & 0xFF);
    bmp_data[37] = (uint8_t)((image_size >> 24) & 0xFF);
    bmp_data[38] = 0x12;
    bmp_data[39] = 0x0B;
    bmp_data[42] = 0x12;
    bmp_data[43] = 0x0B;

    uint8_t *dst = bmp_data + header_size;
    for (int y = (int)height - 1; y >= 0; y--) {
        const uint32_t *src_row = &bmp->pixels32[y * width];
        for (uint32_t x = 0; x < width; x++) {
            uint32_t pix = src_row[x];
            bool transparent = ((pix & 0x00FFFFFF) == DESIGNER_TRANSPARENT_KEY_COLOR) ||
                               ((pix >> 24) != 0 && (pix >> 24) < 128);
            uint8_t r = transparent ? 0 : (uint8_t)RGB32_R(pix);
            uint8_t g = transparent ? 255 : (uint8_t)RGB32_G(pix);
            uint8_t b = transparent ? 0 : (uint8_t)RGB32_B(pix);
            *dst++ = b;
            *dst++ = g;
            *dst++ = r;
            *dst++ = transparent ? 0 : 255;
        }
    }

    *out_data = bmp_data;
    *out_len = file_size;
    return true;
}

static bool write_c_array_file(FILE *file, const char *array_name, const uint8_t *data, size_t len)
{
    if (fprintf(file, "const uint8_t %s[%zu] =\n{\n", array_name, len) < 0)
        return false;

    for (size_t i = 0; i < len; i++) {
        if (i % DESIGNER_BMP_BYTES_PER_LINE == 0)
            if (fputc('\t', file) == EOF)
                return false;

        if (i + 1 == len) {
            if (fprintf(file, "0x%02X", data[i]) < 0)
                return false;
        } else {
            if (fprintf(file, "0x%02X,", data[i]) < 0)
                return false;
        }

        if ((i % DESIGNER_BMP_BYTES_PER_LINE) == DESIGNER_BMP_BYTES_PER_LINE - 1 || i + 1 == len) {
            if (fputc('\n', file) == EOF)
                return false;
        }
    }

    if (fprintf(file, "};\n") < 0)
        return false;

    return true;
}

static bool write_bitmap_c_h(const char *out_dir, const char *base_name,
                             const char *array_name, const uint8_t *data, size_t len)
{
    char c_path[512];
    char h_path[512];

    snprintf(c_path, sizeof(c_path), "%s/%s.c", out_dir, base_name);
    snprintf(h_path, sizeof(h_path), "%s/%s.h", out_dir, base_name);

    FILE *hfile = fopen(h_path, "w");
    if (!hfile)
        return false;
    fprintf(hfile, "#pragma once\n\n#include <stdint.h>\n\n");
    fprintf(hfile, "extern const uint8_t %s[%zu];\n", array_name, len);
    fclose(hfile);

    FILE *cfile = fopen(c_path, "w");
    if (!cfile)
        return false;
    fprintf(cfile, "#include <stdint.h>\n\n");
    if (!write_c_array_file(cfile, array_name, data, len)) {
        fclose(cfile);
        return false;
    }
    fclose(cfile);
    return true;
}

static char *read_text_file(const char *path, size_t *out_len)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    char *data = (char *)malloc((size_t)len + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }
    if (fread(data, 1, (size_t)len, file) != (size_t)len) {
        free(data);
        fclose(file);
        return NULL;
    }
    data[len] = '\0';
    fclose(file);
    if (out_len) *out_len = (size_t)len;
    return data;
}

static bool write_text_file(const char *path, const char *data, size_t len)
{
    FILE *file = fopen(path, "wb");
    if (!file)
        return false;
    if (fwrite(data, 1, len, file) != len) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool insert_before_marker(const char *path, const char *marker, const char *insert_block)
{
    size_t len = 0;
    char *data = read_text_file(path, &len);
    if (!data)
        return false;
    char *pos = strstr(data, marker);
    if (!pos) {
        free(data);
        return false;
    }

    size_t prefix_len = (size_t)(pos - data);
    size_t insert_len = strlen(insert_block);
    size_t total_len = len + insert_len;
    char *out = (char *)malloc(total_len + 1);
    if (!out) {
        free(data);
        return false;
    }

    memcpy(out, data, prefix_len);
    memcpy(out + prefix_len, insert_block, insert_len);
    memcpy(out + prefix_len + insert_len, data + prefix_len, len - prefix_len);
    out[total_len] = '\0';

    bool ok = write_text_file(path, out, total_len);
    free(out);
    free(data);
    return ok;
}

static bool append_if_missing(const char *path, const char *needle, const char *block)
{
    size_t len = 0;
    char *data = read_text_file(path, &len);
    if (!data)
        return false;
    bool exists = strstr(data, needle) != NULL;
    free(data);
    if (exists)
        return true;

    FILE *file = fopen(path, "ab");
    if (!file)
        return false;
    if (fwrite(block, 1, strlen(block), file) != strlen(block)) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool update_gfxdata_header(const char *path, const char *array_name, size_t len)
{
    char line[256];
    snprintf(line, sizeof(line), "extern const uint8_t %s[%zu];\n", array_name, len);
    return append_if_missing(path, array_name, line);
}

static bool update_ui_assets_enum(const char *path, const char *enum_name)
{
    size_t len = 0;
    char *data = read_text_file(path, &len);
    if (!data)
        return false;
    bool exists = strstr(data, enum_name) != NULL;
    free(data);
    if (exists)
        return true;

    char line[256];
    snprintf(line, sizeof(line), "    %s,\n", enum_name);
    return insert_before_marker(path, "    FT2_UI_BITMAP_COUNT", line);
}

static bool update_ui_assets_registry(const char *path, const char *enum_name, const char *array_name, ft2_ui_bmp_format_t fmt)
{
    size_t len = 0;
    char *data = read_text_file(path, &len);
    if (!data)
        return false;
    bool exists = strstr(data, array_name) != NULL;
    if (exists) {
        free(data);
        return true;
    }
    free(data);

    char line[768];
    const char *fmt_name = (fmt == FT2_UI_BMP_FMT_RGB) ? "FT2_UI_BMP_FMT_RGB" : "FT2_UI_BMP_FMT_RLE4";
    snprintf(line, sizeof(line), "    { %s, %s, sizeof(%s), %s },\n", enum_name, array_name, array_name, fmt_name);
    return insert_before_marker(path, "};\n\nconst ft2_ui_asset_registry_t ft2_ui_assets", line);
}

static bool export_custom_bitmaps(const char *layout_prefix, const char *macro_prefix)
{
    const char *gfxdata_dir = "../src/gfxdata";
    const char *gfxdata_header = "../src/ft2_gfxdata.h";
    const char *assets_header = "../src/shared/ft2_ui_assets.h";
    const char *assets_source = "../src/shared/ft2_ui_assets.c";

    int total = designer_bitmap_count();
    int indices[DESIGNER_MAX_BITMAPS];
    int count = 0;

    for (int i = 0; i < total; i++) {
        const designer_bitmap_t *bmp = designer_bitmap_at(i);
        if (!bmp || bmp->id < FT2_UI_BITMAP_COUNT)
            continue;
        indices[count++] = i;
    }

    if (count == 0)
        return true;

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            const designer_bitmap_t *a = designer_bitmap_at(indices[i]);
            const designer_bitmap_t *b = designer_bitmap_at(indices[j]);
            if (a && b && a->id > b->id) {
                int tmp = indices[i];
                indices[i] = indices[j];
                indices[j] = tmp;
            }
        }
    }

    for (int i = 0; i < count; i++) {
        const designer_bitmap_t *bmp = designer_bitmap_at(indices[i]);
        if (!bmp)
            continue;

        char base_name[128];
        char array_name[160];
        char enum_name[192];

        snprintf(base_name, sizeof(base_name), "%s_image%d", layout_prefix, i + 1);
        snprintf(array_name, sizeof(array_name), "%s_image%dBMP", layout_prefix, i + 1);
        snprintf(enum_name, sizeof(enum_name), "FT2_UI_BITMAP_%s_IMAGE_%d", macro_prefix, i + 1);

        uint8_t *bmp_data = NULL;
        size_t bmp_len = 0;
        uint8_t transparent_index = (uint8_t)designer_bitmap_transparent_index(bmp->id);
        ft2_ui_bmp_format_t export_fmt = bmp->pixels32 ? FT2_UI_BMP_FMT_RGB : FT2_UI_BMP_FMT_RLE4;

        if (export_fmt == FT2_UI_BMP_FMT_RGB) {
            if (!build_rgb32_bmp(bmp, &bmp_data, &bmp_len))
                return false;
        } else {
            if (!build_rle4_bmp(bmp, transparent_index, &bmp_data, &bmp_len, NULL, NULL))
                return false;
        }

        if (!write_bitmap_c_h(gfxdata_dir, base_name, array_name, bmp_data, bmp_len)) {
            free(bmp_data);
            return false;
        }
        free(bmp_data);

        if (!update_gfxdata_header(gfxdata_header, array_name, bmp_len))
            return false;
        if (!update_ui_assets_enum(assets_header, enum_name))
            return false;
        if (!update_ui_assets_registry(assets_source, enum_name, array_name, export_fmt))
            return false;
    }

    return true;
}

static void write_optional_c_string(FILE *file, const char *text)
{
    if (!text || text[0] == '\0')
        fputs("NULL", file);
    else
        write_c_string(file, text);
}

static const char *schema_text_or_name(const widget_t *widget, bool use_caption2)
{
    if (!widget)
        return NULL;

    const char *text = use_caption2 ? widget->caption2 : widget->caption;
    if (text && text[0] != '\0')
        return text;
    if (widget->name[0] != '\0')
        return widget->name;
    return NULL;
}

static void make_action_base(const widget_t *widget, char *out, size_t out_len)
{
    char name_buf[64];
    const char *name = widget->name;

    if (!name || name[0] == '\0') {
        snprintf(name_buf, sizeof(name_buf), "widget_%d", widget->id);
        name = name_buf;
    }

    size_t j = 0;
    for (size_t i = 0; name[i] != '\0' && j + 1 < out_len; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c)) {
            if (j == 0 && isdigit((unsigned char)c) && j + 1 < out_len) {
                out[j++] = '_';
            }
            out[j++] = (char)toupper((unsigned char)c);
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';

    if (out[0] == '\0')
        snprintf(out, out_len, "WIDGET_%d", widget->id);
}

static int parse_trailing_index(const char *name)
{
    if (!name || name[0] == '\0')
        return -1;

    int len = (int)strlen(name);
    int i = len - 1;
    while (i >= 0 && !isdigit((unsigned char)name[i]))
        i--;
    if (i < 0)
        return -1;

    int end = i;
    while (i >= 0 && isdigit((unsigned char)name[i]))
        i--;
    int start = i + 1;
    if (start > end)
        return -1;

    return atoi(name + start);
}

static void collect_widget_lists(widget_manager_t *manager,
                                 widget_index_list_t *pushbuttons,
                                 widget_index_list_t *checkboxes,
                                 widget_index_list_t *radiobuttons,
                                 widget_index_list_t *scrollbars,
                                 widget_index_list_t *textboxes,
                                 widget_index_list_t *frameboxes,
                                 widget_index_list_t *bitmaps,
                                 widget_index_list_t *waveform_views,
                                 widget_index_list_t *tf_buttons,
                                 widget_index_list_t *tf_toggles,
                                 widget_index_list_t *tf_labels,
                                 widget_index_list_t *tf_rotaries,
                                 widget_index_list_t *tf_linears,
                                 widget_index_list_t *tf_combos,
                                 widget_index_list_t *tf_meters,
                                 widget_index_list_t *tf_params,
                                 widget_index_list_t *tf_envs,
                                 widget_index_list_t *tf_groups,
                                 widget_index_list_t *mixer_strips,
                                 widget_index_list_t *mixer_gains,
                                 widget_index_list_t *mixer_pans,
                                 widget_index_list_t *mixer_mutes,
                                 widget_index_list_t *mixer_scopes,
                                 widget_index_list_t *mixer_masters,
                                 widget_index_list_t *dsp_windows,
                                 widget_index_list_t *dsp_slots,
                                 widget_index_list_t *dsp_menus,
                                 widget_index_list_t *dsp_params)
{
    memset(pushbuttons, 0, sizeof(*pushbuttons));
    memset(checkboxes, 0, sizeof(*checkboxes));
    memset(radiobuttons, 0, sizeof(*radiobuttons));
    memset(scrollbars, 0, sizeof(*scrollbars));
    memset(textboxes, 0, sizeof(*textboxes));
    memset(frameboxes, 0, sizeof(*frameboxes));
    memset(bitmaps, 0, sizeof(*bitmaps));
    memset(waveform_views, 0, sizeof(*waveform_views));
    memset(tf_buttons, 0, sizeof(*tf_buttons));
    memset(tf_toggles, 0, sizeof(*tf_toggles));
    memset(tf_labels, 0, sizeof(*tf_labels));
    memset(tf_rotaries, 0, sizeof(*tf_rotaries));
    memset(tf_linears, 0, sizeof(*tf_linears));
    memset(tf_combos, 0, sizeof(*tf_combos));
    memset(tf_meters, 0, sizeof(*tf_meters));
    memset(tf_params, 0, sizeof(*tf_params));
    memset(tf_envs, 0, sizeof(*tf_envs));
    memset(tf_groups, 0, sizeof(*tf_groups));
    memset(mixer_strips, 0, sizeof(*mixer_strips));
    memset(mixer_gains, 0, sizeof(*mixer_gains));
    memset(mixer_pans, 0, sizeof(*mixer_pans));
    memset(mixer_mutes, 0, sizeof(*mixer_mutes));
    memset(mixer_scopes, 0, sizeof(*mixer_scopes));
    memset(mixer_masters, 0, sizeof(*mixer_masters));
    memset(dsp_windows, 0, sizeof(*dsp_windows));
    memset(dsp_slots, 0, sizeof(*dsp_slots));
    memset(dsp_menus, 0, sizeof(*dsp_menus));
    memset(dsp_params, 0, sizeof(*dsp_params));

    for (int i = 0; i < manager->widget_count; i++) {
        widget_t *w = &manager->widgets[i];
        switch (w->type) {
            case WIDGET_PUSHBUTTON:
            case WIDGET_CUSTOM_BUTTON:
                pushbuttons->indices[pushbuttons->count++] = i;
                break;
            case WIDGET_CHECKBOX:
                checkboxes->indices[checkboxes->count++] = i;
                break;
            case WIDGET_RADIOBUTTON:
                radiobuttons->indices[radiobuttons->count++] = i;
                break;
            case WIDGET_SCROLLBAR:
                scrollbars->indices[scrollbars->count++] = i;
                break;
            case WIDGET_TEXTBOX:
                textboxes->indices[textboxes->count++] = i;
                break;
            case WIDGET_FRAMEBOX:
                frameboxes->indices[frameboxes->count++] = i;
                break;
            case WIDGET_LOGO:
                bitmaps->indices[bitmaps->count++] = i;
                break;
            case WIDGET_WAVEFORM_VIEW:
                waveform_views->indices[waveform_views->count++] = i;
                break;
            case WIDGET_TF_BUTTON:
                tf_buttons->indices[tf_buttons->count++] = i;
                break;
            case WIDGET_TF_TOGGLE:
                tf_toggles->indices[tf_toggles->count++] = i;
                break;
            case WIDGET_TF_LABEL:
                tf_labels->indices[tf_labels->count++] = i;
                break;
            case WIDGET_TF_ROTARY:
                tf_rotaries->indices[tf_rotaries->count++] = i;
                break;
            case WIDGET_TF_LINEAR:
            case WIDGET_TF_ARP_STEP:
                tf_linears->indices[tf_linears->count++] = i;
                break;
            case WIDGET_TF_COMBO:
                tf_combos->indices[tf_combos->count++] = i;
                break;
            case WIDGET_TF_METER:
                tf_meters->indices[tf_meters->count++] = i;
                break;
            case WIDGET_TF_PARAMETER:
                tf_params->indices[tf_params->count++] = i;
                break;
            case WIDGET_TF_ENVELOPE:
                tf_envs->indices[tf_envs->count++] = i;
                break;
            case WIDGET_TF_GROUP:
                tf_groups->indices[tf_groups->count++] = i;
                break;
            case WIDGET_MIXER_STRIP:
                mixer_strips->indices[mixer_strips->count++] = i;
                break;
            case WIDGET_MIXER_GAIN:
                mixer_gains->indices[mixer_gains->count++] = i;
                break;
            case WIDGET_MIXER_PAN:
                mixer_pans->indices[mixer_pans->count++] = i;
                break;
            case WIDGET_MIXER_MUTE:
                mixer_mutes->indices[mixer_mutes->count++] = i;
                break;
            case WIDGET_MIXER_SCOPE:
                mixer_scopes->indices[mixer_scopes->count++] = i;
                break;
            case WIDGET_MIXER_MASTER:
                mixer_masters->indices[mixer_masters->count++] = i;
                break;
            case WIDGET_DSP_WINDOW:
                dsp_windows->indices[dsp_windows->count++] = i;
                break;
            case WIDGET_DSP_SLOT:
                dsp_slots->indices[dsp_slots->count++] = i;
                break;
            case WIDGET_DSP_MENU:
                dsp_menus->indices[dsp_menus->count++] = i;
                break;
            case WIDGET_DSP_PARAM:
                dsp_params->indices[dsp_params->count++] = i;
                break;
            default:
                break;
        }
    }
}

static bool write_schema_header_file(widget_manager_t *manager,
                                     const char *filename,
                                     const char *base_name,
                                     const char *macro_prefix,
                                     const char *symbol_name,
                                     const widget_index_list_t *pushbuttons,
                                     const widget_index_list_t *checkboxes,
                                     const widget_index_list_t *radiobuttons,
                                     const widget_index_list_t *scrollbars,
                                     const widget_index_list_t *textboxes,
                                     const widget_index_list_t *frameboxes,
                                     const widget_index_list_t *bitmaps,
                                     const widget_index_list_t *waveform_views,
                                     const widget_index_list_t *tf_buttons,
                                     const widget_index_list_t *tf_toggles,
                                     const widget_index_list_t *tf_labels,
                                     const widget_index_list_t *tf_rotaries,
                                     const widget_index_list_t *tf_linears,
                                     const widget_index_list_t *tf_combos,
                                     const widget_index_list_t *tf_meters,
                                     const widget_index_list_t *tf_params,
                                     const widget_index_list_t *tf_envs,
                                     const widget_index_list_t *tf_groups,
                                     const widget_index_list_t *mixer_strips,
                                     const widget_index_list_t *mixer_gains,
                                     const widget_index_list_t *mixer_pans,
                                     const widget_index_list_t *mixer_mutes,
                                     const widget_index_list_t *mixer_scopes,
                                     const widget_index_list_t *mixer_masters,
                                     const widget_index_list_t *dsp_windows,
                                     const widget_index_list_t *dsp_slots,
                                     const widget_index_list_t *dsp_menus,
                                     const widget_index_list_t *dsp_params)
{
    FILE *file = fopen(filename, "w");
    if (!file) return false;

    fprintf(file, "// Auto-generated FT2 GUI layout (schema-based)\n");
    fprintf(file, "#pragma once\n\n");
    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include \"shared/ft2_ui_schema.h\"\n\n");

    fprintf(file, "// Layout name: %s\n\n", base_name);

    fprintf(file, "#ifndef %s_PB_BASE\n#define %s_PB_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_PB_COUNT %d\n\n", macro_prefix, pushbuttons->count);

    fprintf(file, "#ifndef %s_CB_BASE\n#define %s_CB_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_CB_COUNT %d\n\n", macro_prefix, checkboxes->count);

    fprintf(file, "#ifndef %s_RB_BASE\n#define %s_RB_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_RB_COUNT %d\n\n", macro_prefix, radiobuttons->count);

    fprintf(file, "#ifndef %s_SB_BASE\n#define %s_SB_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_SB_COUNT %d\n\n", macro_prefix, scrollbars->count);

    fprintf(file, "#ifndef %s_TB_BASE\n#define %s_TB_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TB_COUNT %d\n\n", macro_prefix, textboxes->count);

    fprintf(file, "#ifndef %s_FB_BASE\n#define %s_FB_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_FB_COUNT %d\n\n", macro_prefix, frameboxes->count);

    fprintf(file, "#ifndef %s_BMP_BASE\n#define %s_BMP_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_BMP_COUNT %d\n\n", macro_prefix, bitmaps->count);

    fprintf(file, "#ifndef %s_WAVE_BASE\n#define %s_WAVE_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_WAVE_COUNT %d\n\n", macro_prefix, waveform_views->count);

    fprintf(file, "#ifndef %s_TF_BUTTON_BASE\n#define %s_TF_BUTTON_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_BUTTON_COUNT %d\n\n", macro_prefix, tf_buttons->count);

    fprintf(file, "#ifndef %s_TF_TOGGLE_BASE\n#define %s_TF_TOGGLE_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_TOGGLE_COUNT %d\n\n", macro_prefix, tf_toggles->count);

    fprintf(file, "#ifndef %s_TF_LABEL_BASE\n#define %s_TF_LABEL_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_LABEL_COUNT %d\n\n", macro_prefix, tf_labels->count);

    fprintf(file, "#ifndef %s_TF_ROTARY_BASE\n#define %s_TF_ROTARY_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_ROTARY_COUNT %d\n\n", macro_prefix, tf_rotaries->count);

    fprintf(file, "#ifndef %s_TF_LINEAR_BASE\n#define %s_TF_LINEAR_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_LINEAR_COUNT %d\n\n", macro_prefix, tf_linears->count);

    fprintf(file, "#ifndef %s_TF_COMBO_BASE\n#define %s_TF_COMBO_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_COMBO_COUNT %d\n\n", macro_prefix, tf_combos->count);

    fprintf(file, "#ifndef %s_TF_METER_BASE\n#define %s_TF_METER_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_METER_COUNT %d\n\n", macro_prefix, tf_meters->count);

    fprintf(file, "#ifndef %s_TF_PARAM_BASE\n#define %s_TF_PARAM_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_PARAM_COUNT %d\n\n", macro_prefix, tf_params->count);

    fprintf(file, "#ifndef %s_TF_ENV_BASE\n#define %s_TF_ENV_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_ENV_COUNT %d\n\n", macro_prefix, tf_envs->count);

    fprintf(file, "#ifndef %s_TF_GROUP_BASE\n#define %s_TF_GROUP_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_TF_GROUP_COUNT %d\n\n", macro_prefix, tf_groups->count);

    fprintf(file, "#ifndef %s_MIXER_STRIP_BASE\n#define %s_MIXER_STRIP_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_MIXER_STRIP_COUNT %d\n\n", macro_prefix, mixer_strips->count);

    fprintf(file, "#ifndef %s_MIXER_GAIN_BASE\n#define %s_MIXER_GAIN_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_MIXER_GAIN_COUNT %d\n\n", macro_prefix, mixer_gains->count);

    fprintf(file, "#ifndef %s_MIXER_PAN_BASE\n#define %s_MIXER_PAN_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_MIXER_PAN_COUNT %d\n\n", macro_prefix, mixer_pans->count);

    fprintf(file, "#ifndef %s_MIXER_MUTE_BASE\n#define %s_MIXER_MUTE_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_MIXER_MUTE_COUNT %d\n\n", macro_prefix, mixer_mutes->count);

    fprintf(file, "#ifndef %s_MIXER_SCOPE_BASE\n#define %s_MIXER_SCOPE_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_MIXER_SCOPE_COUNT %d\n\n", macro_prefix, mixer_scopes->count);

    fprintf(file, "#ifndef %s_MIXER_MASTER_BASE\n#define %s_MIXER_MASTER_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_MIXER_MASTER_COUNT %d\n\n", macro_prefix, mixer_masters->count);

    fprintf(file, "#ifndef %s_DSP_WINDOW_BASE\n#define %s_DSP_WINDOW_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_DSP_WINDOW_COUNT %d\n\n", macro_prefix, dsp_windows->count);

    fprintf(file, "#ifndef %s_DSP_SLOT_BASE\n#define %s_DSP_SLOT_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_DSP_SLOT_COUNT %d\n\n", macro_prefix, dsp_slots->count);

    fprintf(file, "#ifndef %s_DSP_MENU_BASE\n#define %s_DSP_MENU_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_DSP_MENU_COUNT %d\n\n", macro_prefix, dsp_menus->count);

    fprintf(file, "#ifndef %s_DSP_PARAM_BASE\n#define %s_DSP_PARAM_BASE 0\n#endif\n", macro_prefix, macro_prefix);
    fprintf(file, "#define %s_DSP_PARAM_COUNT %d\n\n", macro_prefix, dsp_params->count);

    fprintf(file, "typedef enum {\n");
    fprintf(file, "    %s_ACTION_NONE = 0,\n", macro_prefix);

    int action_id = 1;
    char action_base[96];
    for (int i = 0; i < pushbuttons->count; i++) {
        widget_t *w = &manager->widgets[pushbuttons->indices[i]];
        make_action_base(w, action_base, sizeof(action_base));
        fprintf(file, "    %s_ACTION_%s_%d_DOWN = %d,\n", macro_prefix, action_base, w->id, action_id++);
        fprintf(file, "    %s_ACTION_%s_%d_UP = %d,\n", macro_prefix, action_base, w->id, action_id++);
    }
    for (int i = 0; i < checkboxes->count; i++) {
        widget_t *w = &manager->widgets[checkboxes->indices[i]];
        make_action_base(w, action_base, sizeof(action_base));
        fprintf(file, "    %s_ACTION_%s_%d = %d,\n", macro_prefix, action_base, w->id, action_id++);
    }
    for (int i = 0; i < radiobuttons->count; i++) {
        widget_t *w = &manager->widgets[radiobuttons->indices[i]];
        make_action_base(w, action_base, sizeof(action_base));
        fprintf(file, "    %s_ACTION_%s_%d = %d,\n", macro_prefix, action_base, w->id, action_id++);
    }
    for (int i = 0; i < scrollbars->count; i++) {
        widget_t *w = &manager->widgets[scrollbars->indices[i]];
        make_action_base(w, action_base, sizeof(action_base));
        fprintf(file, "    %s_ACTION_%s_%d = %d,\n", macro_prefix, action_base, w->id, action_id++);
    }

    fprintf(file, "} %s_action_id_t;\n\n", symbol_name);
    fprintf(file, "extern const ft2_ui_layout_desc_t %s_layout;\n", symbol_name);
    fclose(file);
    return true;
}

bool export_gui_schema_code(widget_manager_t *manager, const char *base_filename)
{
    if (!manager || !base_filename) return false;

    char header_filename[256];
    char source_filename[256];
    char header_name[64];
    char base_name[128];
    char macro_prefix[128];
    char symbol_name[128];

    snprintf(header_filename, sizeof(header_filename), "%s.h", base_filename);
    snprintf(source_filename, sizeof(source_filename), "%s.c", base_filename);

    const char *last_slash = strrchr(base_filename, '/');
    if (last_slash) {
        snprintf(header_name, sizeof(header_name), "%s.h", last_slash + 1);
    } else {
        snprintf(header_name, sizeof(header_name), "%s.h", base_filename);
    }

    get_base_name(base_filename, base_name, sizeof(base_name));
    strip_schema_suffix(base_name);
    make_macro_prefix(base_name, macro_prefix, sizeof(macro_prefix));
    make_symbol_name(base_name, symbol_name, sizeof(symbol_name));

    for (int i = 0; i < manager->widget_count; i++) {
        widget_t *w = &manager->widgets[i];
        if (strlen(w->name) == 0) {
            generate_unique_widget_name(w);
        }
        sanitize_widget_name(w->name);
    }

    widget_index_list_t pushbuttons, checkboxes, radiobuttons, scrollbars, textboxes, frameboxes, bitmaps;
    widget_index_list_t waveform_views, tf_buttons, tf_toggles, tf_labels, tf_rotaries;
    widget_index_list_t tf_linears, tf_combos, tf_meters, tf_params, tf_envs, tf_groups;
    widget_index_list_t mixer_strips, mixer_gains, mixer_pans, mixer_mutes, mixer_scopes, mixer_masters;
    widget_index_list_t dsp_windows, dsp_slots, dsp_menus, dsp_params;

    collect_widget_lists(manager, &pushbuttons, &checkboxes, &radiobuttons, &scrollbars, &textboxes, &frameboxes,
                         &bitmaps, &waveform_views, &tf_buttons, &tf_toggles, &tf_labels, &tf_rotaries,
                         &tf_linears, &tf_combos, &tf_meters, &tf_params, &tf_envs, &tf_groups,
                         &mixer_strips, &mixer_gains, &mixer_pans, &mixer_mutes, &mixer_scopes, &mixer_masters,
                         &dsp_windows, &dsp_slots, &dsp_menus, &dsp_params);

    if (!write_schema_header_file(manager, header_filename, base_name, macro_prefix, symbol_name,
                                  &pushbuttons, &checkboxes, &radiobuttons, &scrollbars, &textboxes, &frameboxes,
                                  &bitmaps, &waveform_views, &tf_buttons, &tf_toggles, &tf_labels, &tf_rotaries,
                                  &tf_linears, &tf_combos, &tf_meters, &tf_params, &tf_envs, &tf_groups,
                                  &mixer_strips, &mixer_gains, &mixer_pans, &mixer_mutes, &mixer_scopes, &mixer_masters,
                                  &dsp_windows, &dsp_slots, &dsp_menus, &dsp_params)) {
        return false;
    }

    FILE *file = fopen(source_filename, "w");
    if (!file) return false;

    fprintf(file, "// Auto-generated FT2 GUI layout (schema-based)\n");
    fprintf(file, "#include <stdio.h>\n");
    fprintf(file, "#include \"%s\"\n", header_name);
    fprintf(file, "#include \"shared/ft2_ui_schema.h\"\n\n");

    if (pushbuttons.count > 0) {
        fprintf(file, "static const ft2_ui_pushbutton_desc_t %s_pushbuttons[%s_PB_COUNT] = {\n", symbol_name, macro_prefix);
        for (int i = 0; i < pushbuttons.count; i++) {
            widget_t *w = &manager->widgets[pushbuttons.indices[i]];
            fprintf(file, "    { %s_PB_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, 0, 0, ",
                    w->x, w->y, w->w, w->h);
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, ", ");
            write_optional_c_string(file, schema_text_or_name(w, true));

            char action_base[96];
            make_action_base(w, action_base, sizeof(action_base));
            fprintf(file, ", %s_ACTION_%s_%d_DOWN, %s_ACTION_%s_%d_UP },\n",
                    macro_prefix, action_base, w->id, macro_prefix, action_base, w->id);
        }
        fprintf(file, "};\n\n");
    }

    if (checkboxes.count > 0) {
        fprintf(file, "static const ft2_ui_checkbox_desc_t %s_checkboxes[%s_CB_COUNT] = {\n", symbol_name, macro_prefix);
        for (int i = 0; i < checkboxes.count; i++) {
            widget_t *w = &manager->widgets[checkboxes.indices[i]];
            char action_base[96];
            make_action_base(w, action_base, sizeof(action_base));
            fprintf(file, "    { %s_CB_BASE + %d, %d, %d, %d, %d, %s_ACTION_%s_%d, %s },\n",
                    macro_prefix, i, w->x, w->y, w->w, w->h,
                    macro_prefix, action_base, w->id,
                    w->data.checkbox.checked ? "true" : "false");
        }
        fprintf(file, "};\n\n");
    }

    if (radiobuttons.count > 0) {
        fprintf(file, "static const ft2_ui_radiobutton_desc_t %s_radiobuttons[%s_RB_COUNT] = {\n", symbol_name, macro_prefix);
        for (int i = 0; i < radiobuttons.count; i++) {
            widget_t *w = &manager->widgets[radiobuttons.indices[i]];
            char action_base[96];
            make_action_base(w, action_base, sizeof(action_base));
            fprintf(file, "    { %s_RB_BASE + %d, %d, %d, %d, %s_ACTION_%s_%d, %s },\n",
                    macro_prefix, i, w->x, w->y, w->data.radiobutton.group_id,
                    macro_prefix, action_base, w->id,
                    w->data.radiobutton.selected ? "true" : "false");
        }
        fprintf(file, "};\n\n");
    }

    if (scrollbars.count > 0) {
        fprintf(file, "static const ft2_ui_scrollbar_desc_t %s_scrollbars[%s_SB_COUNT] = {\n", symbol_name, macro_prefix);
        for (int i = 0; i < scrollbars.count; i++) {
            widget_t *w = &manager->widgets[scrollbars.indices[i]];
            char action_base[96];
            make_action_base(w, action_base, sizeof(action_base));

            const char *type = (w->data.scrollbar.orientation == SCROLLBAR_VERTICAL)
                ? "FT2_UI_SCROLLBAR_VERTICAL"
                : "FT2_UI_SCROLLBAR_HORIZONTAL";

            fprintf(file, "    { %s_SB_BASE + %d, %d, %d, %d, %d, %s, FT2_UI_SCROLLBAR_DYNAMIC_THUMB_SIZE, %d, %d, %d, %s_ACTION_%s_%d, %s },\n",
                    macro_prefix, i, w->x, w->y, w->w, w->h,
                    type, w->data.scrollbar.current_value, w->data.scrollbar.thumb_size,
                    w->data.scrollbar.max_value, macro_prefix, action_base, w->id,
                    w->data.scrollbar.has_nudge_buttons ? "true" : "false");
        }
        fprintf(file, "};\n\n");
    }

    if (textboxes.count > 0) {
        fprintf(file, "static const ft2_ui_textbox_desc_t %s_textboxes[%s_TB_COUNT] = {\n", symbol_name, macro_prefix);
        for (int i = 0; i < textboxes.count; i++) {
            widget_t *w = &manager->widgets[textboxes.indices[i]];
            fprintf(file, "    { %s_TB_BASE + %d, %d, %d, %d, %d, 1, 0, 22, false, true, 0 },\n",
                    macro_prefix, i, w->x, w->y, w->w, w->h);
        }
        fprintf(file, "};\n\n");
    }

    if (frameboxes.count > 0) {
        fprintf(file, "static const ft2_ui_framebox_desc_t %s_frameboxes[%s_FB_COUNT] = {\n", symbol_name, macro_prefix);
        for (int i = 0; i < frameboxes.count; i++) {
            widget_t *w = &manager->widgets[frameboxes.indices[i]];
            fprintf(file, "    { %s_FB_BASE + %d, %d, %d, %d, %d, %d, %s, ",
                    macro_prefix, i, w->x, w->y, w->w, w->h,
                    w->data.framebox.border_type,
                    w->data.framebox.filled ? "true" : "false");
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, " },\n");
        }
        fprintf(file, "};\n\n");
    }

    if (bitmaps.count > 0) {
        fprintf(file, "static const ft2_ui_bitmap_desc_t %s_bitmaps[%s_BMP_COUNT] = {\n", symbol_name, macro_prefix);
        for (int i = 0; i < bitmaps.count; i++) {
            widget_t *w = &manager->widgets[bitmaps.indices[i]];
            uint8_t flags = w->data.logo.flags;
            if (designer_bitmap_has_truecolor(w->data.logo.bitmap_id))
                flags |= FT2_UI_BITMAP_FLAG_TRUECOLOR;
            if (w->data.logo.layer != FT2_UI_BITMAP_LAYER_WIDGET)
                flags |= FT2_UI_BITMAP_FLAG_CLICK_THROUGH;
            fprintf(file, "    { %s_BMP_BASE + %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, 255 },\n",
                    macro_prefix, i, w->x, w->y, w->w, w->h, (int)w->page, w->data.logo.bitmap_id,
                    (int)w->data.logo.layer, (int)flags, (int)w->data.logo.skin_part);
        }
        fprintf(file, "};\n\n");
    }

    if (waveform_views.count > 0) {
        fprintf(file, "static const ft2_ui_waveform_view_desc_t %s_waveform_views[%s_WAVE_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < waveform_views.count; i++) {
            widget_t *w = &manager->widgets[waveform_views.indices[i]];
            fprintf(file, "    { %s_WAVE_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, (int)w->page);
        }
        fprintf(file, "};\n\n");
    }

    if (tf_buttons.count > 0) {
        fprintf(file, "static const ft2_ui_tf_button_desc_t %s_tf_buttons[%s_TF_BUTTON_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_buttons.count; i++) {
            widget_t *w = &manager->widgets[tf_buttons.indices[i]];
            fprintf(file, "    { %s_TF_BUTTON_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d, ",
                    w->x, w->y, w->w, w->h, (int)w->page);
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, " },\n");
        }
        fprintf(file, "};\n\n");
    }

    if (tf_toggles.count > 0) {
        fprintf(file, "static const ft2_ui_tf_toggle_desc_t %s_tf_toggles[%s_TF_TOGGLE_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_toggles.count; i++) {
            widget_t *w = &manager->widgets[tf_toggles.indices[i]];
            const char *pressed = (w->state == WIDGET_PRESSED) ? "true" : "false";
            fprintf(file, "    { %s_TF_TOGGLE_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d, ",
                    w->x, w->y, w->w, w->h, (int)w->page);
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, ", %s },\n", pressed);
        }
        fprintf(file, "};\n\n");
    }

    if (tf_labels.count > 0) {
        fprintf(file, "static const ft2_ui_tf_label_desc_t %s_tf_labels[%s_TF_LABEL_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_labels.count; i++) {
            widget_t *w = &manager->widgets[tf_labels.indices[i]];
            fprintf(file, "    { %s_TF_LABEL_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d, ",
                    w->x, w->y, w->w, w->h, (int)w->page);
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, " },\n");
        }
        fprintf(file, "};\n\n");
    }

    if (tf_rotaries.count > 0) {
        fprintf(file, "static const ft2_ui_tf_rotary_slider_desc_t %s_tf_rotaries[%s_TF_ROTARY_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_rotaries.count; i++) {
            widget_t *w = &manager->widgets[tf_rotaries.indices[i]];
            int radius = (w->w < w->h ? w->w : w->h) / 2;
            float start_angle = w->data.tf_rotary.start_angle;
            float end_angle = w->data.tf_rotary.end_angle;
            if (start_angle == 0.0f && end_angle == 0.0f) {
                start_angle = -2.35f;
                end_angle = 2.35f;
            }
            fprintf(file, "    { %s_TF_ROTARY_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %.3ff, %.3ff, ",
                    w->x, w->y, radius, (int)w->page, start_angle, end_angle);
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, " },\n");
        }
        fprintf(file, "};\n\n");
    }

    if (tf_linears.count > 0) {
        fprintf(file, "static const ft2_ui_tf_linear_slider_desc_t %s_tf_linears[%s_TF_LINEAR_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_linears.count; i++) {
            widget_t *w = &manager->widgets[tf_linears.indices[i]];
            bool vertical = w->data.tf_linear.vertical;
            if (w->w > 0 && w->h > 0)
                vertical = w->h >= w->w;
            fprintf(file, "    { %s_TF_LINEAR_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d, %s },\n",
                    w->x, w->y, w->w, w->h, (int)w->page,
                    vertical ? "true" : "false");
        }
        fprintf(file, "};\n\n");
    }

    if (tf_combos.count > 0) {
        for (int i = 0; i < tf_combos.count; i++) {
            widget_t *w = &manager->widgets[tf_combos.indices[i]];
            int item_count = w->data.combobox.item_count;
            if (item_count <= 0) {
                continue;
            }
            fprintf(file, "static const char *const %s_tf_combo_items_%d[] = {\n",
                    symbol_name, i);
            for (int j = 0; j < item_count; j++) {
                fprintf(file, "    ");
                write_c_string(file, w->data.combobox.items[j]);
                fprintf(file, ",\n");
            }
            fprintf(file, "};\n\n");
        }

        fprintf(file, "static const ft2_ui_tf_combo_box_desc_t %s_tf_combos[%s_TF_COMBO_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_combos.count; i++) {
            widget_t *w = &manager->widgets[tf_combos.indices[i]];
            int item_count = w->data.combobox.item_count;
            int selected_item = w->data.combobox.selected_item;
            if (item_count <= 0) {
                item_count = 0;
                selected_item = 0;
            } else if (selected_item < 0 || selected_item >= item_count) {
                selected_item = 0;
            }
            fprintf(file, "    { %s_TF_COMBO_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            if (item_count > 0) {
                fprintf(file, ", %d, %d, %d, %d, %d, %s_tf_combo_items_%d, %d, %d },\n",
                        w->x, w->y, w->w, w->h, (int)w->page,
                        symbol_name, i, item_count, selected_item);
            } else {
                fprintf(file, ", %d, %d, %d, %d, %d, NULL, 0, 0 },\n",
                        w->x, w->y, w->w, w->h, (int)w->page);
            }
        }
        fprintf(file, "};\n\n");
    }

    if (tf_meters.count > 0) {
        fprintf(file, "static const ft2_ui_tf_level_meter_desc_t %s_tf_meters[%s_TF_METER_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_meters.count; i++) {
            widget_t *w = &manager->widgets[tf_meters.indices[i]];
            int num_leds = w->data.tf_meter.num_leds > 0 ? w->data.tf_meter.num_leds : 12;
            const char *show_peak = w->data.tf_meter.show_peak ? "true" : "false";
            fprintf(file, "    { %s_TF_METER_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d, %d, %s },\n",
                    w->x, w->y, w->w, w->h, (int)w->page, num_leds, show_peak);
        }
        fprintf(file, "};\n\n");
    }

    if (tf_params.count > 0) {
        fprintf(file, "static const ft2_ui_tf_parameter_control_desc_t %s_tf_params[%s_TF_PARAM_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_params.count; i++) {
            widget_t *w = &manager->widgets[tf_params.indices[i]];
            fprintf(file, "    { %s_TF_PARAM_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d, ",
                    w->x, w->y, w->w, w->h, (int)w->page);
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, " },\n");
        }
        fprintf(file, "};\n\n");
    }

    if (tf_envs.count > 0) {
        fprintf(file, "static const ft2_ui_tf_envelope_display_desc_t %s_tf_envs[%s_TF_ENV_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_envs.count; i++) {
            widget_t *w = &manager->widgets[tf_envs.indices[i]];
            fprintf(file, "    { %s_TF_ENV_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, (int)w->page);
        }
        fprintf(file, "};\n\n");
    }

    if (tf_groups.count > 0) {
        fprintf(file, "static const ft2_ui_tf_group_box_desc_t %s_tf_groups[%s_TF_GROUP_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < tf_groups.count; i++) {
            widget_t *w = &manager->widgets[tf_groups.indices[i]];
            fprintf(file, "    { %s_TF_GROUP_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d, ",
                    w->x, w->y, w->w, w->h, (int)w->page);
            write_optional_c_string(file, schema_text_or_name(w, false));
            fprintf(file, " },\n");
        }
        fprintf(file, "};\n\n");
    }

    if (mixer_strips.count > 0) {
        fprintf(file, "static const ft2_ui_mixer_strip_desc_t %s_mixer_strips[%s_MIXER_STRIP_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < mixer_strips.count; i++) {
            widget_t *w = &manager->widgets[mixer_strips.indices[i]];
            int channel_index = parse_trailing_index(w->name);
            if (channel_index < 0) channel_index = 0;
            fprintf(file, "    { %s_MIXER_STRIP_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, channel_index);
        }
        fprintf(file, "};\n\n");
    }

    if (mixer_gains.count > 0) {
        fprintf(file, "static const ft2_ui_mixer_gain_desc_t %s_mixer_gains[%s_MIXER_GAIN_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < mixer_gains.count; i++) {
            widget_t *w = &manager->widgets[mixer_gains.indices[i]];
            int channel_index = parse_trailing_index(w->name);
            if (channel_index < 0) channel_index = 0;
            fprintf(file, "    { %s_MIXER_GAIN_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, channel_index);
        }
        fprintf(file, "};\n\n");
    }

    if (mixer_pans.count > 0) {
        fprintf(file, "static const ft2_ui_mixer_pan_desc_t %s_mixer_pans[%s_MIXER_PAN_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < mixer_pans.count; i++) {
            widget_t *w = &manager->widgets[mixer_pans.indices[i]];
            int channel_index = parse_trailing_index(w->name);
            if (channel_index < 0) channel_index = 0;
            fprintf(file, "    { %s_MIXER_PAN_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, channel_index);
        }
        fprintf(file, "};\n\n");
    }

    if (mixer_mutes.count > 0) {
        fprintf(file, "static const ft2_ui_mixer_mute_desc_t %s_mixer_mutes[%s_MIXER_MUTE_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < mixer_mutes.count; i++) {
            widget_t *w = &manager->widgets[mixer_mutes.indices[i]];
            int channel_index = parse_trailing_index(w->name);
            if (channel_index < 0) channel_index = 0;
            fprintf(file, "    { %s_MIXER_MUTE_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, channel_index);
        }
        fprintf(file, "};\n\n");
    }

    if (mixer_scopes.count > 0) {
        fprintf(file, "static const ft2_ui_mixer_scope_desc_t %s_mixer_scopes[%s_MIXER_SCOPE_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < mixer_scopes.count; i++) {
            widget_t *w = &manager->widgets[mixer_scopes.indices[i]];
            int channel_index = parse_trailing_index(w->name);
            if (channel_index < 0) channel_index = 0;
            fprintf(file, "    { %s_MIXER_SCOPE_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, channel_index);
        }
        fprintf(file, "};\n\n");
    }

    if (mixer_masters.count > 0) {
        fprintf(file, "static const ft2_ui_mixer_master_desc_t %s_mixer_masters[%s_MIXER_MASTER_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < mixer_masters.count; i++) {
            widget_t *w = &manager->widgets[mixer_masters.indices[i]];
            fprintf(file, "    { %s_MIXER_MASTER_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h);
        }
        fprintf(file, "};\n\n");
    }

    if (dsp_windows.count > 0) {
        fprintf(file, "static const ft2_ui_dsp_window_desc_t %s_dsp_windows[%s_DSP_WINDOW_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < dsp_windows.count; i++) {
            widget_t *w = &manager->widgets[dsp_windows.indices[i]];
            fprintf(file, "    { %s_DSP_WINDOW_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h);
        }
        fprintf(file, "};\n\n");
    }

    if (dsp_slots.count > 0) {
        fprintf(file, "static const ft2_ui_dsp_slot_desc_t %s_dsp_slots[%s_DSP_SLOT_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < dsp_slots.count; i++) {
            widget_t *w = &manager->widgets[dsp_slots.indices[i]];
            int slot_index = parse_trailing_index(w->name);
            if (slot_index < 0) slot_index = 0;
            fprintf(file, "    { %s_DSP_SLOT_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, slot_index);
        }
        fprintf(file, "};\n\n");
    }

    if (dsp_menus.count > 0) {
        fprintf(file, "static const ft2_ui_dsp_menu_desc_t %s_dsp_menus[%s_DSP_MENU_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < dsp_menus.count; i++) {
            widget_t *w = &manager->widgets[dsp_menus.indices[i]];
            fprintf(file, "    { %s_DSP_MENU_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h);
        }
        fprintf(file, "};\n\n");
    }

    if (dsp_params.count > 0) {
        fprintf(file, "static const ft2_ui_dsp_param_desc_t %s_dsp_params[%s_DSP_PARAM_COUNT] = {\n",
                symbol_name, macro_prefix);
        for (int i = 0; i < dsp_params.count; i++) {
            widget_t *w = &manager->widgets[dsp_params.indices[i]];
            int param_index = parse_trailing_index(w->name);
            if (param_index < 0) param_index = 0;
            fprintf(file, "    { %s_DSP_PARAM_BASE + %d, ", macro_prefix, i);
            write_optional_c_string(file, w->name);
            fprintf(file, ", %d, %d, %d, %d, %d },\n",
                    w->x, w->y, w->w, w->h, param_index);
        }
        fprintf(file, "};\n\n");
    }

    fprintf(file, "const ft2_ui_layout_desc_t %s_layout = {\n", symbol_name);
    fprintf(file, "    \"%s\",\n", base_name);
    fprintf(file, "    FT2_UI_SCHEMA_VERSION,\n");

    fprintf(file, "    { %s_PB_BASE, %s_PB_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_PB_COUNT > 0\n    %s_pushbuttons,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_CB_BASE, %s_CB_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_CB_COUNT > 0\n    %s_checkboxes,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_RB_BASE, %s_RB_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_RB_COUNT > 0\n    %s_radiobuttons,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_SB_BASE, %s_SB_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_SB_COUNT > 0\n    %s_scrollbars,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TB_BASE, %s_TB_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TB_COUNT > 0\n    %s_textboxes,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_FB_BASE, %s_FB_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_FB_COUNT > 0\n    %s_frameboxes,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_BMP_BASE, %s_BMP_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_BMP_COUNT > 0\n    %s_bitmaps,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_WAVE_BASE, %s_WAVE_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_WAVE_COUNT > 0\n    %s_waveform_views,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_BUTTON_BASE, %s_TF_BUTTON_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_BUTTON_COUNT > 0\n    %s_tf_buttons,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_TOGGLE_BASE, %s_TF_TOGGLE_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_TOGGLE_COUNT > 0\n    %s_tf_toggles,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_LABEL_BASE, %s_TF_LABEL_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_LABEL_COUNT > 0\n    %s_tf_labels,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_ROTARY_BASE, %s_TF_ROTARY_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_ROTARY_COUNT > 0\n    %s_tf_rotaries,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_LINEAR_BASE, %s_TF_LINEAR_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_LINEAR_COUNT > 0\n    %s_tf_linears,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_COMBO_BASE, %s_TF_COMBO_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_COMBO_COUNT > 0\n    %s_tf_combos,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_METER_BASE, %s_TF_METER_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_METER_COUNT > 0\n    %s_tf_meters,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_PARAM_BASE, %s_TF_PARAM_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_PARAM_COUNT > 0\n    %s_tf_params,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_ENV_BASE, %s_TF_ENV_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_ENV_COUNT > 0\n    %s_tf_envs,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_TF_GROUP_BASE, %s_TF_GROUP_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_TF_GROUP_COUNT > 0\n    %s_tf_groups,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_MIXER_STRIP_BASE, %s_MIXER_STRIP_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_MIXER_STRIP_COUNT > 0\n    %s_mixer_strips,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_MIXER_GAIN_BASE, %s_MIXER_GAIN_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_MIXER_GAIN_COUNT > 0\n    %s_mixer_gains,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_MIXER_PAN_BASE, %s_MIXER_PAN_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_MIXER_PAN_COUNT > 0\n    %s_mixer_pans,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_MIXER_MUTE_BASE, %s_MIXER_MUTE_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_MIXER_MUTE_COUNT > 0\n    %s_mixer_mutes,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_MIXER_SCOPE_BASE, %s_MIXER_SCOPE_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_MIXER_SCOPE_COUNT > 0\n    %s_mixer_scopes,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_MIXER_MASTER_BASE, %s_MIXER_MASTER_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_MIXER_MASTER_COUNT > 0\n    %s_mixer_masters,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_DSP_WINDOW_BASE, %s_DSP_WINDOW_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_DSP_WINDOW_COUNT > 0\n    %s_dsp_windows,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_DSP_SLOT_BASE, %s_DSP_SLOT_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_DSP_SLOT_COUNT > 0\n    %s_dsp_slots,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_DSP_MENU_BASE, %s_DSP_MENU_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_DSP_MENU_COUNT > 0\n    %s_dsp_menus,\n#else\n    NULL,\n#endif\n", macro_prefix, symbol_name);

    fprintf(file, "    { %s_DSP_PARAM_BASE, %s_DSP_PARAM_COUNT },\n", macro_prefix, macro_prefix);
    fprintf(file, "#if %s_DSP_PARAM_COUNT > 0\n    %s_dsp_params,\n#else\n    NULL\n#endif\n", macro_prefix, symbol_name);
    fprintf(file, "};\n");

    fclose(file);

    if (!export_custom_bitmaps(symbol_name, macro_prefix)) {
        printf("Failed to export custom bitmap gfxdata.\n");
        return false;
    }

    printf("Exported schema layout to %s and %s\n", header_filename, source_filename);
    return true;
}

bool export_gui_code(widget_manager_t *manager, const char *base_filename) {
    return export_gui_schema_code(manager, base_filename);
}
