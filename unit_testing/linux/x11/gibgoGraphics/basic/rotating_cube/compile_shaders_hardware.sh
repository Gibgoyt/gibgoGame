#!/bin/bash

# GibgoCraft Hardware-Direct Shader Compilation Script
# Compiles GLSL shaders to SPIR-V bytecode for the custom graphics layer

set -e

echo "🔧 Compiling shaders for hardware-direct graphics..."

# Check for required tools
if ! command -v glslangValidator &> /dev/null; then
    echo "❌ glslangValidator not found. Please install glslang-tools:"
    echo "   Ubuntu/Debian: sudo apt install glslang-tools"
    echo "   Arch: sudo pacman -S glslang"
    exit 1
fi

if ! command -v xxd &> /dev/null; then
    echo "❌ xxd not found. Please install xxd (usually comes with vim)"
    exit 1
fi

# Create output directory
mkdir -p shaders_compiled

# Compile vertex shader
echo "  📐 Compiling vertex shader..."
glslangValidator -V vertex.vert -o shaders_compiled/vertex.spv
xxd -i shaders_compiled/vertex.spv > shaders_compiled/vertex_spirv.h

# Compile fragment shader
echo "  🎨 Compiling fragment shader..."
glslangValidator -V fragment.frag -o shaders_compiled/fragment.spv
xxd -i shaders_compiled/fragment.spv > shaders_compiled/fragment_spirv.h

# Create combined shader header
echo "  📦 Creating combined shader header..."
cat > shaders_compiled/shaders.h << 'EOF'
#ifndef SHADERS_H
#define SHADERS_H

#include "types.h"

// Generated SPIR-V bytecode for hardware-direct graphics
// These shaders are the SAME as the Vulkan version but will be
// executed by our custom GPU hardware layer!

EOF

# Add vertex shader data
echo "// Vertex shader SPIR-V bytecode" >> shaders_compiled/shaders.h
echo "static const u32 vertex_shader_spirv[] = {" >> shaders_compiled/shaders.h
sed 's/unsigned char/u32/g; s/\[\]/[]/g' shaders_compiled/vertex_spirv.h | \
sed 's/shaders_compiled_vertex_spv/vertex_shader_spirv_data/g' | \
grep -v "unsigned int" | \
sed 's/0x\([0-9a-f][0-9a-f]\), 0x\([0-9a-f][0-9a-f]\), 0x\([0-9a-f][0-9a-f]\), 0x\([0-9a-f][0-9a-f]\)/0x\4\3\2\1/g' >> shaders_compiled/shaders.h
echo "};" >> shaders_compiled/shaders.h
echo "" >> shaders_compiled/shaders.h

# Add fragment shader data
echo "// Fragment shader SPIR-V bytecode" >> shaders_compiled/shaders.h
echo "static const u32 fragment_shader_spirv[] = {" >> shaders_compiled/shaders.h
sed 's/unsigned char/u32/g; s/\[\]/[]/g' shaders_compiled/fragment_spirv.h | \
sed 's/shaders_compiled_fragment_spv/fragment_shader_spirv_data/g' | \
grep -v "unsigned int" | \
sed 's/0x\([0-9a-f][0-9a-f]\), 0x\([0-9a-f][0-9a-f]\), 0x\([0-9a-f][0-9a-f]\), 0x\([0-9a-f][0-9a-f]\)/0x\4\3\2\1/g' >> shaders_compiled/shaders.h
echo "};" >> shaders_compiled/shaders.h
echo "" >> shaders_compiled/shaders.h

# Add size constants
echo "// Shader sizes" >> shaders_compiled/shaders.h
echo "static const u32 vertex_shader_spirv_size = sizeof(vertex_shader_spirv);" >> shaders_compiled/shaders.h
echo "static const u32 fragment_shader_spirv_size = sizeof(fragment_shader_spirv);" >> shaders_compiled/shaders.h
echo "" >> shaders_compiled/shaders.h
echo "#endif // SHADERS_H" >> shaders_compiled/shaders.h

# Create proper SPIR-V C arrays (32-bit words, not bytes!)
echo "  📄 Creating SPIR-V word arrays..."

# Function to convert SPIR-V binary to proper C array
convert_spirv_to_c() {
    local input_file="$1"
    local array_name="$2"

    # Read SPIR-V file as 32-bit words and format as C array
    echo "const u32 ${array_name}[] = {"

    # Use hexdump to convert to 32-bit little-endian words
    hexdump -v -e '4/1 "%02x"' -e '"\n"' "$input_file" | \
    while read word; do
        if [ -n "$word" ]; then
            echo "    0x$word,"
        fi
    done

    echo "};"
    echo ""
    echo "const u32 ${array_name}_size = sizeof(${array_name});"
    echo ""
}

# Create the C source file
cat > shader_data.c << 'EOF'
#include "types.h"

// SPIR-V shader bytecode compiled from vertex.vert and fragment.frag
// This is the SAME shader code as the Vulkan version but will be
// executed by our CUSTOM GPU hardware layer!
//
// Note: SPIR-V is stored as 32-bit words (not bytes) in little-endian format

EOF

echo "// Vertex shader bytecode" >> shader_data.c
convert_spirv_to_c "shaders_compiled/vertex.spv" "vertex_shader_spirv" >> shader_data.c

echo "// Fragment shader bytecode" >> shader_data.c
convert_spirv_to_c "shaders_compiled/fragment.spv" "fragment_shader_spirv" >> shader_data.c

# Print shader information
echo ""
echo "✅ Shader compilation complete!"
echo ""
echo "📊 Shader Statistics:"
echo "  Vertex shader:"
echo "    GLSL source: $(wc -l vertex.vert | cut -d' ' -f1) lines"
echo "    SPIR-V size: $(wc -c shaders_compiled/vertex.spv | cut -d' ' -f1) bytes"
echo "  Fragment shader:"
echo "    GLSL source: $(wc -l fragment.frag | cut -d' ' -f1) lines"
echo "    SPIR-V size: $(wc -c shaders_compiled/fragment.spv | cut -d' ' -f1) bytes"
echo ""
echo "🎯 These shaders will be executed by our CUSTOM GPU hardware layer!"
echo "   - Same GLSL code as Vulkan version"
echo "   - Same SPIR-V bytecode"
echo "   - But executed via DIRECT HARDWARE ACCESS!"

echo ""
echo "📁 Generated files:"
echo "  - shader_data.c        (include in your build)"
echo "  - shaders_compiled/    (intermediate files)"

# Make the script executable
chmod +x "$0"