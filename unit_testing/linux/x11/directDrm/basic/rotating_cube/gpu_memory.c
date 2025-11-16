#include "gpu_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

// Debug logging macros
#define GPU_LOG(device, fmt, ...) \
    do { \
        if ((device) && (device)->debug_enabled) { \
            printf("[GPU] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

#define GPU_ERROR(fmt, ...) \
    fprintf(stderr, "[GPU ERROR] " fmt "\n", ##__VA_ARGS__)

// Memory alignment for GPU operations (typically 256 bytes)
#define GPU_MEMORY_ALIGNMENT 256

// Helper function to align size to GPU requirements
static u64 align_gpu_memory(u64 size, u64 alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// GPU memory allocation implementation
GibgoResult gibgo_allocate_gpu_memory(GibgoGPUDevice* device, u64 size, u64* out_address) {
    if (!device || !out_address || size == 0) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Align size to GPU memory requirements
    u64 aligned_size = align_gpu_memory(size, GPU_MEMORY_ALIGNMENT);

    // Check if we have enough memory pool space available
    if (device->memory_pool.pool_used + aligned_size > device->memory_pool.pool_size) {
        GPU_ERROR("Out of memory pool: requested %lu bytes, available %lu bytes",
                  aligned_size, device->memory_pool.pool_size - device->memory_pool.pool_used);
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    // Check if we have tracking slots available
    if (device->memory_pool.allocation_count >= 256) {
        GPU_ERROR("Too many allocations: maximum 256 allocations supported");
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    // Find an unused allocation slot
    u32 slot_index = 0;
    for (u32 i = 0; i < 256; i++) {
        if (!device->memory_pool.allocations[i].in_use) {
            slot_index = i;
            break;
        }
    }

    // Calculate the GPU address (virtual) and CPU pointer (real)
    u64 gpu_address = device->vram.physical_address + device->vram_allocation_offset;
    u8* cpu_pointer = device->memory_pool.pool_memory + device->memory_pool.pool_used;

    // Record the allocation
    device->memory_pool.allocations[slot_index].gpu_address = gpu_address;
    device->memory_pool.allocations[slot_index].cpu_pointer = cpu_pointer;
    device->memory_pool.allocations[slot_index].size = aligned_size;
    device->memory_pool.allocations[slot_index].in_use = B32_TRUE;

    // Update allocation tracking
    device->memory_pool.pool_used += aligned_size;
    device->memory_pool.allocation_count++;
    device->vram_allocation_offset += aligned_size; // Keep virtual address tracking

    GPU_LOG(device, "Allocated %lu bytes of GPU memory at 0x%016lX (CPU: %p)", aligned_size, gpu_address, cpu_pointer);

    *out_address = gpu_address;
    return GIBGO_RESULT_SUCCESS;
}

// GPU memory deallocation (simplified - real implementation would have a heap allocator)
GibgoResult gibgo_free_gpu_memory(GibgoGPUDevice* device, u64 address, u64 size) {
    if (!device) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(device, "Freed %lu bytes of GPU memory at 0x%016lX", size, address);

    // In a real implementation, you would mark this memory as free in a heap structure
    // For simplicity, we just log the operation for now

    return GIBGO_RESULT_SUCCESS;
}

// Map GPU memory to CPU-accessible address space
// This returns a pointer to the persistent memory allocation for the given GPU address
GibgoResult gibgo_map_gpu_memory(GibgoGPUDevice* device, u64 gpu_address, u64 size, u8** out_cpu_address) {
    if (!device || !out_cpu_address || size == 0) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Find the allocation that corresponds to this GPU address
    for (u32 i = 0; i < 256; i++) {
        if (device->memory_pool.allocations[i].in_use &&
            device->memory_pool.allocations[i].gpu_address == gpu_address) {

            // Verify the requested size doesn't exceed the allocation
            if (size > device->memory_pool.allocations[i].size) {
                GPU_ERROR("Map size %lu exceeds allocation size %lu for address 0x%016lX",
                          size, device->memory_pool.allocations[i].size, gpu_address);
                return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
            }

            u8* cpu_pointer = device->memory_pool.allocations[i].cpu_pointer;
            GPU_LOG(device, "Mapped GPU memory 0x%016lX (%lu bytes) to persistent CPU address %p",
                    gpu_address, size, cpu_pointer);

            *out_cpu_address = cpu_pointer;
            return GIBGO_RESULT_SUCCESS;
        }
    }

    GPU_ERROR("GPU address 0x%016lX not found in allocations", gpu_address);
    return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
}

// Unmap GPU memory from CPU address space
// This just releases the mapping, but keeps the persistent memory intact
GibgoResult gibgo_unmap_gpu_memory(GibgoGPUDevice* device, u8* cpu_address, u64 size) {
    if (!device || !cpu_address) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Verify this CPU address belongs to one of our allocations
    for (u32 i = 0; i < 256; i++) {
        if (device->memory_pool.allocations[i].in_use &&
            device->memory_pool.allocations[i].cpu_pointer == cpu_address) {

            GPU_LOG(device, "Unmapped GPU memory at %p (%lu bytes) - data remains persistent", cpu_address, size);
            return GIBGO_RESULT_SUCCESS;
        }
    }

    GPU_ERROR("CPU address %p not found in persistent allocations", cpu_address);
    return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
}

// Context management implementation
GibgoResult gibgo_create_context(GibgoGPUDevice* device, GibgoContext** out_context) {
    if (!device || !out_context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GibgoContext* context = (GibgoContext*)calloc(1, sizeof(GibgoContext));
    if (!context) {
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    context->device = device;
    context->current_frame_index = 0;
    context->frame_fence = 1;

    // Set default framebuffer parameters
    context->framebuffer_width = 800;
    context->framebuffer_height = 600;
    context->framebuffer_format = 0x8888; // RGBA8

    // Allocate framebuffer memory
    u64 framebuffer_size = context->framebuffer_width * context->framebuffer_height * 4; // 4 bytes per pixel
    GibgoResult result = gibgo_allocate_gpu_memory(device, framebuffer_size, &context->framebuffer_address);
    if (result != GIBGO_RESULT_SUCCESS) {
        free(context);
        return result;
    }

    GPU_LOG(device, "Created graphics context - framebuffer %ux%u at 0x%016lX",
            context->framebuffer_width, context->framebuffer_height, context->framebuffer_address);

    *out_context = context;
    return GIBGO_RESULT_SUCCESS;
}

// Context destruction
GibgoResult gibgo_destroy_context(GibgoContext* context) {
    if (!context) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Free framebuffer memory
    if (context->framebuffer_address) {
        u64 framebuffer_size = context->framebuffer_width * context->framebuffer_height * 4;
        gibgo_free_gpu_memory(context->device, context->framebuffer_address, framebuffer_size);
    }

    // Free vertex buffer memory
    if (context->vertex_buffer_address) {
        // Assuming maximum vertex buffer size for cleanup
        gibgo_free_gpu_memory(context->device, context->vertex_buffer_address, 1024 * 1024);
    }

    GPU_LOG(context->device, "Destroyed graphics context");
    free(context);
    return GIBGO_RESULT_SUCCESS;
}

// Vertex data upload implementation
GibgoResult gibgo_upload_vertices(GibgoContext* context, void* vertex_data, u32 vertex_count, u32 vertex_stride) {
    if (!context || !vertex_data || vertex_count == 0 || vertex_stride == 0) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    u64 buffer_size = vertex_count * vertex_stride;

    // Allocate GPU memory for vertex buffer if not already allocated
    if (context->vertex_buffer_address == 0) {
        GibgoResult result = gibgo_allocate_gpu_memory(context->device, buffer_size, &context->vertex_buffer_address);
        if (result != GIBGO_RESULT_SUCCESS) {
            return result;
        }
    }

    // Map the vertex buffer to CPU address space
    u8* mapped_buffer = NULL;
    GibgoResult result = gibgo_map_gpu_memory(context->device, context->vertex_buffer_address, buffer_size, &mapped_buffer);
    if (result != GIBGO_RESULT_SUCCESS) {
        return result;
    }

    // Copy vertex data to GPU memory
    memcpy(mapped_buffer, vertex_data, buffer_size);

    // Unmap the buffer
    gibgo_unmap_gpu_memory(context->device, mapped_buffer, buffer_size);

    // Update context state
    context->vertex_buffer_stride = vertex_stride;
    context->vertex_count = vertex_count;

    GPU_LOG(context->device, "Uploaded %u vertices (%lu bytes) to GPU buffer at 0x%016lX",
            vertex_count, buffer_size, context->vertex_buffer_address);

    return GIBGO_RESULT_SUCCESS;
}

// Context state debugging
void gibgo_debug_context_state(GibgoContext* context) {
    if (!context) return;

    printf("\n=== Graphics Context State ===\n");
    printf("Framebuffer: %ux%u at 0x%016lX\n",
           context->framebuffer_width, context->framebuffer_height, context->framebuffer_address);
    printf("Vertex Buffer: %u vertices (%u bytes each) at 0x%016lX\n",
           context->vertex_count, context->vertex_buffer_stride, context->vertex_buffer_address);
    printf("Shaders: VS=0x%016lX, FS=0x%016lX\n",
           context->vertex_shader_address, context->fragment_shader_address);
    printf("Frame Index: %u, Fence: %u\n",
           context->current_frame_index, context->frame_fence);
    printf("==============================\n\n");
}