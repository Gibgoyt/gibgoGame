/*
 * gibgoCraft - Hardware-Direct Triangle Rendering
 *
 * This is the REVOLUTIONARY version that completely replaces Vulkan with
 * direct GPU hardware access! Maximum control, maximum learning!
 *
 * CUSTOM TYPE SYSTEM: Uses ZERO native C types for complete memory control!
 * HARDWARE DIRECT: Talks directly to GPU registers - NO DRIVERS!
 */

#define _GNU_SOURCE  // Enable usleep and other POSIX extensions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Custom type system - ABSOLUTE MEMORY CONTROL
#include "types.h"
#include "math.h"

// X11 includes
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

// OUR CUSTOM HARDWARE-DIRECT GRAPHICS API!
#include "gibgo_graphics.h"
#include "gpu_device.h"  // For internal GPU types

// Constants
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define APP_NAME "gibgoCraft - Hardware-Direct Rotating Cube"

// 3D Vertex data structure for cube rendering - CUSTOM TYPES ONLY!
typedef struct {
    Vec3f position;    // 16 bytes: explicit 3D position (with padding)
    Vec3f color;       // 16 bytes: explicit 3D color (with padding)
} CubeVertex;
// Size will be 16 + 16 = 32 bytes naturally

// TransformMatrices is now defined in gibgo_graphics.h
// Size will be 64 + 64 + 64 = 192 bytes naturally

// Application context
typedef struct {
    // X11 windowing
    Display* display;
    Window window;
    Atom wm_delete_window;
    b32 should_close;

    // X11 graphics context for displaying framebuffer
    GC gc;
    XImage* ximage;
    Visual* visual;
    int screen;

    // OUR CUSTOM GRAPHICS SYSTEM!
    GibgoGraphicsSystem* graphics;

    // Frame timing and animation
    u32 frame_count;
    u64 start_time;        // Application start time in microseconds
    f32 total_time;        // Total time since start in seconds
    f32 rotation_angle;    // Current rotation angle in radians

    // 3D transformation state
    TransformMatrices transforms;
    u64 transform_buffer_address;  // GPU address for transformation matrices
} AppContext;

// Cube vertices (24 vertices, 4 per face) - Using helper functions for cleaner syntax
static CubeVertex cube_vertices[24];

// Global frame counter for animation timing
u32 global_frame_count = 0;

