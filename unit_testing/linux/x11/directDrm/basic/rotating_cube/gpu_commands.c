#define _GNU_SOURCE  // Enable usleep and other POSIX extensions
#include "gpu_device.h"
#include "math.h"          // For Vec3f, Vec4f, Mat4f structures
#include "gibgo_graphics.h" // For GibgoVertex structure
#include "uniform_buffer.h" // For UniformBuffer structure
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>  // For cosf/sinf functions

// Debug logging macros
#define GPU_LOG(device, fmt, ...) \
    do { \
        if ((device) && (device)->debug_enabled) { \
            printf("[GPU] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

#define GPU_ERROR(fmt, ...) \
    fprintf(stderr, "[GPU ERROR] " fmt "\n", ##__VA_ARGS__)

// GPU command types (simplified command set)
typedef enum {
    GPU_CMD_NOP = 0x00,
    GPU_CMD_SET_VIEWPORT = 0x01,
    GPU_CMD_SET_VERTEX_BUFFER = 0x02,
    GPU_CMD_SET_VERTEX_SHADER = 0x03,
    GPU_CMD_SET_FRAGMENT_SHADER = 0x04,
    GPU_CMD_SET_UNIFORM_BUFFER = 0x05,       // New: Set uniform buffer for shaders
    GPU_CMD_ENABLE_DEPTH_TEST = 0x06,        // New: Enable hardware depth testing
    GPU_CMD_ENABLE_FACE_CULLING = 0x07,      // New: Enable back-face culling
    GPU_CMD_CLEAR_FRAMEBUFFER = 0x08,        // Renumbered to accommodate new commands
    GPU_CMD_DRAW_PRIMITIVES = 0x09,          // Renumbered
    GPU_CMD_PRESENT_FRAME = 0x0A,            // Renumbered
    GPU_CMD_FENCE = 0x0B                     // Renumbered
} GibgoGPUCommandType;

// GPU command structure (32-bit aligned)
typedef struct {
    u32 command_type;       // GibgoGPUCommandType
    u32 param0;             // Command-specific parameter 0
    u32 param1;             // Command-specific parameter 1
    u32 param2;             // Command-specific parameter 2
} GibgoGPUCommand;

// Current command buffer state
static GibgoGPUCommand* current_commands = NULL;
static u32 current_command_count = 0;
static u32 max_commands = 0;

// Forward declarations for 3D rendering system
static GibgoResult execute_commands_software(GibgoGPUDevice* device, GibgoGPUCommand* commands, u32 command_count);
static void execute_3d_cube_rendering(GibgoGPUDevice* device, GibgoGPUCommand* commands, u32 command_count);
static void render_3d_cube_software(GibgoGPUDevice* device, u32* framebuffer, u32 width, u32 height,
                                   u64 vertex_buffer_addr, u64 uniform_buffer_addr,
                                   u32 vertex_count, u32 first_vertex, b32 face_culling_enabled);

// Helper function to add a command to the current command buffer
static GibgoResult add_command(GibgoGPUCommandType type, u32 param0, u32 param1, u32 param2) {
    if (!current_commands) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    if (current_command_count >= max_commands) {
        GPU_ERROR("Command buffer overflow - too many commands");
        return GIBGO_RESULT_ERROR_COMMAND_FAILED;
    }

    GibgoGPUCommand* cmd = &current_commands[current_command_count++];
    cmd->command_type = type;
    cmd->param0 = param0;
    cmd->param1 = param1;
    cmd->param2 = param2;

    return GIBGO_RESULT_SUCCESS;
}

// Helper function to submit commands to hardware
static GibgoResult submit_commands_to_hardware(GibgoGPUDevice* device, GibgoGPUCommand* commands, u32 command_count) {
    if (!device || !commands) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(device, "Submitting %u commands to GPU hardware", command_count);

    // FIRST: Execute commands using software 3D rasterizer
    GibgoResult execution_result = execute_commands_software(device, commands, command_count);
    if (execution_result != GIBGO_RESULT_SUCCESS) {
        GPU_ERROR("Failed to execute commands using software rasterizer");
        return execution_result;
    }

    // THEN: Submit to hardware (for compatibility/logging)
    // Get command processor registers
    volatile u32* cmd_regs = &device->regs.registers[device->regs.command_processor_offset / sizeof(u32)];

    // Write commands to the hardware ring buffer
    for (u32 i = 0; i < command_count; i++) {
        GibgoGPUCommand* cmd = &commands[i];

        // Check if ring buffer has space
        u32 next_tail = (device->cmd_ring.tail_offset + 1) % device->cmd_ring.capacity;
        if (next_tail == device->cmd_ring.head_offset) {
            // Ring buffer full - wait for GPU to consume commands
            GPU_LOG(device, "Command ring buffer full, waiting for GPU...");

            u32 timeout = 1000000; // 1 second timeout
            while (next_tail == device->cmd_ring.head_offset && timeout > 0) {
                // Read GPU head pointer from hardware
                device->cmd_ring.head_offset = cmd_regs[0x04]; // GPU_HEAD_PTR register
                usleep(1);
                timeout--;
            }

            if (timeout == 0) {
                GPU_ERROR("GPU command submission timeout");
                return GIBGO_RESULT_ERROR_GPU_TIMEOUT;
            }
        }

        // Write command to ring buffer
        u32 ring_index = device->cmd_ring.tail_offset;
        device->cmd_ring.command_buffer[ring_index * 4 + 0] = cmd->command_type;
        device->cmd_ring.command_buffer[ring_index * 4 + 1] = cmd->param0;
        device->cmd_ring.command_buffer[ring_index * 4 + 2] = cmd->param1;
        device->cmd_ring.command_buffer[ring_index * 4 + 3] = cmd->param2;

        // Update tail pointer
        device->cmd_ring.tail_offset = next_tail;

        GPU_LOG(device, "Command %u: type=0x%02X, params=(0x%08X, 0x%08X, 0x%08X)",
                i, cmd->command_type, cmd->param0, cmd->param1, cmd->param2);
    }

    // Notify GPU hardware of new commands
    cmd_regs[0x08] = device->cmd_ring.tail_offset; // GPU_TAIL_PTR register

    // Trigger command processor execution
    cmd_regs[0x00] = 0x00000001; // GPU_COMMAND_START register

    device->commands_submitted += command_count;
    return GIBGO_RESULT_SUCCESS;
}

// Begin command recording
GibgoResult gibgo_begin_commands(GibgoContext* context) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Allocate temporary command buffer
    max_commands = 256; // Maximum commands per frame
    current_commands = (GibgoGPUCommand*)malloc(max_commands * sizeof(GibgoGPUCommand));
    if (!current_commands) {
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    current_command_count = 0;
    GPU_LOG(context->device, "Beginning command recording");

    return GIBGO_RESULT_SUCCESS;
}

// End command recording
GibgoResult gibgo_end_commands(GibgoContext* context) {
    if (!context || !current_commands) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(context->device, "Ending command recording - %u commands recorded", current_command_count);
    return GIBGO_RESULT_SUCCESS;
}

// Submit recorded commands to GPU
GibgoResult gibgo_submit_commands(GibgoContext* context) {
    if (!context || !current_commands) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Submit commands to hardware
    GibgoResult result = submit_commands_to_hardware(context->device, current_commands, current_command_count);

    // Clean up temporary command buffer
    free(current_commands);
    current_commands = NULL;
    current_command_count = 0;
    max_commands = 0;

    return result;
}

// Wait for GPU operations to complete
GibgoResult gibgo_wait_for_completion(GibgoContext* context, u32 fence_value) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(context->device, "Waiting for fence %u", fence_value);

    u32 timeout = 1000000; // 1 second timeout
    while (*context->device->fence_register < fence_value && timeout > 0) {
        usleep(1);
        timeout--;
    }

    if (timeout == 0) {
        GPU_ERROR("GPU fence timeout - fence value %u not reached (current: %u)",
                  fence_value, *context->device->fence_register);
        return GIBGO_RESULT_ERROR_GPU_TIMEOUT;
    }

    GPU_LOG(context->device, "Fence %u completed", fence_value);
    return GIBGO_RESULT_SUCCESS;
}

// Set viewport for rendering
GibgoResult gibgo_set_viewport(GibgoContext* context, u32 width, u32 height) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    context->framebuffer_width = width;
    context->framebuffer_height = height;

    return add_command(GPU_CMD_SET_VIEWPORT, width, height, 0);
}

// Load SPIR-V shaders
GibgoResult gibgo_load_shaders(GibgoContext* context, u32* vertex_spirv, u32 vertex_size,
                              u32* fragment_spirv, u32 fragment_size) {
    if (!context || !vertex_spirv || !fragment_spirv) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GibgoResult result;

    // Allocate GPU memory for vertex shader
    result = gibgo_allocate_gpu_memory(context->device, vertex_size, &context->vertex_shader_address);
    if (result != GIBGO_RESULT_SUCCESS) {
        return result;
    }

    // Allocate GPU memory for fragment shader
    result = gibgo_allocate_gpu_memory(context->device, fragment_size, &context->fragment_shader_address);
    if (result != GIBGO_RESULT_SUCCESS) {
        return result;
    }

    // Upload vertex shader to GPU
    u8* vertex_mapped = NULL;
    result = gibgo_map_gpu_memory(context->device, context->vertex_shader_address, vertex_size, &vertex_mapped);
    if (result == GIBGO_RESULT_SUCCESS) {
        memcpy(vertex_mapped, vertex_spirv, vertex_size);
        gibgo_unmap_gpu_memory(context->device, vertex_mapped, vertex_size);
    }

    // Upload fragment shader to GPU
    u8* fragment_mapped = NULL;
    result = gibgo_map_gpu_memory(context->device, context->fragment_shader_address, fragment_size, &fragment_mapped);
    if (result == GIBGO_RESULT_SUCCESS) {
        memcpy(fragment_mapped, fragment_spirv, fragment_size);
        gibgo_unmap_gpu_memory(context->device, fragment_mapped, fragment_size);
    }

    GPU_LOG(context->device, "Loaded shaders - VS: 0x%016lX (%u bytes), FS: 0x%016lX (%u bytes)",
            context->vertex_shader_address, vertex_size,
            context->fragment_shader_address, fragment_size);

    // Add shader setup commands
    add_command(GPU_CMD_SET_VERTEX_SHADER,
                (u32)(context->vertex_shader_address & 0xFFFFFFFF),
                (u32)(context->vertex_shader_address >> 32),
                vertex_size);

    add_command(GPU_CMD_SET_FRAGMENT_SHADER,
                (u32)(context->fragment_shader_address & 0xFFFFFFFF),
                (u32)(context->fragment_shader_address >> 32),
                fragment_size);

    return GIBGO_RESULT_SUCCESS;
}

// Draw primitives
GibgoResult gibgo_draw_primitives_internal(GibgoContext* context, u32 vertex_count, u32 first_vertex) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Add vertex buffer command
    add_command(GPU_CMD_SET_VERTEX_BUFFER,
                (u32)(context->vertex_buffer_address & 0xFFFFFFFF),
                (u32)(context->vertex_buffer_address >> 32),
                context->vertex_buffer_stride);

    // Add clear framebuffer command (black background)
    add_command(GPU_CMD_CLEAR_FRAMEBUFFER, 0x00000000, 0, 0); // RGBA(0,0,0,1)

    // Add draw command
    add_command(GPU_CMD_DRAW_PRIMITIVES, vertex_count, first_vertex, 0);

    GPU_LOG(context->device, "Drawing %u primitives starting from vertex %u", vertex_count, first_vertex);

    return GIBGO_RESULT_SUCCESS;
}

// Present frame to display
GibgoResult gibgo_present_frame(GibgoContext* context) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Add present command
    add_command(GPU_CMD_PRESENT_FRAME,
                (u32)(context->framebuffer_address & 0xFFFFFFFF),
                (u32)(context->framebuffer_address >> 32),
                context->framebuffer_format);

    // Add fence command for synchronization
    context->frame_fence = ++context->device->fence_counter;
    add_command(GPU_CMD_FENCE, context->frame_fence, 0, 0);

    context->current_frame_index++;
    context->device->frames_rendered++;

    GPU_LOG(context->device, "Present frame %u (fence: %u)",
            context->current_frame_index, context->frame_fence);

    return GIBGO_RESULT_SUCCESS;
}

