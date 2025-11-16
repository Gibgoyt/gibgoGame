#define _GNU_SOURCE  // Enable usleep and other POSIX extensions
#include "gpu_device.h"
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

// GPU command types (simplified command set)
typedef enum {
    GPU_CMD_NOP = 0x00,
    GPU_CMD_SET_VIEWPORT = 0x01,
    GPU_CMD_SET_VERTEX_BUFFER = 0x02,
    GPU_CMD_SET_VERTEX_SHADER = 0x03,
    GPU_CMD_SET_FRAGMENT_SHADER = 0x04,
    GPU_CMD_CLEAR_FRAMEBUFFER = 0x05,
    GPU_CMD_DRAW_PRIMITIVES = 0x06,
    GPU_CMD_PRESENT_FRAME = 0x07,
    GPU_CMD_FENCE = 0x08
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