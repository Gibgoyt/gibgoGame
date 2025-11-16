#!/bin/bash
# gibgoCraft Triangle Test - Shader Compilation Script
# Compiles GLSL shaders to SPIR-V and generates C arrays

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() {
    echo -e "${BLUE}[SHADER]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if required tools are available
check_tools() {
    print_info "Checking shader compilation tools..."

    if ! command -v glslangValidator &> /dev/null; then
        print_error "glslangValidator not found!"
        echo "Please install Vulkan SDK:"
        echo "  Ubuntu/Debian: sudo apt install vulkan-tools"
        echo "  Arch: sudo pacman -S vulkan-tools"
        exit 1
    fi

    if ! command -v xxd &> /dev/null; then
        print_error "xxd not found!"
        echo "  Ubuntu/Debian: sudo apt install xxd"
        exit 1
    fi
}

# Compile shader and generate C array
compile_shader() {
    local glsl_file="$1"
    local shader_name="$(basename "$glsl_file" .vert)"
    shader_name="$(basename "$shader_name" .frag)"

    local spv_file="${shader_name}.spv"

    print_info "Compiling $glsl_file -> $spv_file"

    # Compile GLSL to SPIR-V
    if ! glslangValidator -V "$glsl_file" -o "$spv_file"; then
        print_error "Failed to compile $glsl_file"
        return 1
    fi

    print_success "Generated $spv_file"
}

# Generate C array from SPIR-V
generate_c_array() {
    local spv_file="$1"
    local array_name="${spv_file%.spv}_spv"

    print_info "Converting $spv_file to C array format"

    echo "// Generated from $spv_file"
    echo "static const uint32_t ${array_name}[] = {"

    # Convert SPIR-V to C array format
    xxd -i "$spv_file" | grep -E '^\s*0x' | sed 's/^/  /' | sed 's/,$/,/'

    echo "};"
    echo "static const uint32_t ${array_name}_size = sizeof(${array_name});"
    echo ""
}

# Main compilation
main() {
    echo "==========================================="
    echo "  gibgoCraft Triangle - Shader Compiler"
    echo "==========================================="
    echo ""

    check_tools

    # Check if shader files exist
    if [[ ! -f "vertex.vert" || ! -f "fragment.frag" ]]; then
        print_error "Shader files not found!"
        echo "Expected files:"
        echo "  - vertex.vert"
        echo "  - fragment.frag"
        exit 1
    fi

    # Compile shaders
    print_info "Compiling vertex shader..."
    compile_shader "vertex.vert"

    print_info "Compiling fragment shader..."
    compile_shader "fragment.frag"

    # Generate C arrays
    echo ""
    print_info "Generating C arrays for embedding..."
    echo ""
    echo "// Copy and paste this into your main.c file:"
    echo "// Replace the existing vertex_shader_spv and fragment_shader_spv arrays"
    echo ""
    echo "// Embedded SPIR-V shaders (auto-generated)"
    echo "// vertex.vert -> vertex.spv"
    generate_c_array "vertex.spv"
    echo "// fragment.frag -> fragment.spv"
    generate_c_array "fragment.spv"

    echo ""
    print_success "Shader compilation complete!"
    echo ""
    echo "Next steps:"
    echo "  1. Copy the generated C arrays above"
    echo "  2. Replace the arrays in main.c"
    echo "  3. Rebuild with 'make'"
    echo ""
    echo "Files generated:"
    echo "  - vertex.spv"
    echo "  - fragment.spv"
}

# Check if running from correct directory
if [[ ! -f "main.c" ]]; then
    print_error "This script must be run from the triangle test directory"
    echo "Expected location: unit_testing/linux/x11/vulkan/basic/triangle/"
    exit 1
fi

# Run main function
main "$@"