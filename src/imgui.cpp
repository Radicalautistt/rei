#include "math.hpp"
#include "imgui.hpp"
#include "common.hpp"
#include "window.hpp"

#include <imgui/imgui.h>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::extra::imgui {

static bool mouseButtonsDown[2];

void Context::updateBuffers (const ImDrawData* drawData) {
  if (drawData->TotalVtxCount) {
    counts.index = drawData->TotalIdxCount;
    counts.vertex = drawData->TotalVtxCount;

    VkDeviceSize indexBufferSize = sizeof (ImDrawIdx) * counts.index;
    VkDeviceSize vertexBufferSize = sizeof (ImDrawVert) * counts.vertex;

    if (vertexBuffer.size < vertexBufferSize) {
      if (vertexBuffer.allocation) {
        vmaUnmapMemory (allocator, vertexBuffer.allocation);
        vmaDestroyBuffer (allocator, vertexBuffer.handle, vertexBuffer.allocation);
      }

      vkutils::BufferAllocationInfo allocationInfo;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
      allocationInfo.size = sizeof (ImDrawVert) * counts.vertex;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      allocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

      vkutils::allocateBuffer (allocator, allocationInfo, vertexBuffer);
      VK_CHECK (vmaMapMemory (allocator, vertexBuffer.allocation, &vertexBuffer.mapped));
    }

    if (indexBuffer.size < indexBufferSize) {
      if (indexBuffer.allocation) {
        vmaUnmapMemory (allocator, indexBuffer.allocation);
        vmaDestroyBuffer (allocator, indexBuffer.handle, indexBuffer.allocation);
      }

      vkutils::BufferAllocationInfo allocationInfo;
      allocationInfo.size = sizeof (ImDrawIdx) * counts.index;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      allocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

      vkutils::allocateBuffer (allocator, allocationInfo, indexBuffer);
      VK_CHECK (vmaMapMemory (allocator, indexBuffer.allocation, &indexBuffer.mapped));
    }

    auto indices = RCAST <ImDrawIdx*> (indexBuffer.mapped);
    auto vertices = RCAST <ImDrawVert*> (vertexBuffer.mapped);

    for (int index = 0; index < drawData->CmdListsCount; ++index) {
      const ImDrawList* current = drawData->CmdLists[index];

      memcpy (indices, current->IdxBuffer.Data, sizeof (ImDrawIdx) * current->IdxBuffer.Size);
      memcpy (vertices, current->VtxBuffer.Data, sizeof (ImDrawVert) * current->VtxBuffer.Size);

      indices += current->IdxBuffer.Size;
      vertices += current->VtxBuffer.Size;
    }

    VkDeviceSize offsets[2] {0, 0};
    VkDeviceSize sizes[2] {vertexBufferSize, indexBufferSize};
    VmaAllocation allocations[2] {vertexBuffer.allocation, indexBuffer.allocation};

    VK_CHECK (vmaFlushAllocations (allocator, 2, allocations, offsets, sizes));
  }
}

void Context::newFrame () {
  ImGuiIO& io = ImGui::GetIO ();
  io.DisplaySize.x = 1680;
  io.DisplaySize.y = 1050;

  io.MouseDown[0] = mouseButtonsDown[0];
  io.MouseDown[1] = mouseButtonsDown[1];

  mouseButtonsDown[0] = mouseButtonsDown[1] = false;

  ImGui::NewFrame ();
}

void Context::handleEvents (const xcb_generic_event_t* event) {
  ImGuiIO& io = ImGui::GetIO ();
  window->getMousePosition (&io.MousePos.x);

  switch (event->response_type & ~0x80) {
    case XCB_BUTTON_PRESS: {
      const auto button = RCAST <const xcb_button_press_event_t*> (event);
      if (button->detail == 1) mouseButtonsDown[0] = true;
      if (button->detail == 3) mouseButtonsDown[1] = true;
    } break;
    default: break;
  }
}

