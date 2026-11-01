#include "scene.hpp"
#include <imgui.h>

namespace lab3 {
void drawControls(Scene& s,size_t pointCapacity,size_t spotCapacity,size_t maxPoints,size_t maxSpots) {
    ImGui::SetNextWindowPos({14,14},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({390,690},ImGuiCond_FirstUseEver);
    ImGui::Begin("Lab 3: Texture mapping");
    ImGui::PushItemWidth(170);
    ImGui::TextUnformatted("Variant 8: cylinder / 50 segments");
    if(ImGui::Button("Base demo")) applyDemo(s,"base");
    ImGui::SameLine();if(ImGui::Button("Sampling demo")) applyDemo(s,"sampling");
    if(ImGui::Button("Emission demo")) applyDemo(s,"emission");
    ImGui::SameLine();if(ImGui::Button("Specular demo")) applyDemo(s,"specular");
    if(ImGui::Button("Reset scene")) s.reset();
    if(ImGui::Button("All lights off")) {
        s.ambientEnabled=s.directionalEnabled=false;
        for(auto& l:s.points) l.enabled=false;
        for(auto& l:s.spots) l.enabled=false;
    }
    ImGui::SameLine();
    if(ImGui::Button("Restore lights")) {
        s.ambientEnabled=s.directionalEnabled=true;
        for(auto& l:s.points) l.enabled=true;
        for(auto& l:s.spots) l.enabled=true;
    }
    if(ImGui::CollapsingHeader("Camera",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Camera XYZ",&s.camera.position.x,0.03f,-50,50,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat3("Camera Euler XYZ",&s.camera.rotationDegrees.x,0.5f,-180,180,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Perspective",&s.camera.perspectiveProjection);
        if(s.camera.perspectiveProjection) ImGui::SliderFloat("FOV",&s.camera.fov,10,120,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);
        else ImGui::SliderFloat("Ortho half height",&s.camera.orthoHalfHeight,0.2f,20,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        if(ImGui::Button("Reset camera")) s.camera=Camera{};
    }
    if(ImGui::CollapsingHeader("Model and material",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Combo("Object",&s.selectedModel,"Cylinder 1\0Cylinder 2\0");
        auto& o=s.models[static_cast<size_t>(s.selectedModel)];
        ImGui::Combo("Material",&o.materialIndex,materialNames,3);
        ImGui::TextUnformatted(o.materialIndex==2?"Specular + emissive maps":"Specular/emissive: black 1x1 fallback");
        ImGui::Checkbox("Special sampling",&o.specialSampling);
        ImGui::Checkbox("Play sampling effect",&s.effectPlaying);
        ImGui::SliderFloat("Effect speed",&s.effectSpeed,0,4);
        ImGui::SliderFloat("Effect strength",&s.effectStrength,0,1);
        ImGui::Text("Effect time: %.3f",s.effectTime);
        ImGui::SliderFloat("UV repeat",&o.uvScale,0.25f,4);
        ImGui::SliderFloat("Emissive strength",&o.emissiveStrength,0,4);
        ImGui::DragFloat3("Model XYZ",&o.position.x,0.02f,-10,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat3("Model Euler XYZ",&o.rotation.x,0.5f,-360,360,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat3("Scale XYZ",&o.scale.x,0.01f,0.05f,5,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("Albedo (linear)",&o.material.albedo.x);
        ImGui::ColorEdit3("Specular color (linear)",&o.material.specular.x);
        ImGui::SliderFloat("Shininess",&o.material.shininess,1,256,"%.0f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Play animation",&s.animation);
        ImGui::SliderFloat("Animation speed",&s.animationSpeed,0.05f,4);
        ImGui::SliderFloat("Trajectory radius",&s.trajectoryRadius,0,3);
        ImGui::SliderFloat("Vertical amplitude",&s.verticalAmplitude,0,2);
        ImGui::SliderFloat("Vertical frequency",&s.verticalFrequency,0.25f,5);
        ImGui::SliderFloat("Rotation speed",&s.rotationSpeed,-180,180);
        if(ImGui::Button("Reset model")) {
            o=ModelState{};o.position={s.selectedModel==0?-1.35f:1.35f,0,0};
            s.phase=s.rotationPhase=0;s.animation=false;
        }
    }
    if(ImGui::CollapsingHeader("Ambient and directional",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Ambient",&s.ambientEnabled);
        ImGui::ColorEdit3("Ambient color (linear)",&s.ambientColor.x);
        ImGui::SliderFloat("Ambient intensity",&s.ambientIntensity,0,1);
        ImGui::Checkbox("Directional enabled",&s.directionalEnabled);
        ImGui::DragFloat3("Direction (rays travel)",&s.directionalDirection.x,0.02f,-1,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("Directional color (linear)",&s.directionalColor.x);
        ImGui::SliderFloat("Directional intensity",&s.directionalIntensity,0,4);
        ImGui::Checkbox("Diffuse contribution",&s.diffuseEnabled);
        ImGui::Checkbox("Specular contribution",&s.specularEnabled);
    }
    if(ImGui::CollapsingHeader("Point lights")) {
        ImGui::Text("Count %zu / capacity %zu / device limit %zu",s.points.size(),pointCapacity,maxPoints);
        ImGui::BeginDisabled(s.points.size()>=maxPoints);
        if(ImGui::Button("Add point")) s.points.push_back(PointLight{s.nextId++});
        ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Clear points")) s.points.clear();
        for(size_t i=0;i<s.points.size();) {
            auto& l=s.points[i];ImGui::PushID(l.id);bool remove=false;
            if(ImGui::TreeNode("Point","Point #%d",l.id)) {
                ImGui::Checkbox("Enabled",&l.enabled);
                ImGui::DragFloat3("Position",&l.position.x,0.03f,-20,20,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::ColorEdit3("Color (linear)",&l.color.x);
                ImGui::SliderFloat("Intensity",&l.intensity,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                remove=ImGui::Button("Remove point");ImGui::TreePop();
            }
            ImGui::PopID();
            // erase только после последнего обращения к ссылке l; ID соседей не меняются.
            if(remove) s.points.erase(s.points.begin()+static_cast<ptrdiff_t>(i)); else ++i;
        }
    }
    if(ImGui::CollapsingHeader("Spot lights")) {
        ImGui::Text("Count %zu / capacity %zu / device limit %zu",s.spots.size(),spotCapacity,maxSpots);
        ImGui::BeginDisabled(s.spots.size()>=maxSpots);
        if(ImGui::Button("Add spot")) s.spots.push_back(SpotLight{s.nextId++});
        ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Clear spots")) s.spots.clear();
        for(size_t i=0;i<s.spots.size();) {
            auto& l=s.spots[i];ImGui::PushID(l.id);bool remove=false;
            if(ImGui::TreeNode("Spot","Spot #%d",l.id)) {
                ImGui::Checkbox("Enabled",&l.enabled);
                ImGui::DragFloat3("Position",&l.position.x,0.03f,-20,20,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloat3("Direction",&l.direction.x,0.02f,-10,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::TextUnformatted("Zero direction falls back to (0,-1,0)");
                ImGui::ColorEdit3("Color (linear)",&l.color.x);
                ImGui::SliderFloat("Intensity",&l.intensity,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                ImGui::SliderFloat("Outer half angle",&l.outerDegrees,0,89,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);
                l.innerDegrees=std::min(l.innerDegrees,l.outerDegrees);
                ImGui::SliderFloat("Inner half angle",&l.innerDegrees,0,l.outerDegrees,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);
                remove=ImGui::Button("Remove spot");ImGui::TreePop();
            }
            ImGui::PopID();
            if(remove) s.spots.erase(s.spots.begin()+static_cast<ptrdiff_t>(i)); else ++i;
        }
    }
    if(ImGui::CollapsingHeader("Light visualization",ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show light markers",&s.showMarkers);ImGui::Checkbox("Show spot cones",&s.showCones);
        ImGui::SliderFloat("Cone display length",&s.coneLength,0.2f,5);
        ImGui::TextWrapped("White: outer cone + axis. Blue: inner cone. Display length is not light range.");
    }
    ImGui::PopItemWidth();
    ImGui::End();
}
} // namespace lab3
