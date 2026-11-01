#include "scene.hpp"

namespace lab2 {
Mat4 identity() { Mat4 r{}; r.elements[0]=r.elements[5]=r.elements[10]=r.elements[15]=1; return r; }
Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for(size_t c=0;c<4;++c) for(size_t row=0;row<4;++row) for(size_t k=0;k<4;++k)
        r.elements[c*4+row]+=a.elements[k*4+row]*b.elements[c*4+k];
    return r;
}
Mat4 translation(Vec3 v) { auto r=identity(); r.elements[12]=v.x;r.elements[13]=v.y;r.elements[14]=v.z;return r; }
Mat4 scaling(Vec3 v) { auto r=identity();r.elements[0]=v.x;r.elements[5]=v.y;r.elements[10]=v.z;return r; }
Mat4 rotationX(float a) { auto r=identity();r.elements[5]=r.elements[10]=std::cos(a);r.elements[6]=std::sin(a);r.elements[9]=-std::sin(a);return r; }
Mat4 rotationY(float a) { auto r=identity();r.elements[0]=r.elements[10]=std::cos(a);r.elements[2]=-std::sin(a);r.elements[8]=std::sin(a);return r; }
Mat4 rotationZ(float a) { auto r=identity();r.elements[0]=r.elements[5]=std::cos(a);r.elements[1]=std::sin(a);r.elements[4]=-std::sin(a);return r; }
Mat4 eulerRotation(Vec3 d) { return multiply(rotationZ(d.z*radians),multiply(rotationY(d.y*radians),rotationX(d.x*radians))); }
Mat4 cameraView(const Camera& c) {
    // CameraWorld=T*Rz*Ry*Rx, обратная V=Rx^-1*Ry^-1*Rz^-1*T^-1.
    return multiply(rotationX(-c.rotationDegrees.x*radians),multiply(rotationY(-c.rotationDegrees.y*radians),
        multiply(rotationZ(-c.rotationDegrees.z*radians),translation(c.position*-1))));
}
Mat4 perspective(float fov,float aspect,float n,float f) {
    Mat4 r{}; const float q=1/std::tan(std::clamp(fov,10.f,120.f)*radians/2);
    r.elements[0]=q/aspect;r.elements[5]=-q;r.elements[10]=f/(n-f);r.elements[11]=-1;r.elements[14]=f*n/(n-f);return r;
}
Mat4 orthographic(float h,float aspect,float n,float f) {
    auto r=identity();r.elements[0]=1/(h*aspect);r.elements[5]=-1/h;r.elements[10]=1/(n-f);r.elements[14]=n/(n-f);return r;
}
void Scene::reset() {
    *this=Scene{};
    points.push_back(PointLight{nextId++});
    spots.push_back(SpotLight{nextId++});
}
float inverseSquare(float d2) { return 1/std::max(d2,0.01f); }
float spotFactor(float theta,float inner,float outer) {
    if(inner-outer<1e-6f) return theta>=outer ? 1.f : 0.f;
    float t=std::clamp((theta-outer)/(inner-outer),0.f,1.f); return t*t*(3-2*t);
}
void buildUniforms(const Scene& s,float aspect,bool manualSrgb,SceneGpu& sg,ModelGpu& mg) {
    Vec3 p=s.modelPosition+Vec3{ s.trajectoryRadius*static_cast<float>(std::sin(s.phase)),
        s.verticalAmplitude*static_cast<float>(std::sin(s.phase*s.verticalFrequency)),
        s.trajectoryRadius*static_cast<float>(1-std::cos(s.phase)) };
    Vec3 angles=s.modelRotation+Vec3{0,static_cast<float>(std::remainder(s.rotationPhase,360.)),0};
    const Mat4 r=eulerRotation(angles);
    const Vec3 scale{std::max(s.modelScale.x,0.05f),std::max(s.modelScale.y,0.05f),std::max(s.modelScale.z,0.05f)};
    mg={};mg.model=multiply(translation(p),multiply(r,scaling(scale)));
    // Для M=T*R*S inverse-transpose линейной части равна R*S^-1.
    mg.normalMatrix=multiply(r,scaling({1/scale.x,1/scale.y,1/scale.z}));
    mg.albedo=pack(s.material.albedo,1);mg.specular=pack(s.material.specular,std::max(s.material.shininess,1.f));
    mg.options={s.vertexColors?1u:0u,0,0,0};
    const Mat4 projection=s.camera.perspectiveProjection ? perspective(s.camera.fov,aspect,0.1f,100) :
        orthographic(std::max(s.camera.orthoHalfHeight,0.1f),aspect,0.1f,100);
    sg={};sg.viewProjection=multiply(projection,cameraView(s.camera));sg.cameraPosition=pack(s.camera.position,1);
    sg.ambient=pack(s.ambientColor,s.ambientEnabled?s.ambientIntensity:0);
    sg.directionalDirection=pack(normalized(s.directionalDirection));
    sg.directionalColor=pack(s.directionalColor,s.directionalEnabled?s.directionalIntensity:0);
    sg.counts={static_cast<uint32_t>(s.points.size()),static_cast<uint32_t>(s.spots.size()),s.diffuseEnabled?1u:0u,s.specularEnabled?1u:0u};
    sg.display={manualSrgb?1u:0u,0,0,0};
}
std::vector<PointGpu> packPoints(const Scene& s) {
    std::vector<PointGpu> result;result.reserve(s.points.size());
    for(const auto& l:s.points) result.push_back({pack(l.position,1),pack(l.color,l.enabled?l.intensity:0)});
    return result;
}
std::vector<SpotGpu> packSpots(const Scene& s) {
    std::vector<SpotGpu> result;result.reserve(s.spots.size());
    for(const auto& l:s.spots) {
        const float outer=std::clamp(l.outerDegrees,0.f,89.f),inner=std::clamp(l.innerDegrees,0.f,outer);
        result.push_back({pack(l.position,1),pack(normalized(l.direction)),pack(l.color,l.enabled?l.intensity:0),
            {std::cos(inner*radians),std::cos(outer*radians),0,0}});
    }
    return result;
}
} // namespace lab2
