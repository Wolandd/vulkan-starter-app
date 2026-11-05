#include "application.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <imgui.h>

namespace application {
namespace {

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
};

// Размеры: 2 x 1.5 x 1.
const std::array<glm::vec3, 8> corners = {{
	{ -1.0f, -0.75f, -0.5f },
	{  1.0f, -0.75f, -0.5f },
	{  1.0f,  0.75f, -0.5f },
	{ -1.0f,  0.75f, -0.5f },
	{ -1.0f, -0.75f,  0.5f },
	{  1.0f, -0.75f,  0.5f },
	{  1.0f,  0.75f,  0.5f },
	{ -1.0f,  0.75f,  0.5f },
}};

const std::array<glm::vec3, 6> face_colors = {{
	{ 0.65f, 0.30f, 0.85f },
	{ 1.00f, 0.45f, 0.15f },
	{ 0.15f, 0.80f, 0.85f },
	{ 0.15f, 0.40f, 0.95f },
	{ 0.90f, 0.20f, 0.30f },
	{ 0.30f, 0.85f, 0.40f },
}};

// Дубли углов нужны для разных цветов граней.
const std::array<Vertex, 24> vertices = {{
	{ corners[0], face_colors[0] }, { corners[3], face_colors[0] },
	{ corners[2], face_colors[0] }, { corners[1], face_colors[0] },
	{ corners[4], face_colors[1] }, { corners[5], face_colors[1] },
	{ corners[6], face_colors[1] }, { corners[7], face_colors[1] },
	{ corners[0], face_colors[2] }, { corners[4], face_colors[2] },
	{ corners[7], face_colors[2] }, { corners[3], face_colors[2] },
	{ corners[1], face_colors[3] }, { corners[2], face_colors[3] },
	{ corners[6], face_colors[3] }, { corners[5], face_colors[3] },
	{ corners[0], face_colors[4] }, { corners[1], face_colors[4] },
	{ corners[5], face_colors[4] }, { corners[4], face_colors[4] },
	{ corners[3], face_colors[5] }, { corners[7], face_colors[5] },
	{ corners[6], face_colors[5] }, { corners[2], face_colors[5] },
}};

constexpr std::array<uint16_t, 36> indices = {
	 0,  1,  2,   0,  2,  3, // -Z
	 4,  5,  6,   4,  6,  7, // +Z
	 8,  9, 10,   8, 10, 11, // -X
	12, 13, 14,  12, 14, 15, // +X
	16, 17, 18,  16, 18, 19, // -Y
	20, 21, 22,  20, 22, 23, // +Y
};

VkBuffer vertex_buffer = VK_NULL_HANDLE;
VkBuffer index_buffer = VK_NULL_HANDLE;
VmaAllocation vertex_allocation = nullptr;
VmaAllocation index_allocation = nullptr;
VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkPipeline pipeline = VK_NULL_HANDLE;

glm::vec3 position(0.0f);
glm::vec3 rotation_degrees(0.0f);
glm::vec3 scale(1.0f);
int projection_type = 0;

bool checkResult(VkResult result, const char* operation) {
	if (result == VK_SUCCESS) {
		return true;
	}
	std::cerr << operation << " failed (VkResult " << result << ")\n";
	return false;
}

bool createBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
				  VkBuffer& buffer, VmaAllocation& allocation) {
	const auto& context = graphics::internal::context;
	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	// Буфер маленький, заполняем с CPU.
	const VmaAllocationCreateInfo allocation_info = {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
				 VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};
	VmaAllocationInfo mapped{};
	if (!checkResult(vmaCreateBuffer(context.allocator, &buffer_info, &allocation_info,
									 &buffer, &allocation, &mapped), "vmaCreateBuffer")) {
		return false;
	}
	std::memcpy(mapped.pMappedData, data, static_cast<size_t>(size));
	// Без flush GPU может не увидеть запись.
	return checkResult(vmaFlushAllocation(context.allocator, allocation, 0, size),
					   "vmaFlushAllocation");
}

