#ifndef CAMERA_H
#define CAMERA_H

#include "types.h"
#include "math.h"

// Camera configuration for 3D rendering
typedef struct {
    // Camera position and orientation
    Vec3f position;         // Camera world position
    Vec3f target;           // Point the camera is looking at
    Vec3f up;               // Camera's up vector

    // Perspective projection parameters
    f32 field_of_view;      // FOV in radians
    f32 aspect_ratio;       // Width / Height
    f32 near_plane;         // Near clipping plane
    f32 far_plane;          // Far clipping plane

    // Cached matrices for performance
    Mat4f view_matrix;      // Camera's view transformation
    Mat4f projection_matrix; // Perspective projection
    Mat4f view_projection;  // Combined view * projection

    b32 matrices_dirty;     // True if matrices need recalculation
} Camera;

// Initialize camera with default values for cube viewing
static inline Camera camera_create_default(void) {
    Camera cam = {
        .position = vec3f_create(f32_from_native(0.0f), f32_from_native(0.0f), f32_from_native(8.0f)),  // Camera back 8 units
        .target = vec3f_create(f32_from_native(0.0f), f32_from_native(0.0f), f32_from_native(0.0f)),    // Looking at origin
        .up = vec3f_create(f32_from_native(0.0f), f32_from_native(1.0f), f32_from_native(0.0f)),        // Y-axis up

        .field_of_view = f32_from_native(3.14159f / 4.0f),  // 45 degrees in radians
        .aspect_ratio = f32_from_native(800.0f / 600.0f),   // 4:3 aspect ratio
        .near_plane = f32_from_native(0.1f),                // Near clipping plane
        .far_plane = f32_from_native(100.0f),               // Far clipping plane

        .view_matrix = {.cols = {{F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                         {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                         {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                         {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO}}},
        .projection_matrix = {.cols = {{F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                       {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                       {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                       {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO}}},
        .view_projection = {.cols = {{F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                     {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                     {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO},
                                     {F32_ZERO, F32_ZERO, F32_ZERO, F32_ZERO}}},

        .matrices_dirty = B32_TRUE                          // Need initial computation
    };
    return cam;
}

// Update camera matrices when parameters change
static inline void camera_update_matrices(Camera* camera) {
    if (!camera || !camera->matrices_dirty) {
        return;
    }

    // Compute view matrix (world to camera transform)
    camera->view_matrix = mat4f_look_at(camera->position, camera->target, camera->up);

    // Compute projection matrix (camera to clip space transform)
    camera->projection_matrix = mat4f_perspective(
        camera->field_of_view,
        camera->aspect_ratio,
        camera->near_plane,
        camera->far_plane
    );

    // Compute combined view-projection matrix
    camera->view_projection = mat4f_multiply(&camera->projection_matrix, &camera->view_matrix);

    camera->matrices_dirty = B32_FALSE;
}

// Set camera position (marks matrices as dirty)
static inline void camera_set_position(Camera* camera, Vec3f position) {
    if (!camera) return;
    camera->position = position;
    camera->matrices_dirty = B32_TRUE;
}

// Set camera target (marks matrices as dirty)
static inline void camera_set_target(Camera* camera, Vec3f target) {
    if (!camera) return;
    camera->target = target;
    camera->matrices_dirty = B32_TRUE;
}

// Set camera aspect ratio (marks matrices as dirty)
static inline void camera_set_aspect_ratio(Camera* camera, f32 aspect_ratio) {
    if (!camera) return;
    camera->aspect_ratio = aspect_ratio;
    camera->matrices_dirty = B32_TRUE;
}

// Get camera matrices for rendering (automatically updates if dirty)
static inline const Mat4f* camera_get_view_matrix(Camera* camera) {
    if (!camera) return NULL;
    camera_update_matrices(camera);
    return &camera->view_matrix;
}

static inline const Mat4f* camera_get_projection_matrix(Camera* camera) {
    if (!camera) return NULL;
    camera_update_matrices(camera);
    return &camera->projection_matrix;
}

static inline const Mat4f* camera_get_view_projection_matrix(Camera* camera) {
    if (!camera) return NULL;
    camera_update_matrices(camera);
    return &camera->view_projection;
}

#endif // CAMERA_H