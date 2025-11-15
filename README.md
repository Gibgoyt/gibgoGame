# gibgoCraft

A pure C game engine built on Vulkan, designed for learning and experimentation.

## Overview

gibgoCraft is a minimalist game engine written in pure C (not C++) that uses Vulkan for graphics rendering. The project emphasizes educational value, clean code structure, and cross-platform compatibility.

## Goals

- **Pure C Implementation**: No C++ dependencies, focusing on simplicity and portability
- **Vulkan Graphics**: Modern, low-level graphics API for maximum control and performance
- **Cross-Platform**: Support for Linux, Windows, macOS, and Web (via WebGPU)
- **Educational Focus**: Clear, well-documented code suitable for learning Vulkan concepts
- **Modular Design**: Clean separation between platform-specific and game logic code

## Project Structure

```
gibgoCraft/
├── unit_testing/          # Platform-specific test implementations
│   ├── linux/            # Linux native implementation
│   │   └── x11/          # X11 windowing system
│   │       └── vulkan/   # Vulkan graphics tests
│   │           └── basic/
│   │               ├── window_creation/  # Basic window + Vulkan surface
│   │               └── triangle/         # Render a triangle
│   ├── windows/          # Windows implementation (planned)
│   ├── mac/              # macOS implementation (planned)
│   └── web/              # WebAssembly + WebGPU (planned)
└── README.md
```

## Current Status

**Phase 1: Basic Vulkan Implementation (Current)**
- [x] Project structure creation
- [ ] X11 window creation with Vulkan surface
- [ ] Basic triangle rendering
- [ ] Command buffer management
- [ ] Shader compilation and loading

**Phase 2: Game Foundation (Planned)**
- [ ] Resource management system
- [ ] Basic entity system
- [ ] Input handling abstraction
- [ ] Audio system integration

**Phase 3: Game Features (Future)**
- [ ] 3D camera system
- [ ] Basic physics
- [ ] Texture loading and rendering
- [ ] Model loading and animation

## Building

### Linux (X11)

Requirements:
- GCC or Clang
- Vulkan SDK
- X11 development libraries

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential libvulkan-dev libx11-dev

# Build window creation test
cd unit_testing/linux/x11/vulkan/basic/window_creation
make

# Build triangle rendering test
cd ../triangle
make
```

### Other Platforms

See platform-specific READMEs:
- [Windows](unit_testing/windows/README.md)
- [macOS](unit_testing/mac/README.md)
- [Web](unit_testing/web/README.md)

## Development Philosophy

- **Start Simple**: Begin with the most basic functionality and build incrementally
- **Test-Driven**: Each feature has dedicated test programs in `unit_testing/`
- **Platform Isolation**: Platform-specific code is clearly separated
- **Educational Value**: Code is written to be readable and educational
- **No Dependencies**: Minimal external dependencies, especially avoiding large frameworks

## Learning Resources

This project is designed as a learning tool for:
- Vulkan API fundamentals
- Cross-platform C development
- Graphics programming concepts
- Game engine architecture

## Contributing

This is primarily an educational project. Feel free to study, modify, and learn from the code structure and implementation patterns.

## License

This project is intended for educational purposes. See individual source files for specific licensing information.