bool loadShader(const char* path, VkShaderModule& module) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		std::cerr << "Cannot open shader: " << path << '\n';
		return false;
	}
	const std::streamsize byte_count = file.tellg();
	if (byte_count <= 0 || byte_count % sizeof(uint32_t) != 0) {
		std::cerr << "Invalid SPIR-V size: " << path << '\n';
		return false;
	}
	std::vector<uint32_t> code(static_cast<size_t>(byte_count) / sizeof(uint32_t));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(code.data()), byte_count)) {
		std::cerr << "Cannot read shader: " << path << '\n';
		return false;
	}
	const VkShaderModuleCreateInfo shader_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(byte_count),
		.pCode = code.data(),
	};
	return checkResult(vkCreateShaderModule(graphics::internal::context.device,
											 &shader_info, nullptr, &module), "vkCreateShaderModule");
}

bool createPipeline() {
	const auto& context = graphics::internal::context;
	VkShaderModule vertex_shader = VK_NULL_HANDLE;
	VkShaderModule fragment_shader = VK_NULL_HANDLE;
	if (!loadShader("shaders/box.vert.spv", vertex_shader)) {
		return false;
	}
	if (!loadShader("shaders/box.frag.spv", fragment_shader)) {
		vkDestroyShaderModule(context.device, vertex_shader, nullptr);
		return false;
	}
	const VkPipelineShaderStageCreateInfo stages[] = {
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertex_shader, .pName = "main" },
		{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		  .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragment_shader, .pName = "main" },
	};
	const VkVertexInputBindingDescription binding = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	const VkVertexInputAttributeDescription attributes[] = {
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT,
		  .offset = offsetof(Vertex, position) },
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT,
		  .offset = offsetof(Vertex, color) },
	};
	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &binding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = attributes,
	};
	const VkPipelineInputAssemblyStateCreateInfo assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};
	const VkPipelineViewportStateCreateInfo viewport = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};
	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		// Учитываем переворот Y.
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};
	const VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};
	const VkPipelineDepthStencilStateCreateInfo depth = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
	};
	const VkPipelineColorBlendAttachmentState color_attachment = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
						  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	const VkPipelineColorBlendStateCreateInfo blending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_attachment,
	};
	const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	const VkPipelineDynamicStateCreateInfo dynamic = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_states,
	};
	const VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &assembly,
		.pViewportState = &viewport,
		.pRasterizationState = &rasterization,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth,
		.pColorBlendState = &blending,
		.pDynamicState = &dynamic,
		.layout = pipeline_layout,
		.renderPass = context.render_pass,
		.subpass = 0,
	};
	const VkResult result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1,
													 &pipeline_info, nullptr, &pipeline);
	// Модули уже можно освободить.
	vkDestroyShaderModule(context.device, fragment_shader, nullptr);
	vkDestroyShaderModule(context.device, vertex_shader, nullptr);
	return checkResult(result, "vkCreateGraphicsPipelines");
}

} // namespace

bool initialize() {
	const auto& context = graphics::internal::context;
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(context.physical_device, &properties);
	static_assert(sizeof(glm::mat4) == 64);
	if (properties.limits.maxPushConstantsSize < sizeof(glm::mat4)) {
		std::cerr << "The device cannot hold the MVP push constant\n";
		return false;
	}
	if (!createBuffer(vertices.data(), sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					  vertex_buffer, vertex_allocation) ||
		!createBuffer(indices.data(), sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
					  index_buffer, index_allocation)) {
		shutdown();
		return false;
	}
	const VkPushConstantRange push_constant = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(glm::mat4),
	};
	const VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constant,
	};
	if (!checkResult(vkCreatePipelineLayout(context.device, &layout_info, nullptr,
											   &pipeline_layout), "vkCreatePipelineLayout") ||
		!createPipeline()) {
		shutdown();
		return false;
	}
	return true;
}

