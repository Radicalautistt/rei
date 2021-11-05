#include <string.h>
#include <stdlib.h>

#include "math.hpp"
#include "gltf.hpp"
#include "common.hpp"
#include "gltf_model.hpp"
#include "asset_baker.hpp"

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::gltf {

void loadModel (
  VkDevice device,
  VmaAllocator allocator,
  const vku::TransferContext& transferContext,
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

  output.primitivesCount = SCAST <uint32_t> (gltf.mesh.primitivesCount);

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
    vku::Buffer stagingBuffer;
    VkDeviceSize vertexBufferSize = sizeof (Vertex) * vertexCount;
    vku::allocateStagingBuffer (allocator, vertexBufferSize, stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, vertices, vertexBufferSize);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      vku::BufferAllocationInfo allocationInfo;
      allocationInfo.size = vertexBufferSize;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

      vku::allocateBuffer (allocator, allocationInfo, output.vertexBuffer);
    }

    vku::copyBuffer (device, transferContext, stagingBuffer, output.vertexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  free (vertices);

  {
    vku::Buffer stagingBuffer;
    VkDeviceSize indexBufferSize = sizeof (uint32_t) * indexCount;
    vku::allocateStagingBuffer (allocator, indexBufferSize, stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, indices, indexBufferSize);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      vku::BufferAllocationInfo allocationInfo;
      allocationInfo.size = indexBufferSize;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      vku::allocateBuffer (allocator, allocationInfo, output.indexBuffer);
    }

    vku::copyBuffer (device, transferContext, stagingBuffer, output.indexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  free (indices);

  output.texturesCount = SCAST <uint32_t> (gltf.imagesCount);
  output.textures = MALLOC (vku::Image, gltf.imagesCount);

  simdjson::ondemand::parser parser;

  for (size_t index = 0; index < gltf.imagesCount; ++index) {
    const auto& current = gltf.images[index];

    char texturePath[256] {};
    strcpy (texturePath, relativePath);

    char* fileName = strrchr (texturePath, '/');
    strcpy (fileName + 1, current.uri);
    char* extension = strrchr (texturePath, '.');
    strcpy (extension + 1, "rtex");

    assets::Asset asset;
    REI_CHECK (assets::readAsset (texturePath, parser, asset));

    simdjson::padded_string paddedMetadata {asset.metadata, strlen (asset.metadata)};
    simdjson::ondemand::document metadata = parser.iterate (paddedMetadata);

    uint32_t width = SCAST <uint32_t> (metadata["width"].get_uint64 ());
    uint32_t height = SCAST <uint32_t> (metadata["height"].get_uint64 ());

    vku::TextureAllocationInfo allocationInfo;
    allocationInfo.width = width;
    allocationInfo.height = height;
    allocationInfo.compressed = true;
    allocationInfo.pixels = asset.data;
    allocationInfo.generateMipmaps = true;
    allocationInfo.compressedSize = asset.size;

    vku::allocateTexture (
      device,
      allocator,
      allocationInfo,
      transferContext,
      output.textures[index]
    );

    free (asset.data);
    free (asset.metadata);
  }

  output.materialsCount = SCAST <uint32_t> (gltf.materialsCount);
  output.materials = MALLOC (Material, gltf.materialsCount);

  for (size_t index = 0; index < gltf.materialsCount; ++index)
    output.materials[index].albedoIndex =
      gltf.materials[index].pbrMetallicRoughness.baseColorTexture.index;

  assets::gltf::destroy (gltf);
}

void destroyModel (VkDevice device, VmaAllocator allocator, Model& model) {
  vkDestroyPipeline (device, model.pipeline, nullptr);
  vkDestroyPipelineLayout (device, model.pipelineLayout, nullptr);
  vmaDestroyBuffer (allocator, model.indexBuffer.handle, model.indexBuffer.allocation);
  vmaDestroyBuffer (allocator, model.vertexBuffer.handle, model.vertexBuffer.allocation);

  for (uint32_t index = 0; index < model.materialsCount; ++index) {
    auto& current = model.materials[index];
    vkDestroySampler (device, current.albedoSampler, nullptr);
    vkDestroyDescriptorSetLayout (device, current.descriptorSetLayout, nullptr);
  }

  free (model.materials);

  vkDestroyDescriptorPool (device, model.descriptorPool, nullptr);

  for (uint32_t index = 0; index < model.texturesCount; ++index) {
    auto& current = model.textures[index];
    vkDestroyImageView (device, current.view, nullptr);
    vmaDestroyImage (allocator, current.handle, current.allocation);
  }

  free (model.textures);
  free (model.primitives);
}

void Model::initMaterialDescriptors (VkDevice device) {
  for (uint32_t index = 0; index < materialsCount; ++index) {
    auto& current = materials[index];

    {
      VkDescriptorSetLayoutBinding albedo;
      albedo.binding = 0;
      albedo.descriptorCount = 1;
      albedo.pImmutableSamplers = nullptr;
      albedo.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      albedo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

      VkDescriptorSetLayoutCreateInfo createInfo {DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
      createInfo.bindingCount = 1;
      createInfo.pBindings = &albedo;

      VK_CHECK (vkCreateDescriptorSetLayout (device, &createInfo, nullptr, &current.descriptorSetLayout));
    }

    {
      VkDescriptorSetAllocateInfo allocationInfo {DESCRIPTOR_SET_ALLOCATE_INFO};
      allocationInfo.descriptorSetCount = 1;
      allocationInfo.descriptorPool = descriptorPool;
      allocationInfo.pSetLayouts = &current.descriptorSetLayout;

      VK_CHECK (vkAllocateDescriptorSets (device, &allocationInfo, &current.descriptorSet));
    }

    {
      // TODO get filtering, etc from gltf samplers
      VkSamplerCreateInfo createInfo {SAMPLER_CREATE_INFO};
      createInfo.minFilter = VK_FILTER_LINEAR;
      createInfo.magFilter = VK_FILTER_LINEAR;
      createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

      VK_CHECK (vkCreateSampler (device, &createInfo, nullptr, &current.albedoSampler));
    }

    VkDescriptorImageInfo albedoInfo;
    albedoInfo.sampler = current.albedoSampler;
    albedoInfo.imageView = textures[current.albedoIndex].view;
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &albedoInfo;
    write.dstSet = current.descriptorSet;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    vkUpdateDescriptorSets (device, 1, &write, 0, nullptr);
  }
}

void Model::initDescriptorPool (VkDevice device) {
  VkDescriptorPoolSize poolSize {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturesCount};

  VkDescriptorPoolCreateInfo createInfo {DESCRIPTOR_POOL_CREATE_INFO};
  createInfo.poolSizeCount = 1;
  createInfo.pPoolSizes = &poolSize;
  createInfo.maxSets = materialsCount;

  VK_CHECK (vkCreateDescriptorPool (device, &createInfo, nullptr, &descriptorPool));
}

void Model::initPipelines (
  VkDevice device,
  VkRenderPass renderPass,
  VkPipelineCache pipelineCache,
  const vku::Swapchain& swapchain) {

  {
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof (math::Matrix4);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo createInfo {PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;
    // TODO create different pipelines for every sampler combination (e.g albedo, albedo + normal, etc)
    createInfo.pSetLayouts = &materials[0].descriptorSetLayout;

    VK_CHECK (vkCreatePipelineLayout (device, &createInfo, nullptr, &pipelineLayout));
  }

  vku::GraphicsPipelineCreateInfo createInfos[1];

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
  viewport.maxDepth = 1.f;
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

  rei::vku::createGraphicsPipelines (device, pipelineCache, 1, createInfos, &pipeline);
}

void Model::draw (VkCommandBuffer commandBuffer, const math::Matrix4& mvp) {
  vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers (commandBuffer, 0, 1, &vertexBuffer.handle, &offset);
  vkCmdBindIndexBuffer (commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

  vkCmdPushConstants (commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (math::Matrix4), &mvp);

  for (uint32_t index = 0; index < primitivesCount; ++index) {
    const auto& current = primitives[index];

    vkCmdBindDescriptorSets (
      commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1, &materials[current.materialIndex].descriptorSet,
      0, nullptr
    );

    vkCmdDrawIndexed (commandBuffer, current.indexCount, 1, current.firstIndex, 0, 0);
  }
}

}
