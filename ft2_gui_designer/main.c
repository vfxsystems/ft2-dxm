// FT2 GUI Designer v0.3a
#include <SDL2/SDL.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "widgets.h"
#include "canvas.h"
#include "palette.h"
#include "font.h"
#include "assets.h"
#include "shared/ft2_ui_schema.h"

#define WINDOW_WIDTH 1104
#define WINDOW_HEIGHT 640
#define CUBE_SIZE 100  // Larger cube for easier debugging
#define TOOLBAR_WIDTH 220
#define PROPERTY_PANEL_WIDTH 200
#define CANVAS_WIDTH 632
#define CANVAS_HEIGHT 400
#define PAGE_SELECTOR_BUTTON_WIDTH 20
#define PAGE_SELECTOR_BUTTON_GAP 2
#define CUBE_VIEWPORT_SIZE 180
#define CUBE_VIEWPORT_RIGHT_MARGIN 4

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Property field types
typedef enum {
    PROP_X,
    PROP_Y,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_CAPTION,
    PROP_CAPTION2,
    PROP_NAME,
    PROP_FONT,
    PROP_BITMAP,
    PROP_BITMAP_TRANSP,
    PROP_BITMAP_LAYER,
    PROP_BITMAP_FLAGS,
    PROP_BITMAP_OPACITY,
    PROP_SKIN_PART,
    PROP_PAGE,
    PROP_SCROLLBAR_NUDGE
} PropField;

typedef enum {
    FILE_PROMPT_LOAD,
    FILE_PROMPT_SAVE,
    FILE_PROMPT_EXPORT
} FilePromptMode;

typedef enum {
    LAYOUT_UNKNOWN,
    LAYOUT_TUNEFISH,
    LAYOUT_DEXED,
    LAYOUT_MIXER,
    LAYOUT_V2,
    LAYOUT_OSTIRUS
} LayoutKind;

// Application state
typedef struct {
    SDL_Window* window;
    SDL_GLContext gl_context;
    uint32_t* framebuffer;
    bool running;
    int current_tool;
    bool show_grid;

    // Drag and drop state
    bool dragging;
    bool drag_from_toolbar;
    int drag_start_x, drag_start_y;
    int drag_preview_x, drag_preview_y;
    int drag_tool;

    // UI state
    bool show_properties;

    // Property editing state
    bool property_editing;
    PropField editing_field;
    char edit_buffer[256];
    int edit_cursor_pos;
    int edit_blink_timer;

    // Bitmap import prompt
    bool bitmap_prompt_active;
    char bitmap_prompt_buffer[512];
    int bitmap_prompt_cursor;
    bool bitmap_status_active;
    char bitmap_status_message[512];
    Uint32 bitmap_status_start;
    bool bitmap_status_success;

    // File prompt
    bool file_prompt_active;
    FilePromptMode file_prompt_mode;
    char file_prompt_label[64];
    char file_prompt_buffer[512];
    int file_prompt_cursor;
    bool file_status_active;
    char file_status_message[512];
    Uint32 file_status_start;
    bool file_status_success;
    char last_load_path[512];
    char last_save_path[512];
    char last_export_base[512];
    LayoutKind active_layout;
    ft2_ui_widget_page_t active_page;
    int page_view_number;
    bool theme_dropdown_open;

    // 3D cube animation
    float cube_rotation_x;
    float cube_rotation_y;
    Uint32 last_frame_time;
} AppState;

typedef struct {
    ft2_ui_widget_kind_t kind;
    int widget_type;
    const char *label;
    int default_w;
    int default_h;
    int scrollbar_orientation;
} tool_entry_t;

static tool_entry_t g_tool_entries[32];
static int g_tool_count = 0;

#define TOOL_SELECT_INDEX 0

static void draw_prompt_overlay(void);

// Designer state
typedef struct {
    canvas_t canvas;
    widget_manager_t widget_manager;
    font_system_t font_system;

    // Mouse state
    int mouse_x, mouse_y;
    bool mouse_down;
    int drag_offset_x, drag_offset_y;
} DesignerState;

static AppState app;
static DesignerState g_designer;

// OpenGL texture for the 2D framebuffer
GLuint gui_texture = 0;

static int widget_type_from_kind(ft2_ui_widget_kind_t kind)
{
    switch (kind) {
        case FT2_UI_WIDGET_PUSHBUTTON: return WIDGET_PUSHBUTTON;
        case FT2_UI_WIDGET_RADIOBUTTON: return WIDGET_RADIOBUTTON;
        case FT2_UI_WIDGET_CHECKBOX: return WIDGET_CHECKBOX;
        case FT2_UI_WIDGET_SCROLLBAR: return WIDGET_SCROLLBAR;
        case FT2_UI_WIDGET_TEXTBOX: return WIDGET_TEXTBOX;
        case FT2_UI_WIDGET_FRAMEBOX: return WIDGET_FRAMEBOX;
        case FT2_UI_WIDGET_BITMAP: return WIDGET_LOGO;
        case FT2_UI_WIDGET_WAVEFORM_VIEW: return WIDGET_WAVEFORM_VIEW;
        case FT2_UI_WIDGET_TF_BUTTON: return WIDGET_TF_BUTTON;
        case FT2_UI_WIDGET_TF_TOGGLE_BUTTON: return WIDGET_TF_TOGGLE;
        case FT2_UI_WIDGET_TF_LABEL: return WIDGET_TF_LABEL;
        case FT2_UI_WIDGET_TF_ROTARY_SLIDER: return WIDGET_TF_ROTARY;
        case FT2_UI_WIDGET_TF_LINEAR_SLIDER: return WIDGET_TF_LINEAR;
        case FT2_UI_WIDGET_TF_ARP_STEP: return WIDGET_TF_ARP_STEP;
        case FT2_UI_WIDGET_TF_COMBO_BOX: return WIDGET_TF_COMBO;
        case FT2_UI_WIDGET_TF_LEVEL_METER: return WIDGET_TF_METER;
        case FT2_UI_WIDGET_TF_PARAMETER_CONTROL: return WIDGET_TF_PARAMETER;
        case FT2_UI_WIDGET_TF_ENVELOPE_DISPLAY: return WIDGET_TF_ENVELOPE;
        case FT2_UI_WIDGET_TF_GROUP_BOX: return WIDGET_TF_GROUP;
        case FT2_UI_WIDGET_MIXER_STRIP: return WIDGET_MIXER_STRIP;
        case FT2_UI_WIDGET_MIXER_GAIN: return WIDGET_MIXER_GAIN;
        case FT2_UI_WIDGET_MIXER_PAN: return WIDGET_MIXER_PAN;
        case FT2_UI_WIDGET_MIXER_MUTE: return WIDGET_MIXER_MUTE;
        case FT2_UI_WIDGET_MIXER_SCOPE: return WIDGET_MIXER_SCOPE;
        case FT2_UI_WIDGET_MIXER_MASTER: return WIDGET_MIXER_MASTER;
        case FT2_UI_WIDGET_DSP_WINDOW: return WIDGET_DSP_WINDOW;
        case FT2_UI_WIDGET_DSP_SLOT: return WIDGET_DSP_SLOT;
        case FT2_UI_WIDGET_DSP_MENU: return WIDGET_DSP_MENU;
        case FT2_UI_WIDGET_DSP_PARAM: return WIDGET_DSP_PARAM;
        default: return WIDGET_NONE;
    }
}

static int clamp_page_view_number(int page)
{
    if (page < (int)FT2_UI_WIDGET_PAGE_1)
        return (int)FT2_UI_WIDGET_PAGE_1;
    if (page > (int)FT2_UI_WIDGET_PAGE_7)
        return (int)FT2_UI_WIDGET_PAGE_7;
    return page;
}

static bool is_view_all_mode(void)
{
    return app.active_page == FT2_UI_WIDGET_PAGE_BOTH;
}

static void set_page_view_number(int page)
{
    app.page_view_number = clamp_page_view_number(page);
    app.active_page = (ft2_ui_widget_page_t)app.page_view_number;
}

static void set_view_all_mode(bool enabled)
{
    if (enabled) {
        app.active_page = FT2_UI_WIDGET_PAGE_BOTH;
    } else {
        set_page_view_number(app.page_view_number);
    }
}

static int page_selector_value(void)
{
    return is_view_all_mode() ? 0 : clamp_page_view_number(app.page_view_number);
}

static void step_page_selector(int delta)
{
    int page = page_selector_value() + delta;
    if (page < (int)FT2_UI_WIDGET_PAGE_BOTH)
        page = (int)FT2_UI_WIDGET_PAGE_BOTH;
    if (page > (int)FT2_UI_WIDGET_PAGE_7)
        page = (int)FT2_UI_WIDGET_PAGE_7;

    if (page == (int)FT2_UI_WIDGET_PAGE_BOTH)
        set_view_all_mode(true);
    else
        set_page_view_number(page);
}

static bool widget_has_caption(const widget_t *widget)
{
    if (!widget)
        return false;

    switch (widget->type) {
        case WIDGET_PUSHBUTTON:
        case WIDGET_TEXTBOX:
        case WIDGET_FRAMEBOX:
        case WIDGET_LOGO:
        case WIDGET_CUSTOM_BUTTON:
        case WIDGET_DROPDOWN:
        case WIDGET_TF_BUTTON:
        case WIDGET_TF_TOGGLE:
        case WIDGET_TF_LABEL:
        case WIDGET_TF_ROTARY:
        case WIDGET_TF_COMBO:
        case WIDGET_TF_PARAMETER:
        case WIDGET_TF_GROUP:
        case WIDGET_TF_ENVELOPE:
        case WIDGET_WAVEFORM_VIEW:
        case WIDGET_MIXER_STRIP:
        case WIDGET_MIXER_GAIN:
        case WIDGET_MIXER_PAN:
        case WIDGET_MIXER_MUTE:
        case WIDGET_MIXER_SCOPE:
        case WIDGET_MIXER_MASTER:
        case WIDGET_DSP_WINDOW:
        case WIDGET_DSP_SLOT:
        case WIDGET_DSP_MENU:
        case WIDGET_DSP_PARAM:
            return true;
        default:
            return false;
    }
}

static bool widget_uses_font(const widget_t *widget)
{
    if (!widget)
        return false;

    switch (widget->type) {
        case WIDGET_PUSHBUTTON:
        case WIDGET_TEXTBOX:
        case WIDGET_FRAMEBOX:
        case WIDGET_LOGO:
        case WIDGET_CUSTOM_BUTTON:
        case WIDGET_DROPDOWN:
        case WIDGET_COMBOBOX:
        case WIDGET_LISTBOX:
        case WIDGET_PROGRESS_BAR:
        case WIDGET_WAVEFORM_VIEW:
        case WIDGET_TF_BUTTON:
        case WIDGET_TF_TOGGLE:
        case WIDGET_TF_LABEL:
        case WIDGET_TF_ROTARY:
        case WIDGET_TF_LINEAR:
        case WIDGET_TF_ARP_STEP:
        case WIDGET_TF_COMBO:
        case WIDGET_TF_METER:
        case WIDGET_TF_PARAMETER:
        case WIDGET_TF_ENVELOPE:
        case WIDGET_TF_GROUP:
        case WIDGET_MIXER_STRIP:
        case WIDGET_MIXER_GAIN:
        case WIDGET_MIXER_PAN:
        case WIDGET_MIXER_MUTE:
        case WIDGET_MIXER_SCOPE:
        case WIDGET_MIXER_MASTER:
        case WIDGET_DSP_WINDOW:
        case WIDGET_DSP_SLOT:
        case WIDGET_DSP_MENU:
        case WIDGET_DSP_PARAM:
            return true;
        default:
            return false;
    }
}

static bool widget_uses_bitmap(const widget_t *widget)
{
    return widget && widget->type == WIDGET_LOGO;
}

static bool widget_uses_scrollbar_nudge(const widget_t *widget)
{
    return widget && widget->type == WIDGET_SCROLLBAR;
}

