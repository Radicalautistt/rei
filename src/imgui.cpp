#include "math.hpp"
#include "imgui.hpp"
#include "window.hpp"

#include <imgui/imgui.h>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::imgui {

static Bool32 mouseButtonsDown[2];

void Context::updateBuffers (Uint32 frameIndex, const ImDrawData* drawData) {
  counts.index = drawData->TotalIdxCount;
  counts.vertex = drawData->TotalVtxCount;

  VkDeviceSize indexBufferSize = sizeof (ImDrawIdx) * counts.index;
  VkDeviceSize vertexBufferSize = sizeof (ImDrawVert) * counts.vertex;

  auto vertexBuffer = &vertexBuffers[frameIndex];

  if (vertexBuffer->size < vertexBufferSize) {
    vmaUnmapMemory (allocator, vertexBuffer->allocation);
    vmaDestroyBuffer (allocator, vertexBuffer->handle, vertexBuffer->allocation);

    vku::BufferAllocationInfo allocationInfo;
    allocationInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocationInfo.size = sizeof (ImDrawVert) * counts.vertex;
    allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    allocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vku::allocateBuffer (allocator, &allocationInfo, vertexBuffer);
    VK_CHECK (vmaMapMemory (allocator, vertexBuffer->allocation, &vertexBuffer->mapped));
  }

  auto indexBuffer = &indexBuffers[frameIndex];

  if (indexBuffer->size < indexBufferSize) {
    vmaUnmapMemory (allocator, indexBuffer->allocation);
    vmaDestroyBuffer (allocator, indexBuffer->handle, indexBuffer->allocation);

    vku::BufferAllocationInfo allocationInfo;
    allocationInfo.size = sizeof (ImDrawIdx) * counts.index;
    allocationInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    allocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vku::allocateBuffer (allocator, &allocationInfo, indexBuffer);
    VK_CHECK (vmaMapMemory (allocator, indexBuffer->allocation, &indexBuffer->mapped));
  }

  auto indices = (ImDrawIdx*) indexBuffer->mapped;
  auto vertices = (ImDrawVert*) vertexBuffer->mapped;

  for (Int32 index = 0; index < drawData->CmdListsCount; ++index) {
    const ImDrawList* current = drawData->CmdLists[index];

    memcpy (indices, current->IdxBuffer.Data, sizeof (ImDrawIdx) * current->IdxBuffer.Size);
    memcpy (vertices, current->VtxBuffer.Data, sizeof (ImDrawVert) * current->VtxBuffer.Size);

    indices += current->IdxBuffer.Size;
    vertices += current->VtxBuffer.Size;
  }

  VkDeviceSize offsets[2] {0, 0};
  VkDeviceSize sizes[2] {vertexBufferSize, indexBufferSize};
  VmaAllocation allocations[2] {vertexBuffer->allocation, indexBuffer->allocation};

  VK_CHECK (vmaFlushAllocations (allocator, 2, allocations, offsets, sizes));
}

void Context::newFrame () {
  ImGuiIO& io = ImGui::GetIO ();
  io.DisplaySize.x = 1680;
  io.DisplaySize.y = 1050;

  io.MouseDown[0] = mouseButtonsDown[0];
  io.MouseDown[1] = mouseButtonsDown[1];

  mouseButtonsDown[0] = mouseButtonsDown[1] = False;

  ImGui::NewFrame ();
}

void Context::handleEvents (const xcb_generic_event_t* event) {
  ImGuiIO& io = ImGui::GetIO ();
  window->getMousePosition (&io.MousePos.x);

  switch (event->response_type & ~0x80) {
    case XCB_BUTTON_PRESS: {
      const auto button = (const xcb_button_press_event_t*) event;
      mouseButtonsDown[0] = button->detail == MOUSE_LEFT;
      mouseButtonsDown[1] = button->detail == MOUSE_RIGHT;
    } break;
    default: break;
  }
}

