# gibgoCraft - Window Creation Test Makefile
# Simple Makefile for building the window creation test

# Compiler and tools
CC = gcc
RM = rm -f

# Project settings
TARGET = window_creation
SOURCES = main.c

# Compiler flags
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE

# Libraries
LIBS = -lvulkan -lX11

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

# Build rules
.PHONY: all clean debug release help

all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo "Building $(TARGET) ($(BUILD_TYPE) mode)..."
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "Build complete: $(TARGET)"
	@echo ""
	@echo "Usage:"
	@echo "  ./$(TARGET)          - Run the test"
	@echo "  make clean           - Clean build files"
	@echo "  make release         - Build optimized version"
	@echo "  make help            - Show help"

debug: clean
	@$(MAKE) all

release: clean
	@$(MAKE) all RELEASE=1

clean:
	@echo "Cleaning build files..."
	$(RM) $(TARGET)

help:
	@echo "gibgoCraft Window Creation Test"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build $(TARGET) (default: debug mode)"
	@echo "  debug    - Clean and build debug version"
	@echo "  release  - Clean and build release version"
	@echo "  clean    - Remove build files"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - GCC compiler"
	@echo "  - Vulkan development libraries (libvulkan-dev)"
	@echo "  - X11 development libraries (libx11-dev)"
	@echo ""
	@echo "Example usage:"
	@echo "  make              # Build debug version"
	@echo "  make release      # Build release version"
	@echo "  ./$(TARGET)       # Run the test"
	@echo ""
	@echo "Troubleshooting:"
	@echo "  - Ensure DISPLAY is set: echo $$DISPLAY"
	@echo "  - Test Vulkan: vulkaninfo"
	@echo "  - Check libraries: ldd $(TARGET)"

# Dependencies check (optional target)
check-deps:
	@echo "Checking dependencies..."
	@echo -n "Vulkan: "
	@pkg-config --exists vulkan && echo "Found" || echo "NOT FOUND - install libvulkan-dev"
	@echo -n "X11: "
	@pkg-config --exists x11 && echo "Found" || echo "NOT FOUND - install libx11-dev"
	@echo -n "GCC: "
	@which gcc > /dev/null && echo "Found" || echo "NOT FOUND - install gcc"