static bool widget_uses_page(const widget_t *widget)
{
    if (!widget)
        return false;

    switch (widget->type) {
        case WIDGET_LOGO:
        case WIDGET_WAVEFORM_VIEW:
        case WIDGET_TF_BUTTON:
        case WIDGET_TF_TOGGLE:
        case WIDGET_TF_LABEL:
        case WIDGET_TF_ROTARY:
        case WIDGET_TF_LINEAR:
        case WIDGET_TF_ARP_STEP:
        case WIDGET_TF_COMBO:
        case WIDGET_TF_METER:
        case WIDGET_TF_PARAMETER:
        case WIDGET_TF_ENVELOPE:
        case WIDGET_TF_GROUP:
            return true;
        default:
            return false;
    }
}

static bool widget_allows_resize(const widget_t *widget)
{
    if (!widget)
        return false;

    return widget->type != WIDGET_DSP_MENU;
}

static void add_tool_entry(ft2_ui_widget_kind_t kind, const char *label, int default_w, int default_h, int scrollbar_orientation)
{
    if (g_tool_count >= (int)(sizeof(g_tool_entries) / sizeof(g_tool_entries[0])))
        return;

    tool_entry_t *entry = &g_tool_entries[g_tool_count++];
    entry->kind = kind;
    entry->widget_type = widget_type_from_kind(kind);
    entry->label = label;
    entry->default_w = default_w;
    entry->default_h = default_h;
    entry->scrollbar_orientation = scrollbar_orientation;
}

static void init_tools_from_schema(void)
{
    g_tool_count = 0;
    add_tool_entry(FT2_UI_WIDGET_NONE, "Select", 0, 0, -1);

    for (uint16_t i = 0; i < ft2_ui_widget_type_count; i++) {
        const ft2_ui_widget_type_desc_t *desc = &ft2_ui_widget_types[i];

        switch (desc->kind) {
            case FT2_UI_WIDGET_PUSHBUTTON:
            case FT2_UI_WIDGET_RADIOBUTTON:
            case FT2_UI_WIDGET_CHECKBOX:
            case FT2_UI_WIDGET_TEXTBOX:
            case FT2_UI_WIDGET_FRAMEBOX:
            case FT2_UI_WIDGET_WAVEFORM_VIEW:
            case FT2_UI_WIDGET_TF_BUTTON:
            case FT2_UI_WIDGET_TF_TOGGLE_BUTTON:
            case FT2_UI_WIDGET_TF_LABEL:
            case FT2_UI_WIDGET_TF_ROTARY_SLIDER:
            case FT2_UI_WIDGET_TF_LINEAR_SLIDER:
            case FT2_UI_WIDGET_TF_ARP_STEP:
            case FT2_UI_WIDGET_TF_COMBO_BOX:
            case FT2_UI_WIDGET_TF_LEVEL_METER:
            case FT2_UI_WIDGET_TF_PARAMETER_CONTROL:
            case FT2_UI_WIDGET_TF_ENVELOPE_DISPLAY:
            case FT2_UI_WIDGET_TF_GROUP_BOX:
            case FT2_UI_WIDGET_MIXER_STRIP:
            case FT2_UI_WIDGET_MIXER_GAIN:
            case FT2_UI_WIDGET_MIXER_PAN:
            case FT2_UI_WIDGET_MIXER_MUTE:
            case FT2_UI_WIDGET_MIXER_SCOPE:
            case FT2_UI_WIDGET_MIXER_MASTER:
            case FT2_UI_WIDGET_DSP_WINDOW:
            case FT2_UI_WIDGET_DSP_SLOT:
            case FT2_UI_WIDGET_DSP_MENU:
            case FT2_UI_WIDGET_DSP_PARAM:
                add_tool_entry(desc->kind, desc->name, desc->default_w, desc->default_h, -1);
                break;
            case FT2_UI_WIDGET_SCROLLBAR:
                add_tool_entry(desc->kind, "H-Scroll", desc->default_w, desc->default_h, SCROLLBAR_HORIZONTAL);
                add_tool_entry(desc->kind, "V-Scroll", desc->default_h, desc->default_w, SCROLLBAR_VERTICAL);
                break;
            case FT2_UI_WIDGET_BITMAP:
                add_tool_entry(desc->kind, "Bitmap", desc->default_w, desc->default_h, -1);
                break;
            default:
                break;
        }
    }
}

static const tool_entry_t *get_tool_entry(int tool_index)
{
    if (tool_index < 0 || tool_index >= g_tool_count)
        return NULL;
    return &g_tool_entries[tool_index];
}

static void tool_button_rect(int tool_index, int *x, int *y, int *w, int *h)
{
    int start_y = 42;
    int button_height = 20;
    int button_spacing = 1;
    int col1_x = 6;
    int col_width = (TOOLBAR_WIDTH - 18) / 2;
    int col2_x = col1_x + col_width + 6;
    int tools_per_col = (g_tool_count + 1) / 2;
    int col = tool_index / tools_per_col;
    int row = tool_index % tools_per_col;

    *x = (col == 0) ? col1_x : col2_x;
    *y = start_y + row * (button_height + button_spacing);
    *w = col_width;
    *h = button_height;
}

static int toolbar_info_y(void)
{
    if (g_tool_count <= 0)
        return 390;

    int lowest_edge = 0;
    for (int tool = 0; tool < g_tool_count; tool++) {
        int x, y, w, h;
        tool_button_rect(tool, &x, &y, &w, &h);
        if (y + h > lowest_edge)
            lowest_edge = y + h;
    }
    return lowest_edge + 8;
}

static void toolbar_page_selector_rect(int *x, int *y, int *w, int *h)
{
    *x = 6;
    *y = toolbar_info_y() + 86;
    *w = TOOLBAR_WIDTH - 12;
    *h = 18;
}

static void toolbar_theme_dropdown_rect(int *x, int *y, int *w, int *h)
{
    *x = 6;
    *y = toolbar_info_y() + 138;
    *w = TOOLBAR_WIDTH - 12;
    *h = 18;
}

static void toolbar_page_button_rects(int *minus_x, int *plus_x, int *y, int *w, int *h)
{
    int selector_x, selector_y, selector_w, selector_h;
    toolbar_page_selector_rect(&selector_x, &selector_y, &selector_w, &selector_h);

    *minus_x = selector_x + selector_w - (PAGE_SELECTOR_BUTTON_WIDTH * 2) - PAGE_SELECTOR_BUTTON_GAP;
    *plus_x = *minus_x + PAGE_SELECTOR_BUTTON_WIDTH + PAGE_SELECTOR_BUTTON_GAP;
    *y = selector_y;
    *w = PAGE_SELECTOR_BUTTON_WIDTH;
    *h = selector_h;
}

// Function declarations
void draw_property_field(int x, int y, int w, int h, PropField field_type, int value);
void draw_text_field(int x, int y, int w, int h, PropField field_type, const char *text);
void draw_adjust_button(int x, int y, int w, int h, const char *text);
void clear_framebuffer(uint32_t color);
void draw_toolbar(void);
void draw_property_panel(void);
void setup_3d_projection(void);
void draw_opengl_cube_wireframe(void);
void restore_2d_projection(void);
static void start_file_prompt(FilePromptMode mode);
static void finish_file_prompt(bool apply_changes);
static void handle_file_prompt_text(const char *text);
static void handle_file_prompt_key(SDL_Keycode key);
static void draw_file_prompt(void);
static void draw_bitmap_prompt(void);

static void start_bitmap_prompt(void);
static void finish_bitmap_prompt(bool apply_changes);
static void handle_bitmap_prompt_text(const char *text);
static void handle_bitmap_prompt_key(SDL_Keycode key);

void init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL Init failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Set OpenGL attributes for anti-aliasing
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // 4x anti-aliasing

    app.window = SDL_CreateWindow("FT2 GUI Designer - Enhanced with 3D OpenGL",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
    if (!app.window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Create OpenGL context for 3D rendering
    app.gl_context = SDL_GL_CreateContext(app.window);
    if (!app.gl_context) {
        printf("OpenGL context creation failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Enable VSync for 60Hz
    SDL_GL_SetSwapInterval(1);

    // Set up OpenGL for 3D cube
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_MULTISAMPLE); // Enable anti-aliasing
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.0f);

    // Note: We no longer need SDL renderer since we're using pure OpenGL for everything

    app.framebuffer = malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(uint32_t));
    if (!app.framebuffer) {
        printf("Memory allocation failed\n");
        exit(1);
    }

    // Initialize cube animation
    app.cube_rotation_x = 0.0f;
    app.cube_rotation_y = 0.0f;
    app.last_frame_time = SDL_GetTicks();
}

void cleanup_sdl(void) {
    // Clean up OpenGL texture
    if (gui_texture != 0) {
        glDeleteTextures(1, &gui_texture);
    }

    free(app.framebuffer);
    SDL_GL_DeleteContext(app.gl_context);
    if (app.window) SDL_DestroyWindow(app.window);
    SDL_Quit();
}

void start_toolbar_drag(int tool_type, int mouse_x, int mouse_y) {
    app.dragging = true;
    app.drag_from_toolbar = true;
    app.drag_tool = tool_type;
    app.drag_start_x = mouse_x;
    app.drag_start_y = mouse_y;
    app.drag_preview_x = mouse_x;
    app.drag_preview_y = mouse_y;
}

void start_widget_drag(int widget_id, int mouse_x, int mouse_y) {
    widget_t *widget = get_widget(&g_designer.widget_manager, widget_id);
    if (widget) {
        app.dragging = true;
        app.drag_from_toolbar = false;
        app.drag_tool = TOOL_SELECT_INDEX;

        // Calculate relative offset from widget position to mouse
        int widget_screen_x = widget->x + g_designer.canvas.x;
        int widget_screen_y = widget->y + g_designer.canvas.y;
        app.drag_start_x = mouse_x - widget_screen_x;
        app.drag_start_y = mouse_y - widget_screen_y;
        app.drag_preview_x = mouse_x;
        app.drag_preview_y = mouse_y;
    }
}

void update_drag_preview(int mouse_x, int mouse_y) {
    if (!app.dragging) return;

    app.drag_preview_x = mouse_x;
    app.drag_preview_y = mouse_y;

    if (!app.drag_from_toolbar) {
        // Dragging existing widget
        widget_t *widget = get_selected_widget(&g_designer.widget_manager);
        if (widget) {
            int new_x = mouse_x - g_designer.canvas.x - app.drag_start_x;
            int new_y = mouse_y - g_designer.canvas.y - app.drag_start_y;

            // Snap to grid
            int grid_size = g_designer.canvas.grid_size;
            new_x = ((new_x + grid_size/2) / grid_size) * grid_size;
            new_y = ((new_y + grid_size/2) / grid_size) * grid_size;

            // Keep within bounds
            if (new_x < 0) new_x = 0;
            if (new_y < 0) new_y = 0;
            if (new_x + widget->w > g_designer.canvas.width) new_x = g_designer.canvas.width - widget->w;
            if (new_y + widget->h > g_designer.canvas.height) new_y = g_designer.canvas.height - widget->h;

            widget->x = new_x;
            widget->y = new_y;
        }
    }
}

bool complete_drop(int mouse_x, int mouse_y) {
    if (!app.dragging) return false;

    if (app.drag_from_toolbar) {
        // Dropping new widget from toolbar
        if (point_in_canvas(&g_designer.canvas, mouse_x, mouse_y)) {
            const tool_entry_t *entry = get_tool_entry(app.drag_tool);
            int canvas_x = mouse_x - g_designer.canvas.x;
            int canvas_y = mouse_y - g_designer.canvas.y;

            // Snap to grid
            int grid_size = g_designer.canvas.grid_size;
            canvas_x = (canvas_x / grid_size) * grid_size;
            canvas_y = (canvas_y / grid_size) * grid_size;

            if (entry)
                place_widget(&g_designer.widget_manager, entry->kind, canvas_x, canvas_y, entry->scrollbar_orientation);
            return true;
        }
    } else {
        // Was dragging existing widget - already updated position
        return true;
    }

    return false;
}

