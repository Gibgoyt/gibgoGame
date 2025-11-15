/*
 * gibgoCraft - Triangle Rendering Test
 *
 * This test demonstrates basic triangle rendering using Vulkan.
 * It includes the complete graphics pipeline setup and draw commands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

// X11 includes
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

// Vulkan includes
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>

// Constants
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define APP_NAME "gibgoCraft - Triangle"
#define ENGINE_NAME "gibgoCraft Engine"
#define MAX_FRAMES_IN_FLIGHT 2

// Vertex data structure
typedef struct {
    float pos[2];
    float color[3];
} Vertex;

// Triangle vertices (position + color)
static const Vertex vertices[] = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},   // Top vertex - Red
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},    // Bottom right - Green
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}    // Bottom left - Blue
};

// Embedded SPIR-V shaders (compiled from GLSL)
// vertex.vert: #version 450, layout(location = 0) in vec2 inPosition, layout(location = 1) in vec3 inColor
static const uint32_t vertex_shader_spv[] = {
  0x07230203, 0x00010000, 0x0008000b, 0x00000021, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
  0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
  0x0009000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d, 0x00000012, 0x0000001d,
  0x0000001f, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000,
  0x00060005, 0x0000000b, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x0000000b,
  0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006, 0x0000000b, 0x00000001, 0x505f6c67,
  0x746e696f, 0x657a6953, 0x00000000, 0x00070006, 0x0000000b, 0x00000002, 0x43706c67, 0x4474706c,
  0x61747369, 0x0065636e, 0x00070006, 0x0000000b, 0x00000003, 0x6c43706c, 0x74736944, 0x65636e61,
  0x00000000, 0x00030005, 0x0000000d, 0x00000000, 0x00050005, 0x00000012, 0x6f506e69, 0x69746973,
  0x00006e6f, 0x00050005, 0x0000001d, 0x67617266, 0x6f6c6f43, 0x00000072, 0x00040005, 0x0000001f,
  0x6f436e69, 0x00726f6c, 0x00050048, 0x0000000b, 0x00000000, 0x0000000b, 0x00000000, 0x00050048,
  0x0000000b, 0x00000001, 0x0000000b, 0x00000001, 0x00050048, 0x0000000b, 0x00000002, 0x0000000b,
  0x00000003, 0x00050048, 0x0000000b, 0x00000003, 0x0000000b, 0x00000004, 0x00030047, 0x0000000b,
  0x00000002, 0x00040047, 0x00000012, 0x0000001e, 0x00000000, 0x00040047, 0x0000001d, 0x0000001e,
  0x00000000, 0x00040047, 0x0000001f, 0x0000001e, 0x00000001, 0x00020013, 0x00000002, 0x00030021,
  0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006,
  0x00000004, 0x00040015, 0x00000008, 0x00000020, 0x00000000, 0x0004002b, 0x00000008, 0x00000009,
  0x00000001, 0x0004001c, 0x0000000a, 0x00000006, 0x00000009, 0x0006001e, 0x0000000b, 0x00000007,
  0x00000006, 0x0000000a, 0x0000000a, 0x00040020, 0x0000000c, 0x00000003, 0x0000000b, 0x0004003b,
  0x0000000c, 0x0000000d, 0x00000003, 0x00040015, 0x0000000e, 0x00000020, 0x00000001, 0x0004002b,
  0x0000000e, 0x0000000f, 0x00000000, 0x00040017, 0x00000010, 0x00000006, 0x00000002, 0x00040020,
  0x00000011, 0x00000001, 0x00000010, 0x0004003b, 0x00000011, 0x00000012, 0x00000001, 0x0004002b,
  0x00000006, 0x00000014, 0x00000000, 0x0004002b, 0x00000006, 0x00000015, 0x3f800000, 0x00040020,
  0x00000019, 0x00000003, 0x00000007, 0x00040017, 0x0000001b, 0x00000006, 0x00000003, 0x00040020,
  0x0000001c, 0x00000003, 0x0000001b, 0x0004003b, 0x0000001c, 0x0000001d, 0x00000003, 0x00040020,
  0x0000001e, 0x00000001, 0x0000001b, 0x0004003b, 0x0000001e, 0x0000001f, 0x00000001, 0x00050036,
  0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000010,
  0x00000013, 0x00000012, 0x00050051, 0x00000006, 0x00000016, 0x00000013, 0x00000000, 0x00050051,
  0x00000006, 0x00000017, 0x00000013, 0x00000001, 0x00070050, 0x00000007, 0x00000018, 0x00000016,
  0x00000017, 0x00000014, 0x00000015, 0x00050041, 0x00000019, 0x0000001a, 0x0000000d, 0x0000000f,
  0x0003003e, 0x0000001a, 0x00000018, 0x0004003d, 0x0000001b, 0x00000020, 0x0000001f, 0x0003003e,
  0x0000001d, 0x00000020, 0x000100fd, 0x00010038
};

// fragment.frag: #version 450, layout(location = 0) in vec3 fragColor, layout(location = 0) out vec4 outColor
static const uint32_t fragment_shader_spv[] = {
  0x07230203, 0x00010000, 0x0008000b, 0x00000013, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
  0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
  0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000c, 0x00030010,
  0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d,
  0x00000000, 0x00050005, 0x00000009, 0x4374756f, 0x726f6c6f, 0x00000072, 0x00050005, 0x0000000c,
  0x67617266, 0x6f6c6f43, 0x00000072, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047,
  0x0000000c, 0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002,
  0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
  0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040017,
  0x0000000a, 0x00000006, 0x00000003, 0x00040020, 0x0000000b, 0x00000001, 0x0000000a, 0x0004003b,
  0x0000000b, 0x0000000c, 0x00000001, 0x0004002b, 0x00000006, 0x0000000e, 0x3f800000, 0x00050036,
  0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x0000000a,
  0x0000000d, 0x0000000c, 0x00050051, 0x00000006, 0x0000000f, 0x0000000d, 0x00000000, 0x00050051,
  0x00000006, 0x00000010, 0x0000000d, 0x00000001, 0x00050051, 0x00000006, 0x00000011, 0x0000000d,
  0x00000002, 0x00070050, 0x00000007, 0x00000012, 0x0000000f, 0x00000010, 0x00000011, 0x0000000e,
  0x0003003e, 0x00000009, 0x00000012, 0x000100fd, 0x00010038
};

// Application context structure
typedef struct {
    // X11 context
    Display* display;
    Window window;
    int screen;
    Atom wm_delete_window;

    // Vulkan context
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;

    // Swapchain images
    VkImage* swapchain_images;
    VkImageView* swapchain_image_views;
    VkFramebuffer* framebuffers;
    uint32_t swapchain_image_count;

    // Render pass and pipeline
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    // Vertex buffer
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;

    // Command buffers
    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;

    // Synchronization - per frame in flight to avoid conflicts
    VkSemaphore* image_available_semaphores;    // [MAX_FRAMES_IN_FLIGHT]
    VkSemaphore* render_finished_semaphores;    // [MAX_FRAMES_IN_FLIGHT]
    VkFence* in_flight_fences;                  // [MAX_FRAMES_IN_FLIGHT]

    // Queue family indices
    uint32_t graphics_family_index;
    bool graphics_family_found;

    // Application state
    bool should_close;
    uint32_t current_frame;
} AppContext;

// Function prototypes
static int init_x11(AppContext* ctx);
static int init_vulkan(AppContext* ctx);
static int create_vulkan_instance(AppContext* ctx);
static int find_physical_device(AppContext* ctx);
static int create_logical_device(AppContext* ctx);
static int create_surface(AppContext* ctx);
static int create_swapchain(AppContext* ctx);
static int create_image_views(AppContext* ctx);
static int create_render_pass(AppContext* ctx);
static int create_graphics_pipeline(AppContext* ctx);
static int create_framebuffers(AppContext* ctx);
static int create_command_pool(AppContext* ctx);
static int create_vertex_buffer(AppContext* ctx);
static int create_command_buffers(AppContext* ctx);
static int create_sync_objects(AppContext* ctx);
static void render_frame(AppContext* ctx);
static void handle_events(AppContext* ctx);
static void cleanup(AppContext* ctx);

// Utility functions
static const char* vk_result_string(VkResult result);
static uint32_t find_memory_type(AppContext* ctx, uint32_t type_filter, VkMemoryPropertyFlags properties);

int main(void) {
    printf("Starting %s...\n", APP_NAME);

    AppContext ctx = {0};

    // Initialize X11
    if (init_x11(&ctx) != 0) {
        fprintf(stderr, "Failed to initialize X11\n");
        return EXIT_FAILURE;
    }
    printf("X11 initialized successfully\n");

    // Initialize Vulkan
    if (init_vulkan(&ctx) != 0) {
        fprintf(stderr, "Failed to initialize Vulkan\n");
        cleanup(&ctx);
        return EXIT_FAILURE;
    }
    printf("Vulkan initialized successfully\n");
    printf("Triangle rendering ready. Press ESC to exit.\n");

    // Main render loop
    while (!ctx.should_close) {
        handle_events(&ctx);
        render_frame(&ctx);
    }

    // Wait for device to be idle before cleanup
    vkDeviceWaitIdle(ctx.device);

    printf("Shutting down...\n");
    cleanup(&ctx);
    printf("Cleanup complete\n");

    return EXIT_SUCCESS;
}

// X11 initialization (same as window creation test)
static int init_x11(AppContext* ctx) {
    ctx->display = XOpenDisplay(NULL);
    if (!ctx->display) {
        fprintf(stderr, "Cannot connect to X server\n");
        return -1;
    }

    ctx->screen = DefaultScreen(ctx->display);

    ctx->window = XCreateSimpleWindow(
        ctx->display,
        RootWindow(ctx->display, ctx->screen),
        0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
        1, BlackPixel(ctx->display, ctx->screen),
        BlackPixel(ctx->display, ctx->screen)
    );

    if (!ctx->window) {
        fprintf(stderr, "Failed to create X11 window\n");
        return -1;
    }

    XStoreName(ctx->display, ctx->window, APP_NAME);
    XSelectInput(ctx->display, ctx->window,
                ExposureMask | KeyPressMask | StructureNotifyMask);

    ctx->wm_delete_window = XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(ctx->display, ctx->window, &ctx->wm_delete_window, 1);

    XMapWindow(ctx->display, ctx->window);
    XFlush(ctx->display);

    return 0;
}

static int init_vulkan(AppContext* ctx) {
    if (create_vulkan_instance(ctx) != 0) return -1;
    if (create_surface(ctx) != 0) return -1;
    if (find_physical_device(ctx) != 0) return -1;
    if (create_logical_device(ctx) != 0) return -1;
    if (create_swapchain(ctx) != 0) return -1;
    if (create_image_views(ctx) != 0) return -1;
    if (create_render_pass(ctx) != 0) return -1;
    if (create_graphics_pipeline(ctx) != 0) return -1;
    if (create_framebuffers(ctx) != 0) return -1;
    if (create_command_pool(ctx) != 0) return -1;
    if (create_vertex_buffer(ctx) != 0) return -1;
    if (create_command_buffers(ctx) != 0) return -1;
    if (create_sync_objects(ctx) != 0) return -1;

    return 0;
}

// Vulkan instance creation (same as window creation test)
static int create_vulkan_instance(AppContext* ctx) {
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = APP_NAME;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = ENGINE_NAME;
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    create_info.ppEnabledExtensionNames = extensions;

#ifdef DEBUG
    const char* validation_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = validation_layers;
    printf("Debug build: Validation layers enabled\n");
#else
    create_info.enabledLayerCount = 0;
#endif

    VkResult result = vkCreateInstance(&create_info, NULL, &ctx->instance);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance: %s\n", vk_result_string(result));
        return -1;
    }

    return 0;
}

static int create_surface(AppContext* ctx) {
    VkXlibSurfaceCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.dpy = ctx->display;
    create_info.window = ctx->window;

    VkResult result = vkCreateXlibSurfaceKHR(ctx->instance, &create_info, NULL, &ctx->surface);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan surface: %s\n", vk_result_string(result));
        return -1;
    }

    return 0;
}

// Physical device selection (enhanced for format support)
static int find_physical_device(AppContext* ctx) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);

    if (device_count == 0) {
        fprintf(stderr, "Failed to find GPUs with Vulkan support\n");
        return -1;
    }

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(devices[i], &device_properties);

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, NULL);

        VkQueueFamilyProperties* queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, queue_families);

        for (uint32_t j = 0; j < queue_family_count; j++) {
            if (queue_families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, ctx->surface, &present_support);

                if (present_support) {
                    ctx->physical_device = devices[i];
                    ctx->graphics_family_index = j;
                    ctx->graphics_family_found = true;

                    printf("Selected device: %s\n", device_properties.deviceName);
                    printf("Graphics queue family index: %u\n", j);

                    free(queue_families);
                    free(devices);
                    return 0;
                }
            }
        }

        free(queue_families);
    }

    free(devices);
    fprintf(stderr, "Failed to find a suitable GPU\n");
    return -1;
}

static int create_logical_device(AppContext* ctx) {
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {0};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = ctx->graphics_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = &queue_create_info;
    create_info.queueCreateInfoCount = 1;
    create_info.enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]);
    create_info.ppEnabledExtensionNames = device_extensions;

    VkResult result = vkCreateDevice(ctx->physical_device, &create_info, NULL, &ctx->device);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create logical device: %s\n", vk_result_string(result));
        return -1;
    }

    vkGetDeviceQueue(ctx->device, ctx->graphics_family_index, 0, &ctx->graphics_queue);

    return 0;
}

static int create_swapchain(AppContext* ctx) {
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device, ctx->surface, &surface_capabilities);

    // Choose swap extent
    if (surface_capabilities.currentExtent.width != UINT32_MAX) {
        ctx->swapchain_extent = surface_capabilities.currentExtent;
    } else {
        ctx->swapchain_extent.width = WINDOW_WIDTH;
        ctx->swapchain_extent.height = WINDOW_HEIGHT;

        if (ctx->swapchain_extent.width < surface_capabilities.minImageExtent.width) {
            ctx->swapchain_extent.width = surface_capabilities.minImageExtent.width;
        } else if (ctx->swapchain_extent.width > surface_capabilities.maxImageExtent.width) {
            ctx->swapchain_extent.width = surface_capabilities.maxImageExtent.width;
        }

        if (ctx->swapchain_extent.height < surface_capabilities.minImageExtent.height) {
            ctx->swapchain_extent.height = surface_capabilities.minImageExtent.height;
        } else if (ctx->swapchain_extent.height > surface_capabilities.maxImageExtent.height) {
            ctx->swapchain_extent.height = surface_capabilities.maxImageExtent.height;
        }
    }

    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
        image_count = surface_capabilities.maxImageCount;
    }

    ctx->swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;

    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = ctx->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = ctx->swapchain_format;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = ctx->swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = surface_capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(ctx->device, &create_info, NULL, &ctx->swapchain);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain: %s\n", vk_result_string(result));
        return -1;
    }

    vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &ctx->swapchain_image_count, NULL);
    ctx->swapchain_images = malloc(ctx->swapchain_image_count * sizeof(VkImage));
    vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &ctx->swapchain_image_count, ctx->swapchain_images);

    printf("Swapchain created with %u images, extent: %ux%u\n",
           ctx->swapchain_image_count, ctx->swapchain_extent.width, ctx->swapchain_extent.height);

    return 0;
}

static int create_image_views(AppContext* ctx) {
    ctx->swapchain_image_views = malloc(ctx->swapchain_image_count * sizeof(VkImageView));

    for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
        VkImageViewCreateInfo create_info = {0};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = ctx->swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = ctx->swapchain_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(ctx->device, &create_info, NULL, &ctx->swapchain_image_views[i]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create image view %u: %s\n", i, vk_result_string(result));
            return -1;
        }
    }

    return 0;
}

static int create_render_pass(AppContext* ctx) {
    VkAttachmentDescription color_attachment = {0};
    color_attachment.format = ctx->swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref = {0};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(ctx->device, &create_info, NULL, &ctx->render_pass);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create render pass: %s\n", vk_result_string(result));
        return -1;
    }

    return 0;
}

static int create_graphics_pipeline(AppContext* ctx) {
    // Create shader modules
    VkShaderModuleCreateInfo vert_shader_create_info = {0};
    vert_shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_shader_create_info.codeSize = sizeof(vertex_shader_spv);
    vert_shader_create_info.pCode = vertex_shader_spv;

    VkShaderModule vert_shader_module;
    VkResult result = vkCreateShaderModule(ctx->device, &vert_shader_create_info, NULL, &vert_shader_module);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vertex shader module: %s\n", vk_result_string(result));
        return -1;
    }

    VkShaderModuleCreateInfo frag_shader_create_info = {0};
    frag_shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_shader_create_info.codeSize = sizeof(fragment_shader_spv);
    frag_shader_create_info.pCode = fragment_shader_spv;

    VkShaderModule frag_shader_module;
    result = vkCreateShaderModule(ctx->device, &frag_shader_create_info, NULL, &frag_shader_module);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create fragment shader module: %s\n", vk_result_string(result));
        vkDestroyShaderModule(ctx->device, vert_shader_module, NULL);
        return -1;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo vert_stage_info = {0};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_shader_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info = {0};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_shader_module;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info, frag_stage_info};

    // Vertex input
    VkVertexInputBindingDescription binding_description = {0};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute_descriptions[2] = {0};
    // Position attribute
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(Vertex, pos);

    // Color attribute
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = 2;
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport
    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)ctx->swapchain_extent.width;
    viewport.height = (float)ctx->swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = ctx->swapchain_extent;

    VkPipelineViewportStateCreateInfo viewport_state = {0};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = {0};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    result = vkCreatePipelineLayout(ctx->device, &pipeline_layout_info, NULL, &ctx->pipeline_layout);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline layout: %s\n", vk_result_string(result));
        vkDestroyShaderModule(ctx->device, frag_shader_module, NULL);
        vkDestroyShaderModule(ctx->device, vert_shader_module, NULL);
        return -1;
    }

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = ctx->pipeline_layout;
    pipeline_info.renderPass = ctx->render_pass;
    pipeline_info.subpass = 0;

    result = vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &ctx->graphics_pipeline);

    // Clean up shader modules
    vkDestroyShaderModule(ctx->device, frag_shader_module, NULL);
    vkDestroyShaderModule(ctx->device, vert_shader_module, NULL);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create graphics pipeline: %s\n", vk_result_string(result));
        return -1;
    }

    return 0;
}

static int create_framebuffers(AppContext* ctx) {
    ctx->framebuffers = malloc(ctx->swapchain_image_count * sizeof(VkFramebuffer));

    for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
        VkImageView attachments[] = {ctx->swapchain_image_views[i]};

        VkFramebufferCreateInfo framebuffer_info = {0};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = ctx->render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = ctx->swapchain_extent.width;
        framebuffer_info.height = ctx->swapchain_extent.height;
        framebuffer_info.layers = 1;

        VkResult result = vkCreateFramebuffer(ctx->device, &framebuffer_info, NULL, &ctx->framebuffers[i]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create framebuffer %u: %s\n", i, vk_result_string(result));
            return -1;
        }
    }

    return 0;
}

static int create_command_pool(AppContext* ctx) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = ctx->graphics_family_index;

    VkResult result = vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->command_pool);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool: %s\n", vk_result_string(result));
        return -1;
    }

    return 0;
}

static int create_vertex_buffer(AppContext* ctx) {
    VkDeviceSize buffer_size = sizeof(vertices);

    VkBufferCreateInfo buffer_info = {0};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(ctx->device, &buffer_info, NULL, &ctx->vertex_buffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create vertex buffer: %s\n", vk_result_string(result));
        return -1;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(ctx->device, ctx->vertex_buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(ctx, mem_requirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(ctx->device, &alloc_info, NULL, &ctx->vertex_buffer_memory);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate vertex buffer memory: %s\n", vk_result_string(result));
        return -1;
    }

    vkBindBufferMemory(ctx->device, ctx->vertex_buffer, ctx->vertex_buffer_memory, 0);

    void* data;
    vkMapMemory(ctx->device, ctx->vertex_buffer_memory, 0, buffer_size, 0, &data);
    memcpy(data, vertices, buffer_size);
    vkUnmapMemory(ctx->device, ctx->vertex_buffer_memory);

    return 0;
}

static int create_command_buffers(AppContext* ctx) {
    ctx->command_buffers = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = ctx->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    VkResult result = vkAllocateCommandBuffers(ctx->device, &alloc_info, ctx->command_buffers);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to allocate command buffers: %s\n", vk_result_string(result));
        return -1;
    }

    return 0;
}

static int create_sync_objects(AppContext* ctx) {
    // Use per-frame semaphores and fences for simplicity and correctness
    ctx->image_available_semaphores = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkSemaphore));
    ctx->render_finished_semaphores = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkSemaphore));
    ctx->in_flight_fences = malloc(MAX_FRAMES_IN_FLIGHT * sizeof(VkFence));

    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // Create per-frame-in-flight synchronization objects
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkResult result = vkCreateSemaphore(ctx->device, &semaphore_info, NULL, &ctx->image_available_semaphores[i]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create image available semaphore %d: %s\n", i, vk_result_string(result));
            return -1;
        }

        result = vkCreateSemaphore(ctx->device, &semaphore_info, NULL, &ctx->render_finished_semaphores[i]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create render finished semaphore %d: %s\n", i, vk_result_string(result));
            return -1;
        }

        result = vkCreateFence(ctx->device, &fence_info, NULL, &ctx->in_flight_fences[i]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create fence %d: %s\n", i, vk_result_string(result));
            return -1;
        }
    }

    return 0;
}

static void render_frame(AppContext* ctx) {
    vkWaitForFences(ctx->device, 1, &ctx->in_flight_fences[ctx->current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ctx->in_flight_fences[ctx->current_frame]);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(ctx->device, ctx->swapchain, UINT64_MAX,
                                           ctx->image_available_semaphores[ctx->current_frame],
                                           VK_NULL_HANDLE, &image_index);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to acquire swapchain image: %s\n", vk_result_string(result));
        return;
    }

    // Use the current frame for semaphore selection to avoid conflicts

    vkResetCommandBuffer(ctx->command_buffers[ctx->current_frame], 0);

    // Record command buffer
    VkCommandBuffer command_buffer = ctx->command_buffers[ctx->current_frame];

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkRenderPassBeginInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = ctx->render_pass;
    render_pass_info.framebuffer = ctx->framebuffers[image_index];
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    render_pass_info.renderArea.extent = ctx->swapchain_extent;

    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->graphics_pipeline);

    VkBuffer vertex_buffers[] = {ctx->vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);
    vkEndCommandBuffer(command_buffer);

    // Submit command buffer
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {ctx->image_available_semaphores[ctx->current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkSemaphore signal_semaphores[] = {ctx->render_finished_semaphores[ctx->current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    result = vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, ctx->in_flight_fences[ctx->current_frame]);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit draw command buffer: %s\n", vk_result_string(result));
        return;
    }

    // Present
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = {ctx->swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(ctx->graphics_queue, &present_info);
    if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to present swapchain image: %s\n", vk_result_string(result));
        return;
    }

    ctx->current_frame = (ctx->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

static void handle_events(AppContext* ctx) {
    XEvent event;

    while (XPending(ctx->display)) {
        XNextEvent(ctx->display, &event);

        switch (event.type) {
            case KeyPress: {
                KeySym keysym = XLookupKeysym(&event.xkey, 0);
                if (keysym == XK_Escape) {
                    ctx->should_close = true;
                }
                break;
            }
            case ClientMessage:
                if ((Atom)event.xclient.data.l[0] == ctx->wm_delete_window) {
                    ctx->should_close = true;
                }
                break;
        }
    }
}

static void cleanup(AppContext* ctx) {
    if (ctx->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx->device);

        // Clean up sync objects
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (ctx->image_available_semaphores) {
                vkDestroySemaphore(ctx->device, ctx->image_available_semaphores[i], NULL);
            }
            if (ctx->render_finished_semaphores) {
                vkDestroySemaphore(ctx->device, ctx->render_finished_semaphores[i], NULL);
            }
            if (ctx->in_flight_fences) {
                vkDestroyFence(ctx->device, ctx->in_flight_fences[i], NULL);
            }
        }

        free(ctx->image_available_semaphores);
        free(ctx->render_finished_semaphores);
        free(ctx->in_flight_fences);

        if (ctx->command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
        }

        if (ctx->vertex_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(ctx->device, ctx->vertex_buffer, NULL);
        }

        if (ctx->vertex_buffer_memory != VK_NULL_HANDLE) {
            vkFreeMemory(ctx->device, ctx->vertex_buffer_memory, NULL);
        }

        if (ctx->framebuffers) {
            for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
                vkDestroyFramebuffer(ctx->device, ctx->framebuffers[i], NULL);
            }
            free(ctx->framebuffers);
        }

        if (ctx->graphics_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(ctx->device, ctx->graphics_pipeline, NULL);
        }

        if (ctx->pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(ctx->device, ctx->pipeline_layout, NULL);
        }

        if (ctx->render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(ctx->device, ctx->render_pass, NULL);
        }

        if (ctx->swapchain_image_views) {
            for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
                vkDestroyImageView(ctx->device, ctx->swapchain_image_views[i], NULL);
            }
            free(ctx->swapchain_image_views);
        }

        if (ctx->swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(ctx->device, ctx->swapchain, NULL);
        }

        free(ctx->swapchain_images);
        free(ctx->command_buffers);

        vkDestroyDevice(ctx->device, NULL);
    }

    if (ctx->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
    }

    if (ctx->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(ctx->instance, NULL);
    }

    if (ctx->display) {
        if (ctx->window) {
            XDestroyWindow(ctx->display, ctx->window);
        }
        XCloseDisplay(ctx->display);
    }
}

static uint32_t find_memory_type(AppContext* ctx, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    fprintf(stderr, "Failed to find suitable memory type!\n");
    exit(EXIT_FAILURE);
}

static const char* vk_result_string(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        default: return "Unknown VkResult";
    }
}