// Set uniform buffer for shaders (MVP matrix, camera, time, etc.)
GibgoResult gibgo_set_uniform_buffer(GibgoContext* context, u64 buffer_address, u32 buffer_size) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    context->uniform_buffer_address = buffer_address;
    context->uniform_buffer_size = buffer_size;

    GPU_LOG(context->device, "Setting uniform buffer - Address: 0x%016lX, Size: %u bytes",
            buffer_address, buffer_size);

    // Add uniform buffer command
    return add_command(GPU_CMD_SET_UNIFORM_BUFFER,
                      (u32)(buffer_address & 0xFFFFFFFF),
                      (u32)(buffer_address >> 32),
                      buffer_size);
}

// Enable depth testing for 3D rendering
GibgoResult gibgo_enable_depth_test(GibgoContext* context, b32 enable, f32 near_plane, f32 far_plane) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    context->depth_test_enabled = enable;
    context->depth_near = near_plane;
    context->depth_far = far_plane;

    GPU_LOG(context->device, "Depth test %s - Near: %f, Far: %f",
            enable ? "enabled" : "disabled", f32_to_native(near_plane), f32_to_native(far_plane));

    // Add depth test command
    u32 near_bits = near_plane.bits;
    u32 far_bits = far_plane.bits;
    return add_command(GPU_CMD_ENABLE_DEPTH_TEST, enable ? 1 : 0, near_bits, far_bits);
}