void cancel_drag(void) {
    app.dragging = false;
    app.drag_from_toolbar = false;
}

void draw_drag_preview(void) {
    if (!app.dragging || !app.drag_from_toolbar) return;

    if (point_in_canvas(&g_designer.canvas, app.drag_preview_x, app.drag_preview_y)) {
        int preview_x = app.drag_preview_x - g_designer.canvas.x;
        int preview_y = app.drag_preview_y - g_designer.canvas.y;

        // Snap to grid
        int grid_size = g_designer.canvas.grid_size;
        preview_x = (preview_x / grid_size) * grid_size;
        preview_y = (preview_y / grid_size) * grid_size;

        const tool_entry_t *entry = get_tool_entry(app.drag_tool);
        if (!entry)
            return;

        // Create preview widget
        widget_t preview_widget = (widget_t){0};
        preview_widget.type = entry->widget_type;
        preview_widget.x = preview_x;
        preview_widget.y = preview_y;
        preview_widget.visible = true;
        preview_widget.state = WIDGET_UNPRESSED;
        strcpy(preview_widget.caption, "Preview");
        if (preview_widget.type == WIDGET_LOGO) {
            preview_widget.data.logo.layer = FT2_UI_BITMAP_LAYER_WIDGET;
            preview_widget.data.logo.opacity = 255;
        }
        if (preview_widget.type == WIDGET_TF_ARP_STEP) {
            strcpy(preview_widget.name, "arp_step_01");
            preview_widget.data.tf_linear.vertical = true;
            preview_widget.caption[0] = '\0';
        }

        // Set default sizes
        preview_widget.w = entry->default_w;
        preview_widget.h = entry->default_h;
        if (entry->widget_type == WIDGET_TF_ARP_STEP) {
            preview_widget.w = 18;
            preview_widget.h = 96;
            preview_widget.data.tf_linear.vertical = true;
        }
        if (entry->widget_type == WIDGET_SCROLLBAR && entry->scrollbar_orientation >= 0)
            preview_widget.data.scrollbar.orientation = (ScrollbarOrientation)entry->scrollbar_orientation;
        if (entry->kind == FT2_UI_WIDGET_MIXER_GAIN || entry->kind == FT2_UI_WIDGET_MIXER_MASTER)
            preview_widget.data.scrollbar.orientation = SCROLLBAR_VERTICAL;
        if (entry->kind == FT2_UI_WIDGET_MIXER_PAN || entry->kind == FT2_UI_WIDGET_DSP_PARAM)
            preview_widget.data.scrollbar.orientation = SCROLLBAR_HORIZONTAL;
        if (entry->widget_type == WIDGET_FRAMEBOX) {
            strcpy(preview_widget.caption, "Frame");
            preview_widget.data.framebox.filled = false;
            preview_widget.data.framebox.border_type = 0;
        }

        // Render preview with dotted outline
        render_widget(&preview_widget, app.framebuffer, WINDOW_WIDTH,
                     g_designer.canvas.x, g_designer.canvas.y);

        // Draw dotted outline
        uint32_t outline_color = get_palette_color(PAL_FORGRND);
        int screen_x = g_designer.canvas.x + preview_x;
        int screen_y = g_designer.canvas.y + preview_y;

        // Top and bottom lines
        for (int i = 0; i < preview_widget.w; i += 2) {
            if (screen_x + i < WINDOW_WIDTH) {
                app.framebuffer[screen_y * WINDOW_WIDTH + screen_x + i] = outline_color;
                if (screen_y + preview_widget.h - 1 < WINDOW_HEIGHT) {
                    app.framebuffer[(screen_y + preview_widget.h - 1) * WINDOW_WIDTH + screen_x + i] = outline_color;
                }
            }
        }

        // Left and right lines
        for (int i = 0; i < preview_widget.h; i += 2) {
            if (screen_y + i < WINDOW_HEIGHT) {
                app.framebuffer[(screen_y + i) * WINDOW_WIDTH + screen_x] = outline_color;
                if (screen_x + preview_widget.w - 1 < WINDOW_WIDTH) {
                    app.framebuffer[(screen_y + i) * WINDOW_WIDTH + screen_x + preview_widget.w - 1] = outline_color;
                }
            }
        }
    }
}

void start_property_edit(PropField field_type) {
    widget_t *widget = get_selected_widget(&g_designer.widget_manager);
    if (!widget) return;

    app.property_editing = true;
    app.editing_field = field_type;
    app.edit_cursor_pos = 0;
    app.edit_blink_timer = 0;

    // Fill edit buffer with current value
    switch (field_type) {
        case PROP_X:
            sprintf(app.edit_buffer, "%d", widget->x);
            break;
        case PROP_Y:
            sprintf(app.edit_buffer, "%d", widget->y);
            break;
        case PROP_WIDTH:
            sprintf(app.edit_buffer, "%d", widget->w);
            break;
        case PROP_HEIGHT:
            sprintf(app.edit_buffer, "%d", widget->h);
            break;
        case PROP_CAPTION:
            strncpy(app.edit_buffer, widget->caption, sizeof(app.edit_buffer) - 1);
            break;
        case PROP_CAPTION2:
            strncpy(app.edit_buffer, widget->caption2, sizeof(app.edit_buffer) - 1);
            break;
        case PROP_NAME:
            strncpy(app.edit_buffer, widget->name, sizeof(app.edit_buffer) - 1);
            break;
        case PROP_FONT:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", (int)widget->font_type + 1);
            break;
        case PROP_BITMAP:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", widget->data.logo.bitmap_id);
            break;
        case PROP_BITMAP_TRANSP: {
            int trans_idx = designer_bitmap_transparent_index(widget->data.logo.bitmap_id);
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", trans_idx);
            break;
        }
        case PROP_BITMAP_LAYER:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", (int)widget->data.logo.layer);
            break;
        case PROP_BITMAP_FLAGS:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", (int)widget->data.logo.flags);
            break;
        case PROP_BITMAP_OPACITY:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", (int)widget->data.logo.opacity);
            break;
        case PROP_SKIN_PART:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", (int)widget->data.logo.skin_part);
            break;
        case PROP_PAGE:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", (int)widget->page);
            break;
        case PROP_SCROLLBAR_NUDGE:
            snprintf(app.edit_buffer, sizeof(app.edit_buffer), "%d", widget->data.scrollbar.has_nudge_buttons ? 1 : 0);
            break;
    }
    app.edit_buffer[sizeof(app.edit_buffer) - 1] = '\0';
    app.edit_cursor_pos = strlen(app.edit_buffer);
    SDL_StartTextInput();
}

void finish_property_edit(bool apply_changes) {
    if (!app.property_editing) return;

    if (apply_changes) {
        widget_t *widget = get_selected_widget(&g_designer.widget_manager);
        if (widget) {
            switch (app.editing_field) {
                case PROP_X:
                    widget->x = atoi(app.edit_buffer);
                    if (widget->x < 0) widget->x = 0;
                    break;
                case PROP_Y:
                    widget->y = atoi(app.edit_buffer);
                    if (widget->y < 0) widget->y = 0;
                    break;
                case PROP_WIDTH:
                    widget->w = atoi(app.edit_buffer);
                    if (widget->w < 1) widget->w = 1;
                    break;
                case PROP_HEIGHT:
                    widget->h = atoi(app.edit_buffer);
                    if (widget->h < 1) widget->h = 1;
                    break;
                case PROP_CAPTION:
                    strncpy(widget->caption, app.edit_buffer, MAX_CAPTION_LEN - 1);
                    widget->caption[MAX_CAPTION_LEN - 1] = '\0';
                    break;
                case PROP_CAPTION2:
                    strncpy(widget->caption2, app.edit_buffer, MAX_CAPTION_LEN - 1);
                    widget->caption2[MAX_CAPTION_LEN - 1] = '\0';
                    break;
                case PROP_NAME:
                    strncpy(widget->name, app.edit_buffer, sizeof(widget->name) - 1);
                    widget->name[sizeof(widget->name) - 1] = '\0';
                    sanitize_widget_name(widget->name);
                    break;
                case PROP_FONT: {
                    int font_id = atoi(app.edit_buffer) - 1;
                    if (font_id < 0) font_id = 0;
                    if (font_id >= FT2_UI_FONT_COUNT) font_id = FT2_UI_FONT_COUNT - 1;
                    widget->font_type = (ft2_ui_font_id_t)font_id;
                    break;
                }
                case PROP_BITMAP: {
                    int bitmap_id = atoi(app.edit_buffer);
                    if (designer_bitmap_index_for_id(bitmap_id) >= 0)
                        widget->data.logo.bitmap_id = bitmap_id;
                    break;
                }
                case PROP_BITMAP_TRANSP: {
                    int trans_idx = atoi(app.edit_buffer);
                    designer_set_bitmap_transparent_index(widget->data.logo.bitmap_id, trans_idx);
                    break;
                }
                case PROP_BITMAP_LAYER: {
                    int layer = atoi(app.edit_buffer);
                    if (layer < FT2_UI_BITMAP_LAYER_WIDGET) layer = FT2_UI_BITMAP_LAYER_WIDGET;
                    if (layer > FT2_UI_BITMAP_LAYER_SKIN) layer = FT2_UI_BITMAP_LAYER_SKIN;
                    widget->data.logo.layer = (ft2_ui_bitmap_layer_t)layer;
                    break;
                }
                case PROP_BITMAP_FLAGS: {
                    int flags = atoi(app.edit_buffer);
                    if (flags < 0) flags = 0;
                    if (flags > 255) flags = 255;
                    widget->data.logo.flags = (uint8_t)flags;
                    break;
                }
                case PROP_BITMAP_OPACITY: {
                    int opacity = atoi(app.edit_buffer);
                    if (opacity < 0) opacity = 0;
                    if (opacity > 255) opacity = 255;
                    widget->data.logo.opacity = (uint8_t)opacity;
                    break;
                }
                case PROP_SKIN_PART: {
                    int part = atoi(app.edit_buffer);
                    if (part < FT2_UI_SKIN_PART_NONE) part = FT2_UI_SKIN_PART_NONE;
                    if (part > FT2_UI_SKIN_PART_METER) part = FT2_UI_SKIN_PART_METER;
                    widget->data.logo.skin_part = (ft2_ui_skin_part_t)part;
                    break;
                }
                case PROP_PAGE: {
                    int page = atoi(app.edit_buffer);
                    if (page < 0) page = 0;
                    if (page > (int)FT2_UI_WIDGET_PAGE_7) page = (int)FT2_UI_WIDGET_PAGE_7;
                    widget->page = (ft2_ui_widget_page_t)page;
                    break;
                }
                case PROP_SCROLLBAR_NUDGE: {
                    int value = atoi(app.edit_buffer);
                    widget->data.scrollbar.has_nudge_buttons = (value != 0);
                    break;
                }
            }
        }
    }

    app.property_editing = false;
    app.edit_buffer[0] = '\0';
    app.edit_cursor_pos = 0;
    SDL_StopTextInput();
}

void handle_text_input(const char *text) {
    if (app.file_prompt_active) {
        handle_file_prompt_text(text);
        return;
    }
    if (app.bitmap_prompt_active) {
        handle_bitmap_prompt_text(text);
        return;
    }
    if (!app.property_editing) return;

    size_t len = strlen(app.edit_buffer);
    size_t text_len = strlen(text);
    size_t cursor = (app.edit_cursor_pos < 0) ? 0u : (size_t)app.edit_cursor_pos;
    if (cursor > len) cursor = len;

    if (len + text_len < sizeof(app.edit_buffer) - 1) {
        // Insert text at cursor position
        memmove(&app.edit_buffer[cursor + text_len],
                &app.edit_buffer[cursor],
                len - cursor + 1);
        memcpy(&app.edit_buffer[cursor], text, text_len);
        app.edit_cursor_pos = (int)(cursor + text_len);
        app.edit_blink_timer = 0;
    }
}

