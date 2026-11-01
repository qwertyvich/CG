#include "scene.hpp"

namespace lab2 {
Mesh generateCylinder() {
    Mesh m; constexpr uint32_t n=50;
    // Четыре записи на сегмент: боковые низ/верх и независимые края крышек.
    for(uint32_t i=0;i<n;++i) {
        float a=2*std::numbers::pi_v<float>*static_cast<float>(i)/n;
        float x=std::cos(a),z=std::sin(a);
        Vec3 color{(x+1)/2,0.5f,(z+1)/2};
        m.vertices.push_back({{x,-1,z},color,{x,0,z}});
        m.vertices.push_back({{x,1,z},color,{x,0,z}});
        m.vertices.push_back({{x,-1,z},color,{0,-1,0}});
        m.vertices.push_back({{x,1,z},color,{0,1,0}});
    }
    m.vertices.push_back({{0,-1,0},{0.5f,0.5f,0.5f},{0,-1,0}});
    m.vertices.push_back({{0,1,0},{0.5f,0.5f,0.5f},{0,1,0}});
    for(uint32_t i=0;i<n;++i) {
        uint32_t a=4*i,b=4*((i+1)%n);
        m.indices.insert(m.indices.end(),{a,a+1,b,a+1,b+1,b,200,a+2,b+2,201,b+3,a+3});
    }
    return m;
}
Mesh generateMarkerCube() {
    Mesh m;
    for(int i=0;i<8;++i) m.vertices.push_back({{(i&1)?1.f:-1.f,(i&2)?1.f:-1.f,(i&4)?1.f:-1.f},{1,1,1},{0,1,0}});
    m.indices={0,2,1,1,2,3,4,5,6,5,7,6,0,1,4,1,5,4,2,6,3,3,6,7,0,4,2,2,4,6,1,3,5,3,7,5};
    return m;
}
std::vector<LineVertex> generateSpotCones(const Scene& s) {
    std::vector<LineVertex> lines;
    auto line=[&](Vec3 a,Vec3 b,Vec3 color){lines.push_back({a,color});lines.push_back({b,color});};
    for(const auto& l:s.spots) {
        const Vec3 axis=normalized(l.direction);
        // Опорный вектор меняется около вертикали, чтобы cross не выродился.
        const Vec3 u=normalized(cross(axis,std::abs(axis.y)>0.95f?Vec3{1,0,0}:Vec3{0,1,0}));
        const Vec3 v=cross(axis,u), center=l.position+axis*s.coneLength;
        line(l.position,center,{1,1,1});
        float outer=std::clamp(l.outerDegrees,0.f,89.f),inner=std::clamp(l.innerDegrees,0.f,outer);
        for(int ring=0;ring<2;++ring) {
            const float radius=s.coneLength*std::tan((ring?inner:outer)*radians);
            Vec3 color=ring?Vec3{0.45f,0.7f,1}:Vec3{1,1,1};
            auto point=[&](int i) {float a=2*std::numbers::pi_v<float>*i/48;return center+(u*std::cos(a)+v*std::sin(a))*radius;};
            for(int i=0;i<48;++i) { line(point(i),point(i+1),color);if(i%12==0) line(l.position,point(i),color); }
        }
    }
    return lines;
}
} // namespace lab2
