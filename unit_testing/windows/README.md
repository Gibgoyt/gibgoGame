# Windows Platform Implementation

## Status: Planned

This directory will contain the Windows implementation of gibgoCraft using native Vulkan.

## Goals

- Use native Win32 API for windowing
- Native Windows Vulkan implementation
- Support for both MSVC and MinGW compilers
- DirectX 12 interop possibilities

## Planned Structure

```
windows/
├── win32/
│   ├── vulkan/
│   │   ├── basic/
│   │   │   ├── window_creation/
│   │   │   │   ├── main.c
│   │   │   │   ├── Makefile.mingw
│   │   │   │   └── project.vcxproj
│   │   │   └── triangle/
│   │   │       ├── main.c
│   │   │       ├── Makefile.mingw
│   │   │       └── project.vcxproj
│   │   └── README.md
│   └── README.md
└── README.md
```

## Technical Approach

### Windowing System
- Win32 API for window creation and management
- Handle Windows messages (WM_* events)
- Support for DPI awareness
- Multiple monitor support

### Vulkan Implementation
- Use LunarG Vulkan SDK for Windows
- Native Windows Vulkan loader
- Potential for better performance than translation layers
- Direct access to Windows-specific Vulkan extensions

### Build Systems
- **MinGW**: Cross-platform Makefile compatibility
- **MSVC**: Visual Studio project files and MSBuild
- **CMake**: Future consideration for unified build system

## Dependencies

- Windows SDK (for Win32 API)
- LunarG Vulkan SDK
- Either:
  - MinGW-w64 toolchain
  - Visual Studio 2019+ or Build Tools
  - Clang/LLVM for Windows

## Challenges

- Windows-specific Vulkan surface creation
- Handling different Windows versions
- DLL vs static linking considerations
- Unicode/ANSI string handling

## Future Features

- DirectX 12 interop for hybrid rendering
- Windows-specific optimizations
- UWP/Windows Store compatibility
- Windows 11 specific features

## Build Examples

### MinGW
```bash
# Using MinGW-w64
cd unit_testing/windows/win32/vulkan/basic/window_creation
make -f Makefile.mingw
```

### MSVC
```cmd
rem Using Visual Studio Developer Command Prompt
cd unit_testing\windows\win32\vulkan\basic\window_creation
msbuild project.vcxproj /p:Configuration=Debug
```

## Resources

- [Vulkan SDK for Windows](https://vulkan.lunarg.com/sdk/home#windows)
- [Win32 API Documentation](https://docs.microsoft.com/en-us/windows/win32/api/)
- [Vulkan-Samples Windows Build](https://github.com/KhronosGroup/Vulkan-Samples)

## Notes

This implementation will maintain API compatibility with the Linux version while handling Windows-specific requirements. The goal is to provide both MinGW (for cross-compilation) and MSVC (for native Windows development) build options.