#include "uniform_buffer.h"

// Initialize uniform buffer with identity matrices
void uniform_buffer_initialize(GibgoUniformBuffer* buffer) {
    if (!buffer) {
        return;
    }

    // Initialize all matrices to identity
    buffer->model_matrix = mat4f_identity();
    buffer->view_matrix = mat4f_identity();
    buffer->projection_matrix = mat4f_identity();
    buffer->mvp_matrix = mat4f_identity();

    // Initialize camera at origin looking down negative Z
    buffer->camera_position = vec3f_create(F32_ZERO, F32_ZERO, F32_ZERO);
    buffer->time = F32_ZERO;
}

// Update transformation matrices and recompute MVP
void uniform_buffer_update_matrices(GibgoUniformBuffer* buffer,
                                   const Mat4f* model,
                                   const Mat4f* view,
                                   const Mat4f* projection) {
    if (!buffer || !model || !view || !projection) {
        return;
    }

    // Store individual matrices
    buffer->model_matrix = *model;
    buffer->view_matrix = *view;
    buffer->projection_matrix = *projection;

    // Compute combined MVP matrix: MVP = Projection * View * Model
    Mat4f temp = mat4f_multiply(view, model);          // View * Model
    buffer->mvp_matrix = mat4f_multiply(projection, &temp);  // Projection * (View * Model)
}

// Update animation time parameter
void uniform_buffer_set_time(GibgoUniformBuffer* buffer, f32 time) {
    if (!buffer) {
        return;
    }
    buffer->time = time;
}

// Update camera world position
void uniform_buffer_set_camera_position(GibgoUniformBuffer* buffer, const Vec3f* position) {
    if (!buffer || !position) {
        return;
    }
    buffer->camera_position = *position;
}