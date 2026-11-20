layout(set=0,binding=3,std140) uniform Shadows {
    mat4 sunViewProjection;
    mat4 spotViewProjection;
    uvec4 flags;
} shadows;
layout(set=0,binding=4) uniform sampler2DShadow sunShadowSampler;
layout(set=0,binding=5) uniform sampler2DShadow spotShadowSampler;

float shadowVisibility(sampler2DShadow map,mat4 lightVP,vec3 world) {
    vec4 clip=lightVP*vec4(world,1.0);
    if(clip.w<=0.0) return 1.0;
    vec3 ndc=clip.xyz/clip.w;
    // Both light projections flip Y once. Vulkan Z is already 0..1.
    vec2 uv=ndc.xy*0.5+0.5;
    if(any(lessThan(uv,vec2(0.0))) || any(greaterThan(uv,vec2(1.0))) || ndc.z<0.0 || ndc.z>1.0) return 1.0;
    // LESS: reference depth < stored depth means the light is visible.
    return texture(map,vec3(uv,ndc.z));
}
