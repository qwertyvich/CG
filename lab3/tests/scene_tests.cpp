#include "scene.hpp"
#include "image.hpp"
#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <map>
#include <tuple>

using namespace lab3;
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
    auto mesh=generateCylinder();require(mesh.vertices.size()==206 && mesh.indices.size()==600,"cylinder counts");
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
        require(a<206 && b<206 && c<206,"indices in bounds");
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
    for(size_t i=0;i<mesh.vertices.size();++i) {
        auto uv=mesh.vertices[i].uv;
        require(uv.x>=0 && uv.x<=1 && uv.y>=0 && uv.y<=1,"UV within unit square");
    }
    require(near(mesh.vertices[0].position.x,mesh.vertices[200].position.x) &&
        near(mesh.vertices[0].position.z,mesh.vertices[200].position.z),"seam positions coincide");
    require(near(mesh.vertices[0].uv.x,1) && near(mesh.vertices[200].uv.x,0),"seam u=1 and u=0 (clockwise)");
    for(size_t i=0;i<600;i+=12) {
        float lo=2,hi=-1;
        for(size_t j=0;j<6;++j) {float u=mesh.vertices[mesh.indices[i+j]].uv.x;lo=std::min(lo,u);hi=std::max(hi,u);}
        require(near(hi-lo,1.f/50),"every side segment has local UV interval, including last");
    }
    require(near(mesh.vertices[2].uv.y,0.5f) && near(mesh.vertices[3].uv.y,0.5f),"cap orientation at x axis");
    for(uint64_t align:{1ull,16ull,64ull,256ull}) {
        auto stride=modelStrideFor(align);
        require(stride>=sizeof(ModelGpu) && stride%align==0,"dynamic UBO alignment");
        require(stride+sizeof(ModelGpu)<=stride*2,"second slot stays inside buffer");
    }
    auto black=decodeImage("assets/textures/black.png");
    require(black.width==1 && black.height==1 && black.rgba==std::vector<uint8_t>({0,0,0,255}),"decoder black fallback RGBA");
    auto grid=decodeImage("assets/textures/uv_grid.png");
    require(grid.width==512 && grid.height==512,"real PNG decoder dimensions");
    require(grid.rgba[2]>grid.rgba[0],"flipped bottom row is blue");
    size_t top=static_cast<size_t>(grid.height-1)*grid.width*4;
    require(grid.rgba[top]>grid.rgba[top+2],"flipped top row is orange");
    for(const char* file:{"stripes.png","panels.png","panels_specular.png","panels_emissive.png"})
        require(decodeImage(std::filesystem::path("assets/textures")/file).rgba.size()==512*512*4,"all shipped maps decode");
    bool missing=false,oversize=false,corrupt=false;
    try {decodeImage("assets/textures/not-present.png");} catch(const std::runtime_error&) {missing=true;}
    try {decodeImage("assets/textures/uv_grid.png",1);} catch(const std::runtime_error&) {oversize=true;}
    auto temp=std::filesystem::temp_directory_path()/"lab3-corrupt-image-test.png";
    {std::ofstream f(temp,std::ios::binary);f<<"invalid PNG data";}
    try {decodeImage(temp);} catch(const std::runtime_error&) {corrupt=true;}
    std::filesystem::remove(temp);
    require(missing && oversize && corrupt,"missing/oversize/corrupt images rejected");
    Camera camera;camera.position={2,3,4};camera.rotationDegrees={17,-23,41};
    auto cw=multiply(translation(camera.position),eulerRotation(camera.rotationDegrees));
    auto product=multiply(cameraView(camera),cw);
    for(size_t i=0;i<16;++i) require(near(product.elements[i],identity().elements[i]),"view is inverse camera transform");
    auto projection=perspective(50,1.5f,0.1f,100);
    auto projectZ=[&](float z){auto& a=projection.elements;return (a[10]*z+a[14])/(-z);};
    require(near(projectZ(-0.1f),0) && near(projectZ(-100),1),"Vulkan depth 0..1");
    require(projection.elements[5]<0,"single Y correction");
    Scene s;s.reset();s.models[0].rotation={23,41,-12};s.models[0].scale={0.5f,2,1.3f};
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
    ModelGpu first,second;
    s.reset();buildUniforms(s,1,false,sg,first,0);buildUniforms(s,1,false,sg,second,1);
    require(!near(first.model.elements[12],second.model.elements[12]),"independent model transforms");
    s.models[0].position.x+=1;s.models[0].materialIndex=2;
    buildUniforms(s,1,false,sg,mg,1);
    require(near(mg.model.elements[12],second.model.elements[12]) && s.models[1].materialIndex==0,"editing one object preserves other");
    std::cout<<"PASS: PNG decoding + error cases, UV seam/caps, dynamic alignment, independent objects; geometry, manifold/winding, normals, view inverse, depth, normal matrix, attenuation, cones, packing\n";
}
