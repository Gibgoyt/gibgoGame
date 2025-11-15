#include "gpu_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
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

    // Check if we have enough VRAM available
    if (device->vram_allocation_offset + aligned_size > device->vram.size) {
        GPU_ERROR("Out of VRAM: requested %lu bytes, available %lu bytes",
                  aligned_size, device->vram.size - device->vram_allocation_offset);
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    // Calculate the GPU address for this allocation
    u64 gpu_address = device->vram.physical_address + device->vram_allocation_offset;

    GPU_LOG(device, "Allocated %lu bytes of GPU memory at 0x%016lX", aligned_size, gpu_address);

    // Update allocation offset
    device->vram_allocation_offset += aligned_size;

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

// DRM buffer management structure
typedef struct {
    u32 handle;           // DRM gem handle
    u64 size;            // Buffer size
    u64 offset;          // mmap offset
    void* mapped_ptr;    // CPU mapped pointer
} DRMBuffer;

// Helper function to create DRM dumb buffer
static GibgoResult create_drm_dumb_buffer(GibgoGPUDevice* device, u64 size, DRMBuffer* buffer) {
    #include <drm/drm.h>
    #include <drm/drm_mode.h>

    // Create dumb buffer
    struct drm_mode_create_dumb create_req = {0};
    create_req.width = (size + 3) / 4;  // Convert bytes to "pixels" (4 bytes each)
    create_req.height = 1;
    create_req.bpp = 32;

    if (ioctl(device->device_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        GPU_ERROR("Failed to create DRM dumb buffer: %s", strerror(errno));
        return GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED;
    }

    // Get mmap offset
    struct drm_mode_map_dumb map_req = {0};
    map_req.handle = create_req.handle;

    if (ioctl(device->device_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        GPU_ERROR("Failed to get DRM buffer mmap offset: %s", strerror(errno));
        return GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED;
    }

    // Map the buffer
    void* mapped_ptr = mmap(NULL, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                           device->device_fd, map_req.offset);

    if (mapped_ptr == MAP_FAILED) {
        GPU_ERROR("Failed to mmap DRM buffer: %s", strerror(errno));
        return GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED;
    }

    buffer->handle = create_req.handle;
    buffer->size = create_req.size;
    buffer->offset = map_req.offset;
    buffer->mapped_ptr = mapped_ptr;

    GPU_LOG(device, "Created DRM dumb buffer: handle=%u, size=%lu, mapped=%p",
            buffer->handle, buffer->size, buffer->mapped_ptr);

    return GIBGO_RESULT_SUCCESS;
}

// Map GPU memory to CPU-accessible address space using DRM dumb buffers
GibgoResult gibgo_map_gpu_memory(GibgoGPUDevice* device, u64 gpu_address, u64 size, u8** out_cpu_address) {
    if (!device || !out_cpu_address || size == 0) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Check if the GPU address is valid
    if (gpu_address < device->vram.physical_address ||
        gpu_address + size > device->vram.physical_address + device->vram.size) {
        GPU_ERROR("Invalid GPU memory address range: 0x%016lX - 0x%016lX",
                  gpu_address, gpu_address + size);
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Create a DRM dumb buffer for this allocation
    DRMBuffer* buffer = (DRMBuffer*)malloc(sizeof(DRMBuffer));
    if (!buffer) {
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    GibgoResult result = create_drm_dumb_buffer(device, size, buffer);
    if (result != GIBGO_RESULT_SUCCESS) {
        free(buffer);
        return result;
    }

    // Store buffer info for later cleanup (simplified approach)
    // In a real implementation, you'd maintain a hash table of allocations

    GPU_LOG(device, "Mapped GPU memory 0x%016lX (%lu bytes) to CPU address %p via DRM dumb buffer",
            gpu_address, size, buffer->mapped_ptr);

    *out_cpu_address = (u8*)buffer->mapped_ptr;

    // For simplicity, we're not storing the buffer handle for cleanup
    // In production code, you'd store this in the device or context
    free(buffer);

    return GIBGO_RESULT_SUCCESS;
}

// Unmap GPU memory from CPU address space
GibgoResult gibgo_unmap_gpu_memory(GibgoGPUDevice* device, u8* cpu_address, u64 size) {
    if (!device || !cpu_address) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    if (munmap(cpu_address, size) != 0) {
        GPU_ERROR("Failed to unmap CPU memory at %p (size: %lu): %s",
                  cpu_address, size, strerror(errno));
        return GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED;
    }

    GPU_LOG(device, "Unmapped CPU memory at %p (%lu bytes)", cpu_address, size);
    return GIBGO_RESULT_SUCCESS;
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