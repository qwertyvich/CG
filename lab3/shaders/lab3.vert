#version 450
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"
#include "model.glsl"
layout(location=0) in vec3 position;
layout(location=3) in vec2 uv;
layout(location=2) in vec3 normal;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec2 textureUV;
void main() {
    vec4 p=object.model*vec4(position,1.0);
    worldPosition=p.xyz;
    worldNormal=(object.normalMatrix*vec4(normal,0.0)).xyz;
    textureUV=uv;
    gl_Position=scene.viewProjection*p;
}
