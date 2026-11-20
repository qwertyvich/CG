#version 450
#extension GL_GOOGLE_include_directive : require
#include "model.glsl"
layout(location=0) in vec3 position;
layout(push_constant) uniform Light { mat4 viewProjection; } light;
void main() {
    gl_Position=light.viewProjection*object.model*vec4(position,1.0);
}
