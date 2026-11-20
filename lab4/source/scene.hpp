#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string_view>
#include <vector>

namespace lab4 {
constexpr float radians = std::numbers::pi_v<float> / 180.0f;
struct Vec3 { float x, y, z; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec3 normalized(Vec3 v, Vec3 fallback = {0,-1,0}) {
    const float d = dot(v,v);
    return d > 1e-10f && std::isfinite(d) ? v*(1/std::sqrt(d)) : fallback;
}
// Column-major, column vectors: те же соглашения, что в GLSL.
struct alignas(16) Mat4 { std::array<float,16> elements{}; };
Mat4 identity();
Mat4 multiply(const Mat4& a, const Mat4& b);
Mat4 translation(Vec3 v);
Mat4 scaling(Vec3 v);
Mat4 rotationX(float angle);
Mat4 rotationY(float angle);
Mat4 rotationZ(float angle);
Mat4 eulerRotation(Vec3 degrees);
Mat4 perspective(float fov, float aspect, float nearPlane, float farPlane);
Mat4 orthographic(float halfHeight, float aspect, float nearPlane, float farPlane);
Mat4 lookAt(Vec3 eye, Vec3 target);
Mat4 lightPerspective(float fullFov, float nearPlane, float farPlane);
struct Camera {
    Vec3 position{7,6,11};
    Vec3 rotationDegrees{-25,32,0};
    float fov = 50;
    float orthoHalfHeight = 4.5f;
    bool perspectiveProjection = true;
};
Mat4 cameraView(const Camera& camera);
struct Material {
    Vec3 albedo{1,1,1};
    Vec3 specular{0.65f,0.65f,0.65f};
    float shininess = 64;
};
struct PointLight {
    int id;
    Vec3 position{2,1.7f,2};
    Vec3 color{1,0.65f,0.35f};
    float intensity = 6;
    bool enabled = true;
};
struct SpotLight {
    int id;
    Vec3 position{-3,5,3};
    Vec3 direction{3,-5,-3};
    Vec3 color{0.65f,0.8f,1};
    float intensity = 65;
    float innerDegrees = 25, outerDegrees = 38;
    bool enabled = true;
};
enum class MeshKind : uint32_t { Cylinder, Floor };
struct ModelState {
    Vec3 position{}, rotation{}, scale{0.85f,1.15f,0.85f};
    Material material;
    int materialIndex=0;
    bool specialSampling=false;
    float emissiveStrength=1, uvScale=1;
    MeshKind mesh=MeshKind::Cylinder;
};
inline constexpr const char* materialNames[]={"UV grid (black fallback)","Stripes (black fallback)","Panels (specular + emissive)","Floor (white)"};
inline constexpr size_t materialCount=std::size(materialNames);
inline constexpr const char* modelNames[]={"Cylinder 1","Cylinder 2","Floor"};
ModelState defaultModel(size_t index);
struct ShadowSettings {
    bool directional=true, spot=true;
    float halfExtent=7, constantBias=1.25f, slopeBias=1.75f;
    int spotId=-1; // Stable ID, never a pointer or an erase-sensitive vector index.
};
struct Scene {
    Camera camera;
    std::array<ModelState,3> models{{defaultModel(0),defaultModel(1),defaultModel(2)}};
    int selectedModel=0;
    bool effectPlaying=true;
    float effectSpeed=1, effectStrength=0.65f;
    double effectTime=0;
    Vec3 ambientColor{1,1,1}, directionalDirection{0.8f,-1,0.4f}, directionalColor{1,0.95f,0.85f};
    float ambientIntensity = 0.08f, directionalIntensity = 0.55f;
    bool directionalEnabled = true, ambientEnabled = true, diffuseEnabled = true, specularEnabled = true;
    bool showMarkers = true, showCones = true, animation = false;
    float animationSpeed = 1, trajectoryRadius = 0, verticalAmplitude = 0, verticalFrequency = 2, rotationSpeed = 35;
    float coneLength = 2;
    double phase = 0, rotationPhase = 0;
    int nextId = 1;
    std::vector<PointLight> points;
    std::vector<SpotLight> spots;
    ShadowSettings shadows;
    void reset();
};
// Каждое поле GPU занимает кратное 16 число байт; bool на GPU не передаётся.
using Float4 = std::array<float,4>;
using Uint4 = std::array<uint32_t,4>;
inline Float4 pack(Vec3 v, float w = 0) { return {v.x,v.y,v.z,w}; }
struct alignas(16) SceneGpu {
    Mat4 viewProjection;
    Float4 cameraPosition, ambient, directionalDirection, directionalColor;
    Uint4 counts; // point, spot, diffuse enabled, specular enabled
    Uint4 display; // manual sRGB encode, reserved
};
struct alignas(16) ModelGpu {
    Mat4 model, normalMatrix;
    Float4 albedo, specular; // specular.w = shininess
    Float4 surface; // emissive strength, UV scale X/Y, reserved
};
struct alignas(16) PointGpu { Float4 position, colorIntensity; };
struct alignas(16) SpotGpu { Float4 position, direction, colorIntensity, cone; };
struct alignas(16) ShadowGpu {
    Mat4 sunViewProjection, spotViewProjection;
    Uint4 flags; // sun active, spot active, selected packed spot index, reserved
};
static_assert(sizeof(ShadowGpu)==144 && offsetof(ShadowGpu,flags)==128);
int selectedShadowSpot(const Scene& scene);
void reconcileShadowSpot(Scene& scene);
ShadowGpu buildShadows(const Scene& scene);
static_assert(sizeof(Mat4)==64 && sizeof(SceneGpu)==160 && sizeof(ModelGpu)==176);
static_assert(offsetof(SceneGpu,cameraPosition)==64 && offsetof(SceneGpu,ambient)==80);
static_assert(offsetof(SceneGpu,directionalDirection)==96 && offsetof(SceneGpu,directionalColor)==112);
static_assert(offsetof(SceneGpu,counts)==128 && offsetof(SceneGpu,display)==144);
static_assert(offsetof(ModelGpu,normalMatrix)==64 && offsetof(ModelGpu,albedo)==128);
static_assert(offsetof(ModelGpu,specular)==144 && offsetof(ModelGpu,surface)==160);
static_assert(sizeof(PointGpu)==32 && offsetof(PointGpu,colorIntensity)==16);
static_assert(sizeof(SpotGpu)==64 && offsetof(SpotGpu,direction)==16 && offsetof(SpotGpu,colorIntensity)==32 && offsetof(SpotGpu,cone)==48);
struct alignas(16) SamplingPush { Uint4 flags; Float4 parameters; }; // enabled; time, strength
static_assert(sizeof(SamplingPush)==32 && offsetof(SamplingPush,parameters)==16);
inline uint64_t modelStrideFor(uint64_t alignment) { return (sizeof(ModelGpu)+alignment-1)/alignment*alignment; }
struct Vec2 { float x,y; };
struct Vertex { Vec3 position, color, normal; Vec2 uv{}; };
struct LineVertex { Vec3 position, color; };
static_assert(sizeof(Vertex)==44 && offsetof(Vertex,normal)==24 && offsetof(Vertex,uv)==36);
static_assert(sizeof(LineVertex)==24 && offsetof(LineVertex,color)==12);
struct Mesh { std::vector<Vertex> vertices; std::vector<uint32_t> indices; };
Mesh generateCylinder();
Mesh generateFloor();
Mesh generateMarkerCube();
std::vector<LineVertex> generateSpotCones(const Scene& scene);
void buildUniforms(const Scene& scene, float aspect, bool manualSrgb, SceneGpu& sceneGpu, ModelGpu& modelGpu, size_t modelIndex=0);
std::vector<PointGpu> packPoints(const Scene& scene);
std::vector<SpotGpu> packSpots(const Scene& scene);
float inverseSquare(float distanceSquared);
float spotFactor(float cosTheta, float cosInner, float cosOuter);
void drawControls(Scene& scene, size_t pointCapacity, size_t spotCapacity, size_t maxPoints, size_t maxSpots);
bool applyDemo(Scene& scene,std::string_view name);
} // namespace lab4
