#include "application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include <imgui.h>

namespace application {

namespace {

constexpr uint32_t cylinder_segments = 50;
constexpr float cylinder_radius = 1.0f;
constexpr float cylinder_half_height = 1.0f;
constexpr float minimum_scale = 0.05f;
constexpr float degrees_to_radians = std::numbers::pi_v<float> / 180.0f;

struct Vec3 {
	float x;
	float y;
	float z;
};

// Column-major matrices multiplied by column vectors, matching GLSL's mat4 convention.
struct Mat4 {
	std::array<float, 16> elements{};
};

struct Vertex {
	Vec3 position;
	Vec3 color;
};

struct PushConstants {
	Mat4 mvp;
	std::array<float, 4> tint_color;
};

static_assert(sizeof(PushConstants) == 80);
static_assert(offsetof(PushConstants, tint_color) == 64);

struct Buffer {
	VkBuffer handle = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

struct Controls {
	bool perspective = true;
	Vec3 position{ 0.0f, 0.0f, 0.0f };
	Vec3 rotation_degrees{ 0.0f, 0.0f, 0.0f };
	Vec3 scale{ 1.0f, 1.0f, 1.0f };
	bool animation_playing = true;
	float animation_speed = 1.0f;
	float trajectory_radius = 1.6f;
	float vertical_amplitude = 0.65f;
	float vertical_frequency = 2.0f;
	float rotation_speed_degrees = 55.0f;
	std::array<float, 4> tint_color{ 1.0f, 1.0f, 1.0f, 1.0f };
};

Buffer vertex_buffer;
Buffer index_buffer;
uint32_t index_count = 0;

VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkPipeline pipeline = VK_NULL_HANDLE;

Controls controls;
Mat4 current_mvp;
double previous_time = 0.0;
bool has_previous_time = false;
// Do not wrap phase at 2*pi: a fractional vertical frequency would jump.
double animation_phase = 0.0;
double animation_rotation_degrees = 0.0;

Mat4 identity() {
	Mat4 result{};
	result.elements[0] = 1.0f;
	result.elements[5] = 1.0f;
	result.elements[10] = 1.0f;
	result.elements[15] = 1.0f;
	return result;
}

Mat4 multiply(const Mat4& left, const Mat4& right) {
	Mat4 result{};
	for (size_t column = 0; column < 4; ++column) {
		for (size_t row = 0; row < 4; ++row) {
			for (size_t i = 0; i < 4; ++i) {
				result.elements[column * 4 + row] +=
					left.elements[i * 4 + row] * right.elements[column * 4 + i];
			}
		}
	}
	return result;
}

Mat4 translation(const Vec3& offset) {
	Mat4 result = identity();
	result.elements[12] = offset.x;
	result.elements[13] = offset.y;
	result.elements[14] = offset.z;
	return result;
}

Mat4 rotationX(float radians) {
	Mat4 result = identity();
	const float cosine = std::cos(radians);
	const float sine = std::sin(radians);
	result.elements[5] = cosine;
	result.elements[6] = sine;
	result.elements[9] = -sine;
	result.elements[10] = cosine;
	return result;
}

Mat4 rotationY(float radians) {
	Mat4 result = identity();
	const float cosine = std::cos(radians);
	const float sine = std::sin(radians);
	result.elements[0] = cosine;
	result.elements[2] = -sine;
	result.elements[8] = sine;
	result.elements[10] = cosine;
	return result;
}

Mat4 rotationZ(float radians) {
	Mat4 result = identity();
	const float cosine = std::cos(radians);
	const float sine = std::sin(radians);
	result.elements[0] = cosine;
	result.elements[1] = sine;
	result.elements[4] = -sine;
	result.elements[5] = cosine;
	return result;
}

Mat4 scaling(const Vec3& factors) {
	Mat4 result{};
	result.elements[0] = factors.x;
	result.elements[5] = factors.y;
	result.elements[10] = factors.z;
	result.elements[15] = 1.0f;
	return result;
}

// Right-handed Vulkan projection: depth is [0, 1] and clip-space Y is inverted.
Mat4 perspective(float vertical_fov_radians, float aspect, float near_plane, float far_plane) {
	const float focal_length = 1.0f / std::tan(vertical_fov_radians * 0.5f);
	Mat4 result{};
	result.elements[0] = focal_length / aspect;
	result.elements[5] = -focal_length;
	result.elements[10] = far_plane / (near_plane - far_plane);
	result.elements[11] = -1.0f;
	result.elements[14] = (far_plane * near_plane) / (near_plane - far_plane);
	return result;
}

Mat4 orthographic(float left, float right, float bottom, float top,
				  float near_plane, float far_plane) {
	Mat4 result = identity();
	result.elements[0] = 2.0f / (right - left);
	result.elements[5] = -2.0f / (top - bottom);
	result.elements[10] = 1.0f / (near_plane - far_plane);
	result.elements[12] = -(right + left) / (right - left);
	result.elements[13] = (top + bottom) / (top - bottom);
	result.elements[14] = near_plane / (near_plane - far_plane);
	return result;
}

Vec3 proceduralColor(const Vec3& position) {
	return {
		(position.x / cylinder_radius + 1.0f) * 0.5f,
		(position.y / cylinder_half_height + 1.0f) * 0.5f,
		(position.z / cylinder_radius + 1.0f) * 0.5f,
	};
}

void generateCylinder(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
	vertices.reserve(cylinder_segments * 2 + 2);
	indices.reserve(cylinder_segments * 12);

	for (uint32_t segment = 0; segment < cylinder_segments; ++segment) {
		const float angle = 2.0f * std::numbers::pi_v<float> *
			static_cast<float>(segment) / static_cast<float>(cylinder_segments);
		const float x = cylinder_radius * std::cos(angle);
		const float z = cylinder_radius * std::sin(angle);
		const Vec3 bottom_position{ x, -cylinder_half_height, z };
		const Vec3 top_position{ x, cylinder_half_height, z };
		vertices.push_back({ bottom_position, proceduralColor(bottom_position) });
		vertices.push_back({ top_position, proceduralColor(top_position) });
	}

	const uint32_t bottom_center = static_cast<uint32_t>(vertices.size());
	const Vec3 bottom_center_position{ 0.0f, -cylinder_half_height, 0.0f };
	vertices.push_back({ bottom_center_position, proceduralColor(bottom_center_position) });
	const uint32_t top_center = static_cast<uint32_t>(vertices.size());
	const Vec3 top_center_position{ 0.0f, cylinder_half_height, 0.0f };
	vertices.push_back({ top_center_position, proceduralColor(top_center_position) });

	for (uint32_t segment = 0; segment < cylinder_segments; ++segment) {
		const uint32_t next = (segment + 1) % cylinder_segments;
		const uint32_t bottom = segment * 2;
		const uint32_t top = bottom + 1;
		const uint32_t next_bottom = next * 2;
		const uint32_t next_top = next_bottom + 1;

		// The side and both caps use outward-facing counter-clockwise winding.
		indices.insert(indices.end(), {
			bottom, top, next_bottom,
			top, next_top, next_bottom,
			bottom_center, bottom, next_bottom,
			top_center, next_top, top,
		});
	}
}

bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const void* data, Buffer& buffer) {
	auto& context = graphics::internal::context;
	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	const VmaAllocationCreateInfo allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &buffer_info, &allocation_info,
					&buffer.handle, &buffer.allocation, nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan buffer\n";
		return false;
	}