void Context::renderDrawData (VkCommandBuffer commandBuffer, Uint32 frameIndex, const ImDrawData* drawData) {
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

  VkDeviceSize offset = 0;
  Int32 vertexOffset = 0;
  Uint32 indexOffset = 0;

  vkCmdBindVertexBuffers (commandBuffer, 0, 1, &vertexBuffers[frameIndex].handle, &offset);
  vkCmdBindIndexBuffer (commandBuffer, indexBuffers[frameIndex].handle, 0, VK_INDEX_TYPE_UINT16);

  for (Int32 list = 0; list < drawData->CmdListsCount; ++list) {
    const ImDrawList* commandList = drawData->CmdLists[list];

    for (Int32 command = 0; command < commandList->CmdBuffer.Size; ++command) {
      const ImDrawCmd* drawCommand = &commandList->CmdBuffer[command];

      VkRect2D scissor;
      scissor.offset.x = MAX ((Int32) drawCommand->ClipRect.x, 0);
      scissor.offset.y = MAX ((Int32) drawCommand->ClipRect.y, 0);
      scissor.extent.width = (Uint32) (drawCommand->ClipRect.z - drawCommand->ClipRect.x);
      scissor.extent.height = (Uint32) (drawCommand->ClipRect.w - drawCommand->ClipRect.y);

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

void create (const ContextCreateInfo* createInfo, Context* output) {
  output->handle = ImGui::CreateContext ();
  output->window = createInfo->window;
  output->device = createInfo->device;
  output->allocator = createInfo->allocator;
  output->transferContext = createInfo->transferContext;

  // Create dummy vertex and index buffers for each frame.
  for (Uint8 index = 0; index < FRAMES_COUNT; ++index) {
    auto indexBuffer = &output->indexBuffers[index];
    auto vertexBuffer = &output->vertexBuffers[index];

    vku::BufferAllocationInfo allocationInfo;
    allocationInfo.size = 1;
    allocationInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    allocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vku::allocateBuffer (output->allocator, &allocationInfo, vertexBuffer);
    VK_CHECK (vmaMapMemory (output->allocator, vertexBuffer->allocation, &vertexBuffer->mapped));

    allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    vku::allocateBuffer (output->allocator, &allocationInfo, indexBuffer);
    VK_CHECK (vmaMapMemory (output->allocator, indexBuffer->allocation, &indexBuffer->mapped));
  }

  { // Create font texture
    Int32 width, height;
    Uint8* pixels;
    ImGuiIO& io = ImGui::GetIO ();
    io.Fonts->GetTexDataAsRGBA32 (&pixels, &width, &height);

    vku::TextureAllocationInfo allocationInfo;
    allocationInfo.mipLevels = 1;
    allocationInfo.compressed = False;
    allocationInfo.compressedSize = 0;
    allocationInfo.width = (Uint32) width;
    allocationInfo.pixels = (char*) pixels;
    allocationInfo.height = (Uint32) height;

    vku::allocateTexture (
      output->device,
      output->allocator,
      &allocationInfo,
      output->transferContext,
      &output->fontTexture
    );

    io.Fonts->ClearTexData ();
    io.Fonts->SetTexID ((ImTextureID) ((intptr_t) output->fontTexture.handle));
  }

  { // Create font sampler
    VkSamplerCreateInfo samplerInfo {SAMPLER_CREATE_INFO};
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VK_CHECK (vkCreateSampler (output->device, &samplerInfo, nullptr, &output->fontSampler));
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

    VK_CHECK (vkCreateDescriptorSetLayout (output->device, &layoutInfo, nullptr, &output->descriptorSetLayout));
  }

  { // Allocate descriptor set for font sampler
    VkDescriptorSetAllocateInfo allocationInfo {DESCRIPTOR_SET_ALLOCATE_INFO};
    allocationInfo.descriptorSetCount = 1;
    allocationInfo.pSetLayouts = &output->descriptorSetLayout;
    allocationInfo.descriptorPool = createInfo->descriptorPool;

    VK_CHECK (vkAllocateDescriptorSets (output->device, &allocationInfo, &output->descriptorSet));
  }

  {
    VkDescriptorImageInfo fontImageInfo;
    fontImageInfo.sampler = output->fontSampler;
    fontImageInfo.imageView = output->fontTexture.view;
    fontImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write {WRITE_DESCRIPTOR_SET};
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &fontImageInfo;
    write.dstSet = output->descriptorSet;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    vkUpdateDescriptorSets (output->device, 1, &write, 0, nullptr);
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
    createInfo.pSetLayouts = &output->descriptorSetLayout;

    VK_CHECK (vkCreatePipelineLayout (output->device, &createInfo, nullptr, &output->pipelineLayout));
  }

  {
    VkVertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof (ImDrawVert);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexAttributes[3];

    vertexAttributes[0].binding = 0;
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[0].offset = offsetof (ImDrawVert, pos);

    vertexAttributes[1].binding = 0;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[1].offset = offsetof (ImDrawVert, uv);

    vertexAttributes[2].binding = 0;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertexAttributes[2].offset = offsetof (ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertexInputState {PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &binding;
    vertexInputState.pVertexAttributeDescriptions = vertexAttributes;
    vertexInputState.vertexAttributeDescriptionCount = ARRAY_SIZE (vertexAttributes);

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = 1680;
    viewport.height = 1050;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    VkPipelineViewportStateCreateInfo viewportState {PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.scissorCount = 1;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;

    VkDynamicState dynamicStates[1] {VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState {PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 1;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRasterizationStateCreateInfo rasterizationState {PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationState.lineWidth = 1.f;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineDepthStencilStateCreateInfo depthStencilState {PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilState.minDepthBounds = 0.f;
    depthStencilState.maxDepthBounds = 1.f;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    colorBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    vku::GraphicsPipelineCreateInfo info;
    info.layout = output->pipelineLayout;
    info.cache = createInfo->pipelineCache;
    info.renderPass = createInfo->renderPass;
    info.pixelShaderPath = "assets/shaders/imgui.frag.spv";
    info.vertexShaderPath = "assets/shaders/imgui.vert.spv";

    info.dynamicState = &dynamicState;
    info.viewportState = &viewportState;
    info.vertexInputState = &vertexInputState;
    info.depthStencilState = &depthStencilState;
    info.rasterizationState = &rasterizationState;
    info.colorBlendAttachment = &colorBlendAttachment;

    vku::createGraphicsPipeline (output->device, &info, &output->pipeline);
  }

  {
    VkFenceCreateInfo createInfo {FENCE_CREATE_INFO};
    createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK (vkCreateFence (output->device, &createInfo, nullptr, &output->bufferUpdateFence));
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

void destroy (Context* context) {
  vkDestroyFence (context->device, context->bufferUpdateFence, nullptr);

  for (Uint8 index = 0; index < FRAMES_COUNT; ++index) {
    vmaUnmapMemory (context->allocator, context->indexBuffers[index].allocation);
    vmaDestroyBuffer (context->allocator, context->indexBuffers[index].handle, context->indexBuffers[index].allocation);

    vmaUnmapMemory (context->allocator, context->vertexBuffers[index].allocation);
    vmaDestroyBuffer (context->allocator, context->vertexBuffers[index].handle, context->vertexBuffers[index].allocation);
  }

  vkDestroyPipelineLayout (context->device, context->pipelineLayout, nullptr);
  vkDestroyPipeline (context->device, context->pipeline, nullptr);
  vkDestroyDescriptorSetLayout (context->device, context->descriptorSetLayout, nullptr);
  vkDestroySampler (context->device, context->fontSampler, nullptr);
  vkDestroyImageView (context->device, context->fontTexture.view, nullptr);
  vmaDestroyImage (context->allocator, context->fontTexture.handle, context->fontTexture.allocation);

  ImGuiIO& io = ImGui::GetIO ();
  io.BackendPlatformName = nullptr;
  io.BackendRendererName = nullptr;
  ImGui::DestroyContext (context->handle);
}

void showDebugWindow (Float32* cameraSpeed, VmaAllocator allocator) {
  const ImGuiIO& io = ImGui::GetIO ();
  ImGui::Begin ("REI Debug window");
  ImGui::SetWindowPos ({0.f, 0.f});
  ImGui::SetWindowSize ({320, 250});

  static size_t usedBytes;
  static size_t freeBytes;
  static Uint32 frameIndex;
  static Uint32 allocationCount;

  ImGui::Text ("Frames rendered: %u\nAverage frame time: %.3f ms (%.1f FPS)", frameIndex++, 1000.f / io.Framerate, io.Framerate);
  ImGui::Separator ();

  ImGui::Text ("IMGUI data: %d (Vertices) %d (Indices)", io.MetricsRenderVertices, io.MetricsRenderIndices);
  ImGui::Separator ();

  ImGui::Text ("Vulkan memory allocator stats:");
  ImGui::SameLine ();
  if (ImGui::Button ("Update")) {
    VmaStats stats;
    vmaCalculateStats (allocator, &stats);
    usedBytes = stats.total.usedBytes;
    freeBytes = stats.total.unusedBytes;
    allocationCount = stats.total.allocationCount;
  }

  ImGui::Indent (15.f);
  ImGui::Text ("Number of allocations: %d\nUsed bytes: %zu\nFree bytes: %zu", allocationCount, usedBytes, freeBytes);
  ImGui::Unindent (15.f);
  ImGui::Separator ();

  ImGui::Text ("Window backend: %s", io.BackendPlatformName);
  ImGui::Separator ();

  static bool demoWindow = false;
  ImGui::Checkbox ("Show IMGUI demo window", &demoWindow);
  if (demoWindow) ImGui::ShowDemoWindow ();
  ImGui::Separator ();

  ImGui::Text ("Camera speed: %.1f", *cameraSpeed);
  ImGui::SameLine ();
  if (ImGui::Button ("-")) *cameraSpeed -= 20.f;
  ImGui::SameLine ();
  if (ImGui::Button ("+")) *cameraSpeed += 20.f;

  ImGui::End ();
}

}
