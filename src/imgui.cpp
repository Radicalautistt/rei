#include "imgui.hpp"
#include "window.hpp"
#include "rei_math_types.hpp"

#include <imgui/imgui.h>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::imgui {

void Context::updateBuffers (u32 frameIndex, const ImDrawData* drawData) {
  auto indexBuffer = &indexBuffers[frameIndex];
  auto vertexBuffer = &vertexBuffers[frameIndex];

  VkDeviceSize indexBufferSize = sizeof (ImDrawIdx) * (size_t) drawData->TotalIdxCount;
  VkDeviceSize vertexBufferSize = sizeof (ImDrawVert) * (size_t) drawData->TotalVtxCount;

  if (vertexBuffer->size < vertexBufferSize) {
    vmaUnmapMemory (allocator, vertexBuffer->allocation);
    vmaDestroyBuffer (allocator, vertexBuffer->handle, vertexBuffer->allocation);

    vku::BufferAllocationInfo allocationInfo;
    allocationInfo.size = vertexBufferSize;
    allocationInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    allocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vku::allocateBuffer (allocator, &allocationInfo, vertexBuffer);
    VKC_CHECK (vmaMapMemory (allocator, vertexBuffer->allocation, &vertexBuffer->mapped));

    vmaUnmapMemory (allocator, indexBuffer->allocation);
    vmaDestroyBuffer (allocator, indexBuffer->handle, indexBuffer->allocation);

    allocationInfo.size = indexBufferSize;
    allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    vku::allocateBuffer (allocator, &allocationInfo, indexBuffer);
    VKC_CHECK (vmaMapMemory (allocator, indexBuffer->allocation, &indexBuffer->mapped));
  }

  auto indices = (ImDrawIdx*) indexBuffer->mapped;
  auto vertices = (ImDrawVert*) vertexBuffer->mapped;

  for (i32 index = 0; index < drawData->CmdListsCount; ++index) {
    const ImDrawList* current = drawData->CmdLists[index];

    memcpy (indices, current->IdxBuffer.Data, sizeof (ImDrawIdx) * (size_t) current->IdxBuffer.Size);
    memcpy (vertices, current->VtxBuffer.Data, sizeof (ImDrawVert) * (size_t) current->VtxBuffer.Size);

    indices += current->IdxBuffer.Size;
    vertices += current->VtxBuffer.Size;
  }

  VkDeviceSize offsets[2] {0, 0};
  VkDeviceSize sizes[2] {vertexBufferSize, indexBufferSize};
  VmaAllocation allocations[2] {vertexBuffer->allocation, indexBuffer->allocation};

  VKC_CHECK (vmaFlushAllocations (allocator, 2, allocations, offsets, sizes));
}