// Enable face culling for 3D rendering (hide back-facing triangles)
GibgoResult gibgo_enable_face_culling(GibgoContext* context, b32 enable) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(context->device, "Face culling %s", enable ? "enabled" : "disabled");

    // Add face culling command
    return add_command(GPU_CMD_ENABLE_FACE_CULLING, enable ? 1 : 0, 0, 0);
}

// Execute GPU commands using software rasterization before hardware submission
static GibgoResult execute_commands_software(GibgoGPUDevice* device, GibgoGPUCommand* commands, u32 command_count) {
    if (!device || !commands) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(device, "Executing %u commands using software 3D rasterizer", command_count);

    // For our 3D cube demo, execute the commands using our custom software rasterizer
    execute_3d_cube_rendering(device, commands, command_count);

    return GIBGO_RESULT_SUCCESS;
}

// 3D Cube Software Rasterizer - Executes recorded GPU commands to render 3D cube
static void execute_3d_cube_rendering(GibgoGPUDevice* device, GibgoGPUCommand* commands, u32 command_count) {
    if (!device || !commands) {
        return;
    }

    // Current rendering state (extracted from commands)
    u64 vertex_buffer_address = 0;
    u64 uniform_buffer_address = 0;
    u32 vertex_count = 0;
    u32 first_vertex = 0;
    u32* framebuffer = (u32*)device->regs.registers;
    u32 fb_width = 800, fb_height = 600; // TODO: get from context
    b32 face_culling_enabled = B32_TRUE;  // Enable face culling by default

    GPU_LOG(device, "🎮 Executing 3D cube rendering with %u commands", command_count);

    // Parse commands to extract rendering state
    for (u32 i = 0; i < command_count; i++) {
        GibgoGPUCommand* cmd = &commands[i];

        switch (cmd->command_type) {
            case GPU_CMD_SET_UNIFORM_BUFFER:
                uniform_buffer_address = ((u64)cmd->param1 << 32) | cmd->param0;
                GPU_LOG(device, "  📦 Uniform buffer: 0x%016lX", uniform_buffer_address);
                break;

            case GPU_CMD_SET_VERTEX_BUFFER:
                vertex_buffer_address = ((u64)cmd->param1 << 32) | cmd->param0;
                GPU_LOG(device, "  🎯 Vertex buffer: 0x%016lX", vertex_buffer_address);
                break;

            case GPU_CMD_DRAW_PRIMITIVES:
                vertex_count = cmd->param0;
                first_vertex = cmd->param1;
                GPU_LOG(device, "  ✏️  Drawing %u vertices starting from %u", vertex_count, first_vertex);
                break;

            case GPU_CMD_ENABLE_FACE_CULLING:
                face_culling_enabled = cmd->param0 ? B32_TRUE : B32_FALSE;
                GPU_LOG(device, "  ✂️ Face culling: %s", face_culling_enabled ? "enabled" : "disabled");
                break;

            case GPU_CMD_CLEAR_FRAMEBUFFER:
                // Clear framebuffer to background color
                for (u32 j = 0; j < fb_width * fb_height; j++) {
                    framebuffer[j] = 0xFF111111; // Dark background
                }
                GPU_LOG(device, "  🧽 Cleared framebuffer to dark background");
                break;

            default:
                // Other commands (viewport, shaders, etc.) are noted but not processed
                break;
        }
    }

    // If we have all the data needed for rendering, perform 3D rasterization
    if (vertex_buffer_address && vertex_count > 0) {
        render_3d_cube_software(device, framebuffer, fb_width, fb_height,
                                vertex_buffer_address, uniform_buffer_address,
                                vertex_count, first_vertex, face_culling_enabled);
    } else {
        GPU_LOG(device, "  ⚠️  Missing vertex data for 3D rendering");
    }
}

