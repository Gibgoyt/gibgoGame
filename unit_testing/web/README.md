# Web Platform Implementation

## Status: Planned

This directory will contain the WebAssembly + WebGPU implementation of gibgoCraft.

## Goals

- Compile C code to WebAssembly using Emscripten
- Use WebGPU for graphics rendering (Vulkan equivalent for web)
- Provide browser-based demos and testing
- Support both desktop and mobile web browsers

## Planned Structure

```
web/
├── webgpu/
│   ├── basic/
│   │   ├── window_creation/
│   │   │   ├── main.c
│   │   │   ├── Makefile.em     # Emscripten-specific Makefile
│   │   │   └── index.html
│   │   └── triangle/
│   │       ├── main.c
│   │       ├── Makefile.em
│   │       └── index.html
│   └── README.md
├── canvas/                     # Fallback 2D canvas implementation
└── README.md
```

## Technical Approach

### WebGPU Implementation
- Use WebGPU API as Vulkan equivalent
- WGSL shaders instead of SPIR-V
- Canvas element for rendering surface
- RequestAnimationFrame for render loop

### Build System
- Emscripten toolchain for C to WASM compilation
- Custom Makefiles with `.em` extension
- Generate HTML, JS, and WASM files
- Serve via local HTTP server for testing

## Dependencies

- Emscripten SDK
- Modern browser with WebGPU support
- Local HTTP server (Python, Node.js, etc.)

## Future Features

- Touch input handling for mobile
- WebGL2 fallback for older browsers
- Progressive web app capabilities
- Asset loading via fetch API

## Resources

- [WebGPU Specification](https://www.w3.org/TR/webgpu/)
- [Emscripten Documentation](https://emscripten.org/docs/)
- [WebGPU Samples](https://github.com/webgpu/webgpu-samples)

## Notes

This implementation will be developed after the Linux X11 version is complete and working. The goal is to maintain API compatibility between native Vulkan and WebGPU implementations.