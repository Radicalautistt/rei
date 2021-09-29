#ifndef IMGUI_HPP
#define IMGUI_HPP

#include "vkutils.hpp"

struct ImDrawData;
struct ImGuiContext;

namespace rei::extra::imgui {

struct ContextCreateInfo {
  VkDevice device;
  VmaAllocator allocator;

  VkPipelineCache pipelineCache;

  VkDescriptorPool descriptorPool;
  vkutils::TransferContext* transferContext;

  VkRenderPass renderPass;
  vkutils::Swapchain* swapchain;
};

struct Context {
  struct {
    uint32_t index;
    uint32_t vertex;
  } counts;

  ImGuiContext* handle;

  VkDevice device;
  VmaAllocator allocator;

  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;

  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;
  VkSampler fontSampler;

  vkutils::TransferContext* transferContext;

  vkutils::Buffer indexBuffer;
  vkutils::Buffer vertexBuffer;

  vkutils::Image fontTexture;

  void newFrame ();
  void updateBuffers (const ImDrawData* drawData);
  void renderDrawData (const ImDrawData* drawData, VkCommandBuffer commandBuffer);
};

void create (const ContextCreateInfo& createInfo, Context& output);
void destroy (VkDevice device, VmaAllocator allocator, Context& context);

};

#endif /* IMGUI_HPP */
