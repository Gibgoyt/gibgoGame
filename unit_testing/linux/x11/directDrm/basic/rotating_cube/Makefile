# gibgoCraft - Triangle Rendering Test Makefile
# Simple Makefile for building the triangle rendering test

# Compiler and tools
CC = gcc
RM = rm -f

# Project settings
TARGET = triangle
SOURCES = main.c

# Compiler flags
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE

# Libraries
LIBS = -lvulkan -lX11 -lm

# Debug vs Release
DEBUG_CFLAGS = -g -O0 -DDEBUG -DVK_ENABLE_VALIDATION
RELEASE_CFLAGS = -O2 -DNDEBUG

# Default to debug build
ifndef RELEASE
CFLAGS += $(DEBUG_CFLAGS)
BUILD_TYPE = debug
else
CFLAGS += $(RELEASE_CFLAGS)
BUILD_TYPE = release
endif

# Shader files
SHADER_SOURCES = vertex.vert fragment.frag
SHADER_SPIRV = vertex.spv fragment.spv
SHADER_SCRIPT = ./compile_shaders.sh

# Build rules
.PHONY: all clean debug release help test shaders clean-shaders

all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo "Building $(TARGET) ($(BUILD_TYPE) mode)..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "Build complete: $(TARGET)"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(TARGET)          - Run the triangle test"
	@echo "  make clean           - Clean build files"
	@echo "  make release         - Build optimized version"
	@echo "  make test            - Build and run test"
	@echo "  make help            - Show help"

debug: clean
	@$(MAKE) all

release: clean
	@$(MAKE) all RELEASE=1

test: all
	@echo "Running triangle test..."
	@echo "Press ESC to exit the test"
	./$(TARGET)

# Shader compilation rules
shaders: $(SHADER_SOURCES)
	@echo "Compiling shaders..."
	$(SHADER_SCRIPT)

$(SHADER_SPIRV): $(SHADER_SOURCES)
	@echo "Compiling shaders to SPIR-V..."
	$(SHADER_SCRIPT)

# Clean rules
clean:
	@echo "Cleaning build files..."
	$(RM) $(TARGET)

clean-shaders:
	@echo "Cleaning compiled shaders..."
	$(RM) $(SHADER_SPIRV)

clean-all: clean clean-shaders

help:
	@echo "gibgoCraft Triangle Rendering Test"
	@echo ""
	@echo "This test demonstrates:"
	@echo "  - Complete Vulkan graphics pipeline setup"
	@echo "  - Vertex buffer creation and management"
	@echo "  - Shader loading and compilation"
	@echo "  - Command buffer recording and submission"
	@echo "  - Triangle rendering with per-vertex colors"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build $(TARGET) (default: debug mode)"
	@echo "  debug      - Clean and build debug version"
	@echo "  release    - Clean and build release version"
	@echo "  test       - Build and run the test"
	@echo "  shaders    - Compile GLSL shaders to SPIR-V"
	@echo "  clean      - Remove build files"
	@echo "  clean-shaders - Remove compiled shaders"
	@echo "  clean-all  - Remove all build files and shaders"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - GCC compiler"
	@echo "  - Vulkan development libraries (libvulkan-dev)"
	@echo "  - X11 development libraries (libx11-dev)"
	@echo "  - Math library (libm)"
	@echo ""
	@echo "Expected output:"
	@echo "  - 800x600 window opens"
	@echo "  - Colored triangle appears in center"
	@echo "  - Red (top), Green (bottom-right), Blue (bottom-left)"
	@echo "  - Smooth gradient between vertices"
	@echo "  - Stable rendering at ~60 FPS"
	@echo ""
	@echo "Controls:"
	@echo "  - ESC key: Exit application"
	@echo "  - Window close button: Exit application"
	@echo ""
	@echo "Troubleshooting:"
	@echo "  - Ensure DISPLAY is set: echo $$DISPLAY"
	@echo "  - Test Vulkan: vulkaninfo"
	@echo "  - Check GPU driver: lspci | grep VGA"
	@echo "  - Enable validation: export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d"
	@echo "  - Check dependencies: ldd $(TARGET)"

# Dependencies check (optional target)
check-deps:
	@echo "Checking dependencies..."
	@echo -n "Vulkan: "
	@pkg-config --exists vulkan && echo "Found" || echo "NOT FOUND - install libvulkan-dev"
	@echo -n "X11: "
	@pkg-config --exists x11 && echo "Found" || echo "NOT FOUND - install libx11-dev"
	@echo -n "GCC: "
	@which gcc > /dev/null && echo "Found" || echo "NOT FOUND - install gcc"
	@echo -n "Vulkan tools: "
	@which vulkaninfo > /dev/null && echo "Found" || echo "NOT FOUND - install vulkan-tools"