#ifndef GLTF_MODEL_HPP
#define GLTF_MODEL_HPP

#include "math.hpp"
#include "common.hpp"
#include "vkutils.hpp"

namespace rei::gltf {

struct Vertex {
  Float32 x, y, z;
  Float32 nx, ny, nz;
  Float32 u, v;
};

struct Material {
  size_t albedoIndex;
  VkSampler albedoSampler;
  VkDescriptorSet descriptorSet;
  VkDescriptorSetLayout descriptorSetLayout;
};

// This is used to group multiple primitives with the same material
struct Batch {
  Uint32 firstIndex;
  Uint32 indexCount;
  Uint32 materialIndex;
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

  math::Matrix4 modelMatrix;

  void initDescriptorPool (VkDevice device);
  void initMaterialDescriptors (VkDevice device);

  void initPipelines (
    VkDevice device,
    VkRenderPass renderPass,
    VkPipelineCache pipelineCache,
    const vku::Swapchain* swapchain
  );

  void draw (VkCommandBuffer commandBuffer, const math::Matrix4* viewProjection);
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
