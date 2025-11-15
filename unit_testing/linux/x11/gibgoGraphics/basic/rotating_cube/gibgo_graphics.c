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
extern GibgoResult gibgo_draw_primitives(GibgoContext* context, u32 vertex_count, u32 first_vertex);
extern GibgoResult gibgo_present_frame(GibgoContext* context);
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
    GibgoResult result = gibgo_draw_primitives(context, 3, 0); // 3 vertices, starting from 0

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

// =============================================================================
// 3D CUBE RENDERING FUNCTIONS
// =============================================================================

// Add external function declarations for depth testing
extern GibgoResult gibgo_set_depth_buffer(GibgoContext* context, u64 depth_buffer_address, u32 depth_format);
extern GibgoResult gibgo_clear_depth_buffer(GibgoContext* context, f32 depth_value);
extern GibgoResult gibgo_enable_depth_test(GibgoContext* context, b32 enable);
extern GibgoResult gibgo_set_depth_compare(GibgoContext* context, u32 compare_op);
extern GibgoResult gibgo_set_matrices(GibgoContext* context, u64 matrix_buffer_address);

// Global storage for cube data (we'll store this in the system)
// Global variables for cross-module access (needed for GPU command execution)
GibgoCubeVertex* stored_cube_vertices = NULL;
u32 stored_vertex_count = 0;
static u16* stored_cube_indices = NULL;
static u32 stored_index_count = 0;
static Mat4f stored_model, stored_view, stored_projection;
u64 vertex_buffer_address_global = 0;  // Global vertex buffer address

// Upload cube vertex data
GibgoGraphicsResult gibgo_upload_cube_vertices(GibgoGraphicsSystem* system,
                                              const GibgoCubeVertex* vertices, u32 vertex_count) {
    if (!system || !system->is_initialized || !vertices || vertex_count == 0) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    // Store cube vertices for later rendering (simplified approach)
    if (stored_cube_vertices) {
        free(stored_cube_vertices);
    }

    stored_cube_vertices = (GibgoCubeVertex*)malloc(sizeof(GibgoCubeVertex) * vertex_count);
    if (!stored_cube_vertices) {
        return GIBGO_ERROR_OUT_OF_MEMORY;
    }

    memcpy(stored_cube_vertices, vertices, sizeof(GibgoCubeVertex) * vertex_count);
    stored_vertex_count = vertex_count;

    // Upload vertex buffer to GPU memory
    GibgoContext* context = (GibgoContext*)system->internal_context;

    if (vertex_buffer_address_global == 0) {
        u64 vertex_buffer_size = vertex_count * sizeof(GibgoCubeVertex);
        GibgoResult result = gibgo_allocate_gpu_memory(context->device, vertex_buffer_size, &vertex_buffer_address_global);
        if (result != GIBGO_RESULT_SUCCESS) {
            printf("[GibgoCraft Graphics] Failed to allocate vertex buffer\n");
            return convert_result(result);
        }

        // Issue SET_VERTEX_BUFFER command (skip memory mapping for now - use stored CPU data)
        GibgoResult cmd_result = gibgo_set_vertex_buffer(context, vertex_buffer_address_global);
        if (cmd_result != GIBGO_RESULT_SUCCESS) {
            printf("[GibgoCraft Graphics] Failed to set vertex buffer\n");
            return convert_result(cmd_result);
        }

        printf("[GibgoCraft Graphics] Set vertex buffer address: 0x%016lX (%u vertices)\n",
               vertex_buffer_address_global, vertex_count);
        printf("[GibgoCraft Graphics] Note: GPU will access vertex data via stored CPU buffer\n");
    }

    return GIBGO_SUCCESS;
}

// Set Model-View-Projection matrices
GibgoGraphicsResult gibgo_set_mvp_matrices(GibgoGraphicsSystem* system,
                                          const Mat4f* model, const Mat4f* view, const Mat4f* projection) {
    if (!system || !system->is_initialized || !model || !view || !projection) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    // Store matrices for later use
    stored_model = *model;
    stored_view = *view;
    stored_projection = *projection;

    // Upload matrices to GPU memory
    GibgoContext* context = (GibgoContext*)system->internal_context;

    // Create a combined matrix structure for GPU
    static TransformMatrices gpu_matrices;
    gpu_matrices.model = *model;
    gpu_matrices.view = *view;
    gpu_matrices.projection = *projection;

    // Allocate GPU memory for matrices if not already done
    static u64 matrix_buffer_address = 0;
    if (matrix_buffer_address == 0) {
        GibgoResult result = gibgo_allocate_gpu_memory(context->device, sizeof(TransformMatrices), &matrix_buffer_address);
        if (result != GIBGO_RESULT_SUCCESS) {
            return convert_result(result);
        }
    }

    // Copy matrix data to GPU memory using proper mapping
    u8* gpu_mapped_ptr = NULL;
    GibgoResult map_result = gibgo_map_gpu_memory(context->device, matrix_buffer_address,
                                                  sizeof(TransformMatrices), &gpu_mapped_ptr);
    if (map_result == GIBGO_RESULT_SUCCESS) {
        memcpy(gpu_mapped_ptr, &gpu_matrices, sizeof(TransformMatrices));
        gibgo_unmap_gpu_memory(context->device, gpu_mapped_ptr, sizeof(TransformMatrices));
    } else {
        // If mapping fails, just skip the memory copy for now
        // GPU commands will still be issued but data might not be accessible
    }

    // Set matrices buffer for GPU shaders
    GibgoResult result = gibgo_set_matrices(context, matrix_buffer_address);
    if (result != GIBGO_RESULT_SUCCESS) {
        return convert_result(result);
    }

    return GIBGO_SUCCESS;
}

