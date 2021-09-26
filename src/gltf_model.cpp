#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "gltf.hpp"
#include "common.hpp"
#include "gltf_model.hpp"

#include <stb/stb_image.h>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::gltf {

void loadModel (
  VkDevice device,
  VmaAllocator allocator,
  const vkutils::TransferContext& transferContext,
  const char* relativePath,
  Model& output) {

  assets::gltf::Data gltf;
  assets::gltf::load (relativePath, gltf);

  uint32_t vertexCount = 0, indexCount = 0;

  for (size_t index = 0; index < gltf.mesh.primitivesCount; ++index) {
    const auto& current = gltf.mesh.primitives[index];
    indexCount += gltf.accessors[current.indices].count;
    vertexCount += gltf.accessors[current.attributes.position].count;
  }

  output.vertexCount = vertexCount;
  output.indexCount = indexCount;
  output.primitivesCount = gltf.mesh.primitivesCount;

  uint32_t vertexOffset = 0, indexOffset = 0;

  auto vertices = MALLOC (Vertex, vertexCount);
  auto indices = MALLOC (uint32_t, indexCount);
  output.primitives = MALLOC (Primitive, output.primitivesCount);

  #define GET_ACCESSOR(attribute, result) do {                                                 \
    const auto& accessor = gltf.accessors[currentPrimitive.attributes.attribute];              \
    const auto& bufferView = gltf.bufferViews[accessor.bufferView];                            \
    result = RCAST <const float*> (&gltf.buffer[accessor.byteOffset + bufferView.byteOffset]); \
  } while (false)

  for (size_t primitive = 0; primitive < gltf.mesh.primitivesCount; ++primitive) {
    const auto& currentPrimitive = gltf.mesh.primitives[primitive];

    uint32_t firstIndex = indexOffset;
    uint32_t vertexStart = vertexOffset;

    const float* uvAccessor = nullptr;
    const float* normalAccessor = nullptr;
    const float* positionAccessor = nullptr;

    GET_ACCESSOR (uv, uvAccessor);
    GET_ACCESSOR (normal, normalAccessor);
    GET_ACCESSOR (position, positionAccessor);

    uint8_t uvStride = assets::gltf::countComponents (assets::gltf::AccessorType::Vec2);
    uint8_t positionStride = assets::gltf::countComponents (assets::gltf::AccessorType::Vec3);

    uint32_t currentVertexCount = gltf.accessors[currentPrimitive.attributes.position].count;

    for (uint32_t vertex = 0; vertex < currentVertexCount; ++vertex) {
      auto& newVertex = vertices[vertexOffset++];

      memcpy (&newVertex.u, &uvAccessor[vertex * uvStride], sizeof (float) * 2);
      memcpy (&newVertex.nx, &normalAccessor[vertex * positionStride], sizeof (float) * 3);
      memcpy (&newVertex.x, &positionAccessor[vertex * positionStride], sizeof (float) * 3);
    }

    const auto& accessor = gltf.accessors[currentPrimitive.indices];
    const auto& bufferView = gltf.bufferViews[accessor.bufferView];
    const auto indexAccessor = RCAST <const uint16_t*> (&gltf.buffer[accessor.byteOffset + bufferView.byteOffset]);

    for (uint32_t index = 0; index < accessor.count; ++index)
      indices[indexOffset++] = indexAccessor[index] + vertexStart;

    auto& newPrimitive = output.primitives[primitive];
    newPrimitive.firstIndex = firstIndex;
    newPrimitive.indexCount = accessor.count;
    newPrimitive.materialIndex = currentPrimitive.material;
  }

  #undef GET_ACCESSOR

  {
    vkutils::Buffer stagingBuffer;
    VkDeviceSize vertexBufferSize = sizeof (Vertex) * vertexCount;
    vkutils::allocateStagingBuffer (device, allocator, vertexBufferSize, stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, vertices, vertexBufferSize);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      vkutils::BufferAllocationInfo allocationInfo;
      allocationInfo.size = vertexBufferSize;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

      vkutils::allocateBuffer (device, allocator, allocationInfo, output.vertexBuffer);
    }

    vkutils::copyBuffer (device, transferContext, stagingBuffer, output.vertexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  free (vertices);

  {
    vkutils::Buffer stagingBuffer;
    VkDeviceSize indexBufferSize = sizeof (uint32_t) * indexCount;
    vkutils::allocateStagingBuffer (device, allocator, indexBufferSize, stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, indices, indexBufferSize);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      vkutils::BufferAllocationInfo allocationInfo;
      allocationInfo.size = indexBufferSize;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      vkutils::allocateBuffer (device, allocator, allocationInfo, output.indexBuffer);
    }

    vkutils::copyBuffer (device, transferContext, stagingBuffer, output.indexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  free (indices);

  output.texturesCount = gltf.imagesCount;
  output.textures = MALLOC (vkutils::Image, gltf.imagesCount);

  for (size_t index = 0; index < gltf.imagesCount; ++index) {
    const auto& current = gltf.images[index];

    char texturePath[256] {};
    strcpy (texturePath, relativePath);

    char* fileName = strrchr (texturePath, '/');
    strcpy (fileName + 1, current.uri);

    int width, height, channels;
    auto pixels = stbi_load (texturePath, &width, &height, &channels, STBI_rgb_alpha);
    assert (pixels);

    vkutils::TextureAllocationInfo allocationInfo;
    allocationInfo.width = SCAST <uint32_t> (width);
    allocationInfo.height = SCAST <uint32_t> (height);
    allocationInfo.pixels = RCAST <const char*> (pixels);

    vkutils::allocateTexture (
      device,
      allocator,
      allocationInfo,
      transferContext,
      output.textures[index]
    );

    stbi_image_free (pixels);
  }

  assets::gltf::destroy (gltf);
}

void destroyModel (VkDevice device, VmaAllocator allocator, Model& model) {
  vkDestroyPipeline (device, model.pipeline, nullptr);
  vkDestroyPipelineLayout (device, model.pipelineLayout, nullptr);
  vmaDestroyBuffer (allocator, model.indexBuffer.handle, model.indexBuffer.allocation);
  vmaDestroyBuffer (allocator, model.vertexBuffer.handle, model.vertexBuffer.allocation);

  for (uint32_t index = 0; index < model.texturesCount; ++index) {
    auto& current = model.textures[index];
    vkDestroyImageView (device, current.view, nullptr);
    vmaDestroyImage (allocator, current.handle, current.allocation);
  }

  free (model.textures);
  free (model.primitives);
}

void Model::initPipelines (VkDevice device, VkRenderPass renderPass, const vkutils::Swapchain& swapchain) {
  {
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof (glm::mat4);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo createInfo {PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK (vkCreatePipelineLayout (device, &createInfo, nullptr, &pipelineLayout));
  }

  vkutils::GraphicsPipelineCreateInfo createInfos[1];

  VkVertexInputBindingDescription binding;
  binding.binding = 0;
  binding.stride = sizeof (Vertex);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription attributes[3];

  // Position
  attributes[0].binding = 0;
  attributes[0].location = 0;
  attributes[0].offset = offsetof (Vertex, x);
  attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;

  // Normal
  attributes[1].binding = 0;
  attributes[1].location = 1;
  attributes[1].offset = offsetof (Vertex, nx);
  attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;

  // Uv
  attributes[2].binding = 0;
  attributes[2].location = 2;
  attributes[2].offset = offsetof (Vertex, u);
  attributes[2].format = VK_FORMAT_R32G32_SFLOAT;

  VkPipelineVertexInputStateCreateInfo vertexInputInfo {PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = 3;
  vertexInputInfo.pVertexBindingDescriptions = &binding;
  vertexInputInfo.pVertexAttributeDescriptions = attributes;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo {PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
  inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkRect2D scissor;
  scissor.offset = {0, 0};
  scissor.extent = swapchain.extent;

  VkViewport viewport;
  viewport.x = 0.f;
  viewport.y = 0.f;
  viewport.minDepth = 0.f;
  viewport.minDepth = 1.f;
  viewport.width = SCAST <float> (swapchain.extent.width);
  viewport.height = SCAST <float> (swapchain.extent.height);

  VkPipelineViewportStateCreateInfo viewportInfo {PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  viewportInfo.scissorCount = 1;
  viewportInfo.viewportCount = 1;
  viewportInfo.pScissors = &scissor;
  viewportInfo.pViewports = &viewport;

  VkPipelineRasterizationStateCreateInfo rasterizationInfo {PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rasterizationInfo.lineWidth = 1.f;
  rasterizationInfo.depthBiasEnable = VK_FALSE;
  rasterizationInfo.depthClampEnable = VK_FALSE;
  rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
  rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampleInfo {PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  multisampleInfo.minSampleShading = 1.f;
  multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencilInfo {PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  depthStencilInfo.minDepthBounds = 0.f;
  depthStencilInfo.maxDepthBounds = 1.f;

  depthStencilInfo.depthTestEnable = VK_TRUE;
  depthStencilInfo.depthWriteEnable = VK_TRUE;
  depthStencilInfo.stencilTestEnable = VK_FALSE;
  depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
  depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;

  VkPipelineColorBlendAttachmentState blendAttachmentInfo {};
  blendAttachmentInfo.blendEnable = VK_FALSE,
  blendAttachmentInfo.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlendInfo {PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  colorBlendInfo.attachmentCount = 1;
  colorBlendInfo.logicOpEnable = VK_FALSE;
  colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
  colorBlendInfo.pAttachments = &blendAttachmentInfo;

  createInfos[0].dynamicInfo = nullptr;
  createInfos[0].renderPass = renderPass;
  createInfos[0].layout = pipelineLayout;
  createInfos[0].pixelShaderPath = "assets/shaders/mesh.frag.spv";
  createInfos[0].vertexShaderPath = "assets/shaders/mesh.vert.spv";

  createInfos[0].vertexInputInfo = &vertexInputInfo;
  createInfos[0].inputAssemblyInfo = &inputAssemblyInfo;
  createInfos[0].viewportInfo = &viewportInfo;
  createInfos[0].rasterizationInfo = &rasterizationInfo;
  createInfos[0].multisampleInfo = &multisampleInfo;
  createInfos[0].colorBlendInfo = &colorBlendInfo;
  createInfos[0].depthStencilInfo = &depthStencilInfo;

  rei::vkutils::createGraphicsPipelines (device, VK_NULL_HANDLE, 1, createInfos, &pipeline);
}

void Model::draw (VkCommandBuffer commandBuffer, const glm::mat4& mvp) {
  vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers (commandBuffer, 0, 1, &vertexBuffer.handle, &offset);
  vkCmdBindIndexBuffer (commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

  vkCmdPushConstants (commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (glm::mat4), &mvp);

  for (uint32_t index = 0; index < primitivesCount; ++index) {
    const auto& current = primitives[index];
    vkCmdDrawIndexed (commandBuffer, current.indexCount, 1, current.firstIndex, 0, 0);
  }
}

}