// ==============================================================================
// 3D GRAPHICS PIPELINE - Full vertex transformation and rasterization
// ==============================================================================

// Missing math utility functions for the custom math library
#define F32_HALF        ((f32){.bits = 0x3F000000})  // 0.5

static inline f32 f32_from_u32(u32 value) {
    return f32_from_native((float)value);
}

static inline f32 f32_from_i32(i32 value) {
    return f32_from_native((float)value);
}

static inline f32 f32_from_float(float value) {
    return f32_from_native(value);
}

static inline f32 f32_min(f32 a, f32 b) {
    return f32_lt(a, b) ? a : b;
}

static inline f32 f32_max(f32 a, f32 b) {
    return f32_gt(a, b) ? a : b;
}

static inline f32 f32_clamp(f32 value, f32 min_val, f32 max_val) {
    return f32_min(f32_max(value, min_val), max_val);
}

static inline bool f32_le(f32 a, f32 b) {
    return f32_lt(a, b) || f32_eq(a, b);
}

static inline bool f32_ge(f32 a, f32 b) {
    return f32_gt(a, b) || f32_eq(a, b);
}

// Structure to hold a transformed vertex with screen coordinates
typedef struct {
    Vec3f position;      // Screen coordinates (x, y) + depth (z)
    Vec3f color;        // Interpolated color
    b32 is_valid;       // False if vertex was clipped
} TransformedVertex;