	void* mapped_data = nullptr;
	if (vmaMapMemory(context.allocator, buffer.allocation, &mapped_data) != VK_SUCCESS) {
		std::cerr << "Failed to map Vulkan buffer memory\n";
		vmaDestroyBuffer(context.allocator, buffer.handle, buffer.allocation);
		buffer = {};
		return false;
	}
	std::memcpy(mapped_data, data, static_cast<size_t>(size));
	const VkResult flush_result = vmaFlushAllocation(context.allocator, buffer.allocation, 0, size);
	vmaUnmapMemory(context.allocator, buffer.allocation);
	if (flush_result != VK_SUCCESS) {
		std::cerr << "Failed to flush Vulkan buffer memory\n";
		return false;
	}
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

bool createPipeline() {
	auto& context = graphics::internal::context;
	const VkPushConstantRange push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(PushConstants),
	};
	const VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constant_range,
	};
	if (vkCreatePipelineLayout(context.device, &layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan pipeline layout\n";
		return false;
	}

	const VkShaderModule vertex_shader = createShaderModule("shaders/lab1.vert.spv");
	const VkShaderModule fragment_shader = createShaderModule("shaders/lab1.frag.spv");
	if (vertex_shader == VK_NULL_HANDLE || fragment_shader == VK_NULL_HANDLE) {
		if (vertex_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context.device, vertex_shader, nullptr);
		}
		if (fragment_shader != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context.device, fragment_shader, nullptr);
		}
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
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	const VkVertexInputAttributeDescription attributes[] = {
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
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(std::size(attributes)),
		.pVertexAttributeDescriptions = attributes,
	};
	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};
	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
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
		.depthWriteEnable = VK_TRUE,
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
		.layout = pipeline_layout,
		.renderPass = context.render_pass,
		.subpass = 0,
	};

	const VkResult result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
		&pipeline_info, nullptr, &pipeline);
	vkDestroyShaderModule(context.device, fragment_shader, nullptr);
	vkDestroyShaderModule(context.device, vertex_shader, nullptr);
	if (result != VK_SUCCESS) {
		std::cerr << "Failed to create Vulkan graphics pipeline\n";
		return false;
	}
	return true;
}

