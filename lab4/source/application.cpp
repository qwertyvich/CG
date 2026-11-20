#include "application.hpp"
#include "renderer.hpp"
#include <iostream>
//cd C:\Users\user\Desktop\CG
//.\lab4\tools\run.cmd
//

namespace application {
namespace {
lab4::Scene scene;
double previousTime=0;
bool firstUpdate=true;
}
bool initialize() { scene.reset();return lab4::renderer::initialize(); }
bool selectDemo(std::string_view name) {return lab4::applyDemo(scene,name);}
void shutdown() { lab4::renderer::shutdown(); }
void update(double time) {
    double dt=firstUpdate?0:std::clamp(time-previousTime,0.,0.1);
    firstUpdate=false;previousTime=time;
    lab4::drawControls(scene,lab4::renderer::pointCapacity(),lab4::renderer::spotCapacity(),lab4::renderer::maxPoints(),lab4::renderer::maxSpots());
    if(scene.effectPlaying) scene.effectTime+=dt*scene.effectSpeed;
    if(scene.animation) {scene.phase+=dt*scene.animationSpeed;scene.rotationPhase+=dt*scene.animationSpeed*scene.rotationSpeed;}
}
bool render(const graphics::internal::FrameData& frame) {return lab4::renderer::render(scene,frame);}
// Детерминированные состояния для проверки; обычный запуск использует только UI.
void smokeStep(unsigned frame) {
    using namespace lab4;
    if(frame==0) {applyDemo(scene,"combined");std::cout<<"SMOKE: combined independent maps\n";}
    if(frame==10) {scene.models[0].materialIndex=2;scene.models[0].specialSampling=true;std::cout<<"SMOKE: sampling and ordinary lit variants\n";}
    if(frame==20) {
        scene.models[0].position.x-=0.5f;scene.models[0].rotation={25,40,13};scene.models[0].scale={0.5f,1.8f,1.2f};
        scene.animation=true;scene.trajectoryRadius=0.4f;std::cout<<"SMOKE: moving caster, nonuniform scale\n";
    }
    if(frame==30) {scene.camera.position.x+=1;scene.effectPlaying=false;std::cout<<"SMOKE: camera moved; pause phase="<<scene.effectTime<<"\n";}
    if(frame==39) std::cout<<"SMOKE: paused phase="<<scene.effectTime<<"\n";
    if(frame==40) {scene.effectPlaying=true;scene.shadows.halfExtent=0.5f;scene.directionalDirection={0,0,0};std::cout<<"SMOKE: minimal coverage, zero sun direction\n";}
    if(frame==50) {scene.shadows.halfExtent=30;scene.directionalDirection={0,-1,0};std::cout<<"SMOKE: maximal coverage, vertical sun\n";}
    if(frame==60) {scene.shadows.constantBias=scene.shadows.slopeBias=0;std::cout<<"SMOKE: zero bias\n";}
    if(frame==65) {scene.shadows.constantBias=16;scene.shadows.slopeBias=64;std::cout<<"SMOKE: large bias\n";}
    if(frame==70) {scene.shadows=ShadowSettings{};reconcileShadowSpot(scene);std::cout<<"SMOKE: reset shadows\n";}
    if(frame==80) {
        for(int i=0;i<33;++i) {
            PointLight p{scene.nextId++};p.position.x=static_cast<float>(i%7)-3;p.intensity=0.1f;scene.points.push_back(p);
            SpotLight l{scene.nextId++};l.position.x=static_cast<float>(i%7)-3;l.intensity=0.1f;scene.spots.push_back(l);
        }
        scene.spots[0].position.x+=1;scene.spots[0].direction={2,-5,-3};
        std::cout<<"SMOKE: grow SSBOs to 34, move selected spot\n";
    }
    if(frame==90) {scene.spots[0].direction={0,0,0};scene.spots[0].outerDegrees=0;std::cout<<"SMOKE: zero spot direction/cone\n";}
    if(frame==100) {scene.spots[0].outerDegrees=0.001f;std::cout<<"SMOKE: tiny cone safely skips shadow\n";}
    if(frame==110) {scene.spots[0].innerDegrees=scene.spots[0].outerDegrees=89;scene.camera.perspectiveProjection=false;std::cout<<"SMOKE: wide equal cone, observer ortho\n";}
    if(frame==120) {applyDemo(scene,"spot");std::cout<<"SMOKE: spot without sun; resize restored\n";}
    if(frame==130) {scene.spots.push_back(SpotLight{scene.nextId++});scene.shadows.spotId=scene.spots.back().id;std::cout<<"SMOKE: select second spot\n";}
    if(frame==140) {scene.spots.erase(scene.spots.begin());std::cout<<"SMOKE: erase before selected spot; ID preserved\n";}
    if(frame==150) {scene.spots.erase(scene.spots.begin());std::cout<<"SMOKE: remove selected spot\n";}
    if(frame==155) {scene.spots.push_back(SpotLight{scene.nextId++});std::cout<<"SMOKE: add after selected removal\n";}
    if(frame==160) {scene.spots.clear();scene.points.clear();std::cout<<"SMOKE: clear both lists\n";}
    if(frame==170) {scene.spots.push_back(SpotLight{scene.nextId++});std::cout<<"SMOKE: re-add after clear\n";}
    if(frame==180) {scene.spots[0].intensity=0;scene.shadows.spot=false;std::cout<<"SMOKE: intensity zero and spot shadows off\n";}
    if(frame==190) {applyDemo(scene,"combined");std::cout<<"SMOKE: reset combined\n";}
    if(frame==200) {scene.shadows.directional=false;std::cout<<"SMOKE: only spot visibility, minimize/restore\n";}
    if(frame==210) {scene.shadows.directional=true;scene.shadows.spot=false;std::cout<<"SMOKE: only sun visibility\n";}
    if(frame==220) {applyDemo(scene,"emission");std::cout<<"SMOKE: independent emissive in darkness\n";}
    if(frame==230) {scene.reset();std::cout<<"SMOKE: reset scene\n";}
    reconcileShadowSpot(scene);
}
} // namespace application
