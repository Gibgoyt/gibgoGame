#version 450

// Vertex inputs - now 3D position instead of 2D
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Uniform buffer for 3D transformations
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 mvp;           // Pre-computed MVP matrix for performance
    vec3 cameraPos;
    float time;
} ubo;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 worldPos;    // World position for lighting (if needed later)

void main() {
    // Transform vertex to clip space using pre-computed MVP matrix
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);

    // Pass color to fragment shader
    fragColor = inColor;

    // Pass world position to fragment shader
    worldPos = (ubo.model * vec4(inPosition, 1.0)).xyz;
}