// Transform a 3D vertex through the complete graphics pipeline
// Input: 3D world space vertex, MVP matrix, screen dimensions
// Output: 2D screen coordinates + depth
static TransformedVertex transform_vertex_to_screen(
    const GibgoVertex* vertex,
    const Mat4f* mvp,
    u32 screen_width, u32 screen_height)
{
    TransformedVertex result = {0};

    // Step 1: Convert Vec3f to Vec4f (add w=1 for proper matrix multiplication)
    Vec4f vertex_pos = vec4f_create(vertex->position.x, vertex->position.y, vertex->position.z, F32_ONE);

    // Step 2: Transform to clip space using MVP matrix
    Vec4f clip_space = mat4f_mul_vec4f(mvp, vertex_pos);

    // Debug: Log clip space coordinates for first few vertices
    static u32 debug_count = 0;
    if (debug_count < 6) {
        printf("[CLIP DEBUG] Vertex %d: world[%.3f, %.3f, %.3f] → clip[%.3f, %.3f, %.3f, %.3f]\n",
               debug_count,
               f32_to_native(vertex->position.x), f32_to_native(vertex->position.y), f32_to_native(vertex->position.z),
               f32_to_native(clip_space.x), f32_to_native(clip_space.y), f32_to_native(clip_space.z), f32_to_native(clip_space.w));
        debug_count++;
    }

    // Step 3: Check if vertex is within clip volume (basic clipping)
    f32 w_abs = f32_abs(clip_space.w);

    // Debug the clipping test components
    if (debug_count <= 6) {
        printf("[CLIP DEBUG] w_abs=%.3f, |x|=%.3f, |y|=%.3f, |z|=%.3f\n",
               f32_to_native(w_abs),
               f32_to_native(f32_abs(clip_space.x)),
               f32_to_native(f32_abs(clip_space.y)),
               f32_to_native(f32_abs(clip_space.z)));
        printf("[CLIP DEBUG] Tests: w>0? %d, [CLIPPING DISABLED - ALL VALID]\n",
               f32_gt(w_abs, F32_ZERO));
    }

    // Step 3: Frustum clipping - test if vertex is inside view frustum
    // TEMPORARY: Only test X/Y planes to isolate the Z-clipping issue
    // Standard OpenGL clipping: vertex is inside if -w <= x,y <= w AND w > 0
    b32 inside_frustum = f32_gt(w_abs, F32_ZERO) &&
                         f32_le(f32_abs(clip_space.x), w_abs) &&
                         f32_le(f32_abs(clip_space.y), w_abs);
    // TODO: Fix Z-clipping - currently disabled to test X/Y only

    // Debug: Log clipping results for first few vertices
    if (debug_count <= 6) {
        printf("[CLIP DEBUG] Vertex %d frustum test: w>0? %d, |x|<=w? %d, |y|<=w? %d, [Z-TEST DISABLED] → %s\n",
               debug_count-1,
               f32_gt(w_abs, F32_ZERO),
               f32_le(f32_abs(clip_space.x), w_abs),
               f32_le(f32_abs(clip_space.y), w_abs),
               inside_frustum ? "INSIDE" : "CLIPPED");
    }

    if (inside_frustum) {
        // Step 4: Perspective divide (clip space -> NDC)
        f32 w_inv = f32_div(F32_ONE, clip_space.w);
        f32 ndc_x = f32_mul(clip_space.x, w_inv);
        f32 ndc_y = f32_mul(clip_space.y, w_inv);
        f32 ndc_z = f32_mul(clip_space.z, w_inv);

        // Step 5: Viewport transform (NDC -> screen coordinates)
        // NDC is in [-1, 1] range, convert to [0, width] and [0, height]
        f32 screen_x = f32_mul(f32_add(f32_mul(ndc_x, F32_HALF), F32_HALF), f32_from_u32(screen_width));
        f32 screen_y = f32_mul(f32_sub(F32_ONE, f32_add(f32_mul(ndc_y, F32_HALF), F32_HALF)), f32_from_u32(screen_height));

        // Store results
        result.position = vec3f_create(screen_x, screen_y, ndc_z);  // Keep NDC z for depth testing
        result.color = vertex->color;
        result.is_valid = B32_TRUE;  // Vertex passed all clipping tests

    } else {
        // Vertex is outside view frustum (clipped)
        result.is_valid = B32_FALSE;
    }

    return result;
}

