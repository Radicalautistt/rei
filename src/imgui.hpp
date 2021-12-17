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
  VkPipelineCache pipelineCache;

  VkDescriptorPool descriptorPool;
  xcb::Window* window;
  vku::TransferContext* transferContext;

  VkRenderPass renderPass;
  vku::Swapchain* swapchain;
};

struct Context {
  ImGuiContext* handle;

  VmaAllocator allocator;

  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;

  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;
  VkSampler fontSampler;

  xcb::Window* window;
  vku::Buffer indexBuffers[REI_FRAMES_COUNT];
  vku::Buffer vertexBuffers[REI_FRAMES_COUNT];

  vku::Image fontTexture;
  b32 mouseButtonsDown[2];

  void newFrame ();
  void updateBuffers (u32 frameIndex, const ImDrawData* drawData);
  void handleEvents (const xcb_generic_event_t* event);
  void renderDrawData (VkCommandBuffer cmdBuffer, u32 frameIndex, const ImDrawData* drawData);
};

void create (VkDevice device, VmaAllocator allocator, const ContextCreateInfo* createInfo, Context* output);
void destroy (VkDevice device, Context* context);

void showDebugWindow (f32* cameraSpeed, u32* gbufferOutput, VmaAllocator allocator);

};

#endif /* IMGUI_HPP */
