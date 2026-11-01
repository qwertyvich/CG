layout(set=1,binding=0,std140) uniform Model {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedo;
    vec4 specular;
    uvec4 options;
} object;