void handle_property_key(SDL_Keycode key) {
    if (!app.property_editing) return;

    int len = strlen(app.edit_buffer);

    switch (key) {
        case SDLK_LEFT:
            if (app.edit_cursor_pos > 0) app.edit_cursor_pos--;
            break;
        case SDLK_RIGHT:
            if (app.edit_cursor_pos < len) app.edit_cursor_pos++;
            break;
        case SDLK_HOME:
            app.edit_cursor_pos = 0;
            break;
        case SDLK_END:
            app.edit_cursor_pos = len;
            break;
        case SDLK_BACKSPACE:
            if (app.edit_cursor_pos > 0) {
                memmove(&app.edit_buffer[app.edit_cursor_pos - 1],
                        &app.edit_buffer[app.edit_cursor_pos],
                        len - app.edit_cursor_pos + 1);
                app.edit_cursor_pos--;
            }
            break;
        case SDLK_DELETE:
            if (app.edit_cursor_pos < len) {
                memmove(&app.edit_buffer[app.edit_cursor_pos],
                        &app.edit_buffer[app.edit_cursor_pos + 1],
                        len - app.edit_cursor_pos);
            }
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            finish_property_edit(true);
            break;
        case SDLK_ESCAPE:
            finish_property_edit(false);
            break;
    }
    app.edit_blink_timer = 0;
}

void adjust_property_value(PropField field_type, int delta) {
    widget_t *widget = get_selected_widget(&g_designer.widget_manager);
    if (!widget) return;

    switch (field_type) {
        case PROP_X:
            widget->x += delta;
            if (widget->x < 0) widget->x = 0;
            if (widget->x >= g_designer.canvas.width) widget->x = g_designer.canvas.width - 1;
            break;
        case PROP_Y:
            widget->y += delta;
            if (widget->y < 0) widget->y = 0;
            if (widget->y >= g_designer.canvas.height) widget->y = g_designer.canvas.height - 1;
            break;
        case PROP_WIDTH:
            widget->w += delta;
            if (widget->w < 1) widget->w = 1;
            break;
        case PROP_HEIGHT:
            widget->h += delta;
            if (widget->h < 1) widget->h = 1;
            break;
        case PROP_FONT: {
            int font_id = (int)widget->font_type + delta;
            if (font_id < 0) font_id = 0;
            if (font_id >= FT2_UI_FONT_COUNT) font_id = FT2_UI_FONT_COUNT - 1;
            widget->font_type = (ft2_ui_font_id_t)font_id;
            break;
        }
        case PROP_BITMAP: {
            int count = designer_bitmap_count();
            if (count <= 0)
                break;
            int index = designer_bitmap_index_for_id(widget->data.logo.bitmap_id);
            if (index < 0)
                index = 0;
            index += delta;
            if (index < 0) index = 0;
            if (index >= count) index = count - 1;
            widget->data.logo.bitmap_id = designer_bitmap_id_at(index);
            break;
        }
        case PROP_BITMAP_TRANSP: {
            int trans_idx = designer_bitmap_transparent_index(widget->data.logo.bitmap_id);
            trans_idx += delta;
            if (trans_idx < 0) trans_idx = 0;
            if (trans_idx > 15) trans_idx = 15;
            designer_set_bitmap_transparent_index(widget->data.logo.bitmap_id, trans_idx);
            break;
        }
        case PROP_BITMAP_LAYER: {
            int layer = (int)widget->data.logo.layer + delta;
            if (layer < FT2_UI_BITMAP_LAYER_WIDGET) layer = FT2_UI_BITMAP_LAYER_WIDGET;
            if (layer > FT2_UI_BITMAP_LAYER_SKIN) layer = FT2_UI_BITMAP_LAYER_SKIN;
            widget->data.logo.layer = (ft2_ui_bitmap_layer_t)layer;
            break;
        }
        case PROP_BITMAP_FLAGS: {
            int flags = (int)widget->data.logo.flags + delta;
            if (flags < 0) flags = 0;
            if (flags > 255) flags = 255;
            widget->data.logo.flags = (uint8_t)flags;
            break;
        }
        case PROP_BITMAP_OPACITY: {
            int opacity = (int)widget->data.logo.opacity + delta;
            if (opacity < 0) opacity = 0;
            if (opacity > 255) opacity = 255;
            widget->data.logo.opacity = (uint8_t)opacity;
            break;
        }
        case PROP_SKIN_PART: {
            int part = (int)widget->data.logo.skin_part + delta;
            if (part < FT2_UI_SKIN_PART_NONE) part = FT2_UI_SKIN_PART_NONE;
            if (part > FT2_UI_SKIN_PART_METER) part = FT2_UI_SKIN_PART_METER;
            widget->data.logo.skin_part = (ft2_ui_skin_part_t)part;
            break;
        }
        case PROP_PAGE: {
            int page = (int)widget->page + delta;
            if (page < 0) page = 0;
            if (page > (int)FT2_UI_WIDGET_PAGE_7) page = (int)FT2_UI_WIDGET_PAGE_7;
            widget->page = (ft2_ui_widget_page_t)page;
            break;
        }
        case PROP_SCROLLBAR_NUDGE:
            widget->data.scrollbar.has_nudge_buttons = !widget->data.scrollbar.has_nudge_buttons;
            break;
        default:
            break;
    }
}

bool is_property_field_at(int x, int y, int field_x, int field_y, int field_w, int field_h) {
    return x >= field_x && x < field_x + field_w && y >= field_y && y < field_y + field_h;
}

static LayoutKind detect_layout_kind_from_path(const char *path)
{
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    if (strcmp(base, "tf_complete_layout.gui") == 0)
        return LAYOUT_TUNEFISH;
    if (strcmp(base, "dx_complete_layout.gui") == 0)
        return LAYOUT_DEXED;
    if (strcmp(base, "ft2_mixer_layout.gui") == 0)
        return LAYOUT_MIXER;
    if (strcmp(base, "v2_complete_layout.gui") == 0)
        return LAYOUT_V2;
    if (strcmp(base, "ostirus_complete_layout.gui") == 0)
        return LAYOUT_OSTIRUS;
    return LAYOUT_UNKNOWN;
}

static const char *default_export_base_for_layout(LayoutKind kind)
{
    switch (kind) {
        case LAYOUT_TUNEFISH:
            return "../src/ft2_tunefish_complete_layout_schema";
        case LAYOUT_DEXED:
            return "../src/dexed/dx_complete_layout_schema";
        case LAYOUT_MIXER:
            return "../src/ft2_mixer_layout_schema";
        case LAYOUT_V2:
            return "../src/ft2_v2_complete_layout_schema";
        case LAYOUT_OSTIRUS:
            return "../src/ft2_ostirus_complete_layout_schema";
        default:
            return "exported_gui";
    }
}

static bool readable_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    fclose(file);
    return true;
}

static bool copy_readable_path(char *resolved, size_t resolved_size, const char *path)
{
    if (!readable_file(path))
        return false;

    const int written = snprintf(resolved, resolved_size, "%s", path);
    return written >= 0 && (size_t)written < resolved_size;
}

static bool resolve_design_path(const char *requested, char *resolved, size_t resolved_size)
{
    if (!requested || requested[0] == '\0' || !resolved || resolved_size == 0)
        return false;

    if (copy_readable_path(resolved, resolved_size, requested))
        return true;

    char *base_path = SDL_GetBasePath();
    if (!base_path)
        return false;

    char candidate[1024];
    int written = snprintf(candidate, sizeof(candidate), "%s%s", base_path, requested);
    if (written >= 0 && (size_t)written < sizeof(candidate) &&
        copy_readable_path(resolved, resolved_size, candidate)) {
        SDL_free(base_path);
        return true;
    }

    written = snprintf(candidate, sizeof(candidate), "%s../ft2_gui_designer/%s", base_path, requested);
    const bool found = written >= 0 && (size_t)written < sizeof(candidate) &&
                       copy_readable_path(resolved, resolved_size, candidate);
    SDL_free(base_path);
    return found;
}

static bool load_designer_file(const char *requested)
{
    char resolved[512];
    if (!resolve_design_path(requested, resolved, sizeof(resolved)))
        return false;
    if (!load_design(&g_designer.widget_manager, resolved))
        return false;

    snprintf(app.last_load_path, sizeof(app.last_load_path), "%s", resolved);
    snprintf(app.last_save_path, sizeof(app.last_save_path), "%s", resolved);
    app.active_layout = detect_layout_kind_from_path(resolved);
    if (app.active_layout != LAYOUT_UNKNOWN)
        snprintf(app.last_export_base, sizeof(app.last_export_base), "%s",
                 default_export_base_for_layout(app.active_layout));
    return true;
}

static void start_file_prompt(FilePromptMode mode)
{
    if (app.property_editing)
        finish_property_edit(false);
    if (app.bitmap_prompt_active)
        finish_bitmap_prompt(false);

    app.file_prompt_active = true;
    app.file_prompt_mode = mode;
    app.file_prompt_buffer[0] = '\0';
    app.file_prompt_cursor = 0;
    app.edit_blink_timer = 0;

    const char *default_value = "";
    switch (mode) {
        case FILE_PROMPT_LOAD:
            snprintf(app.file_prompt_label, sizeof(app.file_prompt_label), "Load GUI: ");
            if (app.last_load_path[0] != '\0')
                default_value = app.last_load_path;
            else
                default_value = (app.active_layout == LAYOUT_V2)
                    ? "layouts/v2_complete_layout.gui"
                    : (app.active_layout == LAYOUT_OSTIRUS)
                        ? "layouts/ostirus_complete_layout.gui"
                        : "layouts/tf_complete_layout.gui";
            break;
        case FILE_PROMPT_SAVE:
            snprintf(app.file_prompt_label, sizeof(app.file_prompt_label), "Save GUI: ");
            if (app.last_save_path[0] != '\0')
                default_value = app.last_save_path;
            else
                default_value = "layouts/untitled.gui";
            break;
        case FILE_PROMPT_EXPORT:
            snprintf(app.file_prompt_label, sizeof(app.file_prompt_label), "Export schema base: ");
            if (app.last_export_base[0] != '\0')
                default_value = app.last_export_base;
            else
                default_value = default_export_base_for_layout(app.active_layout);
            break;
    }

    snprintf(app.file_prompt_buffer, sizeof(app.file_prompt_buffer), "%s", default_value);
    app.file_prompt_cursor = (int)strlen(app.file_prompt_buffer);
    SDL_StartTextInput();
}

static void finish_file_prompt(bool apply_changes)
{
    if (!app.file_prompt_active)
        return;

    if (apply_changes && app.file_prompt_buffer[0] != '\0') {
        bool ok = false;
        char name_buf[200];
        snprintf(name_buf, sizeof(name_buf), "%.180s", app.file_prompt_buffer);

        switch (app.file_prompt_mode) {
            case FILE_PROMPT_LOAD:
                ok = load_designer_file(app.file_prompt_buffer);
                if (!ok) {
                    printf("Failed to load design: %s\n", app.file_prompt_buffer);
                }
                snprintf(app.file_status_message, sizeof(app.file_status_message),
                         ok ? "Loaded '%s'" : "Load failed for '%s'. See console.", name_buf);
                break;
            case FILE_PROMPT_SAVE:
                ok = save_design(&g_designer.widget_manager, app.file_prompt_buffer);
                if (ok)
                    snprintf(app.last_save_path, sizeof(app.last_save_path), "%s", app.file_prompt_buffer);
                else
                    printf("Failed to save design: %s\n", app.file_prompt_buffer);
                snprintf(app.file_status_message, sizeof(app.file_status_message),
                         ok ? "Saved '%s'" : "Save failed for '%s'. See console.", name_buf);
                break;
            case FILE_PROMPT_EXPORT:
                ok = export_gui_code(&g_designer.widget_manager, app.file_prompt_buffer);
                if (ok)
                    snprintf(app.last_export_base, sizeof(app.last_export_base), "%s", app.file_prompt_buffer);
                else
                    printf("Failed to export schema: %s\n", app.file_prompt_buffer);
                snprintf(app.file_status_message, sizeof(app.file_status_message),
                         ok ? "Exported schema '%s'" : "Export failed for '%s'. See console.", name_buf);
                break;
        }

        app.file_status_success = ok;
        app.file_status_active = true;
        app.file_status_start = SDL_GetTicks();
    }

    app.file_prompt_active = false;
    app.file_prompt_buffer[0] = '\0';
    app.file_prompt_cursor = 0;
    SDL_StopTextInput();
}

