#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosition;

// Color output
layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional lighting for better 3D visualization
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));  // Light direction

    // Calculate a simple normal based on screen space derivatives
    // This gives us basic surface orientation for crude lighting
    vec3 dFdxPos = dFdx(fragPosition);
    vec3 dFdyPos = dFdy(fragPosition);
    vec3 normal = normalize(cross(dFdxPos, dFdyPos));

    // Simple diffuse lighting
    float diffuse = max(dot(normal, lightDir), 0.0);
    diffuse = diffuse * 0.6 + 0.4;  // Add ambient lighting (40%) + diffuse (60%)

    // Apply lighting to color
    vec3 litColor = fragColor * diffuse;

    // Output final color with full alpha
    outColor = vec4(litColor, 1.0);
}
