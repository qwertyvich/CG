#include "renderer.hpp"
#include "gpu_upload.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace lab3::renderer {
namespace {
using namespace lab3::gpu;
struct MarkerPush { Mat4 model; Float4 color; };
static_assert(sizeof(MarkerPush)==80 && offsetof(MarkerPush,color)==64);
Buffer vertices,indices,cubeVertices,cubeIndices,sceneBuffer,modelBuffer,pointBuffer,spotBuffer,lineBuffer;
VkDescriptorSetLayout sceneLayout{},modelLayout{};
VkDescriptorPool pool{};
VkDescriptorSet sets[1]{},materialSets[3]{};
Texture textures[6]{};
bool textureAvailable[6]{};
VkDeviceSize modelStride=0;
VkPipelineLayout litLayout{},unlitLayout{};
VkPipeline litPipeline{},basePipeline{},markerPipeline{},linePipeline{};
size_t pointLimit=0,spotLimit=0;

void writeDescriptor(uint32_t set,uint32_t binding,VkDescriptorType type,const Buffer& b) {
    VkDescriptorBufferInfo info{b.handle,0,b.bytes};
    VkWriteDescriptorSet write{.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=sets[set],.dstBinding=binding,
        .descriptorCount=1,.descriptorType=type,.pBufferInfo=&info};
    vkUpdateDescriptorSets(graphics::internal::context.device,1,&write,0,nullptr);
}
bool createDescriptors() {
    auto device=graphics::internal::context.device;
    constexpr auto both=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutBinding sceneBindings[]={
        {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,both,nullptr},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo info{.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=3,.pBindings=sceneBindings};
    if(vkCreateDescriptorSetLayout(device,&info,nullptr,&sceneLayout)!=VK_SUCCESS) return false;
    VkDescriptorSetLayoutBinding materialBindings[]={
        {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,both,nullptr},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
    info.bindingCount=4;info.pBindings=materialBindings;
    if(vkCreateDescriptorSetLayout(device,&info,nullptr,&modelLayout)!=VK_SUCCESS) return false;
    VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,3},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,9}};
    VkDescriptorPoolCreateInfo pi{.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=4,.poolSizeCount=4,.pPoolSizes=sizes};
    if(vkCreateDescriptorPool(device,&pi,nullptr,&pool)!=VK_SUCCESS) return false;
    VkDescriptorSetLayout layouts[]={sceneLayout,modelLayout,modelLayout,modelLayout};
    VkDescriptorSet allocated[4]{};
    VkDescriptorSetAllocateInfo ai{.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=pool,.descriptorSetCount=4,.pSetLayouts=layouts};
    if(vkAllocateDescriptorSets(device,&ai,allocated)!=VK_SUCCESS) return false;
    sets[0]=allocated[0];for(size_t i=0;i<3;++i) materialSets[i]=allocated[i+1];
    VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(graphics::internal::context.physical_device,&props);
    const auto alignment=props.limits.minUniformBufferOffsetAlignment;
    modelStride=modelStrideFor(alignment);
    if(sizeof(ModelGpu)>props.limits.maxUniformBufferRange || modelStride>UINT32_MAX/2 ||
        modelStride+sizeof(ModelGpu)>modelStride*2 || props.limits.maxPushConstantsSize<sizeof(SamplingPush)) return false;
    if(!createBuffer(sizeof(SceneGpu),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,sceneBuffer) ||
       !createBuffer(modelStride*2,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,modelBuffer) ||
       !createBuffer(sizeof(PointGpu),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,pointBuffer) ||
       !createBuffer(sizeof(SpotGpu),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,spotBuffer)) return false;
    std::cout<<"Dynamic model UBO: buffers=1 sizeof="<<sizeof(ModelGpu)<<" alignment="<<alignment
        <<" stride="<<modelStride<<" offsets=0,"<<modelStride<<" range="<<sizeof(ModelGpu)<<"\n";
    writeDescriptor(0,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,sceneBuffer);
    writeDescriptor(0,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,pointBuffer);
    writeDescriptor(0,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,spotBuffer);
    // grid/stripes have no optional files: both descriptors bind the same black resource.
    const size_t maps[3][3]={{1,0,0},{2,0,0},{3,textureAvailable[4]?4u:0u,textureAvailable[5]?5u:0u}};
    for(size_t material=0;material<3;++material) {
        VkDescriptorBufferInfo bufferInfo{modelBuffer.handle,0,sizeof(ModelGpu)};
        VkWriteDescriptorSet write{.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=materialSets[material],.dstBinding=0,
            .descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,.pBufferInfo=&bufferInfo};
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
        for(uint32_t slot=0;slot<3;++slot) {
            const auto& t=textures[maps[material][slot]];
            VkDescriptorImageInfo imageInfo{t.sampler,t.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet imageWrite{.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=materialSets[material],.dstBinding=slot+1,
                .descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,.pImageInfo=&imageInfo};
            vkUpdateDescriptorSets(device,1,&imageWrite,0,nullptr);
        }
        std::cout<<"Material "<<material<<": "<<materialNames[material]<<" maps="<<maps[material][0]<<","<<maps[material][1]<<","<<maps[material][2]<<"\n";
    }
    VkPushConstantRange samplingPush{VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(SamplingPush)};
    VkPipelineLayoutCreateInfo li{.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=2,.pSetLayouts=layouts,
        .pushConstantRangeCount=1,.pPushConstantRanges=&samplingPush};
    if(vkCreatePipelineLayout(device,&li,nullptr,&litLayout)!=VK_SUCCESS) return false;
    VkPushConstantRange markerPush{VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(MarkerPush)};
    li.setLayoutCount=1;li.pPushConstantRanges=&markerPush;
    return vkCreatePipelineLayout(device,&li,nullptr,&unlitLayout)==VK_SUCCESS;
}
// Только после prepare(): fence подтверждает завершение чтения старых buffers/sets.
bool ensureStorage(Buffer& b,size_t count,size_t stride,size_t limit,uint32_t binding) {
    if(count>limit) {std::cerr<<"Storage device limit exceeded\n";return false;}
    size_t capacity=static_cast<size_t>(b.bytes)/stride;
    if(count<=capacity) return true;
    size_t next=std::min(limit,std::max(count,capacity*2));
    Buffer replacement;
    if(!createBuffer(next*stride,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,replacement)) return false;
    writeDescriptor(0,binding,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,replacement);
    destroyBuffer(b);b=replacement;
    std::cout<<"SSBO binding "<<binding<<" capacity "<<next<<"\n";
    return true;
}
std::vector<uint32_t> readShader(const std::string& path) {
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file) {
		std::cerr << "Failed to open shader: " << path << '\n';
		return {};
	}

