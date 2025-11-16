#ifndef ANIMATION_H
#define ANIMATION_H

#include "types.h"
#include "math.h"
#include <time.h>  // For clock timing

// Animation system for time-based rotation
typedef struct {
    f32 start_time;         // Animation start time (seconds)
    f32 current_time;       // Current animation time (seconds)
    f32 rotation_speed;     // Radians per second for Y-axis rotation
    f32 current_angle;      // Current rotation angle in radians

    b32 is_running;         // Is animation currently active
} CubeAnimation;

// Get current time in seconds (high precision)
static inline f32 animation_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return f32_from_native((float)ts.tv_sec + (float)ts.tv_nsec / 1e9f);
}

// Initialize animation with rotation speed
static inline CubeAnimation animation_create(f32 rotations_per_second) {
    f32 current_time = animation_get_time();

    CubeAnimation anim = {
        .start_time = current_time,
        .current_time = current_time,
        .rotation_speed = f32_mul(rotations_per_second, f32_from_native(2.0f * 3.14159f)), // Convert to radians/sec
        .current_angle = F32_ZERO,
        .is_running = B32_TRUE
    };
    return anim;
}

// Update animation time and compute current rotation angle
static inline void animation_update(CubeAnimation* anim) {
    if (!anim || !anim->is_running) {
        return;
    }

    // Update current time
    anim->current_time = animation_get_time();

    // Compute elapsed time since start
    f32 elapsed_time = f32_sub(anim->current_time, anim->start_time);

    // Compute current rotation angle (wrap at 2π for numerical stability)
    anim->current_angle = f32_mul(anim->rotation_speed, elapsed_time);

    // Wrap angle to [0, 2π] range
    float angle_native = f32_to_native(anim->current_angle);
    float two_pi = 2.0f * 3.14159f;
    while (angle_native >= two_pi) {
        angle_native -= two_pi;
    }
    while (angle_native < 0.0f) {
        angle_native += two_pi;
    }
    anim->current_angle = f32_from_native(angle_native);
}

// Get current rotation matrix for the cube
static inline Mat4f animation_get_rotation_matrix(CubeAnimation* anim) {
    if (!anim) {
        return mat4f_identity();
    }

    return mat4f_rotate_y(anim->current_angle);
}

// Start/stop animation
static inline void animation_start(CubeAnimation* anim) {
    if (!anim) return;
    anim->is_running = B32_TRUE;
    anim->start_time = animation_get_time();
}

static inline void animation_stop(CubeAnimation* anim) {
    if (!anim) return;
    anim->is_running = B32_FALSE;
}

// Set rotation speed (rotations per second)
static inline void animation_set_speed(CubeAnimation* anim, f32 rotations_per_second) {
    if (!anim) return;
    anim->rotation_speed = f32_mul(rotations_per_second, f32_from_native(2.0f * 3.14159f));
}

#endif // ANIMATION_H