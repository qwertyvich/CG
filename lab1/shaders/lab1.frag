#version 450

layout(location = 0) in vec3 proceduralColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tintColor;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(proceduralColor, 1.0) * pushConstants.tintColor;
}
