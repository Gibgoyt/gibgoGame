#ifndef GPU_DEVICE_H
#define GPU_DEVICE_H

#include "types.h"
#include <sys/types.h>

// Forward declarations
typedef struct GibgoGPUDevice GibgoGPUDevice;
typedef struct GibgoContext GibgoContext;

// Result codes for GPU operations
typedef enum {
    GIBGO_RESULT_SUCCESS = 0,
    GIBGO_RESULT_ERROR_DEVICE_NOT_FOUND,
    GIBGO_RESULT_ERROR_DEVICE_ACCESS_DENIED,
    GIBGO_RESULT_ERROR_MEMORY_MAP_FAILED,
    GIBGO_RESULT_ERROR_OUT_OF_MEMORY,
    GIBGO_RESULT_ERROR_INVALID_PARAMETER,
    GIBGO_RESULT_ERROR_GPU_TIMEOUT,
    GIBGO_RESULT_ERROR_COMMAND_FAILED,
    GIBGO_RESULT_ERROR_DISPLAY_FAILED
} GibgoResult;

// GPU vendor identification
typedef enum {
    GPU_VENDOR_UNKNOWN = 0,
    GPU_VENDOR_INTEL,
    GPU_VENDOR_AMD,
    GPU_VENDOR_NVIDIA
} GibgoGPUVendor;

// GPU device information
typedef struct {
    u16 vendor_id;              // PCI vendor ID
    u16 device_id;              // PCI device ID
    GibgoGPUVendor vendor;      // Detected vendor
    u64 vram_size;              // Total VRAM in bytes
    u64 bar0_base;              // Base address register 0 (memory-mapped registers)
    u64 bar0_size;              // Size of BAR0 region
    u32 max_compute_units;      // Number of shader cores/compute units
    u32 max_clock_frequency;    // Maximum GPU clock in MHz
    char device_name[256];      // Human-readable device name
} GibgoGPUInfo;

// GPU memory region descriptor
typedef struct {
    u64 physical_address;       // Physical GPU memory address
    u64 size;                   // Size of memory region
    u8* mapped_address;         // CPU-accessible mapped address (if mapped)
    u32 memory_type;            // Memory type flags
    b32 is_coherent;            // CPU-GPU coherent memory
    b32 is_device_local;        // Device-local (fast) memory
} GibgoGPUMemoryRegion;

// GPU command ring buffer
typedef struct {
    u32* command_buffer;        // Ring buffer for GPU commands
    u64 buffer_size;            // Size in bytes
    u32 head_offset;            // Current head position (where GPU reads)
    u32 tail_offset;            // Current tail position (where CPU writes)
    u32 capacity;               // Maximum number of commands
    volatile u32* gpu_head_ptr; // GPU hardware head pointer
    volatile u32* gpu_tail_ptr; // GPU hardware tail pointer
} GibgoCommandRing;

// GPU register access interface
typedef struct {
    volatile u32* registers;    // Memory-mapped GPU registers
    u64 register_space_size;    // Size of register space
    u32 command_processor_offset;   // Offset to command processor registers
    u32 memory_controller_offset;   // Offset to memory controller registers
    u32 display_engine_offset;     // Offset to display engine registers
    u32 shader_core_offset;         // Offset to shader core registers
} GibgoGPURegisters;

// Main GPU device structure
struct GibgoGPUDevice {
    // Device identification
    GibgoGPUInfo info;

    // Hardware access
    i32 device_fd;              // File descriptor for GPU device
    GibgoGPURegisters regs;     // Memory-mapped register access

    // Memory management
    GibgoGPUMemoryRegion vram;  // Main VRAM region
    u64 vram_allocation_offset; // Current allocation offset

    // Command submission
    GibgoCommandRing cmd_ring;  // Hardware command ring buffer

    // Synchronization
    u32 fence_counter;          // Global fence counter
    volatile u32* fence_register; // GPU fence register

    // Debug and monitoring
    b32 debug_enabled;          // Enable debug logging
    u64 commands_submitted;     // Statistics counter
    u64 frames_rendered;        // Statistics counter
};

// Graphics rendering context
struct GibgoContext {
    GibgoGPUDevice* device;     // Associated GPU device

    // Framebuffer management
    u32 framebuffer_width;      // Current framebuffer width
    u32 framebuffer_height;     // Current framebuffer height
    u64 framebuffer_address;    // GPU address of current framebuffer
    u32 framebuffer_format;     // Pixel format (RGBA8, etc.)

    // Pipeline state
    u64 vertex_shader_address;  // GPU address of vertex shader
    u64 fragment_shader_address; // GPU address of fragment shader
    u32 primitive_topology;     // Triangle list, etc.

    // Vertex data
    u64 vertex_buffer_address;  // GPU address of vertex buffer
    u32 vertex_buffer_stride;   // Size of each vertex in bytes
    u32 vertex_count;           // Number of vertices to draw

    // Synchronization
    u32 frame_fence;            // Fence for current frame
    u32 current_frame_index;    // For double buffering
};

// Function prototypes

// Device management
GibgoResult gibgo_enumerate_gpus(GibgoGPUInfo** gpu_list, u32* gpu_count);
GibgoResult gibgo_create_device(u32 gpu_index, GibgoGPUDevice** out_device);
GibgoResult gibgo_destroy_device(GibgoGPUDevice* device);

// Context management
GibgoResult gibgo_create_context(GibgoGPUDevice* device, GibgoContext** out_context);
GibgoResult gibgo_destroy_context(GibgoContext* context);

// Memory management
GibgoResult gibgo_allocate_gpu_memory(GibgoGPUDevice* device, u64 size, u64* out_address);
GibgoResult gibgo_free_gpu_memory(GibgoGPUDevice* device, u64 address, u64 size);
GibgoResult gibgo_map_gpu_memory(GibgoGPUDevice* device, u64 gpu_address, u64 size, u8** out_cpu_address);
GibgoResult gibgo_unmap_gpu_memory(GibgoGPUDevice* device, u8* cpu_address, u64 size);

// Command submission
GibgoResult gibgo_begin_commands(GibgoContext* context);
GibgoResult gibgo_end_commands(GibgoContext* context);
GibgoResult gibgo_submit_commands(GibgoContext* context);
GibgoResult gibgo_wait_for_completion(GibgoContext* context, u32 fence_value);

// Drawing operations
GibgoResult gibgo_upload_vertices(GibgoContext* context, void* vertex_data, u32 vertex_count, u32 vertex_stride);
GibgoResult gibgo_load_shaders(GibgoContext* context, u32* vertex_spirv, u32 vertex_size, u32* fragment_spirv, u32 fragment_size);
GibgoResult gibgo_set_viewport(GibgoContext* context, u32 width, u32 height);
GibgoResult gibgo_draw_primitives(GibgoContext* context, u32 vertex_count, u32 first_vertex);
GibgoResult gibgo_present_frame(GibgoContext* context);

// Utility functions
const char* gibgo_result_string(GibgoResult result);
void gibgo_debug_gpu_state(GibgoGPUDevice* device);
void gibgo_debug_context_state(GibgoContext* context);

// Low-level hardware access (for advanced users)
GibgoResult gibgo_write_gpu_register(GibgoGPUDevice* device, u32 offset, u32 value);
GibgoResult gibgo_read_gpu_register(GibgoGPUDevice* device, u32 offset, u32* out_value);
GibgoResult gibgo_flush_gpu_caches(GibgoGPUDevice* device);

#endif // GPU_DEVICE_H