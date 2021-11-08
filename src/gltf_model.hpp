#ifndef GLTF_MODEL_HPP
#define GLTF_MODEL_HPP

#include <stdint.h>

#include "vkutils.hpp"

namespace rei::math {
struct Matrix4;
}

namespace rei::gltf {

struct Vertex {
  float x, y, z;
  float nx, ny, nz;
  float u, v;
};

struct Material {
  size_t albedoIndex;
  VkSampler albedoSampler;
  VkDescriptorSet descriptorSet;
  VkDescriptorSetLayout descriptorSetLayout;
};

// This is used to group multiple primitives with the same material
struct Batch {
  uint32_t firstIndex;
  uint32_t indexCount;
  uint32_t materialIndex;
};

struct Model {
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;

  VkDescriptorPool descriptorPool;

  Material* materials;
  size_t materialsCount;

  Batch* batches;

  vku::Image* textures;
  size_t texturesCount;

  vku::Buffer vertexBuffer;
  vku::Buffer indexBuffer;

  void initDescriptorPool (VkDevice device);
  void initMaterialDescriptors (VkDevice device);

  void initPipelines (
    VkDevice device,
    VkRenderPass renderPass,
    VkPipelineCache pipelineCache,
    const vku::Swapchain* swapchain
  );

  void draw (VkCommandBuffer commandBuffer, const math::Matrix4* mvp);
};

void load (
  VkDevice device,
  VmaAllocator allocator,
  const vku::TransferContext* transferContext,
  const char* relativePath,
  Model* output
);

void destroy (VkDevice device, VmaAllocator allocator, Model* model);

}

#endif /* GLTF_MODEL_HPP */