static void handle_file_prompt_text(const char *text)
{
    int len = strlen(app.file_prompt_buffer);
    int text_len = strlen(text);
    if (len + text_len >= (int)sizeof(app.file_prompt_buffer) - 1)
        return;

    memmove(&app.file_prompt_buffer[app.file_prompt_cursor + text_len],
            &app.file_prompt_buffer[app.file_prompt_cursor],
            len - app.file_prompt_cursor + 1);
    memcpy(&app.file_prompt_buffer[app.file_prompt_cursor], text, text_len);
    app.file_prompt_cursor += text_len;
}

static void handle_file_prompt_key(SDL_Keycode key)
{
    int len = strlen(app.file_prompt_buffer);

    switch (key) {
        case SDLK_LEFT:
            if (app.file_prompt_cursor > 0)
                app.file_prompt_cursor--;
            break;
        case SDLK_RIGHT:
            if (app.file_prompt_cursor < len)
                app.file_prompt_cursor++;
            break;
        case SDLK_HOME:
            app.file_prompt_cursor = 0;
            break;
        case SDLK_END:
            app.file_prompt_cursor = len;
            break;
        case SDLK_BACKSPACE:
            if (app.file_prompt_cursor > 0) {
                memmove(&app.file_prompt_buffer[app.file_prompt_cursor - 1],
                        &app.file_prompt_buffer[app.file_prompt_cursor],
                        len - app.file_prompt_cursor + 1);
                app.file_prompt_cursor--;
            }
            break;
        case SDLK_DELETE:
            if (app.file_prompt_cursor < len) {
                memmove(&app.file_prompt_buffer[app.file_prompt_cursor],
                        &app.file_prompt_buffer[app.file_prompt_cursor + 1],
                        len - app.file_prompt_cursor);
            }
            break;
        case SDLK_RETURN:
            finish_file_prompt(true);
            break;
        case SDLK_ESCAPE:
            finish_file_prompt(false);
            break;
    }
}

static void start_bitmap_prompt(void)
{
    if (app.property_editing)
        finish_property_edit(false);
    if (app.file_prompt_active)
        finish_file_prompt(false);

    app.bitmap_prompt_active = true;
    app.bitmap_prompt_buffer[0] = '\0';
    app.bitmap_prompt_cursor = 0;
    app.edit_blink_timer = 0;
    SDL_StartTextInput();
}

static void finish_bitmap_prompt(bool apply_changes)
{
    if (!app.bitmap_prompt_active)
        return;

    if (apply_changes && app.bitmap_prompt_buffer[0] != '\0') {
        int id = -1;
        char name_buf[200];
        snprintf(name_buf, sizeof(name_buf), "%.180s", app.bitmap_prompt_buffer);
        if (designer_import_bitmap(app.bitmap_prompt_buffer, &id)) {
            widget_t *widget = get_selected_widget(&g_designer.widget_manager);
            if (widget && widget->type == WIDGET_LOGO)
                widget->data.logo.bitmap_id = id;
            snprintf(app.bitmap_status_message, sizeof(app.bitmap_status_message),
                     "Bitmap '%s' imported successfully", name_buf);
            app.bitmap_status_success = true;
        } else {
            printf("Failed to import bitmap: %s\n", app.bitmap_prompt_buffer);
            snprintf(app.bitmap_status_message, sizeof(app.bitmap_status_message),
                     "Bitmap '%s' import failed. See console for details.", name_buf);
            app.bitmap_status_success = false;
        }
        app.bitmap_status_active = true;
        app.bitmap_status_start = SDL_GetTicks();
    }

    app.bitmap_prompt_active = false;
    app.bitmap_prompt_buffer[0] = '\0';
    app.bitmap_prompt_cursor = 0;
    SDL_StopTextInput();
}

static void handle_bitmap_prompt_text(const char *text)
{
    int len = strlen(app.bitmap_prompt_buffer);
    int text_len = strlen(text);
    if (len + text_len >= (int)sizeof(app.bitmap_prompt_buffer) - 1)
        return;

    memmove(&app.bitmap_prompt_buffer[app.bitmap_prompt_cursor + text_len],
            &app.bitmap_prompt_buffer[app.bitmap_prompt_cursor],
            len - app.bitmap_prompt_cursor + 1);
    memcpy(&app.bitmap_prompt_buffer[app.bitmap_prompt_cursor], text, text_len);
    app.bitmap_prompt_cursor += text_len;
    app.edit_blink_timer = 0;
}

static void handle_bitmap_prompt_key(SDL_Keycode key)
{
    int len = strlen(app.bitmap_prompt_buffer);

    switch (key) {
        case SDLK_LEFT:
            if (app.bitmap_prompt_cursor > 0)
                app.bitmap_prompt_cursor--;
            break;
        case SDLK_RIGHT:
            if (app.bitmap_prompt_cursor < len)
                app.bitmap_prompt_cursor++;
            break;
        case SDLK_HOME:
            app.bitmap_prompt_cursor = 0;
            break;
        case SDLK_END:
            app.bitmap_prompt_cursor = len;
            break;
        case SDLK_BACKSPACE:
            if (app.bitmap_prompt_cursor > 0) {
                memmove(&app.bitmap_prompt_buffer[app.bitmap_prompt_cursor - 1],
                        &app.bitmap_prompt_buffer[app.bitmap_prompt_cursor],
                        len - app.bitmap_prompt_cursor + 1);
                app.bitmap_prompt_cursor--;
            }
            break;
        case SDLK_DELETE:
            if (app.bitmap_prompt_cursor < len) {
                memmove(&app.bitmap_prompt_buffer[app.bitmap_prompt_cursor],
                        &app.bitmap_prompt_buffer[app.bitmap_prompt_cursor + 1],
                        len - app.bitmap_prompt_cursor);
            }
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            finish_bitmap_prompt(true);
            break;
        case SDLK_ESCAPE:
            finish_bitmap_prompt(false);
            break;
        default:
            break;
    }
    app.edit_blink_timer = 0;
}

bool init_designer(void) {
    // Initialize subsystems
    designer_set_framebuffer_height(WINDOW_HEIGHT);
    init_palette();
    init_canvas(&g_designer.canvas, TOOLBAR_WIDTH + 10, 50, CANVAS_WIDTH, CANVAS_HEIGHT);
    g_designer.canvas.grid_enabled = true;
    g_designer.canvas.grid_size = 8;

    init_tools_from_schema();

    init_widget_manager(&g_designer.widget_manager);

    if (!init_font_system(&g_designer.font_system, app.framebuffer, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        printf("Failed to initialize font system\n");
        return false;
    }
    widgets_set_font_system(&g_designer.font_system);

    if (!init_designer_assets()) {
        printf("Failed to initialize FT2 assets\n");
        return false;
    }

    // Initialize application state
    app.current_tool = TOOL_SELECT_INDEX;
    app.show_grid = true;
    app.show_properties = true;
    app.dragging = false;
    app.property_editing = false;

    app.active_page = FT2_UI_WIDGET_PAGE_BOTH;
    app.page_view_number = FT2_UI_WIDGET_PAGE_1;
    app.theme_dropdown_open = false;

    printf("FT2 GUI Designer\n");
    printf("Controls:\n");
    printf("  1: Select tool (click widgets to select, drag to move)\n");
    printf("  2-6: Widget tools - Click to activate or DRAG to canvas\n");
    printf("      2=Button, 3=Radio, 4=Checkbox, 5=H-Scroll, 6=V-Scroll\n");
    printf("  Ctrl+S: Save design\n");
    printf("  Ctrl+O: Load design\n");
    printf("  Ctrl+E: Export schema\n");
    printf("  Ctrl+I: Import bitmap\n");
    printf("  T: Cycle designer color theme\n");
    printf("  Delete: Delete selected widget\n");
    printf("  ESC: Cancel drag or exit\n");

    return true;
}

void cleanup_designer(void) {
    free_designer_assets();
    cleanup_font_system(&g_designer.font_system);
    cleanup_widget_manager(&g_designer.widget_manager);
}

void handle_mouse_down(int x, int y, int button) {
    g_designer.mouse_x = x;
    g_designer.mouse_y = y;

    if (app.file_prompt_active)
        return;
    if (app.bitmap_prompt_active)
        return;

    if (button == SDL_BUTTON_LEFT) {
        // Check property panel clicks first
        if (app.show_properties) {
            int panel_x = WINDOW_WIDTH - PROPERTY_PANEL_WIDTH;
            widget_t *widget = get_selected_widget(&g_designer.widget_manager);

            // Property field click detection
            if (x >= panel_x && x < WINDOW_WIDTH) {
                int field_y = 80;
                int label_w = 40;
                int value_w = 60;
                int adjust_w = 16;
                int adjust_gap = 2;

                // Name field (text field)
                if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, 120, 15)) {
                    start_property_edit(PROP_NAME);
                    return;
                }
                field_y += 25;

                // X field
                if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                    start_property_edit(PROP_X);
                    return;
                } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                    adjust_property_value(PROP_X, -1);
                    return;
                } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                    adjust_property_value(PROP_X, 1);
                    return;
                }
                field_y += 20;

                // Y field
                if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                    start_property_edit(PROP_Y);
                    return;
                } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                    adjust_property_value(PROP_Y, -1);
                    return;
                } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                    adjust_property_value(PROP_Y, 1);
                    return;
                }
                field_y += 20;

                if (widget_allows_resize(widget)) {
                    // Width field
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_WIDTH);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_WIDTH, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_WIDTH, 1);
                        return;
                    }
                    field_y += 20;

                    // Height field
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_HEIGHT);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_HEIGHT, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_HEIGHT, 1);
                        return;
                    }
                    field_y += 20;
                }

                if (widget_uses_page(widget)) {
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_PAGE);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_PAGE, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_PAGE, 1);
                        return;
                    }
                    field_y += 20;
                }

                if (widget_has_caption(widget)) {
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, 120, 15)) {
                        start_property_edit(PROP_CAPTION);
                        return;
                    }
                    field_y += 20;
                }

                if (widget && widget->type == WIDGET_PUSHBUTTON) {
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, 120, 15)) {
                        start_property_edit(PROP_CAPTION2);
                        return;
                    }
                    field_y += 20;
                }

                if (widget_uses_scrollbar_nudge(widget)) {
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_SCROLLBAR_NUDGE);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_SCROLLBAR_NUDGE, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_SCROLLBAR_NUDGE, 1);
                        return;
                    }
                    field_y += 20;
                }

                if (widget_uses_font(widget)) {
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_FONT);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_FONT, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_FONT, 1);
                        return;
                    }
                    field_y += 20;
                }

                if (widget_uses_bitmap(widget)) {
                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_BITMAP);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP, 1);
                        return;
                    }
                    field_y += 20;

                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_BITMAP_TRANSP);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_TRANSP, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_TRANSP, 1);
                        return;
                    }
                    field_y += 20;

                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_BITMAP_LAYER);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_LAYER, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_LAYER, 1);
                        return;
                    }
                    field_y += 20;

                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_BITMAP_FLAGS);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_FLAGS, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_FLAGS, 1);
                        return;
                    }
                    field_y += 20;

                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_BITMAP_OPACITY);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_OPACITY, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_BITMAP_OPACITY, 1);
                        return;
                    }
                    field_y += 20;

                    if (is_property_field_at(x, y, panel_x + label_w + 5, field_y, value_w, 15)) {
                        start_property_edit(PROP_SKIN_PART);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_SKIN_PART, -1);
                        return;
                    } else if (is_property_field_at(x, y, panel_x + label_w + value_w + 8 + adjust_w + adjust_gap, field_y, adjust_w, 15)) {
                        adjust_property_value(PROP_SKIN_PART, 1);
                        return;
                    }
                    field_y += 20;
                }
            }
        }

        // Check toolbar clicks
        if (x < TOOLBAR_WIDTH) {
            int drop_x, drop_y, drop_w, drop_h;

            toolbar_page_selector_rect(&drop_x, &drop_y, &drop_w, &drop_h);
            int minus_x, plus_x, button_y, button_w, button_h;
            toolbar_page_button_rects(&minus_x, &plus_x, &button_y, &button_w, &button_h);
            if (x >= minus_x && x < minus_x + button_w &&
                y >= button_y && y < button_y + button_h) {
                step_page_selector(-1);
                app.theme_dropdown_open = false;
                return;
            }
            if (x >= plus_x && x < plus_x + button_w &&
                y >= button_y && y < button_y + button_h) {
                step_page_selector(1);
                app.theme_dropdown_open = false;
                return;
            }

            toolbar_theme_dropdown_rect(&drop_x, &drop_y, &drop_w, &drop_h);
            if (app.theme_dropdown_open) {
                int item_y = drop_y + drop_h + 1;
                for (int i = 0; i < DESIGNER_THEME_COUNT; i++, item_y += 16) {
                    if (x >= drop_x && x < drop_x + drop_w && y >= item_y && y < item_y + 16) {
                        designer_set_palette_theme((designer_palette_theme_t)i);
                        app.theme_dropdown_open = false;
                        return;
                    }
                }
            }

            if (x >= drop_x && x < drop_x + drop_w && y >= drop_y && y < drop_y + drop_h) {
                app.theme_dropdown_open = !app.theme_dropdown_open;
                return;
            }

            app.theme_dropdown_open = false;

            for (int tool = 0; tool < g_tool_count; tool++) {
                int btn_x, btn_y, btn_w, btn_h;
                tool_button_rect(tool, &btn_x, &btn_y, &btn_w, &btn_h);

                if (x >= btn_x && x < btn_x + btn_w &&
                    y >= btn_y && y < btn_y + btn_h) {
                    if (app.current_tool == tool && tool != TOOL_SELECT_INDEX) {
                        start_toolbar_drag(tool, x, y);
                    } else {
                        app.current_tool = tool;
                        cancel_drag();
                    }
                    return;
                }
            }
        }

        app.theme_dropdown_open = false;

        // Check canvas clicks
        if (point_in_canvas(&g_designer.canvas, x, y)) {
            if (app.current_tool == TOOL_SELECT_INDEX) {
                // Select widget at click position
                int canvas_x = x - g_designer.canvas.x;
                int canvas_y = y - g_designer.canvas.y;

                int old_selection = g_designer.widget_manager.selected_widget_id;
                select_widget_at_point_on_page(&g_designer.widget_manager, canvas_x, canvas_y, app.active_page);

                // If we selected a widget, start drag
                if (g_designer.widget_manager.selected_widget_id != -1 &&
                    g_designer.widget_manager.selected_widget_id == old_selection) {
                    start_widget_drag(g_designer.widget_manager.selected_widget_id, x, y);
                }
            } else {
                // Place widget tool
                const tool_entry_t *entry = get_tool_entry(app.current_tool);
                int canvas_x = x - g_designer.canvas.x;
                int canvas_y = y - g_designer.canvas.y;
                if (entry)
                    place_widget(&g_designer.widget_manager, entry->kind, canvas_x, canvas_y, entry->scrollbar_orientation);
            }
        }
    }
}