	const std::streamsize byte_count = file.tellg();
	if (byte_count <= 0 || byte_count % 4 != 0) {
		std::cerr << "Invalid SPIR-V file: " << path << '\n';
		return {};
	}
	std::vector<uint32_t> code(static_cast<size_t>(byte_count) / sizeof(uint32_t));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(code.data()), byte_count)) {
		std::cerr << "Failed to read shader: " << path << '\n';
		return {};
	}
	return code;
}

VkShaderModule createShaderModule(const std::string& path) {
	const std::vector<uint32_t> code = readShader(path);
	if (code.empty()) {
		return VK_NULL_HANDLE;
	}
	const VkShaderModuleCreateInfo module_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size() * sizeof(uint32_t),
		.pCode = code.data(),
	};
	VkShaderModule module = VK_NULL_HANDLE;
	if (vkCreateShaderModule(graphics::internal::context.device, &module_info, nullptr, &module) != VK_SUCCESS) {
		std::cerr << "Failed to create shader module: " << path << '\n';
		return VK_NULL_HANDLE;
	}
	return module;
}

bool createPipeline(bool unlit,bool lines,VkPipeline& output,bool baseOnly=false) {
    auto& context=graphics::internal::context;
    std::string base="shaders";
    auto vertex_shader=createShaderModule(base+(unlit?"/unlit.vert.spv":"/lab3.vert.spv"));
    auto fragment_shader=createShaderModule(base+(unlit?"/unlit.frag.spv":(baseOnly?"/lab3-base.frag.spv":"/lab3.frag.spv")));
    if(!vertex_shader || !fragment_shader) {
        if(vertex_shader) vkDestroyShaderModule(context.device,vertex_shader,nullptr);
        if(fragment_shader) vkDestroyShaderModule(context.device,fragment_shader,nullptr);
        return false;
    }
	const VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertex_shader,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragment_shader,
			.pName = "main",
		},
	};
	const VkVertexInputBindingDescription binding = {
		.binding = 0,
		.stride = lines ? sizeof(LineVertex) : sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	const VkVertexInputAttributeDescription attributes[] = {
        { .location = 3, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv) },
        { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal) },
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position),
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color),
		},
	};
	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding,
		.vertexAttributeDescriptionCount = unlit ? 2u : 3u,
		.pVertexAttributeDescriptions = unlit ? attributes+2 : attributes,
	};
	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};
	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = static_cast<VkCullModeFlags>(unlit ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT),
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};
	const VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};
	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = lines ? VK_FALSE : VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
	};
	const VkPipelineColorBlendAttachmentState color_blend_attachment = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	const VkPipelineColorBlendStateCreateInfo color_blending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
	};
	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	const VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(std::size(dynamic_states)),
		.pDynamicStates = dynamic_states,
	};
	const VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = static_cast<uint32_t>(std::size(shader_stages)),
		.pStages = shader_stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.pDynamicState = &dynamic_state,
		.layout = unlit ? unlitLayout : litLayout,
		.renderPass = context.render_pass,
		.subpass = 0,
	};

	const VkResult result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
		&pipeline_info, nullptr, &output);
	vkDestroyShaderModule(context.device, fragment_shader, nullptr);
	vkDestroyShaderModule(context.device, vertex_shader, nullptr);
	if (result != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan graphics pipeline\n";
		return false;
	}
	return true;
}


