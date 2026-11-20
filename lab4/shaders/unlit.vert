#version 450
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"
layout(push_constant) uniform Marker { mat4 model; vec4 color; } marker;
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=0) out vec3 constantColor;
void main() {
    constantColor=color*marker.color.rgb;
    gl_Position=scene.viewProjection*marker.model*vec4(position,1.0);
}