void handle_mouse_up(int x, int y, int button) {
    (void)button;
    if (app.dragging) {
        if (complete_drop(x, y)) {
            // Successfully placed/moved widget
        }
        cancel_drag();
    }
}

void handle_mouse_motion(int x, int y) {
    g_designer.mouse_x = x;
    g_designer.mouse_y = y;

    if (app.dragging) {
        update_drag_preview(x, y);
    }
}

void handle_key_down(SDL_Keycode key) {
    if (app.file_prompt_active) {
        handle_file_prompt_key(key);
        return;
    }
    if (app.bitmap_prompt_active) {
        handle_bitmap_prompt_key(key);
        return;
    }
    if (app.property_editing) {
        handle_property_key(key);
        return;
    }

    SDL_Keymod mod = SDL_GetModState();

    switch (key) {
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: case SDLK_6:
        case SDLK_7: case SDLK_8: case SDLK_9: case SDLK_0:
            {
                int tool = key - SDLK_1;
                if (key == SDLK_0) tool = 9;
                if (tool < g_tool_count) {
                    app.current_tool = tool;
                    cancel_drag();
                }
            }
            break;
        case SDLK_ESCAPE:
            if (app.dragging) {
                cancel_drag();
            } else {
                app.running = false;
            }
            break;
        case SDLK_DELETE:
            delete_selected_widget(&g_designer.widget_manager);
            break;
        case SDLK_g:
            // Toggle grid
            g_designer.canvas.grid_enabled = !g_designer.canvas.grid_enabled;
            printf("Grid %s\n", g_designer.canvas.grid_enabled ? "enabled" : "disabled");
            break;
        case SDLK_p:
            // Toggle properties panel
            app.show_properties = !app.show_properties;
            printf("Properties panel %s\n", app.show_properties ? "shown" : "hidden");
            break;
        case SDLK_t:
            designer_cycle_palette_theme();
            app.theme_dropdown_open = false;
            printf("Designer theme: %s\n", designer_palette_theme_name(designer_get_palette_theme()));
            break;
        case SDLK_s:
            if (mod & KMOD_CTRL) {
                start_file_prompt(FILE_PROMPT_SAVE);
            }
            break;
        case SDLK_o:
            if (mod & KMOD_CTRL) {
                start_file_prompt(FILE_PROMPT_LOAD);
            }
            break;
        case SDLK_e:
            if (mod & KMOD_CTRL) {
                start_file_prompt(FILE_PROMPT_EXPORT);
            }
            break;
        case SDLK_i:
            if (mod & KMOD_CTRL) {
                start_bitmap_prompt();
            }
            break;
    }
}

void handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                app.running = false;
                break;
            case SDL_MOUSEBUTTONDOWN:
                handle_mouse_down(event.button.x, event.button.y, event.button.button);
                break;
            case SDL_MOUSEBUTTONUP:
                handle_mouse_up(event.button.x, event.button.y, event.button.button);
                break;
            case SDL_MOUSEMOTION:
                handle_mouse_motion(event.motion.x, event.motion.y);
                break;
            case SDL_KEYDOWN:
                handle_key_down(event.key.keysym.sym);
                break;
            case SDL_TEXTINPUT:
                handle_text_input(event.text.text);
                break;
        }
    }
}

// 3D OpenGL Cube rendering functions
void setup_3d_projection(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Position cube in bottom left corner (OpenGL coordinates: Y=0 is bottom)
    int cube_x = 10;  // 10 pixels from left edge
    int cube_y = 10;  // 10 pixels from bottom edge
    int cube_size = 100; // Reasonable size

    glViewport(cube_x, cube_y, cube_size, cube_size);
    gluPerspective(45.0f, 1.0f, 0.1f, 100.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0.0f, 0.0f, 5.0f,  // Camera position
              0.0f, 0.0f, 0.0f,  // Look at origin
              0.0f, 1.0f, 0.0f); // Up vector
}

void draw_opengl_cube_wireframe(void) {
    // Update animation
    Uint32 current_time = SDL_GetTicks();
    float delta_time = (current_time - app.last_frame_time) / 1000.0f;
    app.last_frame_time = current_time;

    // Rotate at 60 degrees per second on X, 30 degrees per second on Y
    app.cube_rotation_x += 60.0f * delta_time;
    app.cube_rotation_y += 30.0f * delta_time;

    if (app.cube_rotation_x >= 360.0f) app.cube_rotation_x -= 360.0f;
    if (app.cube_rotation_y >= 360.0f) app.cube_rotation_y -= 360.0f;

    // Set up 3D rendering
    setup_3d_projection();

    // Make sure we have the right OpenGL state
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(2.0f);  // Clean 2-pixel lines

    // Set cube color to white (classic wireframe)
    glColor3f(1.0f, 1.0f, 1.0f);  // White wireframe

    // Apply rotations
    glPushMatrix();
    glRotatef(app.cube_rotation_x, 1.0f, 0.0f, 0.0f);
    glRotatef(app.cube_rotation_y, 0.0f, 1.0f, 0.0f);

    // Draw cube wireframe
    glBegin(GL_LINES);

    // Define cube vertices
    float vertices[8][3] = {
        {-1.0f, -1.0f, -1.0f}, // 0
        { 1.0f, -1.0f, -1.0f}, // 1
        { 1.0f,  1.0f, -1.0f}, // 2
        {-1.0f,  1.0f, -1.0f}, // 3
        {-1.0f, -1.0f,  1.0f}, // 4
        { 1.0f, -1.0f,  1.0f}, // 5
        { 1.0f,  1.0f,  1.0f}, // 6
        {-1.0f,  1.0f,  1.0f}  // 7
    };

    // Define cube edges
    int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Bottom face
        {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Top face
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Vertical edges
    };

    // Draw all edges
    for (int i = 0; i < 12; i++) {
        glVertex3fv(vertices[edges[i][0]]);
        glVertex3fv(vertices[edges[i][1]]);
    }

    glEnd();
    glPopMatrix();
}

void restore_2d_projection(void) {
    // Restore full window viewport for 2D rendering
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1); // 2D orthographic
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void render_frame(void) {
    // Update cube animation timing
    Uint32 current_time = SDL_GetTicks();
    float delta_time = (current_time - app.last_frame_time) / 1000.0f;
    app.last_frame_time = current_time;

    // Update cube rotation
    app.cube_rotation_x += 60.0f * delta_time; // 60 degrees per second
    app.cube_rotation_y += 30.0f * delta_time; // 30 degrees per second

    if (app.cube_rotation_x >= 360.0f) app.cube_rotation_x -= 360.0f;
    if (app.cube_rotation_y >= 360.0f) app.cube_rotation_y -= 360.0f;

    // Clear and render 2D GUI to framebuffer (all existing FT2 drawing functions)
    clear_framebuffer(get_palette_color(PAL_BCKGRND));

    draw_toolbar();
    draw_canvas(&g_designer.canvas, app.framebuffer, WINDOW_WIDTH);

    // Render widgets
    render_widgets(&g_designer.widget_manager, app.framebuffer, WINDOW_WIDTH,
                   g_designer.canvas.x, g_designer.canvas.y, app.active_page);

    if (app.dragging) {
        draw_drag_preview();
    }

    if (app.show_properties) {
        draw_property_panel();
    }

    draw_prompt_overlay();

    // Now use OpenGL for everything
    SDL_GL_MakeCurrent(app.window, app.gl_context);

    // Clear OpenGL buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // === RENDER 2D GUI USING OPENGL ===
    // Set up 2D orthographic projection for GUI
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1); // Note: flipped Y for traditional 2D coordinates
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Disable depth testing for 2D rendering
    glDisable(GL_DEPTH_TEST);

    // Create or bind the GUI texture
    if (gui_texture == 0) {
        glGenTextures(1, &gui_texture);
        glBindTexture(GL_TEXTURE_2D, gui_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, gui_texture);
    }

    // Upload the framebuffer as texture data
    // Note: framebuffer is in ARGB8888 format (from SDL), so we use GL_BGRA to match
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, app.framebuffer);

    // Render the GUI as a full-screen textured quad
    glEnable(GL_TEXTURE_2D);
    glColor3f(1.0f, 1.0f, 1.0f); // Ensure white color for proper texture display
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0, 0);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(WINDOW_WIDTH, 0);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0, WINDOW_HEIGHT);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // === RENDER 3D CUBE ===
    // Switch to 3D perspective projection for cube
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(WINDOW_WIDTH - CUBE_VIEWPORT_SIZE - CUBE_VIEWPORT_RIGHT_MARGIN, 10,
               CUBE_VIEWPORT_SIZE, CUBE_VIEWPORT_SIZE);
    gluPerspective(45.0f, 1.0f, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);

    // Position and rotate the cube
    glTranslatef(0.0f, 0.0f, -3.0f);
    glRotatef(app.cube_rotation_x, 1.0f, 0.0f, 0.0f);
    glRotatef(app.cube_rotation_y, 0.0f, 1.0f, 0.0f);

    // Set line width and enable line smoothing
    glLineWidth(2.0f);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Draw wireframe cube in white
    glColor3f(1.0f, 1.0f, 1.0f);

    // Cube vertices
    float cube_vertices[8][3] = {
        {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
        {1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f}, {1.0f, -1.0f,  1.0f},
        {1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
    };

    // Cube edges (pairs of vertex indices)
    int cube_edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0}, // back face
        {4,5}, {5,6}, {6,7}, {7,4}, // front face
        {0,4}, {1,5}, {2,6}, {3,7}  // connecting edges
    };

    glBegin(GL_LINES);
    for (int i = 0; i < 12; i++) {
        int v1 = cube_edges[i][0];
        int v2 = cube_edges[i][1];
        glVertex3f(cube_vertices[v1][0], cube_vertices[v1][1], cube_vertices[v1][2]);
        glVertex3f(cube_vertices[v2][0], cube_vertices[v2][1], cube_vertices[v2][2]);
    }
    glEnd();

    // Reset viewport to full window
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Present the frame
    SDL_GL_SwapWindow(app.window);
}

