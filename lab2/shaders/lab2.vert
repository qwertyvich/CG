#version 450
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"
#include "model.glsl"
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec3 vertexColor;
void main() {
    vec4 p=object.model*vec4(position,1.0);
    worldPosition=p.xyz;
    worldNormal=(object.normalMatrix*vec4(normal,0.0)).xyz;
    vertexColor=color;
    gl_Position=scene.viewProjection*p;
}