// Check if a point is inside a triangle using barycentric coordinates
static b32 point_in_triangle_barycentric(f32 px, f32 py, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3,
                                         f32* out_u, f32* out_v, f32* out_w)
{
    // Calculate triangle edge vectors
    f32 v0x = f32_sub(x3, x1);  // C - A
    f32 v0y = f32_sub(y3, y1);
    f32 v1x = f32_sub(x2, x1);  // B - A
    f32 v1y = f32_sub(y2, y1);
    f32 v2x = f32_sub(px, x1);  // P - A
    f32 v2y = f32_sub(py, y1);

    // Calculate dot products
    f32 dot00 = f32_add(f32_mul(v0x, v0x), f32_mul(v0y, v0y));
    f32 dot01 = f32_add(f32_mul(v0x, v1x), f32_mul(v0y, v1y));
    f32 dot02 = f32_add(f32_mul(v0x, v2x), f32_mul(v0y, v2y));
    f32 dot11 = f32_add(f32_mul(v1x, v1x), f32_mul(v1y, v1y));
    f32 dot12 = f32_add(f32_mul(v1x, v2x), f32_mul(v1y, v2y));

    // Calculate barycentric coordinates
    f32 denom = f32_sub(f32_mul(dot00, dot11), f32_mul(dot01, dot01));
    if (f32_le(f32_abs(denom), f32_from_float(0.000001f))) {
        return B32_FALSE;  // Degenerate triangle
    }

    f32 inv_denom = f32_div(F32_ONE, denom);
    f32 u = f32_mul(f32_sub(f32_mul(dot11, dot02), f32_mul(dot01, dot12)), inv_denom);
    f32 v = f32_mul(f32_sub(f32_mul(dot00, dot12), f32_mul(dot01, dot02)), inv_denom);
    f32 w = f32_sub(F32_ONE, f32_add(u, v));

    // Check if point is in triangle
    b32 inside = (f32_ge(u, F32_ZERO) && f32_ge(v, F32_ZERO) && f32_ge(w, F32_ZERO));

    if (inside) {
        *out_u = u;
        *out_v = v;
        *out_w = w;
    }

    return inside;
}

// Interpolate color across triangle surface using barycentric coordinates
static Vec3f interpolate_triangle_color(const Vec3f* c1, const Vec3f* c2, const Vec3f* c3,
                                       f32 u, f32 v, f32 w)
{
    // Color = u*c3 + v*c2 + w*c1 (note: u,v,w correspond to vertices 3,2,1 respectively)
    Vec3f result;
    result.x = f32_add(f32_add(f32_mul(u, c3->x), f32_mul(v, c2->x)), f32_mul(w, c1->x));
    result.y = f32_add(f32_add(f32_mul(u, c3->y), f32_mul(v, c2->y)), f32_mul(w, c1->y));
    result.z = f32_add(f32_add(f32_mul(u, c3->z), f32_mul(v, c2->z)), f32_mul(w, c1->z));
    return result;
}

