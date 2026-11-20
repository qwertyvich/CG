#pragma once
#include "gpu_upload.hpp"
#include "scene.hpp"
#include <functional>

namespace lab4::shadows {
constexpr uint32_t resolution=2048;
struct Map { gpu::Texture texture; bool initialized=false; uint64_t passes=0; };
extern Map maps[2]; // 0: directional, 1: selected spot; never share an image.
bool initializeImages();
bool initializePipeline(VkDescriptorSetLayout sceneLayout,VkDescriptorSetLayout modelLayout);
void record(VkCommandBuffer cmd,size_t index,const Mat4& lightVP,const ShadowSettings& settings,bool active,
    const std::function<void(VkPipelineLayout)>& draw);
void shutdown();
}