void Context::renderDrawData (const ImDrawData* drawData, VkCommandBuffer commandBuffer) {
  vkCmdBindDescriptorSets (
    commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0,
    1, &descriptorSet,
    0, nullptr
  );

  vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  {
    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = 1680;
    viewport.height = 1050;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport (commandBuffer, 0, 1, &viewport);

    math::Vector2 pushConstants[2];
    pushConstants[1] = math::Vector2 {-1.f};
    pushConstants[0] = math::Vector2 {2.f / 1680.f, 2.f / 1050.f};

    vkCmdPushConstants (
      commandBuffer,
      pipelineLayout,
      VK_SHADER_STAGE_VERTEX_BIT,
      0,
      sizeof (pushConstants),
      pushConstants
    );
  }

  if (drawData->CmdListsCount) {
    VkDeviceSize offset = 0;
    int32_t vertexOffset = 0;
    uint32_t indexOffset = 0;

    vkCmdBindVertexBuffers (commandBuffer, 0, 1, &vertexBuffer.handle, &offset);
    vkCmdBindIndexBuffer (commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT16);

    for (int32_t list = 0; list < drawData->CmdListsCount; ++list) {
      const ImDrawList* commandList = drawData->CmdLists[list];

      for (int32_t command = 0; command < commandList->CmdBuffer.Size; ++command) {
	const ImDrawCmd* drawCommand = &commandList->CmdBuffer[command];

	VkRect2D scissor;
	scissor.offset.x = MAX (SCAST <int32_t> (drawCommand->ClipRect.x), 0);
	scissor.offset.y = MAX (SCAST <int32_t> (drawCommand->ClipRect.y), 0);
	scissor.extent.width = SCAST <uint32_t> (drawCommand->ClipRect.z - drawCommand->ClipRect.x);
	scissor.extent.height = SCAST <uint32_t> (drawCommand->ClipRect.w - drawCommand->ClipRect.y);

	vkCmdSetScissor (commandBuffer, 0, 1, &scissor);

	vkCmdDrawIndexed (
	  commandBuffer,
	  drawCommand->ElemCount,
	  1,
	  drawCommand->IdxOffset + indexOffset,
	  drawCommand->VtxOffset + vertexOffset,
          0
	);
      }

      indexOffset += commandList->IdxBuffer.Size;
      vertexOffset += commandList->VtxBuffer.Size;
    }
  }
}

