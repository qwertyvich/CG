// Все параметры UI и свет считаются линейными. sRGB attachment кодирует сам.
vec3 displayColor(vec3 linearColor) {
    vec3 c=clamp(linearColor,0.0,1.0);
    if(scene.display.x==0u) return c;
    return mix(12.92*c,1.055*pow(c,vec3(1.0/2.4))-0.055,greaterThan(c,vec3(0.0031308)));
}