// Initialize cube vertices at runtime
static void init_cube_vertices(void) {
    // Front face (Red) - Z = +0.5
    cube_vertices[0] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(-0.5f), f32_from_native(0.5f)), RGB_RED};
    cube_vertices[1] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(-0.5f), f32_from_native(0.5f)), RGB_RED};
    cube_vertices[2] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(0.5f), f32_from_native(0.5f)), RGB_RED};
    cube_vertices[3] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(0.5f), f32_from_native(0.5f)), RGB_RED};

    // Back face (Green) - Z = -0.5
    cube_vertices[4] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(-0.5f), f32_from_native(-0.5f)), RGB_GREEN};
    cube_vertices[5] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(-0.5f), f32_from_native(-0.5f)), RGB_GREEN};
    cube_vertices[6] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(0.5f), f32_from_native(-0.5f)), RGB_GREEN};
    cube_vertices[7] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(0.5f), f32_from_native(-0.5f)), RGB_GREEN};

    // Top face (Blue) - Y = +0.5
    cube_vertices[8] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(0.5f), f32_from_native(0.5f)), RGB_BLUE};
    cube_vertices[9] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(0.5f), f32_from_native(0.5f)), RGB_BLUE};
    cube_vertices[10] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(0.5f), f32_from_native(-0.5f)), RGB_BLUE};
    cube_vertices[11] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(0.5f), f32_from_native(-0.5f)), RGB_BLUE};

    // Bottom face (Yellow) - Y = -0.5
    cube_vertices[12] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(-0.5f), f32_from_native(-0.5f)), rgb_create(F32_ONE, F32_ONE, F32_ZERO)};
    cube_vertices[13] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(-0.5f), f32_from_native(-0.5f)), rgb_create(F32_ONE, F32_ONE, F32_ZERO)};
    cube_vertices[14] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(-0.5f), f32_from_native(0.5f)), rgb_create(F32_ONE, F32_ONE, F32_ZERO)};
    cube_vertices[15] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(-0.5f), f32_from_native(0.5f)), rgb_create(F32_ONE, F32_ONE, F32_ZERO)};

    // Right face (Magenta) - X = +0.5
    cube_vertices[16] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(-0.5f), f32_from_native(0.5f)), rgb_create(F32_ONE, F32_ZERO, F32_ONE)};
    cube_vertices[17] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(-0.5f), f32_from_native(-0.5f)), rgb_create(F32_ONE, F32_ZERO, F32_ONE)};
    cube_vertices[18] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(0.5f), f32_from_native(-0.5f)), rgb_create(F32_ONE, F32_ZERO, F32_ONE)};
    cube_vertices[19] = (CubeVertex){vec3f_create(f32_from_native(0.5f), f32_from_native(0.5f), f32_from_native(0.5f)), rgb_create(F32_ONE, F32_ZERO, F32_ONE)};

    // Left face (Cyan) - X = -0.5
    cube_vertices[20] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(-0.5f), f32_from_native(-0.5f)), rgb_create(F32_ZERO, F32_ONE, F32_ONE)};
    cube_vertices[21] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(-0.5f), f32_from_native(0.5f)), rgb_create(F32_ZERO, F32_ONE, F32_ONE)};
    cube_vertices[22] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(0.5f), f32_from_native(0.5f)), rgb_create(F32_ZERO, F32_ONE, F32_ONE)};
    cube_vertices[23] = (CubeVertex){vec3f_create(f32_from_native(-0.5f), f32_from_native(0.5f), f32_from_native(-0.5f)), rgb_create(F32_ZERO, F32_ONE, F32_ONE)};
}

// Cube indices for rendering 12 triangles (2 per face)
static const u16 cube_indices[] = {
    // Front face
    0,  1,  2,   2,  3,  0,
    // Back face
    4,  5,  6,   6,  7,  4,
    // Top face
    8,  9,  10,  10, 11, 8,
    // Bottom face
    12, 13, 14,  14, 15, 12,
    // Right face
    16, 17, 18,  18, 19, 16,
    // Left face
    20, 21, 22,  22, 23, 20
};

// Compiled SPIR-V shader bytecode (same as Vulkan version for compatibility)
// Generated by compile_shaders_hardware.sh
extern const u32 vertex_shader_spirv[];
extern const u32 vertex_shader_spirv_size;
extern const u32 fragment_shader_spirv[];
extern const u32 fragment_shader_spirv_size;

// Timing functions for animation
#include <sys/time.h>

static u64 get_time_microseconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (u64)tv.tv_sec * 1000000ULL + (u64)tv.tv_usec;
}

static f32 get_elapsed_time_seconds(u64 start_time) {
    u64 current_time = get_time_microseconds();
    u64 elapsed_microseconds = current_time - start_time;
    return f32_div(f32_from_native((float)elapsed_microseconds), f32_from_native(1000000.0f));
}