void shutdown() {
	const auto& context = graphics::internal::context;
	checkResult(vkQueueWaitIdle(context.graphics_queue), "vkQueueWaitIdle");
	vkDestroyPipeline(context.device, pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
	if (index_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, index_buffer, index_allocation);
	}
	if (vertex_buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(context.allocator, vertex_buffer, vertex_allocation);
	}
	pipeline = VK_NULL_HANDLE;
	pipeline_layout = VK_NULL_HANDLE;
	index_buffer = vertex_buffer = VK_NULL_HANDLE;
	index_allocation = vertex_allocation = nullptr;
}

void update([[maybe_unused]] double time) {
	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Lab 01 - Rectangular box");
	ImGui::TextUnformatted("Variant 3: 2.0 x 1.5 x 1.0");
	ImGui::Combo("Projection", &projection_type, "Perspective\0Orthographic\0");
	ImGui::SliderFloat3("Position", &position.x, -2.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SliderFloat3("Rotation (deg)", &rotation_degrees.x, -180.0f, 180.0f,
						"%.0f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SliderFloat3("Scale", &scale.x, 0.2f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::Button("Reset")) {
		position = glm::vec3(0.0f);
		rotation_degrees = glm::vec3(0.0f);
		scale = glm::vec3(1.0f);
		projection_type = 0;
	}
	ImGui::End();
}

void render(const graphics::internal::FrameData& fd) {
	const auto& context = graphics::internal::context;
	const auto extent = context.swapchain_extent;
	const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
	// К вершине матрицы применяются справа налево.
	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	model = glm::rotate(model, glm::radians(rotation_degrees.x), glm::vec3(1, 0, 0));
	model = glm::rotate(model, glm::radians(rotation_degrees.y), glm::vec3(0, 1, 0));
	model = glm::rotate(model, glm::radians(rotation_degrees.z), glm::vec3(0, 0, 1));
	model = glm::scale(model, scale);
	const glm::mat4 view = glm::lookAtRH(glm::vec3(4, 3, 5), glm::vec3(0), glm::vec3(0, 1, 0));

	// ZO — глубина от 0 до 1.
	glm::mat4 projection;
	if (projection_type == 0) {
		projection = glm::perspectiveRH_ZO(glm::radians(45.0f), aspect, 0.1f, 100.0f);
	} else {
		const float half_height = 2.5f;
		projection = glm::orthoRH_ZO(-half_height * aspect, half_height * aspect,
									-half_height, half_height, 0.1f, 100.0f);
	}
	// Переворачиваем Y для Vulkan.
	projection[1][1] *= -1.0f;
	const glm::mat4 mvp = projection * view * model;

	vkResetCommandBuffer(fd.command_buffer, 0);
	const VkCommandBufferBeginInfo begin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(fd.command_buffer, &begin);

	const VkClearValue clear_values[] = {
		{ .color = { .float32 = { 0.08f, 0.08f, 0.10f, 1.0f } } },
		{ .depthStencil = { 1.0f, 0 } },
	};
	const VkRenderPassBeginInfo render_pass = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .extent = context.swapchain_extent },
		.clearValueCount = sizeof(clear_values) / sizeof(clear_values[0]),
		.pClearValues = clear_values,
	};
	vkCmdBeginRenderPass(fd.command_buffer, &render_pass, VK_SUBPASS_CONTENTS_INLINE);
	const VkViewport viewport = {
		.width = static_cast<float>(extent.width),
		.height = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor = { .extent = extent };
	vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);
	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertex_buffer, &offset);
	vkCmdBindIndexBuffer(fd.command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT16);
	vkCmdPushConstants(fd.command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
					   0, sizeof(mvp), &mvp);
	vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
	vkCmdEndRenderPass(fd.command_buffer);
	vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application
