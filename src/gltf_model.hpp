#ifndef GLTF_MODEL_HPP
#define GLTF_MODEL_HPP

#include <stdint.h>

#include "vkutils.hpp"

#include <glm/mat4x4.hpp>

namespace rei::gltf {

struct Vertex {
  float x, y, z;
  float nx, ny, nz;
  float u, v;
};

struct Primitive {
  uint32_t firstIndex;
  uint32_t indexCount;
  uint32_t materialIndex;
};

struct Model {
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;

  Primitive* primitives;
  uint32_t primitivesCount;

  vkutils::Image* textures;
  uint32_t texturesCount;

  uint32_t vertexCount, indexCount;
  vkutils::Buffer vertexBuffer;
  vkutils::Buffer indexBuffer;

  void initPipelines (VkDevice device, VkRenderPass renderPass, const vkutils::Swapchain& swapchain);
  void draw (VkCommandBuffer commandBuffer, const glm::mat4& mvp);
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