void clear_framebuffer(uint32_t color) {
    for (int i = 0; i < WINDOW_WIDTH * WINDOW_HEIGHT; i++) {
        app.framebuffer[i] = color;
    }
}

static void draw_designer_button(int x, int y, int w, int h, bool selected)
{
    uint32_t bg_color = get_palette_color(selected ? PAL_BUTTON2 : PAL_BUTTONS);
    uint32_t border1 = get_palette_color(PAL_BUTTON1);
    uint32_t border2 = get_palette_color(PAL_BUTTON2);

    designer_vector_fill_rect(app.framebuffer, WINDOW_WIDTH, (float)x, (float)y, (float)w, (float)h, bg_color);
    h_line(app.framebuffer, WINDOW_WIDTH, x, y, w - 1, border1);
    v_line(app.framebuffer, WINDOW_WIDTH, x, y + 1, h - 2, border1);
    h_line(app.framebuffer, WINDOW_WIDTH, x + 1, y + h - 1, w - 1, border2);
    v_line(app.framebuffer, WINDOW_WIDTH, x + w - 1, y + 1, h - 1, border2);
}

static void draw_dropdown_item(int x, int y, int w, const char *label, bool selected)
{
    draw_designer_button(x, y, w, 16, selected);
    font_draw_text(&g_designer.font_system, x + 5, y + 4, label,
                   get_palette_color(selected ? PAL_FORGRND : PAL_BTNTEXT));
}

void draw_toolbar(void) {
    // Toolbar background
    designer_vector_fill_rect(app.framebuffer, WINDOW_WIDTH, 0.0f, 0.0f, (float)TOOLBAR_WIDTH, (float)WINDOW_HEIGHT,
                              get_palette_color(PAL_DESKTOP));

    // Title
    font_draw_text(&g_designer.font_system, 5, 5, "FT2-DXM GUI Designer", get_palette_color(PAL_FORGRND));
    font_draw_text(&g_designer.font_system, 5, 16, "Dusted Edition", get_palette_color(PAL_FORGRND));

    // Tool section
    font_draw_text(&g_designer.font_system, 5, 35, "Tools:", get_palette_color(PAL_FORGRND));

    // Tool buttons - two columns for more tools
    for (int tool = 0; tool < g_tool_count; tool++) {
        const tool_entry_t *entry = get_tool_entry(tool);
        int x, y, w, h;
        tool_button_rect(tool, &x, &y, &w, &h);

        draw_designer_button(x, y, w, h, app.current_tool == tool);

        // Tool name and shortcut key
        if (entry)
            font_draw_text(&g_designer.font_system, x + 4, y + 4, entry->label, get_palette_color(PAL_BTNTEXT));

        // Shortcut key number
        char key_str[4];
        if (tool >= 0 && tool <= 8) {
            snprintf(key_str, sizeof(key_str), "%d", tool + 1);
            font_draw_text(&g_designer.font_system, x + w - 10, y + 4, key_str, get_palette_color(PAL_FORGRND));
        } else if (tool == 9) {
            snprintf(key_str, sizeof(key_str), "0");
            font_draw_text(&g_designer.font_system, x + w - 10, y + 4, key_str, get_palette_color(PAL_FORGRND));
        }
    }

    // Info section
    int info_y = toolbar_info_y();
    font_draw_text(&g_designer.font_system, 5, info_y, "Controls:", get_palette_color(PAL_FORGRND));
    font_draw_text(&g_designer.font_system, 5, info_y + 12, "G - Toggle Grid", get_palette_color(PAL_BTNTEXT));
    font_draw_text(&g_designer.font_system, 5, info_y + 24, "P - Properties", get_palette_color(PAL_BTNTEXT));
    font_draw_text(&g_designer.font_system, 5, info_y + 36, "Del - Delete", get_palette_color(PAL_BTNTEXT));
    font_draw_text(&g_designer.font_system, 5, info_y + 48, "Ctrl+S - Save", get_palette_color(PAL_BTNTEXT));
    font_draw_text(&g_designer.font_system, 5, info_y + 60, "Ctrl+O - Load", get_palette_color(PAL_BTNTEXT));
    font_draw_text(&g_designer.font_system, 5, info_y + 72, "T - Theme", get_palette_color(PAL_BTNTEXT));

    // Page view selector
    int drop_x, drop_y, drop_w, drop_h;
    char page_buf[16];
    toolbar_page_selector_rect(&drop_x, &drop_y, &drop_w, &drop_h);
    font_draw_text(&g_designer.font_system, drop_x, drop_y - 11, "View Page (0=All):", get_palette_color(PAL_FORGRND));
    int minus_x, plus_x, button_y, button_w, button_h;
    toolbar_page_button_rects(&minus_x, &plus_x, &button_y, &button_w, &button_h);
    draw_designer_button(drop_x, drop_y, minus_x - drop_x - 1, drop_h, false);
    snprintf(page_buf, sizeof(page_buf), "%d", page_selector_value());
    font_draw_text(&g_designer.font_system, drop_x + 5, drop_y + 5,
                   page_buf, get_palette_color(PAL_BTNTEXT));
    draw_designer_button(minus_x, button_y, button_w, button_h, false);
    draw_designer_button(plus_x, button_y, button_w, button_h, false);
    font_draw_text(&g_designer.font_system, minus_x + 7, button_y + 5, "-", get_palette_color(PAL_FORGRND));
    font_draw_text(&g_designer.font_system, plus_x + 7, button_y + 5, "+", get_palette_color(PAL_FORGRND));

    toolbar_theme_dropdown_rect(&drop_x, &drop_y, &drop_w, &drop_h);
    font_draw_text(&g_designer.font_system, drop_x, drop_y - 11, "Theme:", get_palette_color(PAL_FORGRND));
    draw_designer_button(drop_x, drop_y, drop_w, drop_h, app.theme_dropdown_open);
    font_draw_text(&g_designer.font_system, drop_x + 5, drop_y + 5,
                   designer_palette_theme_name(designer_get_palette_theme()), get_palette_color(PAL_BTNTEXT));
    font_draw_text(&g_designer.font_system, drop_x + drop_w - 13, drop_y + 5, "v", get_palette_color(PAL_FORGRND));

    if (app.theme_dropdown_open) {
        int item_y = drop_y + drop_h + 1;
        for (int i = 0; i < DESIGNER_THEME_COUNT; i++, item_y += 16)
            draw_dropdown_item(drop_x, item_y, drop_w, designer_palette_theme_name((designer_palette_theme_t)i),
                               designer_get_palette_theme() == (designer_palette_theme_t)i);
    }

    // Separator
    v_line(app.framebuffer, WINDOW_WIDTH, TOOLBAR_WIDTH, 0, WINDOW_HEIGHT, get_palette_color(PAL_DSKTOP1));
}

void draw_property_panel(void) {
    if (!app.show_properties) return;

    int panel_x = WINDOW_WIDTH - PROPERTY_PANEL_WIDTH;
    int panel_w = PROPERTY_PANEL_WIDTH;
    int panel_h = WINDOW_HEIGHT;

    // Property editing blink timer
    app.edit_blink_timer++;

    // Panel background
    fill_rect(app.framebuffer, WINDOW_WIDTH, panel_x, 0, panel_w, panel_h, get_palette_color(PAL_DESKTOP));

    // Title
    font_draw_text(&g_designer.font_system, panel_x + 5, 10, "Properties", get_palette_color(PAL_FORGRND));

    // Widget count
    char info[64];
    snprintf(info, sizeof(info), "Widgets: %d", g_designer.widget_manager.widget_count);
    font_draw_text(&g_designer.font_system, panel_x + 5, 30, info, get_palette_color(PAL_FORGRND));

    // Selected widget info
    widget_t *widget = get_selected_widget(&g_designer.widget_manager);
    if (widget) {
        const char *type_names[] = {
            "None", "Button", "Radio", "Checkbox", "Scrollbar", "TextBox",
            "FrameBox", "Logo", "CustomButton", "ComboBox", "Dropdown",
            "ListBox", "Slider", "Progress", "Waveform", "TF Button",
            "TF Toggle", "TF Label", "TF Knob", "TF Slider", "TF Combo",
            "TF Meter", "TF Param", "TF Envelope", "TF Group",
            "Mix Strip", "Mix Gain", "Mix Pan", "Mix Mute", "Mix Scope",
            "Mix Master", "DSP Window", "DSP Slot", "DSP Menu", "DSP Param",
            "TF Arp Step"
        };

        // Widget type
        char type_buffer[64];
        if (widget->type < sizeof(type_names)/sizeof(type_names[0])) {
            snprintf(type_buffer, sizeof(type_buffer), "Type: %s", type_names[widget->type]);
        } else {
            snprintf(type_buffer, sizeof(type_buffer), "Type: Unknown (%d)", widget->type);
        }
        font_draw_text(&g_designer.font_system, panel_x + 5, 50, type_buffer, get_palette_color(PAL_FORGRND));

        // Property fields
        int field_y = 80;
        int label_w = 40;
        int value_w = 60;
        int adjust_w = 16;
        int adjust_gap = 2;
        int adjust_x = panel_x + label_w + value_w + 8;

        // Widget name property
        font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Name:", get_palette_color(PAL_FORGRND));
        draw_text_field(panel_x + label_w + 5, field_y, 120, 15, PROP_NAME, widget->name);
        field_y += 25;

        // X property
        font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "X:", get_palette_color(PAL_FORGRND));
        draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_X, widget->x);
        draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
        draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
        field_y += 20;

        // Y property
        font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Y:", get_palette_color(PAL_FORGRND));
        draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_Y, widget->y);
        draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
        draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
        field_y += 20;

        if (widget_allows_resize(widget)) {
            // Width property
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "W:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_WIDTH, widget->w);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;

            // Height property
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "H:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_HEIGHT, widget->h);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;
        }

        if (widget_uses_page(widget)) {
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Page:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_PAGE, (int)widget->page);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;
        }

        if (widget_has_caption(widget)) {
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Caption:", get_palette_color(PAL_FORGRND));
            draw_text_field(panel_x + label_w + 5, field_y, 120, 15, PROP_CAPTION, widget->caption);
            field_y += 20;
        }

        if (widget->type == WIDGET_PUSHBUTTON) {
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Caption2:", get_palette_color(PAL_FORGRND));
            draw_text_field(panel_x + label_w + 5, field_y, 120, 15, PROP_CAPTION2, widget->caption2);
            field_y += 20;
        }

        if (widget_uses_scrollbar_nudge(widget)) {
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Nudge:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_SCROLLBAR_NUDGE,
                                widget->data.scrollbar.has_nudge_buttons ? 1 : 0);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;
        }

        if (widget_uses_font(widget)) {
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Font:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_FONT, (int)widget->font_type + 1);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;
        }

        if (widget_uses_bitmap(widget)) {
            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Bitmap:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_BITMAP, widget->data.logo.bitmap_id);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;

            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Trans:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_BITMAP_TRANSP,
                                designer_bitmap_transparent_index(widget->data.logo.bitmap_id));
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;

            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Layer:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_BITMAP_LAYER,
                                (int)widget->data.logo.layer);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;

            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Flags:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_BITMAP_FLAGS,
                                (int)widget->data.logo.flags);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;

            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Opacity:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_BITMAP_OPACITY,
                                (int)widget->data.logo.opacity);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;

            font_draw_text(&g_designer.font_system, panel_x + 5, field_y + 3, "Skin:", get_palette_color(PAL_FORGRND));
            draw_property_field(panel_x + label_w + 5, field_y, value_w, 15, PROP_SKIN_PART,
                                (int)widget->data.logo.skin_part);
            draw_adjust_button(adjust_x, field_y, adjust_w, 15, "-");
            draw_adjust_button(adjust_x + adjust_w + adjust_gap, field_y, adjust_w, 15, "+");
            field_y += 20;
        }
    }

    // Left border
    v_line(app.framebuffer, WINDOW_WIDTH, panel_x, 0, panel_h, get_palette_color(PAL_DSKTOP1));
}

