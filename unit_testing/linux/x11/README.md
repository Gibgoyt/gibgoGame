# Linux X11 Implementation

## Status: Active Development

This directory contains the primary Linux implementation using X11 windowing and native Vulkan.

## Overview

The Linux X11 implementation serves as the reference platform for gibgoCraft development. It uses direct X11 calls for windowing and native Vulkan API for graphics rendering.

## Structure

```
linux/x11/
├── vulkan/
│   ├── basic/
│   │   ├── window_creation/    # Basic X11 window + Vulkan surface
│   │   │   ├── main.c
│   │   │   └── Makefile
│   │   └── triangle/           # Render classic triangle
│   │       ├── main.c
│   │       └── Makefile
│   └── README.md
└── README.md
```

## Current Implementation

### Window Creation Test
**Location**: `vulkan/basic/window_creation/`

Features:
- X11 window creation with proper event handling
- Vulkan instance and surface setup
- Device selection and queue family discovery
- Basic render loop with event processing
- Clean shutdown and resource cleanup

### Triangle Rendering Test
**Location**: `vulkan/basic/triangle/`

Features:
- All window creation functionality
- Vertex buffer creation and management
- Shader loading and pipeline setup
- Command buffer recording and submission
- Swapchain presentation

## Build Requirements

### System Dependencies
```bash
# Ubuntu/Debian
sudo apt install build-essential libvulkan-dev libx11-dev

# Arch Linux
sudo pacman -S base-devel vulkan-devel libx11

# Fedora/RHEL
sudo dnf install gcc make vulkan-devel libX11-devel
```

### Vulkan SDK
- **Option 1**: System packages (recommended for beginners)
- **Option 2**: LunarG Vulkan SDK (for latest features)

## Building

```bash
# Window creation test
cd vulkan/basic/window_creation
make clean && make

# Triangle rendering test
cd ../triangle
make clean && make
```

## Testing

```bash
# Run window creation test
./window_creation

# Run triangle test
./triangle
```

## Technical Details

### X11 Integration
- Direct X11 calls (no GLFW/SDL dependencies)
- Proper event loop handling
- Window management and resize support
- Error handling for missing display

### Vulkan Implementation
- Instance creation with validation layers (debug builds)
- Physical device enumeration and selection
- Logical device creation with graphics queue
- Swapchain setup with proper format selection
- Command pool and buffer management

### Memory Management
- All Vulkan objects properly created and destroyed
- No memory leaks in normal operation
- Error handling with proper cleanup

### Shader Handling
- SPIR-V bytecode embedded as C arrays
- Vertex and fragment shader loading
- Pipeline state object creation

## Debugging

### Validation Layers
Enable Vulkan validation layers for debugging:
```bash
export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
```

### Common Issues
- **"Cannot connect to X server"**: Ensure DISPLAY is set
- **"Vulkan instance creation failed"**: Install vulkan-tools and test with `vulkaninfo`
- **"No suitable device found"**: Check GPU driver installation

## Performance Considerations

- No unnecessary allocations in render loop
- Proper synchronization primitives
- Minimal state changes
- Command buffer reuse where possible

## Future Extensions

- Multi-threaded command buffer recording
- Compute shader integration
- Multi-pass rendering
- Advanced Vulkan features (ray tracing, etc.)

## Resources

- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Vulkan API Documentation](https://www.vulkan.org/learn)
- [X11 Programming Manual](https://tronche.com/gui/x/xlib/)
- [Arch Wiki: Vulkan](https://wiki.archlinux.org/title/Vulkan)