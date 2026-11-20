struct PointLight { vec4 position; vec4 colorIntensity; };
struct SpotLight { vec4 position; vec4 direction; vec4 colorIntensity; vec4 cone; };
layout(set=0,binding=0,std140) uniform Scene {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 ambient;
    vec4 directionalDirection;
    vec4 directionalColor;
    uvec4 counts;
    uvec4 display;
} scene;
