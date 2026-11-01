#include "application.hpp"
#include "renderer.hpp"
#include <iostream>

namespace application {
namespace {
lab3::Scene scene;
double previousTime=0;
bool firstUpdate=true;
}
bool initialize() { scene.reset();return lab3::renderer::initialize(); }
bool selectDemo(std::string_view name) {return lab3::applyDemo(scene,name);}
void shutdown() { lab3::renderer::shutdown(); }
void update(double time) {
    double dt=firstUpdate?0:std::clamp(time-previousTime,0.,0.1);
    firstUpdate=false;previousTime=time;
    lab3::drawControls(scene,lab3::renderer::pointCapacity(),lab3::renderer::spotCapacity(),lab3::renderer::maxPoints(),lab3::renderer::maxSpots());
    if(scene.effectPlaying) scene.effectTime+=dt*scene.effectSpeed;
    if(scene.animation) {scene.phase+=dt*scene.animationSpeed;scene.rotationPhase+=dt*scene.animationSpeed*scene.rotationSpeed;}
}
bool render(const graphics::internal::FrameData& frame) {return lab3::renderer::render(scene,frame);}
// Детерминированные состояния для проверки; обычный запуск использует только UI.
void smokeStep(unsigned frame) {
    using namespace lab3;
    if(frame==10) {scene.models[0].materialIndex=2;scene.models[0].specialSampling=true;std::cout<<"LAB3: panels + animated sampling on object 1, object 2 ordinary\n";}
    if(frame==30) {scene.effectPlaying=false;std::cout<<"LAB3: pause effect phase="<<scene.effectTime<<"\n";}
    if(frame==39) std::cout<<"LAB3: paused phase="<<scene.effectTime<<"\n";
    if(frame==40) scene.effectPlaying=true;
    if(frame==50) {scene.ambientEnabled=scene.directionalEnabled=false;for(auto& l:scene.points)l.enabled=false;for(auto& l:scene.spots)l.enabled=false;std::cout<<"LAB3: emissive in darkness / black fallback on object 2\n";}
    if(frame==60) {
        scene.ambientEnabled=scene.directionalEnabled=true;
        for(auto& l:scene.points)l.enabled=true;
        for(auto& l:scene.spots)l.enabled=true;
        for(auto& o:scene.models)o.materialIndex=2;
        std::cout<<"LAB3: shared panels material, different dynamic offsets\n";
    }
    if(frame>=70 && frame<90) {scene.models[0].materialIndex=static_cast<int>(frame%3);scene.models[1].materialIndex=static_cast<int>((frame+1)%3);}
    if(frame==20) {
        scene.points.clear();scene.spots.clear();
        std::cout<<"SMOKE: empty point/spot lists\n";
    }
    if(frame==40) {
        for(int i=0;i<33;++i) {
            PointLight p{scene.nextId++};p.position.x=static_cast<float>(i%7)-3;p.intensity=0.4f;scene.points.push_back(p);
            SpotLight s{scene.nextId++};s.position.x=static_cast<float>(i%7)-3;s.intensity=0.4f;scene.spots.push_back(s);
        }
        std::cout<<"SMOKE: grow both arrays to 33\n";
    }
    if(frame==70) {
        for(size_t i:{size_t(0),size_t(15),size_t(30)}) {scene.points.erase(scene.points.begin()+i);scene.spots.erase(scene.spots.begin()+i);}
        std::cout<<"SMOKE: erase first, middle, last; 30 remain\n";
    }
    if(frame==90) {
        scene.spots[0].direction={0,0,0};scene.spots[0].innerDegrees=scene.spots[0].outerDegrees;
        scene.points[0].intensity=0;scene.spots[1].intensity=0;
        scene.models[0].rotation={25,40,13};scene.models[0].scale={0.5f,1.8f,1.2f};
        scene.camera.position={1,2,8};scene.camera.rotationDegrees={-12,7,3};scene.camera.fov=65;
        std::cout<<"SMOKE: equal cones, zero direction/intensity, camera and nonuniform scale\n";
    }
    if(frame==110) {scene.camera.perspectiveProjection=false;scene.showCones=scene.showMarkers=false;scene.animation=true;}
    if(frame==130) {scene.reset();scene.ambientIntensity=0;scene.directionalEnabled=false;std::cout<<"SMOKE: ambient off, point + spot only\n";}
    if(frame==150) {scene.points.clear();scene.spots.clear();scene.ambientIntensity=0.3f;std::cout<<"SMOKE: ambient only\n";}
    if(frame==170) {scene.reset();scene.points.clear();scene.spots.clear();scene.ambientIntensity=0;scene.diffuseEnabled=false;std::cout<<"SMOKE: directional specular only\n";}
    if(frame==190) {scene.reset();std::cout<<"SMOKE: reset scene\n";}
}
} // namespace application