void create (const ContextCreateInfo& createInfo, Context& output) {
  output.handle = ImGui::CreateContext ();
  output.window = createInfo.window;
  output.device = createInfo.device;
  output.allocator = createInfo.allocator;
  output.transferContext = createInfo.transferContext;

  output.indexBuffer.size = 0;
  output.vertexBuffer.size = 0;
  output.indexBuffer.handle = VK_NULL_HANDLE;
  output.vertexBuffer.handle = VK_NULL_HANDLE;
  output.indexBuffer.allocation = VK_NULL_HANDLE;
  output.vertexBuffer.allocation = VK_NULL_HANDLE;

  { // Create font texture
    int width, height;
    unsigned char* pixels;
    ImGuiIO& io = ImGui::GetIO ();
    io.Fonts->GetTexDataAsRGBA32 (&pixels, &width, &height);

    vkutils::TextureAllocationInfo allocationInfo;
    allocationInfo.compressed = false;
    allocationInfo.compressedSize = 0;
    allocationInfo.generateMipmaps = false;
    allocationInfo.width = SCAST <uint32_t> (width);
    allocationInfo.height = SCAST <uint32_t> (height);
    allocationInfo.pixels = RCAST <const char*> (pixels);

    vkutils::allocateTexture (
      output.device,
      output.allocator,
      allocationInfo,
      *output.transferContext,
      output.fontTexture
    );

    ImTextureID fontID = RCAST <ImTextureID>
      (RCAST <intptr_t> (output.fontTexture.handle));

    io.Fonts->SetTexID (fontID);
  }

  { // Create font sampler
    VkSamplerCreateInfo samplerInfo {SAMPLER_CREATE_INFO};
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VK_CHECK (vkCreateSampler (output.device, &samplerInfo, nullptr, &output.fontSampler));
  }

  { // Create descriptor set layout for font sampler
    VkDescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    VkDescriptorSetLayoutCreateInfo layoutInfo {DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    VK_CHECK (vkCreateDescriptorSetLayout (output.device, &layoutInfo, nullptr, &output.descriptorSetLayout));
  }

  { // Allocate descriptor set for font sampler
    VkDescriptorSetAllocateInfo allocationInfo {DESCRIPTOR_SET_ALLOCATE_INFO};
    allocationInfo.descriptorSetCount = 1;
    allocationInfo.pSetLayouts = &output.descriptorSetLayout;
    allocationInfo.descriptorPool = createInfo.descriptorPool;

    VK_CHECK (vkAllocateDescriptorSets (output.device, &allocationInfo, &output.descriptorSet));
  }

  {
    VkDescriptorImageInfo fontImageInfo;
    fontImageInfo.sampler = output.fontSampler;
    fontImageInfo.imageView = output.fontTexture.view;
    fontImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write {WRITE_DESCRIPTOR_SET};
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &fontImageInfo;
    write.dstSet = output.descriptorSet;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    vkUpdateDescriptorSets (output.device, 1, &write, 0, nullptr);
  }

  {
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof (math::Vector2) * 2;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo createInfo {PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;
    createInfo.pSetLayouts = &output.descriptorSetLayout;

    VK_CHECK (vkCreatePipelineLayout (output.device, &createInfo, nullptr, &output.pipelineLayout));
  }

  {
    vkutils::GraphicsPipelineCreateInfo createInfos[1];

    VkVertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof (ImDrawVert);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3];

    attributes[0].location = 0;
    attributes[0].binding = binding.binding;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = offsetof (ImDrawVert, pos);

    attributes[1].location = 1;
    attributes[1].binding = binding.binding;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = offsetof (ImDrawVert, uv);

    attributes[2].location = 2;
    attributes[2].binding = binding.binding;
    attributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attributes[2].offset = offsetof (ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo {PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &binding;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;
    vertexInputInfo.vertexAttributeDescriptionCount = ARRAY_SIZE (attributes);

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo {PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportInfo {PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportInfo.scissorCount = 1;
    viewportInfo.viewportCount = 1;

    VkDynamicState dynamicStates[2] {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo {PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicInfo.pDynamicStates = dynamicStates;
    dynamicInfo.dynamicStateCount = ARRAY_SIZE (dynamicStates);

    VkPipelineRasterizationStateCreateInfo rasterizationInfo {PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationInfo.lineWidth = 1.f;
    rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampleInfo {PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleInfo.minSampleShading = 1.f;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo {PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilInfo.minDepthBounds = 0.f;
    depthStencilInfo.maxDepthBounds = 1.f;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState blendAttachmentInfo;
    blendAttachmentInfo.blendEnable = VK_TRUE;
    blendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    blendAttachmentInfo.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    blendAttachmentInfo.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    blendAttachmentInfo.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    blendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo {PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &blendAttachmentInfo;

    createInfos[0].layout = output.pipelineLayout;
    createInfos[0].renderPass = createInfo.renderPass;
    createInfos[0].pixelShaderPath = "assets/shaders/imgui.frag.spv";
    createInfos[0].vertexShaderPath = "assets/shaders/imgui.vert.spv";

    createInfos[0].vertexInputInfo = &vertexInputInfo;
    createInfos[0].inputAssemblyInfo = &inputAssemblyInfo;
    createInfos[0].viewportInfo = &viewportInfo;
    createInfos[0].dynamicInfo = &dynamicInfo;
    createInfos[0].rasterizationInfo = &rasterizationInfo;
    createInfos[0].multisampleInfo = &multisampleInfo;
    createInfos[0].colorBlendInfo = &colorBlendInfo;
    createInfos[0].depthStencilInfo = &depthStencilInfo;

    rei::vkutils::createGraphicsPipelines (
      output.device,
      VK_NULL_HANDLE,
      ARRAY_SIZE (createInfos),
      createInfos,
      &output.pipeline
    );
  }

  ImGuiIO& io = ImGui::GetIO ();
  io.BackendRendererName = "Rei";
  io.BackendPlatformName = "Xcb";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

  // Set color theme
  ImGui::StyleColorsClassic ();
  auto& style = ImGui::GetStyle ();

  style.WindowRounding = 0.f;
  style.ScrollbarRounding = 0.f;
  style.Colors[ImGuiCol_WindowBg].w = 1.f;
  style.Colors[ImGuiCol_TitleBg] = {0.f, 0.f, 0.f, 1.f};
  style.Colors[ImGuiCol_ScrollbarBg] = {0.f, 0.f, 0.f, 1.f};
  style.Colors[ImGuiCol_TitleBgActive] = {0.f, 0.f, 0.f, 1.f};
}

void destroy (VkDevice device, VmaAllocator allocator, Context& context) {
  if (context.indexBuffer.allocation) {
    vmaUnmapMemory (allocator, context.indexBuffer.allocation);
    vmaDestroyBuffer (allocator, context.indexBuffer.handle, context.indexBuffer.allocation);
  }

  if (context.vertexBuffer.allocation) {
    vmaUnmapMemory (allocator, context.vertexBuffer.allocation);
    vmaDestroyBuffer (allocator, context.vertexBuffer.handle, context.vertexBuffer.allocation);
  }

  vkDestroyPipelineLayout (device, context.pipelineLayout, nullptr);
  vkDestroyPipeline (device, context.pipeline, nullptr);
  vkDestroyDescriptorSetLayout (device, context.descriptorSetLayout, nullptr);
  vkDestroySampler (device, context.fontSampler, nullptr);
  vkDestroyImageView (device, context.fontTexture.view, nullptr);
  vmaDestroyImage (allocator, context.fontTexture.handle, context.fontTexture.allocation);

  ImGuiIO& io = ImGui::GetIO ();
  io.BackendPlatformName = nullptr;
  io.BackendRendererName = nullptr;
  ImGui::DestroyContext (context.handle);
}

}
