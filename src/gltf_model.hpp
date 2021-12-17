#ifndef GLTF_MODEL_HPP
#define GLTF_MODEL_HPP

#include "math.hpp"
#include "vkutils.hpp"

namespace rei::gltf {

struct Material {
  size_t albedoIndex;
  VkDescriptorSet descriptorSet;
};

// This is used to group multiple primitives with the same material
struct Batch {
  u32 firstIndex;
  u32 indexCount;
  u32 materialIndex;
};

struct Model {
  VkSampler sampler;
  VkDescriptorPool descriptorPool;

  Material* materials;
  size_t materialsCount;

  Batch* batches;

  vku::Image* textures;
  size_t texturesCount;

  vku::Buffer vertexBuffer;
  vku::Buffer indexBuffer;

  math::Matrix4 modelMatrix;

  void initDescriptors (VkDevice device, VkDescriptorSetLayout descriptorLayout);
  void draw (VkCommandBuffer cmdBuffer, VkPipelineLayout layout, const math::Matrix4* viewProjection);
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