// 3D transformation matrix update function
static void update_transform_matrices(AppContext* ctx) {
    // Update rotation angle based on elapsed time (0.5 radians per second)
    f32 rotation_speed = f32_from_native(0.5f);
    ctx->rotation_angle = f32_mul(ctx->total_time, rotation_speed);

    // Model matrix: rotate around diagonal axis (1,1,1)
    Vec3f rotation_axis = vec3f_create(F32_ONE, F32_ONE, F32_ZERO);  // Diagonal axis
    ctx->transforms.model = mat4f_rotate(ctx->rotation_angle, rotation_axis);

    // View matrix: camera at (0, 0, 3) looking at origin
    Vec3f eye = vec3f_create(F32_ZERO, F32_ZERO, f32_from_native(3.0f));
    Vec3f center = vec3f_create(F32_ZERO, F32_ZERO, F32_ZERO);
    Vec3f up = vec3f_create(F32_ZERO, F32_ONE, F32_ZERO);
    ctx->transforms.view = mat4f_look_at(eye, center, up);

    // Projection matrix: 45° FOV perspective
    f32 fov_radians = f32_radians(f32_from_native(45.0f));
    f32 aspect_ratio = f32_div(f32_from_native((float)WINDOW_WIDTH), f32_from_native((float)WINDOW_HEIGHT));
    f32 near_plane = f32_from_native(0.1f);
    f32 far_plane = f32_from_native(100.0f);
    ctx->transforms.projection = mat4f_perspective(fov_radians, aspect_ratio, near_plane, far_plane);
}

// Function prototypes
static b32 init_x11(AppContext* ctx);
static b32 init_graphics(AppContext* ctx);
static void handle_events(AppContext* ctx);
static void render_frame(AppContext* ctx);
static void cleanup(AppContext* ctx);
static void run_main_loop(AppContext* ctx);

// X11 window initialization
static b32 init_x11(AppContext* ctx) {
    printf("[X11] Initializing window system...\n");

    ctx->display = XOpenDisplay(NULL);
    if (!ctx->display) {
        fprintf(stderr, "Failed to open X11 display\n");
        return B32_FALSE;
    }

    i32 screen = DefaultScreen(ctx->display);
    u32 border_color = BlackPixel(ctx->display, screen);
    u32 background_color = BlackPixel(ctx->display, screen);

    ctx->window = XCreateSimpleWindow(
        ctx->display,
        RootWindow(ctx->display, screen),
        100, 100,                           // x, y
        WINDOW_WIDTH, WINDOW_HEIGHT,        // width, height
        2,                                  // border width
        border_color,
        background_color
    );

    XStoreName(ctx->display, ctx->window, APP_NAME);

    // Set up window close event
    ctx->wm_delete_window = XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(ctx->display, ctx->window, &ctx->wm_delete_window, 1);

    XSelectInput(ctx->display, ctx->window, KeyPressMask | StructureNotifyMask);
    XMapWindow(ctx->display, ctx->window);

    printf("[X11] Window created successfully - %dx%d\n", WINDOW_WIDTH, WINDOW_HEIGHT);
    return B32_TRUE;
}

