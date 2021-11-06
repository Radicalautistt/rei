#ifndef IMGUI_HPP
#define IMGUI_HPP

#include "vkutils.hpp"

struct ImDrawData;
struct ImGuiContext;

namespace rei::xcb {
struct Window;
}

namespace rei::imgui {

struct ContextCreateInfo {
  VkDevice device;
  VmaAllocator allocator;

  VkPipelineCache pipelineCache;

  VkDescriptorPool descriptorPool;
  xcb::Window* window;
  vku::TransferContext* transferContext;

  VkRenderPass renderPass;
  vku::Swapchain* swapchain;
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

  xcb::Window* window;
  vku::TransferContext* transferContext;

  vku::Buffer indexBuffer;
  vku::Buffer vertexBuffer;

  vku::Image fontTexture;

  void newFrame ();
  void updateBuffers (const ImDrawData* drawData);
  void handleEvents (const xcb_generic_event_t* event);
  void renderDrawData (const ImDrawData* drawData, VkCommandBuffer commandBuffer);
};

void create (const ContextCreateInfo* createInfo, Context* output);
void destroy (VkDevice device, VmaAllocator allocator, Context* context);

};

#endif /* IMGUI_HPP */
