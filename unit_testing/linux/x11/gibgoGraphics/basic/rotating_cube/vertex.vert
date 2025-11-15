#version 450

// 3D vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Transformation matrices (using uniform buffer for hardware-direct compatibility)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosition;  // World position for potential lighting

void main() {
    // Transform vertex position through MVP matrices
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    vec4 viewPos = ubo.view * worldPos;
    gl_Position = ubo.projection * viewPos;

    // Pass color and world position to fragment shader
    fragColor = inColor;
    fragPosition = worldPos.xyz;
}