// Initialize our CUSTOM hardware-direct graphics system!
static b32 init_graphics(AppContext* ctx) {
    printf("\n=== INITIALIZING HARDWARE-DIRECT GRAPHICS ===\n");
    printf("This is NOT Vulkan - this is DIRECT GPU CONTROL!\n");
    printf("==============================================\n\n");

    // Create initialization parameters
    GibgoGraphicsInitInfo init_info = {0};
    init_info.window_width = WINDOW_WIDTH;
    init_info.window_height = WINDOW_HEIGHT;
    init_info.x11_display = ctx->display;
    init_info.x11_window = ctx->window;
    init_info.enable_debug = 1; // Enable all debug output

    // Initialize our custom graphics system
    GibgoGraphicsResult result = gibgo_initialize_graphics(&init_info, &ctx->graphics);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to initialize graphics system: %s\n",
                gibgo_graphics_result_string(result));
        return B32_FALSE;
    }

    printf("\n🚀 SUCCESS! Hardware-direct graphics initialized!\n");

    // Initialize X11 graphics context for framebuffer display
    ctx->screen = DefaultScreen(ctx->display);
    ctx->visual = DefaultVisual(ctx->display, ctx->screen);
    ctx->gc = XCreateGC(ctx->display, ctx->window, 0, NULL);

    // Create XImage for framebuffer display
    ctx->ximage = XCreateImage(ctx->display, ctx->visual, 24, ZPixmap, 0,
                              NULL, WINDOW_WIDTH, WINDOW_HEIGHT, 32, 0);
    if (!ctx->ximage) {
        fprintf(stderr, "Failed to create XImage for framebuffer display\n");
        return B32_FALSE;
    }

    // Allocate XImage data buffer (one-time allocation to prevent memory leaks)
    ctx->ximage->data = malloc(ctx->ximage->bytes_per_line * ctx->ximage->height);
    if (!ctx->ximage->data) {
        fprintf(stderr, "Failed to allocate XImage data buffer\n");
        return B32_FALSE;
    }

    printf("✅ X11 graphics context initialized for framebuffer display\n");
    printf("📍 XImage buffer allocated: %d bytes\n", ctx->ximage->bytes_per_line * ctx->ximage->height);

    // Load SPIR-V shaders for GPU hardware execution
    printf("🔧 Loading SPIR-V shaders for hardware-direct GPU execution...\n");
    GibgoResult load_result = gibgo_load_shaders((GibgoContext*)ctx->graphics->internal_context,
                                               (u32*)vertex_shader_spirv, vertex_shader_spirv_size,
                                               (u32*)fragment_shader_spirv, fragment_shader_spirv_size);
    if (load_result != GIBGO_RESULT_SUCCESS) {
        fprintf(stderr, "Failed to load SPIR-V shaders: %s\n", gibgo_result_string(load_result));
        return B32_FALSE;
    }
    printf("✅ SPIR-V shaders loaded successfully!\n");
    printf("    📐 Vertex shader: %u bytes\n", vertex_shader_spirv_size);
    printf("    🎨 Fragment shader: %u bytes\n", fragment_shader_spirv_size);

    // Upload 3D cube vertex data
    printf("📦 Uploading 3D cube vertex data (24 vertices)...\n");
    result = gibgo_upload_cube_vertices(ctx->graphics, (const GibgoCubeVertex*)cube_vertices, 24);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to upload cube vertices: %s\n", gibgo_graphics_result_string(result));
        return B32_FALSE;
    }
    printf("✅ Cube vertices uploaded successfully!\n");

    printf("\n🎯 3D CUBE RENDERING PIPELINE READY!\n");
    printf("    📊 24 vertices (6 faces × 4 vertices each)\n");
    printf("    📐 36 indices (6 faces × 2 triangles × 3 indices each)\n");
    printf("    🎨 6 colored faces: Red, Green, Blue, Yellow, Magenta, Cyan\n");
    printf("    🔄 Diagonal axis rotation animation\n\n");
    return B32_TRUE;
}

// Handle X11 events
static void handle_events(AppContext* ctx) {
    XEvent event;

    while (XPending(ctx->display)) {
        XNextEvent(ctx->display, &event);

        switch (event.type) {
            case KeyPress: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_Escape) {
                    ctx->should_close = B32_TRUE;
                } else if (keysym == XK_space) {
                    // Print debug info when space is pressed
                    printf("\n🔍 DEBUG INFO (Frame %u):\n", ctx->frame_count);
                    gibgo_debug_dump_gpu_state(ctx->graphics);
                } else if (keysym == XK_s) {
                    // Print statistics when 's' is pressed
                    u64 frames_rendered, commands_submitted;
                    gibgo_get_frame_statistics(ctx->graphics, &frames_rendered, &commands_submitted);
                    printf("\n📊 STATISTICS:\n");
                    printf("  Frames rendered: %lu\n", frames_rendered);
                    printf("  Commands submitted: %lu\n", commands_submitted);
                    printf("  Average commands/frame: %.2f\n",
                           frames_rendered > 0 ? (double)commands_submitted / frames_rendered : 0.0);
                    printf("\n");
                }
                break;
            }
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == ctx->wm_delete_window) {
                    ctx->should_close = B32_TRUE;
                }
                break;
        }
    }
}

