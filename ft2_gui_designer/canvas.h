#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int x, y;        // Position of canvas
    int width, height; // Canvas dimensions
    bool grid_enabled; // Show grid
    int grid_size;     // Grid spacing
} canvas_t;

void init_canvas(canvas_t *canvas, int x, int y, int width, int height);
void draw_canvas(canvas_t *canvas, uint32_t *framebuffer, int fb_width);
bool point_in_canvas(canvas_t *canvas, int x, int y);
void canvas_to_world(canvas_t *canvas, int canvas_x, int canvas_y, int *world_x, int *world_y);
void world_to_canvas(canvas_t *canvas, int world_x, int world_y, int *canvas_x, int *canvas_y); 