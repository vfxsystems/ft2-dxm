#include "canvas.h"
#include "palette.h"

void init_canvas(canvas_t *canvas, int x, int y, int width, int height)
{
    canvas->x = x;
    canvas->y = y;
    canvas->width = width;
    canvas->height = height;
    canvas->grid_enabled = true;
    canvas->grid_size = 8; // 8-pixel grid like FT2
}

void draw_canvas(canvas_t *canvas, uint32_t *framebuffer, int fb_width)
{
    // Draw canvas border (sunken framework)
    draw_framework(framebuffer, fb_width, canvas->x - 2, canvas->y - 2, 
                   canvas->width + 4, canvas->height + 4, FRAMEWORK_TYPE2);
    
    // Fill canvas background
    fill_rect(framebuffer, fb_width, canvas->x, canvas->y,
              canvas->width, canvas->height, get_palette_color(PAL_DESKTOP));
    
    // Draw grid if enabled
    if (canvas->grid_enabled) {
        uint32_t grid_color = get_palette_color(PAL_DSKTOP2);
        
        // Vertical grid lines
        for (int x = canvas->grid_size; x < canvas->width; x += canvas->grid_size) {
            v_line(framebuffer, fb_width, canvas->x + x, canvas->y, 
                   canvas->height, grid_color);
        }
        
        // Horizontal grid lines
        for (int y = canvas->grid_size; y < canvas->height; y += canvas->grid_size) {
            h_line(framebuffer, fb_width, canvas->x, canvas->y + y, 
                   canvas->width, grid_color);
        }
    }
}

bool point_in_canvas(canvas_t *canvas, int x, int y)
{
    return (x >= canvas->x && x < canvas->x + canvas->width &&
            y >= canvas->y && y < canvas->y + canvas->height);
}

void canvas_to_world(canvas_t *canvas, int canvas_x, int canvas_y, int *world_x, int *world_y)
{
    *world_x = canvas_x - canvas->x;
    *world_y = canvas_y - canvas->y;
}

void world_to_canvas(canvas_t *canvas, int world_x, int world_y, int *canvas_x, int *canvas_y)
{
    *canvas_x = world_x + canvas->x;
    *canvas_y = world_y + canvas->y;
} 
