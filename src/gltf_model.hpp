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
  uint32_t albedoIndex;
  VkSampler albedoSampler;
  VkDescriptorSet descriptorSet;
  VkDescriptorSetLayout descriptorSetLayout;
};

struct Primitive {
  uint32_t firstIndex;
  uint32_t indexCount;
  uint32_t materialIndex;
};

struct Model {
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;

  VkDescriptorPool descriptorPool;

  Material* materials;
  uint32_t materialsCount;

  Primitive* primitives;
  uint32_t primitivesCount;

  vkutils::Image* textures;
  uint32_t texturesCount;

  vkutils::Buffer vertexBuffer;
  vkutils::Buffer indexBuffer;

  void initDescriptorPool (VkDevice device);

  void initMaterialDescriptors (VkDevice device);

  void initPipelines (
    VkDevice device,
    VkRenderPass renderPass,
    VkPipelineCache pipelineCache,
    const vkutils::Swapchain& swapchain
  );

  void draw (VkCommandBuffer commandBuffer, const math::Matrix4& mvp);
};

void loadModel (
  VkDevice device,
  VmaAllocator allocator,
  const vkutils::TransferContext& transferContext,
  const char* relativePath,
  Model& output
);

void destroyModel (VkDevice device, VmaAllocator allocator, Model& model);

}

#endif /* GLTF_MODEL_HPP */