void Context::newFrame () {
  ImGuiIO& io = ImGui::GetIO ();
  io.DisplaySize.x = 1680;
  io.DisplaySize.y = 1050;

  io.MouseDown[0] = mouseButtonsDown[0];
  io.MouseDown[1] = mouseButtonsDown[1];

  mouseButtonsDown[0] = mouseButtonsDown[1] = REI_FALSE;

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

void Context::renderDrawData (VkCommandBuffer cmdBuffer, u32 frameIndex, const ImDrawData* drawData) {
  VKC_BIND_DESCRIPTORS (cmdBuffer, pipelineLayout, 1, &descriptorSet);
  vkCmdBindPipeline (cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  math::Vec2 pushConstants {2.f / 1680.f, 2.f / 1050.f};
  vkCmdPushConstants (cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (math::Vec2), &pushConstants);

  i32 vertexOffset = 0;
  u32 indexOffset = 0;
  VkDeviceSize offset = 0;

  vkCmdBindVertexBuffers (cmdBuffer, 0, 1, &vertexBuffers[frameIndex].handle, &offset);
  vkCmdBindIndexBuffer (cmdBuffer, indexBuffers[frameIndex].handle, 0, VK_INDEX_TYPE_UINT16);

  for (i32 list = 0; list < drawData->CmdListsCount; ++list) {
    const ImDrawList* commandList = drawData->CmdLists[list];

    for (i32 command = 0; command < commandList->CmdBuffer.Size; ++command) {
      const ImDrawCmd* drawCommand = &commandList->CmdBuffer[command];

      VkRect2D scissor;
      scissor.offset.x = REI_MAX ((i32) drawCommand->ClipRect.x, 0);
      scissor.offset.y = REI_MAX ((i32) drawCommand->ClipRect.y, 0);
      scissor.extent.width = (u32) (drawCommand->ClipRect.z - drawCommand->ClipRect.x);
      scissor.extent.height = (u32) (drawCommand->ClipRect.w - drawCommand->ClipRect.y);

      vkCmdSetScissor (cmdBuffer, 0, 1, &scissor);

      vkCmdDrawIndexed (
        cmdBuffer,
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

void create (VkDevice device, VmaAllocator allocator, const ContextCreateInfo* createInfo, Context* output) {
  output->allocator = allocator;
  output->handle = ImGui::CreateContext ();
  output->window = createInfo->window;

  // Create dummy vertex and index buffers for each frame.
  for (u8 index = 0; index < REI_FRAMES_COUNT; ++index) {
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
    VKC_CHECK (vmaMapMemory (output->allocator, vertexBuffer->allocation, &vertexBuffer->mapped));

    allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    vku::allocateBuffer (output->allocator, &allocationInfo, indexBuffer);
    VKC_CHECK (vmaMapMemory (output->allocator, indexBuffer->allocation, &indexBuffer->mapped));
  }

  { // Create font texture
    i32 width, height;
    u8* pixels;
    ImGuiIO& io = ImGui::GetIO ();
    io.Fonts->GetTexDataAsRGBA32 (&pixels, &width, &height);

    VkExtent3D extent {(u32) width, (u32) height, 1};
    VkDeviceSize size = (VkDeviceSize) (extent.width * extent.height * 4);

    vku::Buffer stagingBuffer;
    vku::allocateStagingBuffer (allocator, size, &stagingBuffer);

    VKC_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, pixels, size);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      VkImageCreateInfo info;
      info.pNext = nullptr;
      info.flags = VKC_NO_FLAGS;
      info.sType = IMAGE_CREATE_INFO;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices = nullptr;

      info.extent = extent;
      info.mipLevels = 1;
      info.arrayLayers = 1;
      info.imageType = VK_IMAGE_TYPE_2D;
      info.format = VKC_TEXTURE_FORMAT;
      info.samples = VK_SAMPLE_COUNT_1_BIT;
      info.tiling = VK_IMAGE_TILING_OPTIMAL;
      info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      VmaAllocationCreateInfo vmaAllocationInfo {};
      vmaAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      vmaAllocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      VKC_CHECK (vmaCreateImage (
        allocator,
        &info,
        &vmaAllocationInfo,
        &output->fontTexture.handle,
        &output->fontTexture.allocation,
        nullptr
      ));
    }

    VkImageSubresourceRange subresourceRange;
    subresourceRange.layerCount = 1;
    subresourceRange.levelCount = 1;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkCommandBuffer cmdBuffer;
    vku::startImmediateCmd (device, createInfo->transferContext, &cmdBuffer);

    {
      vku::ImageLayoutTransitionInfo transitionInfo;
      transitionInfo.subresourceRange = &subresourceRange;
      transitionInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      transitionInfo.source = VK_PIPELINE_STAGE_TRANSFER_BIT;
      transitionInfo.destination = VK_PIPELINE_STAGE_TRANSFER_BIT;
      transitionInfo.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

      vku::transitionImageLayout (cmdBuffer, &transitionInfo, output->fontTexture.handle);
    }

    {
      VkBufferImageCopy copyRegion;
      copyRegion.imageOffset.x = 0;
      copyRegion.imageOffset.y = 0;
      copyRegion.imageOffset.z = 0;

      copyRegion.bufferOffset = 0;
      copyRegion.bufferRowLength = 0;
      copyRegion.imageExtent = extent;
      copyRegion.bufferImageHeight = 0;

      copyRegion.imageSubresource.mipLevel = 0;
      copyRegion.imageSubresource.layerCount = 1;
      copyRegion.imageSubresource.baseArrayLayer = 0;
      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

      vkCmdCopyBufferToImage (
        cmdBuffer,
        stagingBuffer.handle,
        output->fontTexture.handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion
      );
    }

    {
      vku::ImageLayoutTransitionInfo transitionInfo;
      transitionInfo.subresourceRange = &subresourceRange;
      transitionInfo.source = VK_PIPELINE_STAGE_TRANSFER_BIT;
      transitionInfo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      transitionInfo.destination = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      transitionInfo.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      vku::transitionImageLayout (cmdBuffer, &transitionInfo, output->fontTexture.handle);
    }

    vku::submitImmediateCmd (device, createInfo->transferContext, cmdBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);

    VkImageViewCreateInfo info;
    info.pNext = nullptr;
    info.flags = VKC_NO_FLAGS;
    info.sType = IMAGE_VIEW_CREATE_INFO;
    info.format = VKC_TEXTURE_FORMAT;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.image = output->fontTexture.handle;
    info.subresourceRange = subresourceRange;
    info.components.r = VK_COMPONENT_SWIZZLE_R;
    info.components.g = VK_COMPONENT_SWIZZLE_G;
    info.components.b = VK_COMPONENT_SWIZZLE_B;
    info.components.a = VK_COMPONENT_SWIZZLE_A;

    VKC_CHECK (vkCreateImageView (device, &info, nullptr, &output->fontTexture.view));

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

    VKC_CHECK (vkCreateSampler (device, &samplerInfo, nullptr, &output->fontSampler));
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

    VKC_CHECK (vkCreateDescriptorSetLayout (device, &layoutInfo, nullptr, &output->descriptorSetLayout));
  }

  { // Allocate descriptor set for font sampler
    VkDescriptorSetAllocateInfo allocationInfo {DESCRIPTOR_SET_ALLOCATE_INFO};
    allocationInfo.descriptorSetCount = 1;
    allocationInfo.pSetLayouts = &output->descriptorSetLayout;
    allocationInfo.descriptorPool = createInfo->descriptorPool;

    VKC_CHECK (vkAllocateDescriptorSets (device, &allocationInfo, &output->descriptorSet));
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

    vkUpdateDescriptorSets (device, 1, &write, 0, nullptr);
  }

  {
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof (math::Vec2);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo createInfo {PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;
    createInfo.pSetLayouts = &output->descriptorSetLayout;

    VKC_CHECK (vkCreatePipelineLayout (device, &createInfo, nullptr, &output->pipelineLayout));
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
    vertexAttributes[0].offset = REI_OFFSET_OF (ImDrawVert, pos);

    vertexAttributes[1].binding = 0;
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[1].offset = REI_OFFSET_OF (ImDrawVert, uv);

    vertexAttributes[2].binding = 0;
    vertexAttributes[2].location = 2;
    vertexAttributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertexAttributes[2].offset = REI_OFFSET_OF (ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertexInputState {PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &binding;
    vertexInputState.pVertexAttributeDescriptions = vertexAttributes;
    vertexInputState.vertexAttributeDescriptionCount = REI_ARRAY_SIZE (vertexAttributes);

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
    info.colorBlendAttachmentCount = 1;
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

    vku::createGraphicsPipeline (device, &info, &output->pipeline);
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

void destroy (VkDevice device, Context* context) {
  for (u8 index = 0; index < REI_FRAMES_COUNT; ++index) {
    vmaUnmapMemory (context->allocator, context->indexBuffers[index].allocation);
    vmaDestroyBuffer (context->allocator, context->indexBuffers[index].handle, context->indexBuffers[index].allocation);

    vmaUnmapMemory (context->allocator, context->vertexBuffers[index].allocation);
    vmaDestroyBuffer (context->allocator, context->vertexBuffers[index].handle, context->vertexBuffers[index].allocation);
  }

  vkDestroyPipelineLayout (device, context->pipelineLayout, nullptr);
  vkDestroyPipeline (device, context->pipeline, nullptr);
  vkDestroyDescriptorSetLayout (device, context->descriptorSetLayout, nullptr);
  vkDestroySampler (device, context->fontSampler, nullptr);
  vkDestroyImageView (device, context->fontTexture.view, nullptr);
  vmaDestroyImage (context->allocator, context->fontTexture.handle, context->fontTexture.allocation);

  ImGuiIO& io = ImGui::GetIO ();
  io.BackendPlatformName = nullptr;
  io.BackendRendererName = nullptr;
  ImGui::DestroyContext (context->handle);
}

void showDebugWindow (f32* cameraSpeed, u32* gbufferOutput, VmaAllocator allocator) {
  const ImGuiIO& io = ImGui::GetIO ();
  ImGui::Begin ("REI debug menu");
  ImGui::SetWindowPos ({0.f, 0.f});
  ImGui::SetWindowSize ({320, 270});

  static size_t usedBytes;
  static size_t freeBytes;
  static u32 frameIndex;
  static u32 allocationCount;

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

  ImGui::Text ("View gbuffer contents:");
  if (ImGui::Button ("All")) *gbufferOutput = 0;
  ImGui::SameLine ();
  if (ImGui::Button ("Albedo")) *gbufferOutput = 1;
  ImGui::SameLine ();
  if (ImGui::Button ("Normal")) *gbufferOutput = 2;
  ImGui::SameLine ();
  if (ImGui::Button ("Position")) *gbufferOutput = 3;

  ImGui::End ();
}

}