// Rasterize a single triangle with color interpolation
static void rasterize_triangle(
    u32* framebuffer, u32 width, u32 height,
    const TransformedVertex* v1, const TransformedVertex* v2, const TransformedVertex* v3)
{
    // Calculate triangle bounding box for efficient rasterization
    f32 min_x = f32_min(f32_min(v1->position.x, v2->position.x), v3->position.x);
    f32 max_x = f32_max(f32_max(v1->position.x, v2->position.x), v3->position.x);
    f32 min_y = f32_min(f32_min(v1->position.y, v2->position.y), v3->position.y);
    f32 max_y = f32_max(f32_max(v1->position.y, v2->position.y), v3->position.y);

    // Convert to integer pixel bounds
    i32 start_x = (i32)f32_to_native(f32_max(min_x, F32_ZERO));
    i32 end_x   = (i32)f32_to_native(f32_min(max_x, f32_from_u32(width - 1)));
    i32 start_y = (i32)f32_to_native(f32_max(min_y, F32_ZERO));
    i32 end_y   = (i32)f32_to_native(f32_min(max_y, f32_from_u32(height - 1)));

    // Rasterize triangle by testing each pixel in bounding box
    for (i32 y = start_y; y <= end_y; y++) {
        for (i32 x = start_x; x <= end_x; x++) {
            f32 pixel_x = f32_from_i32(x);
            f32 pixel_y = f32_from_i32(y);

            f32 u, v, w;
            if (point_in_triangle_barycentric(pixel_x, pixel_y,
                                            v1->position.x, v1->position.y,
                                            v2->position.x, v2->position.y,
                                            v3->position.x, v3->position.y,
                                            &u, &v, &w)) {

                // Interpolate color
                Vec3f pixel_color = interpolate_triangle_color(&v1->color, &v2->color, &v3->color, u, v, w);

                // Convert color to RGB format (0xAARRGGBB)
                u32 r = (u32)(f32_to_native(f32_clamp(pixel_color.x, F32_ZERO, F32_ONE)) * 255.0f);
                u32 g = (u32)(f32_to_native(f32_clamp(pixel_color.y, F32_ZERO, F32_ONE)) * 255.0f);
                u32 b = (u32)(f32_to_native(f32_clamp(pixel_color.z, F32_ZERO, F32_ONE)) * 255.0f);
                u32 final_color = 0xFF000000 | (r << 16) | (g << 8) | b;

                // Write pixel to framebuffer
                u32 pixel_index = y * width + x;
                if (pixel_index < width * height) {
                    framebuffer[pixel_index] = final_color;
                }
            }
        }
    }
}

// Check if triangle is back-facing (for face culling)
// Returns true if triangle is back-facing (should be culled)
static b32 is_triangle_back_facing(const TransformedVertex* v1, const TransformedVertex* v2, const TransformedVertex* v3) {
    // Calculate triangle normal in screen space using cross product
    f32 edge1_x = f32_sub(v2->position.x, v1->position.x);
    f32 edge1_y = f32_sub(v2->position.y, v1->position.y);
    f32 edge2_x = f32_sub(v3->position.x, v1->position.x);
    f32 edge2_y = f32_sub(v3->position.y, v1->position.y);

    // 2D cross product gives us the Z component of the normal
    // Positive Z = counter-clockwise (front-facing), negative Z = clockwise (back-facing)
    f32 cross_product_z = f32_sub(f32_mul(edge1_x, edge2_y), f32_mul(edge1_y, edge2_x));

    // If cross product Z is negative, triangle is back-facing (clockwise winding)
    return f32_lt(cross_product_z, F32_ZERO);
}

