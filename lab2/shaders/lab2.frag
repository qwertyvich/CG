#version 450
#extension GL_GOOGLE_include_directive : require
#include "scene.glsl"
#include "model.glsl"
#include "display.glsl"
layout(set=0,binding=1,std430) readonly buffer PointLights { PointLight lights[]; } points;
layout(set=0,binding=2,std430) readonly buffer SpotLights { SpotLight lights[]; } spots;
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec3 vertexColor;
layout(location=0) out vec4 outColor;
vec3 safeNormalize(vec3 v) { float d=dot(v,v);return d>1e-10 ? v*inversesqrt(d) : vec3(0.0); }
vec3 evaluateBlinnPhong(vec3 N,vec3 L,vec3 V,vec3 albedo) {
    float NdotL=max(dot(N,L),0.0);
    if(NdotL<=0.0) return vec3(0.0);
    // H: направление посередине между векторами к свету и к камере.
    vec3 H=safeNormalize(L+V);
    float specularFactor=pow(max(dot(N,H),0.0),object.specular.w);
    vec3 diffuse=scene.counts.z!=0u ? albedo*NdotL : vec3(0.0);
    vec3 specular=scene.counts.w!=0u ? object.specular.rgb*specularFactor : vec3(0.0);
    return diffuse+specular;
}
vec3 evaluateDirectionalLight(vec3 N,vec3 V,vec3 albedo) {
    vec3 L=safeNormalize(-scene.directionalDirection.xyz);
    return evaluateBlinnPhong(N,L,V,albedo)*scene.directionalColor.rgb*scene.directionalColor.w;
}
float inverseSquare(float distanceSquared) { return 1.0/max(distanceSquared,0.01); }
vec3 evaluatePointLight(PointLight light,vec3 N,vec3 V,vec3 albedo) {
    vec3 delta=light.position.xyz-worldPosition;
    vec3 L=safeNormalize(delta);
    return evaluateBlinnPhong(N,L,V,albedo)*light.colorIntensity.rgb*light.colorIntensity.w*inverseSquare(dot(delta,delta));
}
float spotFactor(float cosTheta,float cosInner,float cosOuter) {
    // При совпадении границ smoothstep не определён: используем порог.
    if(cosInner-cosOuter<1e-6) return step(cosOuter,cosTheta);
    return smoothstep(cosOuter,cosInner,cosTheta);
}
vec3 evaluateSpotLight(SpotLight light,vec3 N,vec3 V,vec3 albedo) {
    vec3 delta=light.position.xyz-worldPosition;
    vec3 L=safeNormalize(delta);
    // direction идёт ОТ источника, L идёт К источнику.
    float cosTheta=dot(-L,light.direction.xyz);
    float cone=spotFactor(cosTheta,light.cone.x,light.cone.y);
    return evaluateBlinnPhong(N,L,V,albedo)*light.colorIntensity.rgb*light.colorIntensity.w*inverseSquare(dot(delta,delta))*cone;
}
void main() {
    vec3 N=safeNormalize(worldNormal);
    vec3 V=safeNormalize(scene.cameraPosition.xyz-worldPosition);
    vec3 albedo=object.albedo.rgb*(object.options.x!=0u ? vertexColor : vec3(1.0));
    // Ambient добавляется ровно один раз и не затухает по числу ламп.
    vec3 result=scene.ambient.rgb*scene.ambient.w*albedo;
    result+=evaluateDirectionalLight(N,V,albedo);
    for(uint i=0u;i<scene.counts.x;++i) result+=evaluatePointLight(points.lights[i],N,V,albedo);
    for(uint i=0u;i<scene.counts.y;++i) result+=evaluateSpotLight(spots.lights[i],N,V,albedo);
    outColor=vec4(displayColor(result),1.0);
}
