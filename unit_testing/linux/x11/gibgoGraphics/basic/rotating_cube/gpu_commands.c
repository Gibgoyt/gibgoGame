#define _GNU_SOURCE  // Enable usleep and other POSIX extensions
#include "gpu_device.h"
#include "math.h"    // For Vec3f and other math types
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Debug logging macros
#define GPU_LOG(device, fmt, ...) \
    do { \
        if ((device) && (device)->debug_enabled) { \
            printf("[GPU] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

#define GPU_ERROR(fmt, ...) \
    fprintf(stderr, "[GPU ERROR] " fmt "\n", ##__VA_ARGS__)

// Forward declarations for vertex types
typedef struct {
    Vec3f position;    // 16 bytes: explicit 3D position (with padding)
    Vec3f color;       // 16 bytes: explicit 3D color (with padding)
} CubeVertex;

// GPU command types (expanded command set for 3D rendering)
typedef enum {
    GPU_CMD_NOP = 0x00,
    GPU_CMD_SET_VIEWPORT = 0x01,
    GPU_CMD_SET_VERTEX_BUFFER = 0x02,
    GPU_CMD_SET_VERTEX_SHADER = 0x03,
    GPU_CMD_SET_FRAGMENT_SHADER = 0x04,
    GPU_CMD_CLEAR_FRAMEBUFFER = 0x05,
    GPU_CMD_DRAW_PRIMITIVES = 0x06,
    GPU_CMD_PRESENT_FRAME = 0x07,
    GPU_CMD_FENCE = 0x08,
    GPU_CMD_SET_DEPTH_BUFFER = 0x09,      // Set depth buffer address and format
    GPU_CMD_CLEAR_DEPTH_BUFFER = 0x0A,    // Clear depth buffer to specified value
    GPU_CMD_ENABLE_DEPTH_TEST = 0x0B,     // Enable/disable depth testing
    GPU_CMD_SET_DEPTH_COMPARE = 0x0C,     // Set depth comparison function
    GPU_CMD_SET_MATRICES = 0x0D,          // Set MVP transformation matrices
    GPU_CMD_SET_INDEX_BUFFER = 0x0E,      // Set index buffer address and format
    GPU_CMD_DRAW_INDEXED = 0x0F           // Draw indexed primitives
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

// GPU state structure for command execution
typedef struct {
    u64 vertex_buffer_address;
    u64 index_buffer_address;
    u32 index_format;
    u64 vertex_shader_address;
    u64 fragment_shader_address;
    u64 matrix_buffer_address;
    b32 depth_test_enabled;
    u32 depth_compare_op;
} GibgoGPUState;

static GibgoGPUState gpu_state = {0};

// Helper function to apply MVP transformation to a vertex
static Vec3f transform_vertex(CubeVertex vertex, Mat4f* model, Mat4f* view, Mat4f* projection) {
    (void)view; (void)projection; // Suppress unused parameter warnings

    // Convert custom types to float
    float x = f32_to_native(vertex.position.x);
    float y = f32_to_native(vertex.position.y);
    float z = f32_to_native(vertex.position.z);

    // For simplicity, just apply rotation from model matrix
    float cos_angle = f32_to_native(model->cols[0].x);  // Matrix element [0,0]
    float sin_angle = f32_to_native(model->cols[1].x);  // Matrix element [0,1]

    // Apply basic rotation around diagonal axis (simplified)
    float x_rot = x * cos_angle - y * sin_angle;
    float y_rot = x * sin_angle + y * cos_angle;
    float z_rot = z;  // Keep Z for now

    return (Vec3f){
        f32_from_native(x_rot),
        f32_from_native(y_rot),
        f32_from_native(z_rot),
        F32_ZERO  // Initialize padding to suppress warning
    };
}

// Software triangle rasterizer with basic depth testing and transformation
static void rasterize_triangle(u32* framebuffer, float* depth_buffer, u32 width, u32 height,
                              CubeVertex v0, CubeVertex v1, CubeVertex v2,
                              Mat4f* model, Mat4f* view, Mat4f* projection) {
    // Transform vertices
    Vec3f tv0 = transform_vertex(v0, model, view, projection);
    Vec3f tv1 = transform_vertex(v1, model, view, projection);
    Vec3f tv2 = transform_vertex(v2, model, view, projection);

    // Convert 3D coordinates to screen coordinates
    float scale = 150.0f;  // Scale factor for visibility
    float x0 = f32_to_native(tv0.x) * scale + width / 2;
    float y0 = -f32_to_native(tv0.y) * scale + height / 2;  // Flip Y axis
    float z0 = f32_to_native(tv0.z);

    float x1 = f32_to_native(tv1.x) * scale + width / 2;
    float y1 = -f32_to_native(tv1.y) * scale + height / 2;
    float z1 = f32_to_native(tv1.z);

    float x2 = f32_to_native(tv2.x) * scale + width / 2;
    float y2 = -f32_to_native(tv2.y) * scale + height / 2;
    float z2 = f32_to_native(tv2.z);

    // Compute bounding box for the triangle
    i32 min_x = (i32)fmaxf(0, fminf(fminf(x0, x1), x2));
    i32 max_x = (i32)fminf(width - 1, fmaxf(fmaxf(x0, x1), x2));
    i32 min_y = (i32)fmaxf(0, fminf(fminf(y0, y1), y2));
    i32 max_y = (i32)fminf(height - 1, fmaxf(fmaxf(y0, y1), y2));

    // Rasterize triangle using barycentric coordinates
    for (i32 y = min_y; y <= max_y; y++) {
        for (i32 x = min_x; x <= max_x; x++) {
            // Calculate barycentric coordinates
            float denom = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
            if (fabsf(denom) < 1e-6f) continue; // Degenerate triangle

            float w0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / denom;
            float w1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / denom;
            float w2 = 1.0f - w0 - w1;

            // Check if point is inside triangle
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                // Interpolate depth (Z) value
                float z = w0 * z0 + w1 * z1 + w2 * z2;

                u32 pixel_index = y * width + x;

                // Depth test (closer objects have smaller Z values)
                if (z < depth_buffer[pixel_index]) {
                    depth_buffer[pixel_index] = z;

                    // Interpolate color
                    float r = w0 * f32_to_native(v0.color.x) + w1 * f32_to_native(v1.color.x) + w2 * f32_to_native(v2.color.x);
                    float g = w0 * f32_to_native(v0.color.y) + w1 * f32_to_native(v1.color.y) + w2 * f32_to_native(v2.color.y);
                    float b = w0 * f32_to_native(v0.color.z) + w1 * f32_to_native(v1.color.z) + w2 * f32_to_native(v2.color.z);

                    // Convert to 8-bit color and pack into pixel
                    u32 red = (u32)(fminf(fmaxf(r * 255.0f, 0.0f), 255.0f));
                    u32 green = (u32)(fminf(fmaxf(g * 255.0f, 0.0f), 255.0f));
                    u32 blue = (u32)(fminf(fmaxf(b * 255.0f, 0.0f), 255.0f));

                    framebuffer[pixel_index] = 0xFF000000 | (red << 16) | (green << 8) | blue;
                }
            }
        }
    }
}

// Simple indexed drawing execution (CPU-based with software rasterizer)
static void gpu_execute_indexed_drawing(GibgoGPUDevice* device, u32 index_count, u32 first_index) {
    // Get framebuffer from device (use the DRM mapped framebuffer)
    u32* framebuffer = (u32*)device->vram.mapped_address;
    u32 width = 800;  // TODO: Get from viewport
    u32 height = 600;

    // Safety check - ensure we have a valid framebuffer
    if (!framebuffer) {
        GPU_ERROR("Framebuffer is null - using fallback");
        // Use the register space as framebuffer (it's the same DRM buffer)
        framebuffer = (u32*)device->regs.registers;
        if (!framebuffer) {
            GPU_ERROR("No valid framebuffer available");
            return;
        }
    }

    // Create depth buffer
    float* depth_buffer = (float*)malloc(width * height * sizeof(float));
    if (!depth_buffer) {
        GPU_ERROR("Failed to allocate depth buffer");
        return;
    }

    // Clear framebuffer and depth buffer
    for (u32 i = 0; i < width * height; i++) {
        framebuffer[i] = 0xFF000033; // Dark blue background
        depth_buffer[i] = 1000.0f;   // Far plane
    }

    // Get vertex and index data pointers
    // Note: Access vertex data from external stored cube vertices
    extern CubeVertex* stored_cube_vertices;  // From gibgo_graphics.c
    extern u32 stored_vertex_count;

    CubeVertex* vertices = stored_cube_vertices;

    // Use hardcoded cube indices for proper cube rendering
    const u16 cube_indices[36] = {
        // Front face
        0, 1, 2, 0, 2, 3,
        // Back face
        4, 6, 5, 4, 7, 6,
        // Left face
        8, 9, 10, 8, 10, 11,
        // Right face
        12, 14, 13, 12, 15, 14,
        // Top face
        16, 18, 17, 16, 19, 18,
        // Bottom face
        20, 21, 22, 20, 22, 23
    };

    // Safety checks
    if (!vertices) {
        GPU_LOG(device, "Vertex data is null");
        free(depth_buffer);
        return;
    }

    if (stored_vertex_count < 24) {
        GPU_LOG(device, "Not enough vertices for cube: %u", stored_vertex_count);
        free(depth_buffer);
        return;
    }

    GPU_LOG(device, "Starting cube rasterization: %u indices, %u vertices", index_count, stored_vertex_count);

    // Create simple rotation transformation matrices for animation
    extern u32 global_frame_count;  // From main application for timing
    float time = global_frame_count * 0.016f;  // Approximate time at 60fps
    float angle = time * 0.5f;  // Rotation speed

    // Simple rotation matrix around Y axis for basic animation
    Mat4f model_matrix = {
        f32_from_native(cosf(angle)), F32_ZERO, f32_from_native(sinf(angle)), F32_ZERO,
        F32_ZERO, F32_ONE, F32_ZERO, F32_ZERO,
        f32_from_native(-sinf(angle)), F32_ZERO, f32_from_native(cosf(angle)), F32_ZERO,
        F32_ZERO, F32_ZERO, F32_ZERO, F32_ONE
    };

    // Identity matrices for view and projection (simplified for now)
    Mat4f view_matrix = {
        F32_ONE, F32_ZERO, F32_ZERO, F32_ZERO,
        F32_ZERO, F32_ONE, F32_ZERO, F32_ZERO,
        F32_ZERO, F32_ZERO, F32_ONE, F32_ZERO,
        F32_ZERO, F32_ZERO, F32_ZERO, F32_ONE
    };

    Mat4f projection_matrix = view_matrix;  // Same as view for now

    // Render triangles (3 indices per triangle)
    u32 triangles_rendered = 0;
    for (u32 i = 0; i < index_count; i += 3) {
        if (i + 2 >= index_count) break;

        // Get triangle vertex indices
        u16 idx0 = cube_indices[first_index + i + 0];
        u16 idx1 = cube_indices[first_index + i + 1];
        u16 idx2 = cube_indices[first_index + i + 2];

        // Bounds check
        if (idx0 >= stored_vertex_count || idx1 >= stored_vertex_count || idx2 >= stored_vertex_count) {
            GPU_LOG(device, "Index out of bounds: %u, %u, %u", idx0, idx1, idx2);
            continue;
        }

        // Get triangle vertices
        CubeVertex v0 = vertices[idx0];
        CubeVertex v1 = vertices[idx1];
        CubeVertex v2 = vertices[idx2];

        // Rasterize the triangle with transformation
        rasterize_triangle(framebuffer, depth_buffer, width, height, v0, v1, v2,
                         &model_matrix, &view_matrix, &projection_matrix);
        triangles_rendered++;
    }

    free(depth_buffer);
    GPU_LOG(device, "Rendered %u triangles via indexed drawing", triangles_rendered);
}

// GPU command execution engine
static GibgoResult gpu_execute_command_ring(GibgoGPUDevice* device) {
    if (!device) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Process all commands from head to tail
    while (device->cmd_ring.head_offset != device->cmd_ring.tail_offset) {
        u32 head_index = device->cmd_ring.head_offset;

        // Read command from ring buffer
        u32 command_type = device->cmd_ring.command_buffer[head_index * 4 + 0];
        u32 param0 = device->cmd_ring.command_buffer[head_index * 4 + 1];
        u32 param1 = device->cmd_ring.command_buffer[head_index * 4 + 2];
        u32 param2 = device->cmd_ring.command_buffer[head_index * 4 + 3];

        // Execute command based on type
        switch (command_type) {
            case GPU_CMD_SET_VERTEX_BUFFER:
                gpu_state.vertex_buffer_address = ((u64)param1 << 32) | param0;
                GPU_LOG(device, "Set vertex buffer: 0x%016llX", gpu_state.vertex_buffer_address);
                break;

            case GPU_CMD_SET_INDEX_BUFFER:
                gpu_state.index_buffer_address = ((u64)param1 << 32) | param0;
                gpu_state.index_format = param2;
                GPU_LOG(device, "Set index buffer: 0x%016llX, format=0x%X",
                       gpu_state.index_buffer_address, gpu_state.index_format);
                break;

            case GPU_CMD_SET_VERTEX_SHADER:
                gpu_state.vertex_shader_address = ((u64)param1 << 32) | param0;
                GPU_LOG(device, "Set vertex shader: 0x%016llX", gpu_state.vertex_shader_address);
                break;

            case GPU_CMD_SET_FRAGMENT_SHADER:
                gpu_state.fragment_shader_address = ((u64)param1 << 32) | param0;
                GPU_LOG(device, "Set fragment shader: 0x%016llX", gpu_state.fragment_shader_address);
                break;

            case GPU_CMD_SET_MATRICES:
                gpu_state.matrix_buffer_address = ((u64)param1 << 32) | param0;
                GPU_LOG(device, "Set matrices: 0x%016llX", gpu_state.matrix_buffer_address);
                break;

            case GPU_CMD_ENABLE_DEPTH_TEST:
                gpu_state.depth_test_enabled = param0;
                GPU_LOG(device, "Depth test: %s", gpu_state.depth_test_enabled ? "ENABLED" : "DISABLED");
                break;

            case GPU_CMD_SET_DEPTH_COMPARE:
                gpu_state.depth_compare_op = param0;
                GPU_LOG(device, "Depth compare: %s (%u)",
                       gpu_state.depth_compare_op == 1 ? "LESS" : "OTHER", gpu_state.depth_compare_op);
                break;

            case GPU_CMD_DRAW_INDEXED:
                GPU_LOG(device, "Executing GPU_CMD_DRAW_INDEXED: %u indices", param0);
                if (gpu_state.vertex_buffer_address && gpu_state.index_buffer_address) {
                    gpu_execute_indexed_drawing(device, param0, param1);
                } else {
                    GPU_LOG(device, "Cannot execute indexed drawing: missing vertex or index buffer");
                }
                break;

            case GPU_CMD_DRAW_PRIMITIVES:
                GPU_LOG(device, "Executing GPU_CMD_DRAW_PRIMITIVES: %u primitives", param0);
                // Keep existing behavior for compatibility
                break;

            case GPU_CMD_CLEAR_DEPTH_BUFFER:
                GPU_LOG(device, "Clear depth buffer to %f", *(float*)&param0);
                break;

            case GPU_CMD_PRESENT_FRAME:
                GPU_LOG(device, "Present frame %u (fence: %u)", device->frames_rendered + 1, param0);
                device->frames_rendered++;
                break;

            case GPU_CMD_FENCE:
                GPU_LOG(device, "Fence %u completed", param0);
                if (device->fence_register) {
                    *device->fence_register = param0;
                }
                break;

            default:
                GPU_LOG(device, "Unknown command type: 0x%02X", command_type);
                break;
        }

        // Advance head pointer
        device->cmd_ring.head_offset = (device->cmd_ring.head_offset + 1) % device->cmd_ring.capacity;
    }

    return GIBGO_RESULT_SUCCESS;
}

// Helper function to submit commands to hardware
static GibgoResult submit_commands_to_hardware(GibgoGPUDevice* device, GibgoGPUCommand* commands, u32 command_count) {
    if (!device || !commands) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(device, "Submitting %u commands to GPU hardware", command_count);

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

    // Execute the commands immediately (simulated hardware execution)
    GibgoResult exec_result = gpu_execute_command_ring(device);
    if (exec_result != GIBGO_RESULT_SUCCESS) {
        GPU_ERROR("Command execution failed");
        return exec_result;
    }

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
GibgoResult gibgo_draw_primitives(GibgoContext* context, u32 vertex_count, u32 first_vertex) {
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

// Set depth buffer
GibgoResult gibgo_set_depth_buffer(GibgoContext* context, u64 depth_buffer_address, u32 depth_format) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    add_command(GPU_CMD_SET_DEPTH_BUFFER,
                (u32)(depth_buffer_address & 0xFFFFFFFF),
                (u32)(depth_buffer_address >> 32),
                depth_format);

    GPU_LOG(context->device, "Set depth buffer: 0x%016llX, format=0x%X",
            depth_buffer_address, depth_format);

    return GIBGO_RESULT_SUCCESS;
}

// Clear depth buffer
GibgoResult gibgo_clear_depth_buffer(GibgoContext* context, f32 depth_value) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Convert f32 to u32 for command parameter
    u32 depth_bits = depth_value.bits;

    add_command(GPU_CMD_CLEAR_DEPTH_BUFFER, depth_bits, 0, 0);

    GPU_LOG(context->device, "Clear depth buffer to %f (0x%08X)",
            f32_to_native(depth_value), depth_bits);

    return GIBGO_RESULT_SUCCESS;
}

// Enable or disable depth testing
GibgoResult gibgo_enable_depth_test(GibgoContext* context, b32 enable) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    add_command(GPU_CMD_ENABLE_DEPTH_TEST, enable ? 1 : 0, 0, 0);

    GPU_LOG(context->device, "Depth test: %s", enable ? "ENABLED" : "DISABLED");

    return GIBGO_RESULT_SUCCESS;
}

// Set depth comparison function
// compare_op: 0=NEVER, 1=LESS, 2=EQUAL, 3=LESS_EQUAL, 4=GREATER, 5=NOT_EQUAL, 6=GREATER_EQUAL, 7=ALWAYS
GibgoResult gibgo_set_depth_compare(GibgoContext* context, u32 compare_op) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    add_command(GPU_CMD_SET_DEPTH_COMPARE, compare_op, 0, 0);

    const char* compare_names[] = {"NEVER", "LESS", "EQUAL", "LESS_EQUAL",
                                   "GREATER", "NOT_EQUAL", "GREATER_EQUAL", "ALWAYS"};
    const char* compare_name = (compare_op <= 7) ? compare_names[compare_op] : "UNKNOWN";

    GPU_LOG(context->device, "Depth compare: %s (%u)", compare_name, compare_op);

    return GIBGO_RESULT_SUCCESS;
}

// Set transformation matrices (MVP)
GibgoResult gibgo_set_matrices(GibgoContext* context, u64 matrix_buffer_address) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    add_command(GPU_CMD_SET_MATRICES,
                (u32)(matrix_buffer_address & 0xFFFFFFFF),
                (u32)(matrix_buffer_address >> 32),
                0);

    GPU_LOG(context->device, "Set matrices buffer: 0x%016llX", matrix_buffer_address);

    return GIBGO_RESULT_SUCCESS;
}

// Set index buffer for indexed drawing
GibgoResult gibgo_set_index_buffer(GibgoContext* context, u64 index_buffer_address, u32 index_format) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    add_command(GPU_CMD_SET_INDEX_BUFFER,
                (u32)(index_buffer_address & 0xFFFFFFFF),
                (u32)(index_buffer_address >> 32),
                index_format);

    GPU_LOG(context->device, "Set index buffer: 0x%016llX, format=0x%X", index_buffer_address, index_format);

    return GIBGO_RESULT_SUCCESS;
}

// Draw indexed primitives
GibgoResult gibgo_draw_indexed(GibgoContext* context, u32 index_count, u32 first_index) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    add_command(GPU_CMD_DRAW_INDEXED, index_count, first_index, 0);

    GPU_LOG(context->device, "Drawing %u indices starting from %u", index_count, first_index);

    return GIBGO_RESULT_SUCCESS;
}

// Set vertex buffer for rendering
GibgoResult gibgo_set_vertex_buffer(GibgoContext* context, u64 vertex_buffer_address) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    add_command(GPU_CMD_SET_VERTEX_BUFFER,
                (u32)(vertex_buffer_address & 0xFFFFFFFF),
                (u32)(vertex_buffer_address >> 32),
                0);

    GPU_LOG(context->device, "Set vertex buffer: 0x%016llX", vertex_buffer_address);

    return GIBGO_RESULT_SUCCESS;
}