// Core 3D software rasterizer implementation
// Note: This is a simplified educational implementation - in reality, this would be done by GPU hardware
static void render_3d_cube_software(GibgoGPUDevice* device, u32* framebuffer, u32 width, u32 height,
                                   u64 vertex_buffer_addr, u64 uniform_buffer_addr,
                                   u32 vertex_count, u32 first_vertex, b32 face_culling_enabled) {

    GPU_LOG(device, "🎨 Rendering 3D cube: %u vertices from %u, buffers at 0x%lX, 0x%lX",
            vertex_count, first_vertex, vertex_buffer_addr, uniform_buffer_addr);

    // Step 1: Load vertex data from GPU memory
    u8* vertex_memory = NULL;
    u64 vertex_buffer_size = vertex_count * sizeof(GibgoVertex);
    GibgoResult result = gibgo_map_gpu_memory(device, vertex_buffer_addr, vertex_buffer_size, &vertex_memory);

    if (result != GIBGO_RESULT_SUCCESS || !vertex_memory) {
        GPU_LOG(device, "❌ Failed to map vertex buffer memory");
        return;
    }

    GibgoVertex* vertices = (GibgoVertex*)vertex_memory;

    // Step 2: Load uniform buffer (MVP matrix) from GPU memory
    u8* uniform_memory = NULL;
    u64 uniform_buffer_size = sizeof(GibgoUniformBuffer);
    result = gibgo_map_gpu_memory(device, uniform_buffer_addr, uniform_buffer_size, &uniform_memory);

    if (result != GIBGO_RESULT_SUCCESS || !uniform_memory) {
        GPU_LOG(device, "❌ Failed to map uniform buffer memory");
        gibgo_unmap_gpu_memory(device, vertex_memory, vertex_buffer_size);
        return;
    }

    GibgoUniformBuffer* uniforms = (GibgoUniformBuffer*)uniform_memory;
    Mat4f* mvp = &uniforms->mvp_matrix;

    // Debug: Log MVP matrix for verification
    GPU_LOG(device, "🔍 MVP Matrix Debug:");
    GPU_LOG(device, "  Col 0: [%.3f, %.3f, %.3f, %.3f]",
            f32_to_native(mvp->cols[0].x), f32_to_native(mvp->cols[0].y),
            f32_to_native(mvp->cols[0].z), f32_to_native(mvp->cols[0].w));
    GPU_LOG(device, "  Col 1: [%.3f, %.3f, %.3f, %.3f]",
            f32_to_native(mvp->cols[1].x), f32_to_native(mvp->cols[1].y),
            f32_to_native(mvp->cols[1].z), f32_to_native(mvp->cols[1].w));
    GPU_LOG(device, "  Col 2: [%.3f, %.3f, %.3f, %.3f]",
            f32_to_native(mvp->cols[2].x), f32_to_native(mvp->cols[2].y),
            f32_to_native(mvp->cols[2].z), f32_to_native(mvp->cols[2].w));
    GPU_LOG(device, "  Col 3: [%.3f, %.3f, %.3f, %.3f]",
            f32_to_native(mvp->cols[3].x), f32_to_native(mvp->cols[3].y),
            f32_to_native(mvp->cols[3].z), f32_to_native(mvp->cols[3].w));

    GPU_LOG(device, "📊 Processing %u vertices as %u triangles with real 3D pipeline",
            vertex_count, vertex_count / 3);

    // Step 3: Process triangles (every 3 vertices = 1 triangle)
    u32 triangles_rendered = 0;
    u32 triangles_clipped = 0;

    for (u32 i = first_vertex; i < vertex_count; i += 3) {
        // Ensure we have 3 vertices for a complete triangle
        if (i + 2 >= vertex_count) break;

        // Get triangle vertices
        const GibgoVertex* v1 = &vertices[i];
        const GibgoVertex* v2 = &vertices[i + 1];
        const GibgoVertex* v3 = &vertices[i + 2];

        // Step 4: Transform vertices through 3D pipeline
        TransformedVertex tv1 = transform_vertex_to_screen(v1, mvp, width, height);
        TransformedVertex tv2 = transform_vertex_to_screen(v2, mvp, width, height);
        TransformedVertex tv3 = transform_vertex_to_screen(v3, mvp, width, height);

        // Debug: Log first triangle transformation details
        if (triangles_rendered == 0 && triangles_clipped == 0) {
            GPU_LOG(device, "🔬 First Triangle Debug:");
            GPU_LOG(device, "  V1 world: [%.3f, %.3f, %.3f] → valid: %d",
                    f32_to_native(v1->position.x), f32_to_native(v1->position.y), f32_to_native(v1->position.z), tv1.is_valid);
            GPU_LOG(device, "  V2 world: [%.3f, %.3f, %.3f] → valid: %d",
                    f32_to_native(v2->position.x), f32_to_native(v2->position.y), f32_to_native(v2->position.z), tv2.is_valid);
            GPU_LOG(device, "  V3 world: [%.3f, %.3f, %.3f] → valid: %d",
                    f32_to_native(v3->position.x), f32_to_native(v3->position.y), f32_to_native(v3->position.z), tv3.is_valid);
            if (tv1.is_valid) {
                GPU_LOG(device, "  V1 screen: [%.1f, %.1f] depth: %.3f",
                        f32_to_native(tv1.position.x), f32_to_native(tv1.position.y), f32_to_native(tv1.position.z));
            }
        }

        // Step 5: Skip triangles with clipped vertices (basic culling)
        if (!tv1.is_valid || !tv2.is_valid || !tv3.is_valid) {
            triangles_clipped++;
            continue;
        }

        // Step 6: Face culling - skip back-facing triangles if enabled
        if (face_culling_enabled && is_triangle_back_facing(&tv1, &tv2, &tv3)) {
            triangles_clipped++;
            continue;
        }

        // Step 7: Rasterize the triangle
        rasterize_triangle(framebuffer, width, height, &tv1, &tv2, &tv3);
        triangles_rendered++;
    }

    GPU_LOG(device, "✅ 3D cube rendered successfully: %u triangles rendered, %u clipped",
            triangles_rendered, triangles_clipped);

    // Clean up GPU memory mappings
    gibgo_unmap_gpu_memory(device, vertex_memory, vertex_buffer_size);
    gibgo_unmap_gpu_memory(device, uniform_memory, uniform_buffer_size);
}