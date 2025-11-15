# macOS Platform Implementation

## Status: Planned

This directory will contain the macOS implementation of gibgoCraft using MoltenVK.

## Goals

- Use MoltenVK to translate Vulkan calls to Metal
- Support native Cocoa windowing system
- Provide universal binary support (x86_64 + ARM64)
- Maintain API compatibility with Linux implementation

## Planned Structure

```
mac/
├── cocoa/
│   ├── vulkan/
│   │   ├── basic/
│   │   │   ├── window_creation/
│   │   │   │   ├── main.c
│   │   │   │   ├── Makefile
│   │   │   │   └── window.m        # Objective-C windowing
│   │   │   └── triangle/
│   │   │       ├── main.c
│   │   │       ├── Makefile
│   │   │       └── window.m
│   │   └── README.md
│   └── README.md
└── README.md
```

## Technical Approach

### MoltenVK Integration
- Use MoltenVK library for Vulkan to Metal translation
- Link against Metal framework
- Handle Metal-specific limitations and differences
- Use Vulkan SDK for macOS

### Windowing System
- Cocoa/AppKit for native window management
- NSView for rendering surface
- Objective-C interop from C code
- Handle retina display scaling

### Build System
- Makefile with macOS-specific flags
- Link against Cocoa, Metal, and MoltenVK frameworks
- Universal binary compilation (-arch x86_64 -arch arm64)
- Code signing for distribution

## Dependencies

- Xcode Command Line Tools
- Vulkan SDK for macOS (includes MoltenVK)
- macOS 10.15+ (for Metal/Vulkan support)

## Challenges

- MoltenVK limitations compared to native Vulkan
- Metal performance differences
- Universal binary complexity
- Code signing requirements

## Future Features

- Metal Performance Shaders integration
- Apple Silicon optimizations
- macOS-specific input handling (trackpad, Force Touch)
- App Bundle packaging

## Resources

- [MoltenVK Documentation](https://github.com/KhronosGroup/MoltenVK)
- [Vulkan on Apple Platforms](https://vulkan.lunarg.com/doc/sdk/latest/mac/getting_started.html)
- [Apple Metal Documentation](https://developer.apple.com/metal/)

## Notes

This implementation will be developed after the Linux X11 version is complete. The goal is to maintain maximum code sharing between platforms while handling macOS-specific requirements through the MoltenVK translation layer.