bool uploadScene(const Scene& s,SceneGpu& sg) {
    auto& context=graphics::internal::context;
    ModelGpu mg;
    bool srgb=context.swapchain_format==VK_FORMAT_B8G8R8A8_SRGB || context.swapchain_format==VK_FORMAT_R8G8B8A8_SRGB;
    for(size_t i=0;i<s.models.size();++i) {
        buildUniforms(s,static_cast<float>(context.swapchain_extent.width)/context.swapchain_extent.height,!srgb,sg,mg,i);
        if(!writeBuffer(modelBuffer,&mg,sizeof(mg),i*modelStride)) return false;
    }
    auto points=packPoints(s);auto spots=packSpots(s);
    if(!ensureStorage(pointBuffer,points.size(),sizeof(PointGpu),pointLimit,1) ||
       !ensureStorage(spotBuffer,spots.size(),sizeof(SpotGpu),spotLimit,2)) return false;
    return writeBuffer(sceneBuffer,&sg,sizeof(sg)) &&
        writeBuffer(pointBuffer,points.data(),points.size()*sizeof(PointGpu)) &&
        writeBuffer(spotBuffer,spots.data(),spots.size()*sizeof(SpotGpu));
}
bool uploadLines(const std::vector<LineVertex>& lines) {
    static std::vector<LineVertex> previous;
    VkDeviceSize bytes=lines.size()*sizeof(LineVertex);
    if(previous.size()==lines.size() && (!bytes || (lineBuffer.handle && std::memcmp(previous.data(),lines.data(),static_cast<size_t>(bytes))==0))) return true;
    if(!bytes) {previous.clear();return true;}
    Buffer replacement;
    if(!uploadGeometry(bytes,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,replacement,lines.data())) return false;
    destroyBuffer(lineBuffer);lineBuffer=replacement;previous=lines;
    return true;
}

