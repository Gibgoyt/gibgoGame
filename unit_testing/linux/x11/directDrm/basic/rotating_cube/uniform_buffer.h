#ifndef UNIFORM_BUFFER_H
#define UNIFORM_BUFFER_H

#include "types.h"
#include "math.h"
#include <stddef.h>  // For offsetof macro

// Uniform buffer object for 3D transformations
// This structure will be uploaded to GPU memory and accessible to shaders
typedef struct {
    Mat4f model_matrix;         // Model transformation (rotation, translation, scale)
    Mat4f view_matrix;          // View transformation (camera position and orientation)
    Mat4f projection_matrix;    // Projection transformation (perspective or orthographic)
    Mat4f mvp_matrix;           // Pre-computed Model-View-Projection matrix
    Vec3f camera_position;      // Camera world position (for lighting calculations)
    f32   time;                 // Animation time parameter
} GIBGO_ALIGN_16 GibgoUniformBuffer;

// Verify alignment is correct for GPU hardware
_Static_assert(sizeof(GibgoUniformBuffer) % 16 == 0, "GibgoUniformBuffer must be 16-byte aligned for GPU");
_Static_assert(offsetof(GibgoUniformBuffer, mvp_matrix) % 16 == 0, "mvp_matrix must be 16-byte aligned");

// Helper functions for managing uniform buffer data
void uniform_buffer_initialize(GibgoUniformBuffer* buffer);
void uniform_buffer_update_matrices(GibgoUniformBuffer* buffer,
                                   const Mat4f* model,
                                   const Mat4f* view,
                                   const Mat4f* projection);
void uniform_buffer_set_time(GibgoUniformBuffer* buffer, f32 time);
void uniform_buffer_set_camera_position(GibgoUniformBuffer* buffer, const Vec3f* position);

#endif // UNIFORM_BUFFER_H