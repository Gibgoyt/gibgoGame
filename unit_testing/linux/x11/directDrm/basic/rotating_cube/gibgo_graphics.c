#include "gibgo_graphics.h"
#include "gpu_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal function prototypes from other modules
extern GibgoResult gibgo_enumerate_gpus(GibgoGPUInfo** gpu_list, u32* gpu_count);
extern GibgoResult gibgo_create_device(u32 gpu_index, GibgoGPUDevice** out_device);
extern GibgoResult gibgo_destroy_device(GibgoGPUDevice* device);
extern GibgoResult gibgo_create_context(GibgoGPUDevice* device, GibgoContext** out_context);
extern GibgoResult gibgo_destroy_context(GibgoContext* context);
extern GibgoResult gibgo_upload_vertices(GibgoContext* context, void* vertex_data, u32 vertex_count, u32 vertex_stride);
extern GibgoResult gibgo_load_shaders(GibgoContext* context, u32* vertex_spirv, u32 vertex_size, u32* fragment_spirv, u32 fragment_size);
extern GibgoResult gibgo_set_viewport(GibgoContext* context, u32 width, u32 height);
extern GibgoResult gibgo_begin_commands(GibgoContext* context);
extern GibgoResult gibgo_end_commands(GibgoContext* context);
extern GibgoResult gibgo_submit_commands(GibgoContext* context);
extern GibgoResult gibgo_draw_primitives_internal(GibgoContext* context, u32 vertex_count, u32 first_vertex);
extern GibgoResult gibgo_present_frame(GibgoContext* context);
extern GibgoResult gibgo_enable_face_culling(GibgoContext* context, b32 enable);
extern GibgoResult gibgo_wait_for_completion(GibgoContext* context, u32 fence_value);
extern void gibgo_debug_gpu_state(GibgoGPUDevice* device);
extern void gibgo_debug_context_state(GibgoContext* context);

// Helper function to convert internal results to public API results
static GibgoGraphicsResult convert_result(GibgoResult internal_result) {
    switch (internal_result) {
        case GIBGO_RESULT_SUCCESS:
            return GIBGO_SUCCESS;
        case GIBGO_RESULT_ERROR_DEVICE_NOT_FOUND:
        case GIBGO_RESULT_ERROR_DEVICE_ACCESS_DENIED:
        case GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED:
            return GIBGO_ERROR_INITIALIZATION_FAILED;
        case GIBGO_RESULT_ERROR_OUT_OF_MEMORY:
            return GIBGO_ERROR_OUT_OF_MEMORY;
        case GIBGO_RESULT_ERROR_GPU_TIMEOUT:
        case GIBGO_RESULT_ERROR_COMMAND_FAILED:
            return GIBGO_ERROR_DEVICE_LOST;
        default:
            return GIBGO_ERROR_INVALID_PARAMETER;
    }
}