void drawMarkers(const Scene& s,VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,markerPipeline);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,unlitLayout,0,1,sets,0,nullptr);
    const VkDeviceSize offset=0;
    vkCmdBindVertexBuffers(cmd,0,1,&cubeVertices.handle,&offset);
    vkCmdBindIndexBuffer(cmd,cubeIndices.handle,0,VK_INDEX_TYPE_UINT32);
    auto marker=[&](Vec3 p,Vec3 color) {
        MarkerPush push{multiply(translation(p),scaling({0.09f,0.09f,0.09f})),pack(color,1)};
        vkCmdPushConstants(cmd,unlitLayout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(push),&push);
        vkCmdDrawIndexed(cmd,36,1,0,0,0);
    };
    // Постоянный цвет обозначает источник независимо от intensity и материала.
    for(const auto& l:s.points) marker(l.position,l.color);
    for(const auto& l:s.spots) marker(l.position,l.color);
}
} // namespace
size_t pointCapacity() {return static_cast<size_t>(pointBuffer.bytes)/sizeof(PointGpu);}
size_t spotCapacity() {return static_cast<size_t>(spotBuffer.bytes)/sizeof(SpotGpu);}
size_t maxPoints() {return pointLimit;}
size_t maxSpots() {return spotLimit;}
bool initialize() {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(graphics::internal::context.physical_device,&props);
    pointLimit=props.limits.maxStorageBufferRange/sizeof(PointGpu);
    spotLimit=props.limits.maxStorageBufferRange/sizeof(SpotGpu);
    std::cout<<"GPU: "<<props.deviceName<<" / maxStorageBufferRange "<<props.limits.maxStorageBufferRange<<"\n";
    auto cylinder=generateCylinder(),cube=generateMarkerCube();
    bool ok=initializeUploads();
    const char* files[]={"black.png","uv_grid.png","stripes.png","panels.png","panels_specular.png","panels_emissive.png"};
    for(size_t i=0;ok && i<6;++i) {
        const auto format=(i==0 || i==4)?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R8G8B8A8_SRGB;
        const auto path=std::filesystem::path("assets/textures")/files[i];
        if(i>=4 && !std::filesystem::exists(path)) {
            std::cout<<"Optional map missing: "<<path.string()<<" -> black 1x1 fallback\n";
            continue;
        }
        ok=loadTexture(path,format,textures[i]);textureAvailable[i]=ok;
    }
    std::cout<<"Cylinder vertices="<<cylinder.vertices.size()<<" indices="<<cylinder.indices.size()<<"\n";
    ok=ok && uploadGeometry(cylinder.vertices.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,vertices,cylinder.vertices.data()) &&
        uploadGeometry(cylinder.indices.size()*sizeof(uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,indices,cylinder.indices.data()) &&
        uploadGeometry(cube.vertices.size()*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,cubeVertices,cube.vertices.data()) &&
        uploadGeometry(cube.indices.size()*sizeof(uint32_t),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,cubeIndices,cube.indices.data()) &&
        createDescriptors() && createPipeline(false,false,litPipeline) && createPipeline(false,false,basePipeline,true) && createPipeline(true,false,markerPipeline) && createPipeline(true,true,linePipeline);
    if(!ok) {std::cerr<<"Failed to initialize Lab 3 renderer\n";shutdown();}
    return ok;
}
void shutdown() {
    auto& c=graphics::internal::context;
    if(!c.device) return;
    vkDeviceWaitIdle(c.device);
    for(auto p:{litPipeline,basePipeline,markerPipeline,linePipeline}) if(p) vkDestroyPipeline(c.device,p,nullptr);
    litPipeline=basePipeline=markerPipeline=linePipeline={};
    if(litLayout) vkDestroyPipelineLayout(c.device,litLayout,nullptr);
    if(unlitLayout) vkDestroyPipelineLayout(c.device,unlitLayout,nullptr);
    litLayout=unlitLayout={};
    if(pool) vkDestroyDescriptorPool(c.device,pool,nullptr);pool={};
    if(sceneLayout) vkDestroyDescriptorSetLayout(c.device,sceneLayout,nullptr);
    if(modelLayout) vkDestroyDescriptorSetLayout(c.device,modelLayout,nullptr);
    sceneLayout=modelLayout={};
    for(auto& t:textures) destroyTexture(t);
    for(Buffer* b:{&vertices,&indices,&cubeVertices,&cubeIndices,&sceneBuffer,&modelBuffer,&pointBuffer,&spotBuffer,&lineBuffer}) destroyBuffer(*b);
    shutdownUploads();
}
bool render(const Scene& s,const graphics::internal::FrameData& fd) {
    if(!fd.command_buffer || !fd.framebuffer) return false;
    // prepare() дождался fence: теперь можно записывать и заменять GPU buffers/sets.
    SceneGpu sg;
    if(!uploadScene(s,sg)) {std::cerr<<"Scene upload failed\n";return false;}
    std::vector<LineVertex> lines=s.showCones?generateSpotCones(s):std::vector<LineVertex>{};
    if(lines.size()>std::numeric_limits<uint32_t>::max() || !uploadLines(lines)) return false;
    auto& c=graphics::internal::context;
    auto cmd=fd.command_buffer;
    if(vkResetCommandBuffer(cmd,0)!=VK_SUCCESS) return false;
    VkCommandBufferBeginInfo begin{.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    if(vkBeginCommandBuffer(cmd,&begin)!=VK_SUCCESS) return false;
    VkClearValue clear[]={ {.color={{0.012f,0.018f,0.03f,1}}},{.depthStencil={1,0}} };
    VkRenderPassBeginInfo rp{.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,.renderPass=c.render_pass,.framebuffer=fd.framebuffer,
        .renderArea={.extent=c.swapchain_extent},.clearValueCount=2,.pClearValues=clear};
    vkCmdBeginRenderPass(cmd,&rp,VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp{0,0,static_cast<float>(c.swapchain_extent.width),static_cast<float>(c.swapchain_extent.height),0,1};
    VkRect2D scissor{{0,0},c.swapchain_extent};
    vkCmdSetViewport(cmd,0,1,&vp);vkCmdSetScissor(cmd,0,1,&scissor);
    const VkDeviceSize offset=0;
    vkCmdBindVertexBuffers(cmd,0,1,&vertices.handle,&offset);
    vkCmdBindIndexBuffer(cmd,indices.handle,0,VK_INDEX_TYPE_UINT32);
    for(size_t i=0;i<s.models.size();++i) {
        const auto& o=s.models[i];
        if(o.materialIndex<0 || o.materialIndex>=3) return false;
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,o.specialSampling?litPipeline:basePipeline);
        VkDescriptorSet bound[]={sets[0],materialSets[o.materialIndex]};
        const uint32_t dynamicOffset=static_cast<uint32_t>(i*modelStride);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,litLayout,0,2,bound,1,&dynamicOffset);
        // Base shader does not declare/use push constants. Only the effect needs them.
        if(o.specialSampling) {
            SamplingPush push{{1,0,0,0},{static_cast<float>(s.effectTime),s.effectStrength,0,0}};
            vkCmdPushConstants(cmd,litLayout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
        }
        vkCmdDrawIndexed(cmd,600,1,0,0,0);
    }
    if(s.showMarkers) drawMarkers(s,cmd);
    if(!lines.empty()) {
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,linePipeline);
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,unlitLayout,0,1,sets,0,nullptr);
        vkCmdBindVertexBuffers(cmd,0,1,&lineBuffer.handle,&offset);
        MarkerPush push{identity(),{1,1,1,1}};
        vkCmdPushConstants(cmd,unlitLayout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(push),&push);
        vkCmdDraw(cmd,static_cast<uint32_t>(lines.size()),1,0,0);
    }
    vkCmdEndRenderPass(cmd);
    return vkEndCommandBuffer(cmd)==VK_SUCCESS;
}
} // namespace lab3::renderer