void destroyResources() {
	auto& context = graphics::internal::context;
	if (pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context.device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	if (pipeline_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
		pipeline_layout = VK_NULL_HANDLE;
	}
	if (index_buffer.handle != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, index_buffer.handle, index_buffer.allocation);
		index_buffer = {};
	}
	if (vertex_buffer.handle != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vertex_buffer.handle, vertex_buffer.allocation);
		vertex_buffer = {};
	}
}

void resetControls() {
	controls = {};
	animation_phase = 0.0f;
	animation_rotation_degrees = 0.0f;
}

void drawControls() {
	ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(410.0f, 510.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin("Lab 1 Controls");
	ImGui::TextUnformatted("Variant 8: cylinder, 50 segments, 200 triangles");
	ImGui::TextUnformatted("Projection");
	if (ImGui::RadioButton("Perspective", controls.perspective)) {
		controls.perspective = true;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Orthographic", !controls.perspective)) {
		controls.perspective = false;
	}

	ImGui::SeparatorText("Model transform");
	ImGui::DragFloat3("Position X/Y/Z", &controls.position.x, 0.02f, -10.0f, 10.0f);
	ImGui::DragFloat3("Rotation X/Y/Z", &controls.rotation_degrees.x, 0.5f, -360.0f, 360.0f, "%.1f deg");
	ImGui::DragFloat3("Scale X/Y/Z", &controls.scale.x, 0.01f, minimum_scale, 5.0f);
	controls.scale.x = std::max(controls.scale.x, minimum_scale);
	controls.scale.y = std::max(controls.scale.y, minimum_scale);
	controls.scale.z = std::max(controls.scale.z, minimum_scale);

	ImGui::SeparatorText("Animation");
	ImGui::Checkbox("Play", &controls.animation_playing);
	ImGui::SliderFloat("Animation speed", &controls.animation_speed, 0.05f, 4.0f, "%.2fx");
	ImGui::SliderFloat("Trajectory radius", &controls.trajectory_radius, 0.0f, 4.0f);
	ImGui::SliderFloat("Vertical amplitude", &controls.vertical_amplitude, 0.0f, 3.0f);
	ImGui::SliderFloat("Vertical frequency", &controls.vertical_frequency, 0.25f, 5.0f);
	ImGui::SliderFloat("Rotation speed", &controls.rotation_speed_degrees, -180.0f, 180.0f, "%.0f deg/s");

	ImGui::SeparatorText("Appearance");
	ImGui::ColorEdit3("Tint color", controls.tint_color.data());
	ImGui::TextUnformatted("Vertex color * tint (white keeps original colors)");
	if (ImGui::Button("Reset")) {
		resetControls();
	}
	ImGui::End();
}

void updateMvp() {
	const float trajectory_x = controls.trajectory_radius * static_cast<float>(std::cos(animation_phase));
	const float trajectory_z = controls.trajectory_radius * static_cast<float>(std::sin(animation_phase));
	const float trajectory_y = controls.vertical_amplitude *
		static_cast<float>(std::sin(controls.vertical_frequency * animation_phase));
	const Vec3 animated_position{
		controls.position.x + trajectory_x,
		controls.position.y + trajectory_y,
		controls.position.z + trajectory_z,
	};
	const Vec3 animated_rotation{
		(controls.rotation_degrees.x + static_cast<float>(std::remainder(animation_rotation_degrees * 0.55, 360.0))) * degrees_to_radians,
		(controls.rotation_degrees.y + static_cast<float>(std::remainder(animation_rotation_degrees, 360.0))) * degrees_to_radians,
		controls.rotation_degrees.z * degrees_to_radians,
	};

	// With column vectors this is applied right-to-left: Scale, rotations, Translation.
	const Mat4 rotation = multiply(rotationZ(animated_rotation.z),
		multiply(rotationY(animated_rotation.y), rotationX(animated_rotation.x)));
	const Mat4 model = multiply(translation(animated_position), multiply(rotation, scaling(controls.scale)));
	// Inverse camera transform: camera at (0, 0, 10), looking along -Z.
	const Mat4 view = translation({ 0.0f, 0.0f, -10.0f });

	const auto& extent = graphics::internal::context.swapchain_extent;
	const float safe_height = static_cast<float>(std::max(extent.height, 1u));
	const float aspect = static_cast<float>(std::max(extent.width, 1u)) / safe_height;
	Mat4 projection{};
	if (controls.perspective) {
		projection = perspective(50.0f * degrees_to_radians, aspect, 0.1f, 100.0f);
	} else {
		constexpr float half_height = 4.65f;
		projection = orthographic(-half_height * aspect, half_height * aspect,
			-half_height, half_height, 0.1f, 100.0f);
	}
	current_mvp = multiply(projection, multiply(view, model));
}

} // namespace