// Initialize graphics system
GibgoGraphicsResult gibgo_initialize_graphics(const GibgoGraphicsInitInfo* init_info,
                                             GibgoGraphicsSystem** out_system) {
    if (!init_info || !out_system) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    printf("[GibgoCraft Graphics] Initializing hardware-direct graphics layer...\n");

    // Allocate graphics system structure
    GibgoGraphicsSystem* system = (GibgoGraphicsSystem*)calloc(1, sizeof(GibgoGraphicsSystem));
    if (!system) {
        return GIBGO_ERROR_OUT_OF_MEMORY;
    }

    // Enumerate available GPUs
    GibgoGPUInfo* gpu_list = NULL;
    u32 gpu_count = 0;
    GibgoResult result = gibgo_enumerate_gpus(&gpu_list, &gpu_count);
    if (result != GIBGO_RESULT_SUCCESS) {
        free(system);
        return convert_result(result);
    }

    printf("[GibgoCraft Graphics] Found %u GPU(s):\n", gpu_count);
    for (u32 i = 0; i < gpu_count; i++) {
        printf("  [%u] %s\n", i, gpu_list[i].device_name);
    }

    // Create GPU device (use first available GPU)
    GibgoGPUDevice* device = NULL;
    result = gibgo_create_device(0, &device);
    if (result != GIBGO_RESULT_SUCCESS) {
        free(gpu_list);
        free(system);
        return convert_result(result);
    }

    // Enable debug logging if requested
    device->debug_enabled = init_info->enable_debug;

    printf("[GibgoCraft Graphics] Using GPU: %s\n", device->info.device_name);
    printf("[GibgoCraft Graphics] VRAM: %lu MB\n", device->info.vram_size / (1024 * 1024));

    // Create graphics context
    GibgoContext* context = NULL;
    result = gibgo_create_context(device, &context);
    if (result != GIBGO_RESULT_SUCCESS) {
        gibgo_destroy_device(device);
        free(gpu_list);
        free(system);
        return convert_result(result);
    }

    // Set up viewport (TODO: implement gibgo_set_viewport function)
    // For now, skip viewport setup - the framebuffer is already created
    printf("[GibgoCraft Graphics] Viewport setup skipped - using framebuffer size\n");

    // Enable face culling for proper 3D rendering (hide back faces)
    result = gibgo_enable_face_culling(context, B32_TRUE);
    if (result != GIBGO_RESULT_SUCCESS) {
        printf("[GibgoCraft Graphics] Warning: Failed to enable face culling\n");
    } else {
        printf("[GibgoCraft Graphics] Face culling enabled\n");
    }

    // Store internal structures
    system->internal_device = device;
    system->internal_context = context;
    system->frame_width = init_info->window_width;
    system->frame_height = init_info->window_height;
    system->current_frame = 0;
    system->is_initialized = 1;

    printf("[GibgoCraft Graphics] Hardware-direct graphics layer initialized successfully!\n");
    printf("[GibgoCraft Graphics] Framebuffer: %ux%u\n", system->frame_width, system->frame_height);

    free(gpu_list);
    *out_system = system;
    return GIBGO_SUCCESS;
}

// Shutdown graphics system
GibgoGraphicsResult gibgo_shutdown_graphics(GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    printf("[GibgoCraft Graphics] Shutting down hardware-direct graphics layer...\n");

    // Destroy context
    if (system->internal_context) {
        gibgo_destroy_context((GibgoContext*)system->internal_context);
    }

    // Destroy device
    if (system->internal_device) {
        gibgo_destroy_device((GibgoGPUDevice*)system->internal_device);
    }

    system->is_initialized = 0;
    free(system);

    printf("[GibgoCraft Graphics] Graphics layer shutdown complete.\n");
    return GIBGO_SUCCESS;
}

// Create shaders from SPIR-V bytecode
GibgoGraphicsResult gibgo_create_shaders_from_spirv(GibgoGraphicsSystem* system,
                                                   const u32* vertex_spirv_data, u32 vertex_spirv_size,
                                                   const u32* fragment_spirv_data, u32 fragment_spirv_size) {
    if (!system || !system->is_initialized || !vertex_spirv_data || !fragment_spirv_data) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoResult result = gibgo_load_shaders(context,
                                           (u32*)vertex_spirv_data, vertex_spirv_size,
                                           (u32*)fragment_spirv_data, fragment_spirv_size);

    if (result == GIBGO_RESULT_SUCCESS) {
        printf("[GibgoCraft Graphics] Shaders loaded successfully\n");
        printf("  Vertex shader: %u bytes\n", vertex_spirv_size);
        printf("  Fragment shader: %u bytes\n", fragment_spirv_size);
    }

    return convert_result(result);
}

// Upload vertex data to GPU
GibgoGraphicsResult gibgo_upload_vertex_data(GibgoGraphicsSystem* system,
                                            const GibgoVertex* vertices, u32 vertex_count) {
    if (!system || !system->is_initialized || !vertices || vertex_count == 0) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoResult result = gibgo_upload_vertices(context, (void*)vertices, vertex_count, sizeof(GibgoVertex));

    if (result == GIBGO_RESULT_SUCCESS) {
        printf("[GibgoCraft Graphics] Uploaded %u vertices (%lu bytes)\n",
               vertex_count, vertex_count * sizeof(GibgoVertex));
    }

    return convert_result(result);
}

// Begin rendering a new frame
GibgoGraphicsResult gibgo_begin_frame(GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoResult result = gibgo_begin_commands(context);

    return convert_result(result);
}

// Draw triangle using uploaded vertex data
GibgoGraphicsResult gibgo_draw_triangle(GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoResult result = gibgo_draw_primitives_internal(context, 3, 0); // 3 vertices, starting from 0

    return convert_result(result);
}

