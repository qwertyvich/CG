#include "scene.hpp"

namespace lab4 {
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
Mat4 lookAt(Vec3 eye,Vec3 target) {
    const Vec3 forward=normalized(target-eye);
    const Vec3 side=normalized(cross(forward,std::abs(forward.y)>0.95f?Vec3{1,0,0}:Vec3{0,1,0}));
    const Vec3 up=cross(side,forward);
    auto r=identity();
    r.elements[0]=side.x;r.elements[4]=side.y;r.elements[8]=side.z;
    r.elements[1]=up.x;r.elements[5]=up.y;r.elements[9]=up.z;
    r.elements[2]=-forward.x;r.elements[6]=-forward.y;r.elements[10]=-forward.z;
    r.elements[12]=-dot(side,eye);r.elements[13]=-dot(up,eye);r.elements[14]=dot(forward,eye);
    return r;
}
Mat4 lightPerspective(float fullFov,float n,float f) {
    // Spot FOV must also support cones narrower/wider than the observer's camera.
    n=std::max(n,0.01f);f=std::max(f,n+0.1f);
    Mat4 r{};const float q=1/std::tan(std::clamp(fullFov,0.2f,178.f)*radians/2);
    r.elements[0]=q;r.elements[5]=-q;r.elements[10]=f/(n-f);r.elements[11]=-1;r.elements[14]=f*n/(n-f);
    return r;
}
ModelState defaultModel(size_t index) {
    ModelState m;
    if(index<2) m.position={index==0?-1.35f:1.35f,0,0};
    else {
        m.position={0,-1.16f,0};m.scale={1,1,1};m.mesh=MeshKind::Floor;m.materialIndex=3;
        m.material.albedo={0.68f,0.7f,0.73f};m.material.specular={0.1f,0.1f,0.1f};
    }
    return m;
}
int selectedShadowSpot(const Scene& s) {
    for(size_t i=0;i<s.spots.size();++i) if(s.spots[i].id==s.shadows.spotId) return static_cast<int>(i);
    return -1;
}
void reconcileShadowSpot(Scene& s) {
    if(selectedShadowSpot(s)<0) s.shadows.spotId=s.spots.empty()?-1:s.spots.front().id;
}
ShadowGpu buildShadows(const Scene& s) {
    ShadowGpu g{};
    const Vec3 target{0,-0.2f,0},dir=normalized(s.directionalDirection);
    g.sunViewProjection=multiply(orthographic(std::clamp(s.shadows.halfExtent,0.5f,30.f),1,0.1f,60),
        lookAt(target-dir*25,target));
    g.spotViewProjection=identity();
    g.flags={s.shadows.directional && s.directionalEnabled && s.directionalIntensity>0?1u:0u,0,0,0};
    int index=selectedShadowSpot(s);
    if(index>=0) {
        const auto& l=s.spots[static_cast<size_t>(index)];
        // Below 0.1 degrees the light cone is effectively a ray: skip its shadow.
        if(s.shadows.spot && l.enabled && l.intensity>0 && l.outerDegrees>=0.1f) {
            const float fullFov=2*std::clamp(l.outerDegrees,0.1f,89.f);
            g.spotViewProjection=multiply(lightPerspective(fullFov,0.1f,50),lookAt(l.position,l.position+normalized(l.direction)));
            g.flags[1]=1;g.flags[2]=static_cast<uint32_t>(index);
        }
    }
    return g;
}
void Scene::reset() {
    *this=Scene{};
    points.push_back(PointLight{nextId++});
    spots.push_back(SpotLight{nextId++});
    points[0].enabled=false;spots[0].enabled=false;
    reconcileShadowSpot(*this);
}
bool applyDemo(Scene& s,std::string_view name) {
    if(name=="sampling-static" || name=="sampling-static-no-shadows") {
        applyDemo(s,"sampling");s.effectPlaying=false;
        if(name=="sampling-static-no-shadows") s.shadows.directional=s.shadows.spot=false;
        return true;
    }
    // Deterministic pairs for visual acceptance; the interactive UI has the same controls.
    if(name=="spot-no-shadows" || name=="combined-no-shadows" || name=="sampling-no-shadows") {
        applyDemo(s,name=="spot-no-shadows"?"spot":(name=="combined-no-shadows"?"combined":"sampling"));
        s.shadows.directional=s.shadows.spot=false;return true;
    }
    if(name=="model-shift" || name=="camera-shift" || name=="small-coverage" || name=="zero-bias" || name=="large-bias" || name=="sun-shift") {
        applyDemo(s,"directional");
        if(name=="model-shift") s.models[0].position.x-=1;
        if(name=="camera-shift") s.camera.position.x+=1;
        if(name=="small-coverage") s.shadows.halfExtent=1.5f;
        if(name=="zero-bias") s.shadows.constantBias=s.shadows.slopeBias=0;
        if(name=="large-bias") {s.shadows.constantBias=16;s.shadows.slopeBias=64;}
        if(name=="sun-shift") s.directionalDirection={0.6f,-1,-0.2f};
        return true;
    }
    if(name=="spot-shift") {
        applyDemo(s,"spot");s.spots[0].position.x+=2;s.spots[0].direction={1,-5,-3};s.spots[0].outerDegrees=45;return true;
    }
    if(name!="base" && name!="directional" && name!="spot" && name!="combined" && name!="no-shadows" && name!="sampling" && name!="emission" && name!="specular" && name!="caps" && name!="seam") return false;
    s.reset();
    s.showMarkers=s.showCones=false;
    if(name=="spot" || name=="combined") {
        s.directionalEnabled=name=="combined";s.spots[0].enabled=true;s.ambientIntensity=0.035f;
        s.showMarkers=s.showCones=true;
    }
    if(name=="no-shadows") s.shadows.directional=s.shadows.spot=false;
    if(name=="sampling") {s.models[0].specialSampling=true;s.effectTime=1.25;}
    if(name=="emission") {
        s.models[0].materialIndex=2;
        s.ambientEnabled=s.directionalEnabled=false;
        s.points.clear();s.spots.clear();
    }
    if(name=="specular") {
        for(auto& o:s.models) {o.materialIndex=2;o.emissiveStrength=0;o.material.shininess=24;}
        s.models[1].material.specular={0,0,0};
        s.ambientIntensity=0.025f;s.directionalDirection={0,0,-1};s.directionalIntensity=1;
        s.points.clear();s.spots.clear();
    }
    if(name=="caps") {s.models[0].rotation.x=65;s.models[1].rotation.x=-65;}
    if(name=="seam") for(auto& o:s.models) o.rotation.y=-90;
    return true;
}
float inverseSquare(float d2) { return 1/std::max(d2,0.01f); }
float spotFactor(float theta,float inner,float outer) {
    if(inner-outer<1e-6f) return theta>=outer ? 1.f : 0.f;
    float t=std::clamp((theta-outer)/(inner-outer),0.f,1.f); return t*t*(3-2*t);
}
void buildUniforms(const Scene& s,float aspect,bool manualSrgb,SceneGpu& sg,ModelGpu& mg,size_t modelIndex) {
    const auto& o=s.models.at(modelIndex);
    const bool animate=o.mesh==MeshKind::Cylinder;
    Vec3 p=o.position+(animate?Vec3{ s.trajectoryRadius*static_cast<float>(std::sin(s.phase)),
        s.verticalAmplitude*static_cast<float>(std::sin(s.phase*s.verticalFrequency)),
        s.trajectoryRadius*static_cast<float>(1-std::cos(s.phase)) }:Vec3{});
    Vec3 angles=o.rotation+Vec3{0,animate?static_cast<float>(std::remainder(s.rotationPhase,360.)):0,0};
    const Mat4 r=eulerRotation(angles);
    const Vec3 scale{std::max(o.scale.x,0.05f),std::max(o.scale.y,0.05f),std::max(o.scale.z,0.05f)};
    mg={};mg.model=multiply(translation(p),multiply(r,scaling(scale)));
    // Для M=T*R*S inverse-transpose линейной части равна R*S^-1.
    mg.normalMatrix=multiply(r,scaling({1/scale.x,1/scale.y,1/scale.z}));
    mg.albedo=pack(o.material.albedo,1);mg.specular=pack(o.material.specular,std::max(o.material.shininess,1.f));
    mg.surface={o.emissiveStrength,o.uvScale,o.uvScale,0};
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
} // namespace lab4
