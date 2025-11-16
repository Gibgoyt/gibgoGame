#define _DEFAULT_SOURCE
#include "gpu_device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

// PCI vendor IDs
#define PCI_VENDOR_INTEL    0x8086
#define PCI_VENDOR_AMD      0x1002
#define PCI_VENDOR_NVIDIA   0x10DE

// GPU device classes (PCI class codes)
#define PCI_CLASS_DISPLAY_VGA       0x0300
#define PCI_CLASS_DISPLAY_3D        0x0302

// Maximum number of GPUs to enumerate
#define MAX_GPUS 16

// Debug logging macro
#define GPU_LOG(device, fmt, ...) \
    do { \
        if ((device) && (device)->debug_enabled) { \
            printf("[GPU] " fmt "\n", ##__VA_ARGS__); \
        } \
    } while(0)

// Error logging macro
#define GPU_ERROR(fmt, ...) \
    fprintf(stderr, "[GPU ERROR] " fmt "\n", ##__VA_ARGS__)

// Helper function to read PCI configuration space
static GibgoResult read_pci_config(const char* pci_path, u32 offset, u32* value) {
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/config", pci_path);

    i32 fd = open(config_path, O_RDONLY);
    if (fd < 0) {
        return GIBGO_RESULT_ERROR_DEVICE_ACCESS_DENIED;
    }

    if (lseek(fd, offset, SEEK_SET) < 0) {
        close(fd);
        return GIBGO_RESULT_ERROR_DEVICE_ACCESS_DENIED;
    }

    if (read(fd, value, sizeof(u32)) != sizeof(u32)) {
        close(fd);
        return GIBGO_RESULT_ERROR_DEVICE_ACCESS_DENIED;
    }

    close(fd);
    return GIBGO_RESULT_SUCCESS;
}

// Helper function to get GPU vendor from vendor ID
static GibgoGPUVendor get_gpu_vendor(u16 vendor_id) {
    switch (vendor_id) {
        case PCI_VENDOR_INTEL:  return GPU_VENDOR_INTEL;
        case PCI_VENDOR_AMD:    return GPU_VENDOR_AMD;
        case PCI_VENDOR_NVIDIA: return GPU_VENDOR_NVIDIA;
        default:                return GPU_VENDOR_UNKNOWN;
    }
}

// Helper function to get vendor name string
static const char* get_vendor_name(GibgoGPUVendor vendor) {
    switch (vendor) {
        case GPU_VENDOR_INTEL:  return "Intel";
        case GPU_VENDOR_AMD:    return "AMD";
        case GPU_VENDOR_NVIDIA: return "NVIDIA";
        default:                return "Unknown";
    }
}

// Device enumeration implementation
GibgoResult gibgo_enumerate_gpus(GibgoGPUInfo** gpu_list, u32* gpu_count) {
    if (!gpu_list || !gpu_count) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Allocate array for GPU information
    GibgoGPUInfo* gpus = (GibgoGPUInfo*)malloc(MAX_GPUS * sizeof(GibgoGPUInfo));
    if (!gpus) {
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    u32 found_gpus = 0;

    // Enumerate PCI devices
    DIR* pci_dir = opendir("/sys/bus/pci/devices");
    if (!pci_dir) {
        free(gpus);
        GPU_ERROR("Cannot access PCI devices directory: %s", strerror(errno));
        return GIBGO_RESULT_ERROR_DEVICE_NOT_FOUND;
    }

    struct dirent* entry;
    while ((entry = readdir(pci_dir)) != NULL && found_gpus < MAX_GPUS) {
        if (entry->d_name[0] == '.') continue;

        char pci_path[512];
        snprintf(pci_path, sizeof(pci_path), "/sys/bus/pci/devices/%s", entry->d_name);

        // Read PCI configuration
        u32 vendor_device = 0;
        u32 class_code = 0;

        if (read_pci_config(pci_path, 0x00, &vendor_device) != GIBGO_RESULT_SUCCESS) {
            continue;
        }

        if (read_pci_config(pci_path, 0x08, &class_code) != GIBGO_RESULT_SUCCESS) {
            continue;
        }

        // Extract vendor and device IDs
        u16 vendor_id = (u16)(vendor_device & 0xFFFF);
        u16 device_id = (u16)((vendor_device >> 16) & 0xFFFF);
        u32 class_id = (class_code >> 16) & 0xFFFF;

        // Check if this is a display/graphics device
        if (class_id != PCI_CLASS_DISPLAY_VGA && class_id != PCI_CLASS_DISPLAY_3D) {
            continue;
        }

        // Check if this is a supported GPU vendor
        GibgoGPUVendor vendor = get_gpu_vendor(vendor_id);
        if (vendor == GPU_VENDOR_UNKNOWN) {
            continue;
        }

        // Read BAR0 for memory-mapped register access
        u32 bar0_low = 0;
        if (read_pci_config(pci_path, 0x10, &bar0_low) != GIBGO_RESULT_SUCCESS) {
            continue;
        }

        // Check if BAR0 is 64-bit (bit 2-1 = 10) or 32-bit (bit 2-1 = 00)
        u32 bar_type = (bar0_low >> 1) & 0x3;
        u64 bar0_address;

        if (bar_type == 2) {
            // 64-bit BAR - read high part too
            u32 bar0_high = 0;
            if (read_pci_config(pci_path, 0x14, &bar0_high) != GIBGO_RESULT_SUCCESS) {
                continue;
            }
            bar0_address = ((u64)bar0_high << 32) | (bar0_low & ~0xF);
        } else {
            // 32-bit BAR - use only low part
            bar0_address = bar0_low & ~0xF;
        }

        // Populate GPU info structure
        GibgoGPUInfo* gpu = &gpus[found_gpus];
        memset(gpu, 0, sizeof(GibgoGPUInfo));

        gpu->vendor_id = vendor_id;
        gpu->device_id = device_id;
        gpu->vendor = vendor;
        gpu->bar0_base = bar0_address;
        gpu->bar0_size = 0x1000000; // Default 16MB register space, will be refined later

        // Create device name
        snprintf(gpu->device_name, sizeof(gpu->device_name),
                "%s GPU [%04X:%04X]", get_vendor_name(vendor), vendor_id, device_id);

        // Estimate VRAM size based on vendor (will be refined with actual hardware access)
        switch (vendor) {
            case GPU_VENDOR_INTEL:
                gpu->vram_size = 512 * 1024 * 1024; // 512MB default for integrated Intel
                gpu->max_compute_units = 24;
                gpu->max_clock_frequency = 1200;
                break;
            case GPU_VENDOR_AMD:
                gpu->vram_size = 8ULL * 1024 * 1024 * 1024; // 8GB default for discrete AMD
                gpu->max_compute_units = 64;
                gpu->max_clock_frequency = 2400;
                break;
            case GPU_VENDOR_NVIDIA:
                gpu->vram_size = 8ULL * 1024 * 1024 * 1024; // 8GB default for discrete NVIDIA
                gpu->max_compute_units = 128;
                gpu->max_clock_frequency = 1900;
                break;
            default:
                gpu->vram_size = 1024 * 1024 * 1024; // 1GB default
                gpu->max_compute_units = 16;
                gpu->max_clock_frequency = 1000;
                break;
        }

        printf("[GPU] Found GPU: %s at BAR0=0x%016lX\n", gpu->device_name, gpu->bar0_base);
        found_gpus++;
    }

    closedir(pci_dir);

    if (found_gpus == 0) {
        free(gpus);
        GPU_ERROR("No supported GPUs found");
        return GIBGO_RESULT_ERROR_DEVICE_NOT_FOUND;
    }

    *gpu_list = gpus;
    *gpu_count = found_gpus;
    return GIBGO_RESULT_SUCCESS;
}

// Create DRM framebuffer for direct triangle rendering
static int create_drm_framebuffer(int drm_fd, u32 width, u32 height, u32** framebuffer_out, u32* fb_size_out, u32* fb_handle_out) {
    // Create dumb buffer for framebuffer
    struct drm_mode_create_dumb create_req = {0};
    create_req.width = width;
    create_req.height = height;
    create_req.bpp = 32; // 32 bits per pixel (RGBA)

    if (ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        return -1;
    }

    // Map the buffer for CPU access
    struct drm_mode_map_dumb map_req = {0};
    map_req.handle = create_req.handle;

    if (ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) < 0) {
        return -1;
    }

    // Memory map the framebuffer
    void* framebuffer = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map_req.offset);
    if (framebuffer == MAP_FAILED) {
        return -1;
    }

    *framebuffer_out = (u32*)framebuffer;
    *fb_size_out = create_req.size;
    *fb_handle_out = create_req.handle;

    return 0; // Success
}

// Removed old triangle rasterizer - now using 3D cube rendering system

// Clear framebuffer to a solid color (preparation for 3D rendering)
static void clear_framebuffer_to_color(u32* framebuffer, u32 width, u32 height, u32 color) {
    for (u32 i = 0; i < width * height; i++) {
        framebuffer[i] = color;
    }
}

// Device creation implementation
GibgoResult gibgo_create_device(u32 gpu_index, GibgoGPUDevice** out_device) {
    if (!out_device) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // First enumerate GPUs to get the requested device
    GibgoGPUInfo* gpu_list = NULL;
    u32 gpu_count = 0;
    GibgoResult result = gibgo_enumerate_gpus(&gpu_list, &gpu_count);
    if (result != GIBGO_RESULT_SUCCESS) {
        return result;
    }

    if (gpu_index >= gpu_count) {
        free(gpu_list);
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Allocate device structure
    GibgoGPUDevice* device = (GibgoGPUDevice*)calloc(1, sizeof(GibgoGPUDevice));
    if (!device) {
        free(gpu_list);
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    // Copy GPU info
    device->info = gpu_list[gpu_index];
    device->debug_enabled = 1; // Enable debug by default
    device->device_fd = -1;

    GPU_LOG(device, "Creating device for GPU: %s", device->info.device_name);

    // Open DRM device for modern GPU framebuffer rendering
    char drm_path[256];
    for (u32 i = 0; i < 16; i++) {
        snprintf(drm_path, sizeof(drm_path), "/dev/dri/card%u", i);
        device->device_fd = open(drm_path, O_RDWR);
        if (device->device_fd >= 0) {
            GPU_LOG(device, "✅ SUCCESS: Opened DRM device: %s", drm_path);
            GPU_LOG(device, "🎯 MODERN GPU ACCESS: Direct framebuffer rendering enabled");
            GPU_LOG(device, "🚀 TRIANGLE RENDERING: CPU rasterizer + GPU memory");
            break;
        }
    }

    if (device->device_fd < 0) {
        GPU_ERROR("Cannot open GPU device - try running as root or with DRI permissions");
        free(gpu_list);
        free(device);
        return GIBGO_RESULT_ERROR_DEVICE_ACCESS_DENIED;
    }

    // Create DRM framebuffer for triangle rendering
    u32* framebuffer = NULL;
    u32 fb_size = 0;
    u32 fb_handle = 0;
    u32 fb_width = 800;
    u32 fb_height = 600;

    if (create_drm_framebuffer(device->device_fd, fb_width, fb_height, &framebuffer, &fb_size, &fb_handle) != 0) {
        GPU_ERROR("Failed to create DRM framebuffer: %s", strerror(errno));
        close(device->device_fd);
        free(gpu_list);
        free(device);
        return GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED;
    }

    GPU_LOG(device, "✅ Created DRM framebuffer: %ux%u pixels (%u bytes)", fb_width, fb_height, fb_size);
    GPU_LOG(device, "📍 Framebuffer mapped at %p", framebuffer);

    // Store framebuffer info in device (repurpose register fields for framebuffer)
    device->regs.registers = (volatile u32*)framebuffer;
    device->regs.register_space_size = fb_size;

    // Clear framebuffer to prepare for 3D rendering
    GPU_LOG(device, "🎨 Clearing framebuffer for 3D rendering...");
    clear_framebuffer_to_color(framebuffer, fb_width, fb_height, 0xFF222222); // Dark gray background
    GPU_LOG(device, "✅ Framebuffer cleared and ready for 3D rendering!");

    // Set up register offsets (vendor-specific, these are examples)
    switch (device->info.vendor) {
        case GPU_VENDOR_INTEL:
            device->regs.command_processor_offset = 0x2000;
            device->regs.memory_controller_offset = 0x4000;
            device->regs.display_engine_offset = 0x6000;
            device->regs.shader_core_offset = 0x8000;
            break;
        case GPU_VENDOR_AMD:
            device->regs.command_processor_offset = 0x8000;
            device->regs.memory_controller_offset = 0xC000;
            device->regs.display_engine_offset = 0x6000;
            device->regs.shader_core_offset = 0x20000;
            break;
        case GPU_VENDOR_NVIDIA:
            device->regs.command_processor_offset = 0x10000;
            device->regs.memory_controller_offset = 0x100000;
            device->regs.display_engine_offset = 0x600000;
            device->regs.shader_core_offset = 0x400000;
            break;
        default:
            // Generic fallback
            device->regs.command_processor_offset = 0x1000;
            device->regs.memory_controller_offset = 0x2000;
            device->regs.display_engine_offset = 0x3000;
            device->regs.shader_core_offset = 0x4000;
            break;
    }

    // Initialize VRAM memory region
    device->vram.physical_address = device->info.bar0_base + 0x01000000; // Offset past registers
    device->vram.size = device->info.vram_size;
    device->vram.is_device_local = 1;
    device->vram_allocation_offset = 0;

    // Initialize command ring buffer
    device->cmd_ring.capacity = 1024; // 1024 command slots
    device->cmd_ring.buffer_size = device->cmd_ring.capacity * sizeof(u32);
    device->cmd_ring.command_buffer = (u32*)malloc(device->cmd_ring.buffer_size);
    if (!device->cmd_ring.command_buffer) {
        munmap((void*)device->regs.registers, device->regs.register_space_size);
        close(device->device_fd);
        free(gpu_list);
        free(device);
        return GIBGO_RESULT_ERROR_OUT_OF_MEMORY;
    }

    device->cmd_ring.head_offset = 0;
    device->cmd_ring.tail_offset = 0;

    // Set up fence register pointer
    device->fence_register = &device->regs.registers[device->regs.command_processor_offset / sizeof(u32) + 0x100];
    device->fence_counter = 1;

    GPU_LOG(device, "GPU device created successfully");
    GPU_LOG(device, "  VRAM: %lu MB at 0x%016lX", device->vram.size / (1024*1024), device->vram.physical_address);
    GPU_LOG(device, "  Command ring: %u slots", device->cmd_ring.capacity);

    free(gpu_list);
    *out_device = device;
    return GIBGO_RESULT_SUCCESS;
}

// Device destruction
GibgoResult gibgo_destroy_device(GibgoGPUDevice* device) {
    if (!device) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    GPU_LOG(device, "Destroying GPU device");

    // Free command ring buffer
    if (device->cmd_ring.command_buffer) {
        free(device->cmd_ring.command_buffer);
    }

    // Unmap GPU registers
    if (device->regs.registers && device->regs.registers != MAP_FAILED) {
        munmap((void*)device->regs.registers, device->regs.register_space_size);
    }

    // Close device file descriptor
    if (device->device_fd >= 0) {
        close(device->device_fd);
    }

    free(device);
    return GIBGO_RESULT_SUCCESS;
}

// Result string conversion
const char* gibgo_result_string(GibgoResult result) {
    switch (result) {
        case GIBGO_RESULT_SUCCESS:                  return "Success";
        case GIBGO_RESULT_ERROR_DEVICE_NOT_FOUND:   return "Device not found";
        case GIBGO_RESULT_ERROR_DEVICE_ACCESS_DENIED: return "Device access denied";
        case GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED:  return "Memory mapping failed";
        case GIBGO_RESULT_ERROR_OUT_OF_MEMORY:      return "Out of memory";
        case GIBGO_RESULT_ERROR_INVALID_PARAMETER:  return "Invalid parameter";
        case GIBGO_RESULT_ERROR_GPU_TIMEOUT:        return "GPU operation timeout";
        case GIBGO_RESULT_ERROR_COMMAND_FAILED:     return "Command execution failed";
        case GIBGO_RESULT_ERROR_DISPLAY_FAILED:     return "Display operation failed";
        default:                                    return "Unknown error";
    }
}

// Debug functions
void gibgo_debug_gpu_state(GibgoGPUDevice* device) {
    if (!device) return;

    printf("\n=== GPU Device State ===\n");
    printf("Device: %s\n", device->info.device_name);
    printf("Vendor: %s (0x%04X)\n", get_vendor_name(device->info.vendor), device->info.vendor_id);
    printf("Device ID: 0x%04X\n", device->info.device_id);
    printf("BAR0 Base: 0x%016lX\n", device->info.bar0_base);
    printf("VRAM Size: %lu MB\n", device->info.vram_size / (1024*1024));
    printf("VRAM Used: %lu MB\n", device->vram_allocation_offset / (1024*1024));
    printf("Commands Submitted: %lu\n", device->commands_submitted);
    printf("Frames Rendered: %lu\n", device->frames_rendered);
    printf("Current Fence: %u\n", device->fence_counter);
    printf("========================\n\n");
}

// Low-level register access
GibgoResult gibgo_write_gpu_register(GibgoGPUDevice* device, u32 offset, u32 value) {
    if (!device || !device->regs.registers) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    if (offset >= device->regs.register_space_size) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    device->regs.registers[offset / sizeof(u32)] = value;
    return GIBGO_RESULT_SUCCESS;
}

GibgoResult gibgo_read_gpu_register(GibgoGPUDevice* device, u32 offset, u32* out_value) {
    if (!device || !device->regs.registers || !out_value) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    if (offset >= device->regs.register_space_size) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    *out_value = device->regs.registers[offset / sizeof(u32)];
    return GIBGO_RESULT_SUCCESS;
}

GibgoResult gibgo_flush_gpu_caches(GibgoGPUDevice* device) {
    if (!device) {
        return GIBGO_RESULT_ERROR_INVALID_PARAMETER;
    }

    // Vendor-specific cache flush commands would go here
    GPU_LOG(device, "Flushing GPU caches");

    // For now, just perform a memory barrier
    __sync_synchronize();

    return GIBGO_RESULT_SUCCESS;
}