// End frame and present to display
GibgoGraphicsResult gibgo_end_frame_and_present(GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;

    // Present frame
    GibgoResult result = gibgo_present_frame(context);
    if (result != GIBGO_RESULT_SUCCESS) {
        return convert_result(result);
    }

    // End command recording
    result = gibgo_end_commands(context);
    if (result != GIBGO_RESULT_SUCCESS) {
        return convert_result(result);
    }

    // Submit commands to GPU
    result = gibgo_submit_commands(context);
    if (result != GIBGO_RESULT_SUCCESS) {
        return convert_result(result);
    }

    system->current_frame++;
    return GIBGO_SUCCESS;
}

// Wait for frame completion
GibgoGraphicsResult gibgo_wait_for_frame_completion(GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoResult result = gibgo_wait_for_completion(context, context->frame_fence);

    return convert_result(result);
}

// Convert result code to string
const char* gibgo_graphics_result_string(GibgoGraphicsResult result) {
    switch (result) {
        case GIBGO_SUCCESS:                         return "Success";
        case GIBGO_ERROR_INITIALIZATION_FAILED:    return "Initialization failed";
        case GIBGO_ERROR_DEVICE_LOST:              return "Device lost";
        case GIBGO_ERROR_OUT_OF_MEMORY:            return "Out of memory";
        case GIBGO_ERROR_INVALID_PARAMETER:        return "Invalid parameter";
        default:                                   return "Unknown error";
    }
}

// Print system information
void gibgo_debug_print_system_info(const GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        printf("[GibgoCraft Graphics] System not initialized\n");
        return;
    }

    printf("\n=== GibgoCraft Graphics System Info ===\n");
    printf("Status: %s\n", system->is_initialized ? "Initialized" : "Not Initialized");
    printf("Framebuffer: %ux%u\n", system->frame_width, system->frame_height);
    printf("Current Frame: %u\n", system->current_frame);
    printf("======================================\n\n");
}

// Debug GPU state
GibgoGraphicsResult gibgo_debug_dump_gpu_state(GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    printf("\n[GibgoCraft Graphics] Debug Information:\n");

    // Print system info
    gibgo_debug_print_system_info(system);

    // Print GPU device state
    GibgoGPUDevice* device = (GibgoGPUDevice*)system->internal_device;
    gibgo_debug_gpu_state(device);

    // Print context state
    GibgoContext* context = (GibgoContext*)system->internal_context;
    gibgo_debug_context_state(context);

    return GIBGO_SUCCESS;
}

// Get frame statistics
GibgoGraphicsResult gibgo_get_frame_statistics(GibgoGraphicsSystem* system,
                                              u64* frames_rendered, u64* commands_submitted) {
    if (!system || !system->is_initialized || !frames_rendered || !commands_submitted) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoGPUDevice* device = (GibgoGPUDevice*)system->internal_device;
    *frames_rendered = device->frames_rendered;
    *commands_submitted = device->commands_submitted;

    return GIBGO_SUCCESS;
}

// Draw primitives with specified vertex count and starting vertex
GibgoGraphicsResult gibgo_draw_primitives(GibgoGraphicsSystem* system, u32 vertex_count, u32 first_vertex) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoResult result = gibgo_draw_primitives_internal(context, vertex_count, first_vertex);
    return convert_result(result);
}

// Set uniform buffer data for shaders
GibgoGraphicsResult gibgo_set_uniform_buffer_data(GibgoGraphicsSystem* system, const void* data, u32 size) {
    if (!system || !system->is_initialized || !data) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoGPUDevice* device = (GibgoGPUDevice*)system->internal_device;

    // Allocate uniform buffer if needed (simplified implementation)
    u64 uniform_buffer_address;
    GibgoResult result = gibgo_allocate_gpu_memory(device, size, &uniform_buffer_address);
    if (result != GIBGO_RESULT_SUCCESS) {
        return convert_result(result);
    }

    // Map uniform buffer memory and copy data
    u8* mapped_memory;
    result = gibgo_map_gpu_memory(device, uniform_buffer_address, size, &mapped_memory);
    if (result == GIBGO_RESULT_SUCCESS) {
        memcpy(mapped_memory, data, size);
        gibgo_unmap_gpu_memory(device, mapped_memory, size);
    }

    // Set uniform buffer in context
    result = gibgo_set_uniform_buffer(context, uniform_buffer_address, size);
    return convert_result(result);
}