bool initialize() {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	generateCylinder(vertices, indices);
	index_count = static_cast<uint32_t>(indices.size());

	if (!createBuffer(vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			vertices.data(), vertex_buffer) ||
		!createBuffer(indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			indices.data(), index_buffer) ||
		!createPipeline()) {
		destroyResources();
		return false;
	}

	resetControls();
	updateMvp();
	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);
	destroyResources();
}

void update(double time) {
	if (!has_previous_time) {
		previous_time = time;
		has_previous_time = true;
	}
	const double elapsed = std::clamp(time - previous_time, 0.0, 0.1);
	previous_time = time;
	if (controls.animation_playing) {
		const float scaled_elapsed = static_cast<float>(elapsed) * controls.animation_speed;
		animation_phase += scaled_elapsed;
		animation_rotation_degrees += scaled_elapsed * controls.rotation_speed_degrees;
	}

	drawControls();
}

void render(const graphics::internal::FrameData& fd) {
	auto& context = graphics::internal::context;
	// prepare() may rebuild the swapchain after update(); use the new extent now.
	updateMvp();
	vkResetCommandBuffer(fd.command_buffer, 0);
	const VkCommandBufferBeginInfo command_buffer_begin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(fd.command_buffer, &command_buffer_begin);

	const VkClearValue clear_values[] = {
		{ .color = { { 0.035f, 0.045f, 0.065f, 1.0f } } },
		{ .depthStencil = { 1.0f, 0 } },
	};
	const VkRenderPassBeginInfo render_pass_begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .offset = { 0, 0 }, .extent = context.swapchain_extent },
		.clearValueCount = static_cast<uint32_t>(std::size(clear_values)),
		.pClearValues = clear_values,
	};
	vkCmdBeginRenderPass(fd.command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	const VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(context.swapchain_extent.width),
		.height = static_cast<float>(context.swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor = { .offset = { 0, 0 }, .extent = context.swapchain_extent };
	vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

	const VkDeviceSize vertex_offset = 0;
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertex_buffer.handle, &vertex_offset);
	vkCmdBindIndexBuffer(fd.command_buffer, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
	const PushConstants push_constants = {
		.mvp = current_mvp,
		.tint_color = controls.tint_color,
	};
	vkCmdPushConstants(fd.command_buffer, pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof(PushConstants), &push_constants);
	vkCmdDrawIndexed(fd.command_buffer, index_count, 1, 0, 0, 0);

	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application
