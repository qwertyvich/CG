#include "scene.hpp"
#include <cstdlib>
#include <iostream>
#include <map>
#include <tuple>

using namespace lab2;
void require(bool condition,const char* message) {
    if(!condition) {std::cerr<<"FAIL: "<<message<<"\n";std::exit(1);}
}
bool near(float a,float b) {return std::abs(a-b)<1e-4f;}
Vec3 transform(const Mat4& m,Vec3 v,float w) {
    auto& a=m.elements;
    return {a[0]*v.x+a[4]*v.y+a[8]*v.z+a[12]*w,
            a[1]*v.x+a[5]*v.y+a[9]*v.z+a[13]*w,
            a[2]*v.x+a[6]*v.y+a[10]*v.z+a[14]*w};
}
int main() {
    auto mesh=generateCylinder();require(mesh.vertices.size()==202 && mesh.indices.size()==600,"cylinder counts");
    // Weld equal positions for manifold test; hard normal edges intentionally duplicate records.
    using Position=std::tuple<int,int,int>;
    std::map<Position,int> welded;std::vector<int> ids;
    for(auto& v:mesh.vertices) {
        require(near(dot(v.normal,v.normal),1),"unit normal");
        Position p{static_cast<int>(std::lround(v.position.x*100000)),static_cast<int>(std::lround(v.position.y*100000)),static_cast<int>(std::lround(v.position.z*100000))};
        auto [it,inserted]=welded.emplace(p,static_cast<int>(welded.size()));ids.push_back(it->second);
    }
    std::map<std::pair<int,int>,std::pair<int,int>> edges;
    for(size_t i=0;i<mesh.indices.size();i+=3) {
        auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
        require(a<202 && b<202 && c<202,"indices in bounds");
        Vec3 n=cross(mesh.vertices[b].position-mesh.vertices[a].position,mesh.vertices[c].position-mesh.vertices[a].position);
        require(dot(n,n)>1e-8f,"nondegenerate triangle");
        require(dot(n,mesh.vertices[a].normal+mesh.vertices[b].normal+mesh.vertices[c].normal)>0,"outward winding");
        for(auto pair:{std::pair{a,b},std::pair{b,c},std::pair{c,a}}) {
            int u=ids[pair.first],v=ids[pair.second];auto& edge=edges[std::minmax(u,v)];++edge.first;edge.second+=(u<v?1:-1);
        }
    }
    for(auto& [edge,counts]:edges) require(counts.first==2 && counts.second==0,"closed seam / manifold edge orientation");
    for(size_t i=0;i<200;i+=4) {
        require(near(mesh.vertices[i].normal.y,0) && near(mesh.vertices[i+2].normal.y,-1) && near(mesh.vertices[i+3].normal.y,1),"hard cap normals");
    }
    Camera camera;camera.position={2,3,4};camera.rotationDegrees={17,-23,41};
    auto cw=multiply(translation(camera.position),eulerRotation(camera.rotationDegrees));
    auto product=multiply(cameraView(camera),cw);
    for(size_t i=0;i<16;++i) require(near(product.elements[i],identity().elements[i]),"view is inverse camera transform");
    auto projection=perspective(50,1.5f,0.1f,100);
    auto projectZ=[&](float z){auto& a=projection.elements;return (a[10]*z+a[14])/(-z);};
    require(near(projectZ(-0.1f),0) && near(projectZ(-100),1),"Vulkan depth 0..1");
    require(projection.elements[5]<0,"single Y correction");
    Scene s;s.reset();s.modelRotation={23,41,-12};s.modelScale={0.5f,2,1.3f};
    SceneGpu sg;ModelGpu mg;buildUniforms(s,1.5f,false,sg,mg);
    Vec3 tangent=transform(mg.model,{-1,0,1},0),normal=transform(mg.normalMatrix,{1,0,1},0);
    require(std::abs(dot(tangent,normal))<1e-4f,"inverse-transpose preserves orthogonality");
    require(near(inverseSquare(16)/inverseSquare(4),0.25f),"fixed-angle linear light obeys 1/r^2");
    float inner=std::cos(18*radians),outer=std::cos(28*radians);
    require(near(spotFactor(inner,inner,outer),1) && near(spotFactor(outer,inner,outer),0),"cone boundaries");
    require(near(spotFactor((inner+outer)/2,inner,outer),0.5f),"soft cone midpoint");
    require(near(spotFactor(inner,inner,inner),1) && near(spotFactor(inner-0.01f,inner,inner),0),"equal angles hard threshold");
    s.spots[0].direction={0,0,0};auto spots=packSpots(s);
    require(near(spots[0].direction[1],-1),"zero direction fallback");
    s.spots[0].direction={0,1,0};auto lines=generateSpotCones(s);
    require(lines.size()==210,"two rings, eight rays and axis");
    for(auto& l:lines) require(std::isfinite(l.position.x+l.position.y+l.position.z),"vertical cone stable basis");
    s.points.clear();s.spots.clear();buildUniforms(s,1,false,sg,mg);
    require(sg.counts[0]==0 && sg.counts[1]==0,"empty lists have zero active count");
    require(packPoints(s).empty() && packSpots(s).empty(),"empty packing");
    std::cout<<"PASS: geometry, manifold/winding, normals, view inverse, depth, normal matrix, attenuation, cones, packing\n";
}