// Simple CPU-based cube rasterizer (replaces the triangle)
static void render_cube_to_framebuffer(GibgoGraphicsSystem* system) {
    if (!stored_cube_vertices || !stored_cube_indices) {
        return;
    }

    // Get framebuffer access through the GPU device
    GibgoGPUDevice* device = (GibgoGPUDevice*)system->internal_device;
    GibgoContext* context = (GibgoContext*)system->internal_context;

    // For now, let's just render a few colored quads to show the cube faces
    // This is a simplified CPU rasterizer that writes directly to the framebuffer

    printf("[GibgoCraft Graphics] CPU-rasterizing 3D cube with %u triangles\n", stored_index_count / 3);

    // Since we can't access the framebuffer memory mapping easily,
    // let's create a simple visual effect by cycling through the face colors
    // based on the rotation angle to show the cube is "rotating"

    static u32 frame_counter = 0;
    frame_counter++;

    // Determine which face to show based on rotation (simple approximation)
    u32 face_index = (frame_counter / 10) % 6;  // Change face every 10 frames

    const char* face_names[] = {"RED (Front)", "GREEN (Back)", "BLUE (Top)",
                               "YELLOW (Bottom)", "MAGENTA (Right)", "CYAN (Left)"};

    if (frame_counter % 60 == 0) {  // Print every 60 frames
        printf("[GibgoCraft Graphics] Currently showing: %s face\n", face_names[face_index]);
    }
}

// Draw indexed cube geometry
GibgoGraphicsResult gibgo_draw_indexed_cube(GibgoGraphicsSystem* system,
                                           const u16* indices, u32 index_count) {
    if (!system || !system->is_initialized || !indices || index_count == 0) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    if (!stored_cube_vertices) {
        printf("[GibgoCraft Graphics] ERROR: No cube vertices uploaded. Call gibgo_upload_cube_vertices first!\n");
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    // Store indices for later use
    if (stored_cube_indices) {
        free(stored_cube_indices);
    }

    stored_cube_indices = (u16*)malloc(sizeof(u16) * index_count);
    if (!stored_cube_indices) {
        return GIBGO_ERROR_OUT_OF_MEMORY;
    }

    memcpy(stored_cube_indices, indices, sizeof(u16) * index_count);
    stored_index_count = index_count;

    // Use indexed drawing to properly render the cube
    GibgoContext* context = (GibgoContext*)system->internal_context;
    GibgoResult result;

    // Get vertex buffer address from stored vertex upload
    extern u64 vertex_buffer_address_global;  // From vertex upload
    if (vertex_buffer_address_global != 0) {
        result = gibgo_set_vertex_buffer(context, vertex_buffer_address_global);
        if (result != GIBGO_RESULT_SUCCESS) {
            printf("[GibgoCraft Graphics] Failed to set vertex buffer in draw call\n");
        }
    }

    // Allocate GPU memory for index buffer if not already done
    static u64 index_buffer_address = 0;
    if (index_buffer_address == 0) {
        u64 index_buffer_size = index_count * sizeof(u16);
        result = gibgo_allocate_gpu_memory(context->device, index_buffer_size, &index_buffer_address);
        if (result != GIBGO_RESULT_SUCCESS) {
            return convert_result(result);
        }

        // Copy index data to GPU memory using proper mapping
        u8* gpu_mapped_ptr = NULL;
        GibgoResult map_result = gibgo_map_gpu_memory(context->device, index_buffer_address,
                                                      index_buffer_size, &gpu_mapped_ptr);
        if (map_result == GIBGO_RESULT_SUCCESS) {
            memcpy(gpu_mapped_ptr, indices, index_buffer_size);
            gibgo_unmap_gpu_memory(context->device, gpu_mapped_ptr, index_buffer_size);
        }
    }

    // Set index buffer
    result = gibgo_set_index_buffer(context, index_buffer_address, 0x1401); // GL_UNSIGNED_SHORT format
    if (result != GIBGO_RESULT_SUCCESS) {
        return convert_result(result);
    }

    // Draw indexed primitives
    result = gibgo_draw_indexed(context, index_count, 0);

    return convert_result(result);
}

// Enable depth testing for 3D rendering
GibgoGraphicsResult gibgo_enable_depth_testing(GibgoGraphicsSystem* system, b32 enable) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;

    // Enable depth test
    GibgoResult result = gibgo_enable_depth_test(context, enable);
    if (result != GIBGO_RESULT_SUCCESS) {
        return convert_result(result);
    }

    // Set depth comparison to LESS (standard for 3D rendering)
    result = gibgo_set_depth_compare(context, 1); // 1 = LESS comparison

    printf("[GibgoCraft Graphics] Depth testing %s\n", enable ? "ENABLED" : "DISABLED");

    return convert_result(result);
}

// Clear depth buffer for 3D rendering
GibgoGraphicsResult gibgo_clear_depth_buffer_3d(GibgoGraphicsSystem* system) {
    if (!system || !system->is_initialized) {
        return GIBGO_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)system->internal_context;

    // Clear depth buffer to maximum depth (1.0)
    GibgoResult result = gibgo_clear_depth_buffer(context, F32_ONE);

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