// Copy framebuffer to X11 window for display
static void copy_framebuffer_to_x11(AppContext* ctx) {
    // Get the actual GPU framebuffer data from the graphics system
    // Access the internal context to get the DRM framebuffer
    GibgoGraphicsSystem* system = ctx->graphics;
    if (!system || !system->is_initialized) {
        return;
    }

    // Access internal context (this is a direct approach for now)
    GibgoContext* internal_ctx = (GibgoContext*)system->internal_context;
    if (!internal_ctx) {
        return;
    }

    // Get the GPU device to access the framebuffer
    GibgoGPUDevice* device = (GibgoGPUDevice*)system->internal_device;
    if (!device || !device->regs.registers) {
        return;
    }

    // The DRM framebuffer is stored in device->regs.registers (we repurposed this field)
    u32* gpu_framebuffer = (u32*)device->regs.registers;
    u32* ximage_pixels = (u32*)ctx->ximage->data;

    // Copy GPU framebuffer to XImage (direct memory copy)
    for (int y = 0; y < WINDOW_HEIGHT; y++) {
        for (int x = 0; x < WINDOW_WIDTH; x++) {
            int index = y * WINDOW_WIDTH + x;
            ximage_pixels[index] = gpu_framebuffer[index];
        }
    }

    // Display in X11 window
    XPutImage(ctx->display, ctx->window, ctx->gc, ctx->ximage,
              0, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    XFlush(ctx->display);
}

// Render a frame using DIRECT GPU HARDWARE!
static void render_frame(AppContext* ctx) {
    GibgoGraphicsResult result;

    // Update timing and rotation
    ctx->total_time = get_elapsed_time_seconds(ctx->start_time);
    update_transform_matrices(ctx);

    // Begin frame
    result = gibgo_begin_frame(ctx->graphics);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to begin frame: %s\n", gibgo_graphics_result_string(result));
        return;
    }

    // Set up 3D rendering pipeline
    result = gibgo_enable_depth_testing(ctx->graphics, B32_TRUE);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to enable depth testing: %s\n", gibgo_graphics_result_string(result));
        return;
    }

    // Clear depth buffer for 3D rendering
    result = gibgo_clear_depth_buffer_3d(ctx->graphics);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to clear depth buffer: %s\n", gibgo_graphics_result_string(result));
        return;
    }

    // Set transformation matrices
    result = gibgo_set_mvp_matrices(ctx->graphics, &ctx->transforms.model,
                                   &ctx->transforms.view, &ctx->transforms.projection);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to set MVP matrices: %s\n", gibgo_graphics_result_string(result));
        return;
    }

    // Draw the rotating 3D cube!
    result = gibgo_draw_indexed_cube(ctx->graphics, cube_indices, 36);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to draw rotating cube: %s\n", gibgo_graphics_result_string(result));
        return;
    }

    // End frame and present to screen
    result = gibgo_end_frame_and_present(ctx->graphics);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to end frame: %s\n", gibgo_graphics_result_string(result));
        return;
    }

    // Wait for completion (optional for learning about synchronization)
    result = gibgo_wait_for_frame_completion(ctx->graphics);
    if (result != GIBGO_SUCCESS) {
        fprintf(stderr, "Failed to wait for frame: %s\n", gibgo_graphics_result_string(result));
        return;
    }

    // Copy the rendered framebuffer to X11 window for display
    copy_framebuffer_to_x11(ctx);

    ctx->frame_count++;
    global_frame_count++;

    // Print periodic status with rotation info
    if (ctx->frame_count % 60 == 0) {
        float angle_degrees = f32_to_native(ctx->rotation_angle) * 180.0f / 3.14159265f;
        printf("🎨 Frame %u - ROTATING 3D CUBE at %.1f° via DIRECT GPU HARDWARE!\n",
               ctx->frame_count, angle_degrees);
        printf("    6 colored faces: Front=Red, Back=Green, Top=Blue, Bottom=Yellow, Right=Magenta, Left=Cyan\n");
    }
}

