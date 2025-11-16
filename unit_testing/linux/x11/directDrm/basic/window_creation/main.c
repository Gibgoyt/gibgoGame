/*
 * gibgoCraft - Window Creation Test
 *
 * This test demonstrates basic X11 window creation with Vulkan surface setup.
 * It establishes the foundation for all graphics rendering tests.
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
#define APP_NAME "gibgoCraft - Window Creation"
#define ENGINE_NAME "gibgoCraft Engine"

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

    // Queue family indices
    uint32_t graphics_family_index;
    bool graphics_family_found;

    // Application state
    bool should_close;
} AppContext;

// Function prototypes
static int init_x11(AppContext* ctx);
static int init_vulkan(AppContext* ctx);
static int create_vulkan_instance(AppContext* ctx);
static int find_physical_device(AppContext* ctx);
static int create_logical_device(AppContext* ctx);
static int create_surface(AppContext* ctx);
static int create_swapchain(AppContext* ctx);
static void handle_events(AppContext* ctx);
static void cleanup(AppContext* ctx);

// Utility functions
static const char* vk_result_string(VkResult result);

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

    printf("Window created and ready. Press ESC to exit.\n");

    // Main event loop
    while (!ctx.should_close) {
        handle_events(&ctx);

        // Add a small delay to prevent excessive CPU usage
        usleep(16000); // ~60 FPS equivalent
    }

    printf("Shutting down...\n");
    cleanup(&ctx);
    printf("Cleanup complete\n");

    return EXIT_SUCCESS;
}

static int init_x11(AppContext* ctx) {
    // Open connection to X server
    ctx->display = XOpenDisplay(NULL);
    if (!ctx->display) {
        fprintf(stderr, "Cannot connect to X server\n");
        return -1;
    }

    ctx->screen = DefaultScreen(ctx->display);

    // Create window
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

    // Set window properties
    XStoreName(ctx->display, ctx->window, APP_NAME);

    // Select input events
    XSelectInput(ctx->display, ctx->window,
                ExposureMask | KeyPressMask | StructureNotifyMask);

    // Handle window close button
    ctx->wm_delete_window = XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(ctx->display, ctx->window, &ctx->wm_delete_window, 1);

    // Make window visible
    XMapWindow(ctx->display, ctx->window);
    XFlush(ctx->display);

    return 0;
}

static int init_vulkan(AppContext* ctx) {
    if (create_vulkan_instance(ctx) != 0) {
        return -1;
    }

    if (create_surface(ctx) != 0) {
        return -1;
    }

    if (find_physical_device(ctx) != 0) {
        return -1;
    }

    if (create_logical_device(ctx) != 0) {
        return -1;
    }

    if (create_swapchain(ctx) != 0) {
        return -1;
    }

    return 0;
}

static int create_vulkan_instance(AppContext* ctx) {
    // Application info
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = APP_NAME;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = ENGINE_NAME;
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    // Required extensions
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
    };

    // Instance create info
    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    create_info.ppEnabledExtensionNames = extensions;

#ifdef DEBUG
    // Enable validation layers in debug builds
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

static int find_physical_device(AppContext* ctx) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);

    if (device_count == 0) {
        fprintf(stderr, "Failed to find GPUs with Vulkan support\n");
        return -1;
    }

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);

    // Find a suitable device
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(devices[i], &device_properties);

        // Find graphics queue family
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, NULL);

        VkQueueFamilyProperties* queue_families = malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, queue_families);

        for (uint32_t j = 0; j < queue_family_count; j++) {
            if (queue_families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                // Check if this queue family supports presenting to our surface
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
    // Queue create info
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {0};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = ctx->graphics_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    // Required device extensions
    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Device create info
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

    // Get the graphics queue
    vkGetDeviceQueue(ctx->device, ctx->graphics_family_index, 0, &ctx->graphics_queue);

    return 0;
}

static int create_swapchain(AppContext* ctx) {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device, ctx->surface, &surface_capabilities);

    // Choose swap extent
    VkExtent2D extent;
    if (surface_capabilities.currentExtent.width != UINT32_MAX) {
        extent = surface_capabilities.currentExtent;
    } else {
        extent.width = WINDOW_WIDTH;
        extent.height = WINDOW_HEIGHT;

        // Clamp to supported range
        if (extent.width < surface_capabilities.minImageExtent.width) {
            extent.width = surface_capabilities.minImageExtent.width;
        } else if (extent.width > surface_capabilities.maxImageExtent.width) {
            extent.width = surface_capabilities.maxImageExtent.width;
        }

        if (extent.height < surface_capabilities.minImageExtent.height) {
            extent.height = surface_capabilities.minImageExtent.height;
        } else if (extent.height > surface_capabilities.maxImageExtent.height) {
            extent.height = surface_capabilities.maxImageExtent.height;
        }
    }

    // Choose image count
    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
        image_count = surface_capabilities.maxImageCount;
    }

    // Swapchain create info
    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = ctx->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM; // Common format
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = surface_capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Always available
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(ctx->device, &create_info, NULL, &ctx->swapchain);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create swapchain: %s\n", vk_result_string(result));
        return -1;
    }

    printf("Swapchain created with %u images, extent: %ux%u\n",
           image_count, extent.width, extent.height);

    return 0;
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
            case ConfigureNotify:
                // Handle window resize
                printf("Window resized to: %dx%d\n",
                       event.xconfigure.width, event.xconfigure.height);
                break;
        }
    }
}

static void cleanup(AppContext* ctx) {
    // Cleanup Vulkan objects
    if (ctx->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx->device);

        if (ctx->swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(ctx->device, ctx->swapchain, NULL);
        }

        vkDestroyDevice(ctx->device, NULL);
    }

    if (ctx->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
    }

    if (ctx->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(ctx->instance, NULL);
    }

    // Cleanup X11
    if (ctx->display) {
        if (ctx->window) {
            XDestroyWindow(ctx->display, ctx->window);
        }
        XCloseDisplay(ctx->display);
    }
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
        default: return "Unknown VkResult";
    }
}
