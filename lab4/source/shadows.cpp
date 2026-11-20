#include "shadows.hpp"
#include <fstream>
#include <iostream>

namespace lab4::shadows {
Map maps[2];
namespace {
VkFormat depthFormat=VK_FORMAT_UNDEFINED;
VkPipeline pipeline{};
VkPipelineLayout layout{};

void transition(VkCommandBuffer cmd,Map& map,bool toAttachment) {
    const bool first=!map.initialized;
    VkImageMemoryBarrier barrier{.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask=first?0u:static_cast<VkAccessFlags>(toAttachment?VK_ACCESS_SHADER_READ_BIT:VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
        .dstAccessMask=static_cast<VkAccessFlags>(toAttachment?VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:VK_ACCESS_SHADER_READ_BIT),
        .oldLayout=first?VK_IMAGE_LAYOUT_UNDEFINED:(toAttachment?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL),
        .newLayout=toAttachment?VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .image=map.texture.image,.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}};
    constexpr auto depthStages=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    const VkPipelineStageFlags src=first?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:(toAttachment?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:depthStages);
    const VkPipelineStageFlags dst=toAttachment?depthStages:VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    vkCmdPipelineBarrier(cmd,src,dst,0,0,nullptr,0,nullptr,1,&barrier);
}
}
bool initializeImages() {
    auto& c=graphics::internal::context;
    VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(c.physical_device,&properties);
    if(resolution>properties.limits.maxImageDimension2D) {std::cerr<<"Shadow map exceeds maxImageDimension2D\n";return false;}
    constexpr auto needed=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    for(auto candidate:{VK_FORMAT_D32_SFLOAT,VK_FORMAT_D16_UNORM}) {
        VkFormatProperties p{};vkGetPhysicalDeviceFormatProperties(c.physical_device,candidate,&p);
        VkImageFormatProperties imageProps{};
        if((p.optimalTilingFeatures & needed)==needed &&
           vkGetPhysicalDeviceImageFormatProperties(c.physical_device,candidate,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,0,&imageProps)==VK_SUCCESS &&
           imageProps.maxExtent.width>=resolution && imageProps.maxExtent.height>=resolution) {depthFormat=candidate;break;}
    }
    if(depthFormat==VK_FORMAT_UNDEFINED) {std::cerr<<"No sampleable depth attachment format for shadow maps\n";return false;}
    for(auto& map:maps) {
        auto& t=map.texture;
        VkImageCreateInfo image{.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,.imageType=VK_IMAGE_TYPE_2D,.format=depthFormat,
            .extent={resolution,resolution,1},.mipLevels=1,.arrayLayers=1,.samples=VK_SAMPLE_COUNT_1_BIT,.tiling=VK_IMAGE_TILING_OPTIMAL,
            .usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateInfo allocation{.usage=VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,.requiredFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
        if(vmaCreateImage(c.allocator,&image,&allocation,&t.image,&t.allocation,nullptr)!=VK_SUCCESS) return false;
        VkImageViewCreateInfo view{.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,.image=t.image,.viewType=VK_IMAGE_VIEW_TYPE_2D,
            .format=depthFormat,.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}};
        if(vkCreateImageView(c.device,&view,nullptr,&t.view)!=VK_SUCCESS) return false;
        VkSamplerCreateInfo sampler{.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,.magFilter=VK_FILTER_NEAREST,.minFilter=VK_FILTER_NEAREST,
            .mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            .compareEnable=VK_TRUE,.compareOp=VK_COMPARE_OP_LESS,.maxLod=0,.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE};
        if(vkCreateSampler(c.device,&sampler,nullptr,&t.sampler)!=VK_SUCCESS) return false;
    }
    std::cout<<"Shadow maps: count=2 resolution="<<resolution<<" format="<<depthFormat<<" compare=LESS filter=NEAREST\n";
    return true;
}
bool initializePipeline(VkDescriptorSetLayout sceneLayout,VkDescriptorSetLayout modelLayout) {
    auto& c=graphics::internal::context;
    VkDescriptorSetLayout sets[]={sceneLayout,modelLayout};
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Mat4)};
    VkPipelineLayoutCreateInfo li{.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=2,.pSetLayouts=sets,
        .pushConstantRangeCount=1,.pPushConstantRanges=&push};
    if(vkCreatePipelineLayout(c.device,&li,nullptr,&layout)!=VK_SUCCESS) return false;
    std::ifstream file("shaders/shadow.vert.spv",std::ios::binary|std::ios::ate);
    if(!file || file.tellg()<=0 || static_cast<size_t>(file.tellg())%4) return false;
    std::vector<uint32_t> code(static_cast<size_t>(file.tellg())/4);file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(code.data()),static_cast<std::streamsize>(code.size()*4))) return false;
    VkShaderModuleCreateInfo mi{.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=code.size()*4,.pCode=code.data()};
    VkShaderModule shader{};if(vkCreateShaderModule(c.device,&mi,nullptr,&shader)!=VK_SUCCESS) return false;
    VkPipelineShaderStageCreateInfo stage{.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=shader,.pName="main"};
    VkVertexInputBindingDescription binding{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attribute{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,position)};
    VkPipelineVertexInputStateCreateInfo vertex{.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount=1,.pVertexBindingDescriptions=&binding,.vertexAttributeDescriptionCount=1,.pVertexAttributeDescriptions=&attribute};
    VkPipelineInputAssemblyStateCreateInfo assembly{.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo viewport{.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.scissorCount=1};
    VkPipelineRasterizationStateCreateInfo raster{.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL,
        .cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.depthBiasEnable=VK_TRUE,.lineWidth=1};
    VkPipelineMultisampleStateCreateInfo multisample{.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo depth{.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable=VK_TRUE,.depthWriteEnable=VK_TRUE,.depthCompareOp=VK_COMPARE_OP_LESS};
    const VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_DEPTH_BIAS};
    VkPipelineDynamicStateCreateInfo dynamic{.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=3,.pDynamicStates=states};
    VkPipelineRenderingCreateInfo rendering{.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,.colorAttachmentCount=0,
        .depthAttachmentFormat=depthFormat,.stencilAttachmentFormat=VK_FORMAT_UNDEFINED};
    VkGraphicsPipelineCreateInfo pi{.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&rendering,.stageCount=1,.pStages=&stage,
        .pVertexInputState=&vertex,.pInputAssemblyState=&assembly,.pViewportState=&viewport,.pRasterizationState=&raster,
        .pMultisampleState=&multisample,.pDepthStencilState=&depth,.pDynamicState=&dynamic,.layout=layout,.renderPass=VK_NULL_HANDLE};
    const auto result=vkCreateGraphicsPipelines(c.device,VK_NULL_HANDLE,1,&pi,nullptr,&pipeline);
    vkDestroyShaderModule(c.device,shader,nullptr);
    return result==VK_SUCCESS;
}
void record(VkCommandBuffer cmd,size_t index,const Mat4& lightVP,const ShadowSettings& settings,bool active,
    const std::function<void(VkPipelineLayout)>& draw) {
    auto& map=maps[index];
    // Even an inactive map gets a first clear/layout transition: descriptors are always valid.
    if(!active && map.initialized) return;
    transition(cmd,map,true);
    VkRenderingAttachmentInfo attachment{.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,.imageView=map.texture.view,
        .imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp=VK_ATTACHMENT_STORE_OP_STORE,.clearValue={.depthStencil={1,0}}};
    VkRenderingInfo rendering{.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,.renderArea={.extent={resolution,resolution}},
        .layerCount=1,.colorAttachmentCount=0,.pDepthAttachment=&attachment};
    auto& c=graphics::internal::context;
    c.beginRendering(cmd,&rendering); // vkCmdBeginRendering, loaded via vkGetDeviceProcAddr.
    if(active) {
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        VkViewport viewport{0,0,static_cast<float>(resolution),static_cast<float>(resolution),0,1};
        VkRect2D scissor{{0,0},{resolution,resolution}};
        vkCmdSetViewport(cmd,0,1,&viewport);vkCmdSetScissor(cmd,0,1,&scissor);
        vkCmdSetDepthBias(cmd,std::clamp(settings.constantBias,0.f,16.f),0,std::clamp(settings.slopeBias,0.f,64.f));
        vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(Mat4),&lightVP);
        draw(layout);++map.passes;
    }
    c.endRendering(cmd); // vkCmdEndRendering.
    // The first transition was UNDEFINED -> attachment; this one must wait for the clear/draw writes.
    map.initialized=true;
    transition(cmd,map,false);
}
void shutdown() {
    auto& c=graphics::internal::context;
    if(pipeline) vkDestroyPipeline(c.device,pipeline,nullptr);pipeline={};
    if(layout) vkDestroyPipelineLayout(c.device,layout,nullptr);layout={};
    for(size_t i=0;i<2;++i) {
        std::cout<<"Shadow pass count["<<i<<"]="<<maps[i].passes<<"\n";
        gpu::destroyTexture(maps[i].texture);maps[i]={};
    }
    depthFormat=VK_FORMAT_UNDEFINED;
}
}
