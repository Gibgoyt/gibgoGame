#ifndef CUBE_GEOMETRY_H
#define CUBE_GEOMETRY_H

#include "types.h"
#include "math.h"
#include "gibgo_graphics.h"

// Cube geometry constants
#define CUBE_VERTEX_COUNT 8
#define CUBE_TRIANGLE_COUNT 12
#define CUBE_INDEX_COUNT (CUBE_TRIANGLE_COUNT * 3)  // 36 indices

// Define cube vertices in 3D space
// Cube centered at origin, extends from -1 to +1 in each axis
static const GibgoVertex cube_vertices[CUBE_VERTEX_COUNT] = {
    // Front face vertices (z = +1.0)
    { .position = {{.bits = 0xBF800000}, {.bits = 0xBF800000}, {.bits = 0x3F800000}}, .color = {{.bits = 0x3F800000}, {.bits = 0x00000000}, {.bits = 0x00000000}} }, // Bottom-left:  (-1, -1, +1) Red
    { .position = {{.bits = 0x3F800000}, {.bits = 0xBF800000}, {.bits = 0x3F800000}}, .color = {{.bits = 0x00000000}, {.bits = 0x3F800000}, {.bits = 0x00000000}} }, // Bottom-right: (+1, -1, +1) Green
    { .position = {{.bits = 0x3F800000}, {.bits = 0x3F800000}, {.bits = 0x3F800000}}, .color = {{.bits = 0x00000000}, {.bits = 0x00000000}, {.bits = 0x3F800000}} }, // Top-right:    (+1, +1, +1) Blue
    { .position = {{.bits = 0xBF800000}, {.bits = 0x3F800000}, {.bits = 0x3F800000}}, .color = {{.bits = 0x3F800000}, {.bits = 0x3F800000}, {.bits = 0x00000000}} }, // Top-left:     (-1, +1, +1) Yellow

    // Back face vertices (z = -1.0)
    { .position = {{.bits = 0xBF800000}, {.bits = 0xBF800000}, {.bits = 0xBF800000}}, .color = {{.bits = 0x3F800000}, {.bits = 0x00000000}, {.bits = 0x3F800000}} }, // Bottom-left:  (-1, -1, -1) Magenta
    { .position = {{.bits = 0x3F800000}, {.bits = 0xBF800000}, {.bits = 0xBF800000}}, .color = {{.bits = 0x00000000}, {.bits = 0x3F800000}, {.bits = 0x3F800000}} }, // Bottom-right: (+1, -1, -1) Cyan
    { .position = {{.bits = 0x3F800000}, {.bits = 0x3F800000}, {.bits = 0xBF800000}}, .color = {{.bits = 0x3F800000}, {.bits = 0x3F800000}, {.bits = 0x3F800000}} }, // Top-right:    (+1, +1, -1) White
    { .position = {{.bits = 0xBF800000}, {.bits = 0x3F800000}, {.bits = 0xBF800000}}, .color = {{.bits = 0x00000000}, {.bits = 0x00000000}, {.bits = 0x00000000}} }  // Top-left:     (-1, +1, -1) Black
};

// Define indices for 12 triangles (6 faces, 2 triangles per face)
// Consistent COUNTER-CLOCKWISE winding for ALL faces when viewed from OUTSIDE the cube
static const u32 cube_indices[CUBE_INDEX_COUNT] = {
    // Front face (z = +1) - looking at cube from positive Z
    0, 1, 2,    0, 2, 3,

    // Back face (z = -1) - looking at cube from negative Z (so reverse winding)
    5, 4, 7,    5, 7, 6,

    // Left face (x = -1) - looking at cube from negative X (so reverse winding)
    4, 0, 3,    4, 3, 7,

    // Right face (x = +1) - looking at cube from positive X
    1, 5, 6,    1, 6, 2,

    // Bottom face (y = -1) - looking at cube from negative Y (so reverse winding)
    4, 5, 1,    4, 1, 0,

    // Top face (y = +1) - looking at cube from positive Y
    3, 2, 6,    3, 6, 7
};

// Helper function to get cube vertices for rendering
static inline const GibgoVertex* get_cube_vertices(void) {
    return cube_vertices;
}

// Helper function to get cube indices
static inline const u32* get_cube_indices(void) {
    return cube_indices;
}

// Function to generate a triangle list from the cube (for immediate mode rendering)
// Expands indexed triangles into a flat array of vertices
void cube_generate_triangle_list(GibgoVertex* out_vertices, u32* out_vertex_count);

#endif // CUBE_GEOMETRY_H