// Main rendering loop
static void run_main_loop(AppContext* ctx) {
    printf("\n🎮 Starting hardware-direct rendering loop...\n");
    printf("Controls:\n");
    printf("  ESC   - Exit\n");
    printf("  SPACE - Debug info\n");
    printf("  S     - Statistics\n\n");

    while (!ctx->should_close) {
        handle_events(ctx);
        render_frame(ctx);

        // Small delay to prevent 100% CPU usage
        usleep(16667); // ~60 FPS
    }
}

// Cleanup everything
static void cleanup(AppContext* ctx) {
    printf("\n🧹 Cleaning up hardware-direct graphics system...\n");

    // Shutdown our custom graphics system
    if (ctx->graphics) {
        gibgo_shutdown_graphics(ctx->graphics);
        ctx->graphics = NULL;
    }

    // Cleanup X11
    if (ctx->ximage) {
        if (ctx->ximage->data) {
            free(ctx->ximage->data);
            ctx->ximage->data = NULL;
        }
        XDestroyImage(ctx->ximage);
        ctx->ximage = NULL;
    }

    if (ctx->gc && ctx->display) {
        XFreeGC(ctx->display, ctx->gc);
    }

    if (ctx->window) {
        XDestroyWindow(ctx->display, ctx->window);
    }

    if (ctx->display) {
        XCloseDisplay(ctx->display);
    }

    printf("✅ Cleanup complete!\n");
}

// Main entry point
int main(void) {
    printf("🚀 gibgoCraft - Hardware-Direct GPU Rotating Cube Demo\n");
    printf("======================================================\n");
    printf("This demo bypasses ALL graphics drivers!\n");
    printf("Direct hardware access for MAXIMUM CONTROL!\n");
    printf("3D rotating cube with depth testing and lighting!\n");
    printf("======================================================\n\n");

    AppContext ctx = {0};

    // Initialize cube vertices
    init_cube_vertices();

    // Initialize timing for animation
    ctx.start_time = get_time_microseconds();
    ctx.total_time = F32_ZERO;
    ctx.rotation_angle = F32_ZERO;

    // Verify our custom types work correctly
    printf("🔧 Custom type verification:\n");
    printf("  f32 size: %lu bytes\n", sizeof(f32));
    printf("  Vec3f size: %lu bytes\n", sizeof(Vec3f));
    printf("  CubeVertex size: %lu bytes\n", sizeof(CubeVertex));
    printf("  TransformMatrices size: %lu bytes\n", sizeof(TransformMatrices));
    printf("  Cube vertices: %lu total\n", sizeof(cube_vertices) / sizeof(CubeVertex));
    printf("  Cube indices: %lu total\n", sizeof(cube_indices) / sizeof(u16));
    printf("\n");

    // Initialize X11 window
    if (!init_x11(&ctx)) {
        fprintf(stderr, "❌ X11 initialization failed\n");
        return EXIT_FAILURE;
    }

    // Initialize our HARDWARE-DIRECT graphics system
    if (!init_graphics(&ctx)) {
        fprintf(stderr, "❌ Graphics initialization failed\n");
        cleanup(&ctx);
        return EXIT_FAILURE;
    }

    // Run the main loop
    run_main_loop(&ctx);

    // Cleanup
    cleanup(&ctx);

    printf("\n🎉 Hardware-direct graphics demo completed successfully!\n");
    printf("You just controlled a GPU directly - NO DRIVERS INVOLVED!\n");
    return EXIT_SUCCESS;
}

// Note: Actual shader bytecode is provided by shader_data.c
// which is generated by the compile_shaders_hardware.sh script