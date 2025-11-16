#include "cube_geometry.h"

// Generate a triangle list from the cube indices
// Converts indexed geometry into a flat vertex array for immediate mode rendering
void cube_generate_triangle_list(GibgoVertex* out_vertices, u32* out_vertex_count) {
    if (out_vertices == NULL || out_vertex_count == NULL) {
        return;
    }

    *out_vertex_count = CUBE_INDEX_COUNT;  // 36 vertices (12 triangles * 3 vertices each)

    // Expand indexed triangles into vertex list
    for (u32 i = 0; i < CUBE_INDEX_COUNT; i++) {
        u32 vertex_index = cube_indices[i];
        out_vertices[i] = cube_vertices[vertex_index];
    }
}