#version 450
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"
#include "display.glsl"
layout(location=0) in vec3 constantColor;
layout(location=0) out vec4 outColor;
void main() { outColor=vec4(displayColor(constantColor),1.0); }