static void draw_prompt_overlay(void)
{
    if (app.file_prompt_active || app.file_status_active)
        draw_file_prompt();
    if (app.bitmap_prompt_active || app.bitmap_status_active)
        draw_bitmap_prompt();
}

static void draw_file_prompt(void)
{
    if (!app.file_prompt_active && !app.file_status_active)
        return;

    if (app.file_status_active) {
        Uint32 now = SDL_GetTicks();
        if (now - app.file_status_start > 3000) {
            app.file_status_active = false;
            app.file_status_message[0] = '\0';
            return;
        }
    }

    int x = g_designer.canvas.x + 5;
    int y = g_designer.canvas.y + g_designer.canvas.height + 8;

    if (app.file_prompt_active) {
        uint32_t label_color = get_palette_color(PAL_FORGRND);
        uint32_t text_color = get_palette_color(PAL_PATTEXT);

        font_draw_text(&g_designer.font_system, x, y, app.file_prompt_label, label_color);
        font_draw_text(&g_designer.font_system, x + font_get_text_width(app.file_prompt_label), y,
                       app.file_prompt_buffer, text_color);

        if ((SDL_GetTicks() / 500) % 2 == 0) {
            char temp[512];
            size_t len = (size_t)app.file_prompt_cursor;
            if (len >= sizeof(temp))
                len = sizeof(temp) - 1;
            memcpy(temp, app.file_prompt_buffer, len);
            temp[len] = '\0';
            int cursor_x = x + font_get_text_width(app.file_prompt_label) + font_get_text_width(temp);
            v_line(app.framebuffer, WINDOW_WIDTH, cursor_x, y, 10, text_color);
        }

        y += 12;
    }

    if (app.file_status_active) {
        uint32_t color = get_palette_color(app.file_status_success ? PAL_PATTEXT : PAL_TEXTMRK);
        font_draw_text(&g_designer.font_system, x, y, app.file_status_message, color);
    }
}

static void draw_bitmap_prompt(void)
{
    if (!app.bitmap_prompt_active && !app.bitmap_status_active)
        return;

    if (app.bitmap_status_active) {
        Uint32 now = SDL_GetTicks();
        if (now - app.bitmap_status_start > 3000) {
            app.bitmap_status_active = false;
            app.bitmap_status_message[0] = '\0';
            return;
        }
    }

    int x = g_designer.canvas.x + 5;
    int y = g_designer.canvas.y + g_designer.canvas.height + 8;

    if (app.bitmap_prompt_active) {
        const char *label = "Import BMP/PNG: ";
        uint32_t label_color = get_palette_color(PAL_FORGRND);
        uint32_t text_color = get_palette_color(PAL_PATTEXT);

        font_draw_text(&g_designer.font_system, x, y, label, label_color);
        font_draw_text(&g_designer.font_system, x + font_get_text_width(label), y,
                       app.bitmap_prompt_buffer, text_color);

        if ((SDL_GetTicks() / 500) % 2 == 0) {
            char temp[512];
            size_t len = (size_t)app.bitmap_prompt_cursor;
            if (len >= sizeof(temp))
                len = sizeof(temp) - 1;
            memcpy(temp, app.bitmap_prompt_buffer, len);
            temp[len] = '\0';
            int cursor_x = x + font_get_text_width(label) + font_get_text_width(temp);
            v_line(app.framebuffer, WINDOW_WIDTH, cursor_x, y, 10, text_color);
        }

        y += 12;
    }

    if (app.bitmap_status_active) {
        uint32_t color = get_palette_color(app.bitmap_status_success ? PAL_PATTEXT : PAL_TEXTMRK);
        font_draw_text(&g_designer.font_system, x, y, app.bitmap_status_message, color);
    }
}

void draw_property_field(int x, int y, int w, int h, PropField field_type, int value) {
    bool is_editing = (app.property_editing && app.editing_field == field_type);

    uint32_t bg_color = get_palette_color(is_editing ? PAL_FORGRND : PAL_BUTTONS);
    uint32_t text_color = get_palette_color(is_editing ? PAL_BCKGRND : PAL_BTNTEXT);

    // Field background
    fill_rect(app.framebuffer, WINDOW_WIDTH, x, y, w, h, bg_color);

    // Field borders (sunken when editing)
    uint32_t border1 = get_palette_color(is_editing ? PAL_BUTTON2 : PAL_BUTTON1);
    uint32_t border2 = get_palette_color(is_editing ? PAL_BUTTON1 : PAL_BUTTON2);

    h_line(app.framebuffer, WINDOW_WIDTH, x, y, w-1, border1);
    v_line(app.framebuffer, WINDOW_WIDTH, x, y+1, h-2, border1);
    h_line(app.framebuffer, WINDOW_WIDTH, x+1, y+h-1, w-1, border2);
    v_line(app.framebuffer, WINDOW_WIDTH, x+w-1, y+1, h-1, border2);

    // Value text
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%d", value);

    if (is_editing) {
        font_draw_text(&g_designer.font_system, x + 3, y + 3, app.edit_buffer, text_color);

        // Draw cursor
        if ((app.edit_blink_timer / 30) % 2 == 0) {
            int cursor_x = x + 3 + font_get_text_width(app.edit_buffer);
            v_line(app.framebuffer, WINDOW_WIDTH, cursor_x, y + 2, h - 4, text_color);
        }
    } else {
        font_draw_text(&g_designer.font_system, x + 3, y + 3, value_str, text_color);
    }
}

void draw_text_field(int x, int y, int w, int h, PropField field_type, const char *text) {
    bool is_editing = (app.property_editing && app.editing_field == field_type);

    uint32_t bg_color = get_palette_color(is_editing ? PAL_FORGRND : PAL_BUTTONS);
    uint32_t text_color = get_palette_color(is_editing ? PAL_BCKGRND : PAL_BTNTEXT);

    // Field background
    fill_rect(app.framebuffer, WINDOW_WIDTH, x, y, w, h, bg_color);

    // Field borders
    uint32_t border1 = get_palette_color(is_editing ? PAL_BUTTON2 : PAL_BUTTON1);
    uint32_t border2 = get_palette_color(is_editing ? PAL_BUTTON1 : PAL_BUTTON2);

    h_line(app.framebuffer, WINDOW_WIDTH, x, y, w-1, border1);
    v_line(app.framebuffer, WINDOW_WIDTH, x, y+1, h-2, border1);
    h_line(app.framebuffer, WINDOW_WIDTH, x+1, y+h-1, w-1, border2);
    v_line(app.framebuffer, WINDOW_WIDTH, x+w-1, y+1, h-1, border2);

    // Text
    if (is_editing) {
        font_draw_text(&g_designer.font_system, x + 3, y + 3, app.edit_buffer, text_color);

        // Draw cursor
        if ((app.edit_blink_timer / 30) % 2 == 0) {
            int cursor_x = x + 3 + font_get_text_width(app.edit_buffer);
            v_line(app.framebuffer, WINDOW_WIDTH, cursor_x, y + 2, h - 4, text_color);
        }
    } else {
        font_draw_text(&g_designer.font_system, x + 3, y + 3, text, text_color);
    }
}

void draw_adjust_button(int x, int y, int w, int h, const char *text) {
    uint32_t bg_color = get_palette_color(PAL_BUTTONS);
    uint32_t border1 = get_palette_color(PAL_BUTTON1);
    uint32_t border2 = get_palette_color(PAL_BUTTON2);
    uint32_t text_color = get_palette_color(PAL_BTNTEXT);

    fill_rect(app.framebuffer, WINDOW_WIDTH, x, y, w, h, bg_color);

    // Draw 3D border
    h_line(app.framebuffer, WINDOW_WIDTH, x, y, w-1, border1);
    v_line(app.framebuffer, WINDOW_WIDTH, x, y+1, h-2, border1);
    h_line(app.framebuffer, WINDOW_WIDTH, x+1, y+h-1, w-1, border2);
    v_line(app.framebuffer, WINDOW_WIDTH, x+w-1, y+1, h-1, border2);

    // Draw text
    if (text) {
        int text_w = font_get_text_width(text);
        int text_x = x + (w - text_w) / 2;
        int text_y = y + (h - 8) / 2;
        font_draw_text(&g_designer.font_system, text_x, text_y, text, text_color);
    }
}

// Note: draw_2d_cube_overlay function removed - cube now rendered with OpenGL in render_frame

static void print_usage(const char *program)
{
    printf("Usage: %s [design.gui]\n", program);
    printf("       %s --validate design.gui [design.gui ...]\n", program);
}

static int validate_design_files(int file_count, char **files)
{
    int failures = 0;

    for (int i = 0; i < file_count; i++) {
        char resolved[512];
        widget_manager_t manager;
        init_widget_manager(&manager);

        if (!resolve_design_path(files[i], resolved, sizeof(resolved)) ||
            !load_design(&manager, resolved)) {
            fprintf(stderr, "INVALID %s\n", files[i]);
            failures++;
            continue;
        }

        int highest_page = 0;
        for (int widget = 0; widget < manager.widget_count; widget++) {
            if ((int)manager.widgets[widget].page > highest_page)
                highest_page = (int)manager.widgets[widget].page;
        }

        printf("VALID %s: %d widgets, highest page %d\n",
               resolved, manager.widget_count, highest_page);
        cleanup_widget_manager(&manager);
    }

    return failures == 0 ? 0 : 1;
}

int main(int argc, char *argv[]) {
    if (argc >= 2 && strcmp(argv[1], "--validate") == 0) {
        if (argc < 3) {
            print_usage(argv[0]);
            return 2;
        }
        return validate_design_files(argc - 2, &argv[2]);
    }

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 2) {
        print_usage(argv[0]);
        return 2;
    }

    init_sdl();

    if (!init_designer()) {
        printf("Failed to initialize designer\n");
        cleanup_sdl();
        return 1;
    }

    if (argc == 2) {
        if (!load_designer_file(argv[1])) {
            fprintf(stderr, "Failed to load design: %s\n", argv[1]);
            cleanup_designer();
            cleanup_sdl();
            return 1;
        }
        printf("Loaded %s (%d widgets)\n", app.last_load_path,
               g_designer.widget_manager.widget_count);
    }

    app.running = true;

    while (app.running) {
        handle_events();
        render_frame();
        SDL_Delay(16); // ~60 FPS
    }

    cleanup_designer();
    cleanup_sdl();
    return 0;
}
