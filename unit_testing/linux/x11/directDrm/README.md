# Vulkan Basic Tests

## Overview

This directory contains fundamental Vulkan tests that demonstrate core concepts and serve as building blocks for more complex rendering.

## Test Structure

### Basic Tests (`basic/`)

These tests implement the most fundamental Vulkan operations:

#### 1. Window Creation (`window_creation/`)
**Purpose**: Establish the foundation for all graphics work
**What it tests**:
- X11 window creation and event handling
- Vulkan instance creation and validation
- Physical device enumeration and selection
- Logical device and queue creation
- Surface creation and swapchain setup
- Basic render loop without actual rendering

**Learning outcomes**:
- Understand Vulkan initialization sequence
- Learn X11 windowing basics
- Grasp device/queue architecture
- Master resource lifecycle management

#### 2. Triangle Rendering (`triangle/`)
**Purpose**: First actual rendering - the "Hello World" of graphics
**What it tests**:
- Everything from window_creation
- Vertex buffer creation and management
- Shader module loading (vertex + fragment)
- Graphics pipeline creation
- Command buffer recording
- Drawing and presentation

**Learning outcomes**:
- Understand Vulkan rendering pipeline
- Learn command buffer usage
- Grasp synchronization basics
- Master draw commands

## Build System

Each test has its own `Makefile` with:
- Debug and release configurations
- Vulkan validation layer integration
- X11 linking
- Clean compilation flags

### Common Makefile Features
```makefile
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra
LIBS = -lvulkan -lX11
DEBUG_FLAGS = -g -DDEBUG -DVK_ENABLE_VALIDATION
RELEASE_FLAGS = -O2 -DNDEBUG
```

## Code Structure

### Standard File Layout
Each test follows this pattern:

```c
// Headers and includes
#include <vulkan/vulkan.h>
#include <X11/Xlib.h>

// Constants and structures
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

typedef struct {
    // Application state
} AppContext;

// Function prototypes
static int init_x11(AppContext* ctx);
static int init_vulkan(AppContext* ctx);
static void render_frame(AppContext* ctx);
static void cleanup(AppContext* ctx);

// Main function
int main(void) {
    // Initialize, run, cleanup
}
```

### Error Handling Pattern
All tests follow consistent error handling:
```c
VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
if (result != VK_SUCCESS) {
    fprintf(stderr, "Failed to create Vulkan instance: %d\n", result);
    return -1;
}
```

## Running the Tests

### Prerequisites Check
```bash
# Verify Vulkan installation
vulkaninfo

# Check X11 display
echo $DISPLAY

# Test basic Vulkan
vkcube  # If vulkan-tools installed
```

### Build and Run
```bash
# Window creation test
cd basic/window_creation
make
./window_creation

# Triangle test
cd ../triangle
make
./triangle
```

### Expected Behavior

#### Window Creation Test
- Opens 800x600 window titled "gibgoCraft - Window Creation"
- Window should be responsive (moveable, resizable)
- Clean exit on ESC key or window close
- No rendering content (black/undefined background)
- Console output showing Vulkan initialization steps

#### Triangle Test
- Opens 800x600 window titled "gibgoCraft - Triangle"
- Renders a single colored triangle in center
- Triangle should be stable (no flickering)
- Smooth frame rate (typically 60fps)
- Clean exit on ESC or window close

## Debugging

### Common Issues

**1. Vulkan Instance Creation Fails**
```bash
# Check if Vulkan is properly installed
vulkaninfo | head -20

# Verify loader is accessible
ldconfig -p | grep vulkan
```

**2. No Physical Device Found**
```bash
# List available devices
vulkaninfo | grep deviceName

# Check driver installation
lspci | grep VGA
```

**3. X11 Connection Fails**
```bash
# Verify X11 is running
ps aux | grep X

# Check display variable
echo $DISPLAY
xrandr  # Should show display info
```

**4. Validation Layer Warnings**
- Enable validation layers for detailed error info
- Check shader compilation issues
- Verify resource binding correctness

### Debug Build
```bash
make debug
VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d ./window_creation
```

## Learning Path

**Recommended Order**:
1. Read through window_creation code thoroughly
2. Build and run window_creation
3. Experiment with window properties (size, title)
4. Move to triangle test
5. Modify triangle color/position
6. Try adding more vertices

**Key Concepts to Master**:
- Vulkan object lifecycle
- Command buffer usage
- Pipeline state objects
- Memory management
- Synchronization primitives

## Extension Ideas

Once basic tests work, consider:
- Adding texture loading
- Implementing uniform buffers
- Creating multiple triangles
- Adding basic animation
- Implementing basic 3D transformations

## Resources

- **Vulkan**: [vulkan-tutorial.com](https://vulkan-tutorial.com/)
- **X11**: [Xlib Programming Manual](https://tronche.com/gui/x/xlib/)
- **Graphics**: [LearnOpenGL](https://learnopengl.com/) (concepts apply to Vulkan)
- **Math**: [GLM Documentation](https://glm.g-truc.net/) for 3D math