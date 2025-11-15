#ifndef GIBGO_GRAPHICS_H
#define GIBGO_GRAPHICS_H

#include "types.h"
#include "math.h"

// Forward declarations
typedef struct GibgoGraphicsSystem GibgoGraphicsSystem;

// Simplified graphics API result codes
typedef enum {
    GIBGO_SUCCESS = 0,
    GIBGO_ERROR_INITIALIZATION_FAILED,
    GIBGO_ERROR_DEVICE_LOST,
    GIBGO_ERROR_OUT_OF_MEMORY,
    GIBGO_ERROR_INVALID_PARAMETER
} GibgoGraphicsResult;

// 2D Vertex structure for backwards compatibility
typedef struct {
    Vec2f position;
    Vec3f color;
} GibgoVertex;

// 3D Vertex structure for cube rendering
typedef struct {
    Vec3f position;    // 3D position
    Vec3f color;       // RGB color
} GibgoCubeVertex;

// Transformation matrices structure for 3D rendering
typedef struct {
    Mat4f model;       // 64 bytes: model transformation matrix
    Mat4f view;        // 64 bytes: view/camera transformation matrix
    Mat4f projection;  // 64 bytes: perspective projection matrix
} TransformMatrices;

// Simplified graphics initialization parameters
typedef struct {
    u32 window_width;
    u32 window_height;
    void* x11_display;      // Display* from X11
    u64 x11_window;         // Window from X11
    b32 enable_debug;       // Enable debug logging
} GibgoGraphicsInitInfo;

// Main graphics system structure
struct GibgoGraphicsSystem {
    // Implementation details hidden from user
    void* internal_device;      // GibgoGPUDevice*
    void* internal_context;     // GibgoContext*

    // Public state
    u32 frame_width;
    u32 frame_height;
    u32 current_frame;
    b32 is_initialized;
};

// API Functions - Direct Vulkan Replacements

// Initialization (replaces Vulkan instance/device creation)
GibgoGraphicsResult gibgo_initialize_graphics(const GibgoGraphicsInitInfo* init_info,
                                             GibgoGraphicsSystem** out_system);
GibgoGraphicsResult gibgo_shutdown_graphics(GibgoGraphicsSystem* system);

// Shader management (replaces Vulkan shader modules)
GibgoGraphicsResult gibgo_create_shaders_from_spirv(GibgoGraphicsSystem* system,
                                                   const u32* vertex_spirv_data, u32 vertex_spirv_size,
                                                   const u32* fragment_spirv_data, u32 fragment_spirv_size);

// Vertex buffer management (replaces Vulkan buffer operations)
GibgoGraphicsResult gibgo_upload_vertex_data(GibgoGraphicsSystem* system,
                                            const GibgoVertex* vertices, u32 vertex_count);

// Rendering operations (replaces Vulkan command buffer recording)
GibgoGraphicsResult gibgo_begin_frame(GibgoGraphicsSystem* system);
GibgoGraphicsResult gibgo_draw_triangle(GibgoGraphicsSystem* system);
GibgoGraphicsResult gibgo_end_frame_and_present(GibgoGraphicsSystem* system);

// 3D Cube rendering operations
GibgoGraphicsResult gibgo_upload_cube_vertices(GibgoGraphicsSystem* system,
                                              const GibgoCubeVertex* vertices, u32 vertex_count);
GibgoGraphicsResult gibgo_set_mvp_matrices(GibgoGraphicsSystem* system,
                                          const Mat4f* model, const Mat4f* view, const Mat4f* projection);
GibgoGraphicsResult gibgo_draw_indexed_cube(GibgoGraphicsSystem* system,
                                           const u16* indices, u32 index_count);
GibgoGraphicsResult gibgo_enable_depth_testing(GibgoGraphicsSystem* system, b32 enable);
GibgoGraphicsResult gibgo_clear_depth_buffer_3d(GibgoGraphicsSystem* system);

// Synchronization (replaces Vulkan fences/semaphores)
GibgoGraphicsResult gibgo_wait_for_frame_completion(GibgoGraphicsSystem* system);

// Utility functions
const char* gibgo_graphics_result_string(GibgoGraphicsResult result);
void gibgo_debug_print_system_info(const GibgoGraphicsSystem* system);

// Advanced functions for learning/debugging
GibgoGraphicsResult gibgo_debug_dump_gpu_state(GibgoGraphicsSystem* system);
GibgoGraphicsResult gibgo_get_frame_statistics(GibgoGraphicsSystem* system,
                                              u64* frames_rendered, u64* commands_submitted);

#endif // GIBGO_GRAPHICS_H