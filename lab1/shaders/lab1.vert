#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tintColor;
} pushConstants;

layout(location = 0) out vec3 proceduralColor;

void main() {
    gl_Position = pushConstants.mvp * vec4(inPosition, 1.0);
    proceduralColor = inColor;
}
