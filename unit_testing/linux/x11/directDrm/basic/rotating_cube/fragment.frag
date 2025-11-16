#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 worldPos;

// Output color - hardware depth testing automatically handles gl_FragDepth
layout(location = 0) out vec4 outColor;

void main() {
    // Simple pass-through of interpolated vertex colors
    // Hardware depth testing will automatically handle depth comparison
    outColor = vec4(fragColor, 1.0);

    // Note: gl_FragDepth is automatically set by hardware from vertex shader gl_Position.z
    // No need to manually set depth value for standard perspective rendering
}
