#include "scene.glsl"
#include "model.glsl"
#include "display.glsl"
layout(set=0,binding=1,std430) readonly buffer PointLights { PointLight lights[]; } points;
layout(set=0,binding=2,std430) readonly buffer SpotLights { SpotLight lights[]; } spots;
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec2 textureUV;
layout(set=1,binding=1) uniform sampler2D albedoSampler;
layout(set=1,binding=2) uniform sampler2D specularSampler;
layout(set=1,binding=3) uniform sampler2D emissiveSampler;
#ifndef BASE_ONLY
layout(push_constant) uniform Sampling { uvec4 flags; vec4 parameters; } sampling;
#endif
vec3 specularMask;
layout(location=0) out vec4 outColor;
vec3 safeNormalize(vec3 v) { float d=dot(v,v);return d>1e-10 ? v*inversesqrt(d) : vec3(0.0); }
vec3 evaluateBlinnPhong(vec3 N,vec3 L,vec3 V,vec3 albedo) {
    float NdotL=max(dot(N,L),0.0);
    if(NdotL<=0.0) return vec3(0.0);
    // H: направление посередине между векторами к свету и к камере.
    vec3 H=safeNormalize(L+V);
    float specularFactor=pow(max(dot(N,H),0.0),object.specular.w);
    vec3 diffuse=scene.counts.z!=0u ? albedo*NdotL : vec3(0.0);
    vec3 specular=scene.counts.w!=0u ? object.specular.rgb*specularMask*specularFactor : vec3(0.0);
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
    vec2 uv=textureUV*object.surface.yz;
    vec3 sampledAlbedo=texture(albedoSampler,uv).rgb;
#ifndef BASE_ONLY
    if(sampling.flags.x!=0u) {
        float t=sampling.parameters.x;
        vec2 uvA=uv+vec2(0.09,0.03)*t;
        vec2 uvB=mat2(0.8,-0.6,0.6,0.8)*(uv-0.5)*1.4+0.5+vec2(-0.04,0.08)*t;
        vec3 sampleA=texture(albedoSampler,uvA).rgb;
        vec3 sampleB=texture(albedoSampler,uvB).rgb;
        float blend=0.5+0.3*sin(t);
        vec3 modulation=0.75+0.25*cos(vec3(0,2,4)+t);
        vec3 effect=mix(sampleA,sampleB,blend)*modulation;
        sampledAlbedo=mix(sampledAlbedo,effect,clamp(sampling.parameters.y,0.0,1.0));
    }
#endif
    vec3 albedo=sampledAlbedo*object.albedo.rgb;
    // Auxiliary maps intentionally stay on original UVs.
    specularMask=texture(specularSampler,uv).rgb;
    // Ambient добавляется ровно один раз и не затухает по числу ламп.
    vec3 result=scene.ambient.rgb*scene.ambient.w*albedo;
    result+=evaluateDirectionalLight(N,V,albedo);
    for(uint i=0u;i<scene.counts.x;++i) result+=evaluatePointLight(points.lights[i],N,V,albedo);
    for(uint i=0u;i<scene.counts.y;++i) result+=evaluateSpotLight(spots.lights[i],N,V,albedo);
    result+=texture(emissiveSampler,uv).rgb*object.surface.x;
    outColor=vec4(displayColor(result),1.0);
}
