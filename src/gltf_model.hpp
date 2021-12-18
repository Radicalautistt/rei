#ifndef GLTF_MODEL_HPP
#define GLTF_MODEL_HPP

#include "vkutils.hpp"
#include "rei_math_types.hpp"

namespace rei::gltf {

struct Material {
  size_t albedoIndex;
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
  VkDescriptorSet* descriptors;

  // Count of descriptors and batches
  size_t materialsCount;

  Batch* batches;

  vku::Image* textures;
  size_t texturesCount;

  vku::Buffer vertexBuffer;
  vku::Buffer indexBuffer;

  math::Mat4 modelMatrix;

  void draw (VkCommandBuffer cmdBuffer, VkPipelineLayout layout, const math::Mat4* viewProjection);
};

void load (
  VkDevice device,
  VmaAllocator allocator,
  const vku::TransferContext* transferContext,
  VkDescriptorSetLayout descriptorLayout,
  const char* relativePath,
  Model* out
);

void destroy (VkDevice device, VmaAllocator allocator, Model* model);

}

#endif /* GLTF_MODEL_HPP */
