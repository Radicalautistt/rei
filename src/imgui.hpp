#ifndef IMGUI_HPP
#define IMGUI_HPP

#include "common.hpp"
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
    Uint32 index;
    Uint32 vertex;
  } counts;

  ImGuiContext* handle;

  VkDevice device;
  VmaAllocator allocator;

  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;

  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;
  VkSampler fontSampler;

  VkFence bufferUpdateFence;

  xcb::Window* window;
  vku::TransferContext* transferContext;

  vku::Buffer indexBuffers[FRAMES_COUNT];
  vku::Buffer vertexBuffers[FRAMES_COUNT];

  vku::Image fontTexture;

  void newFrame ();
  void updateBuffers (Uint32 frameIndex, const ImDrawData* drawData);
  void handleEvents (const xcb_generic_event_t* event);
  void renderDrawData (VkCommandBuffer commandBuffer, Uint32 frameIndex, const ImDrawData* drawData);
};

void create (const ContextCreateInfo* createInfo, Context* output);
void destroy (Context* context);

void showDebugWindow (Float32* cameraSpeed, VmaAllocator allocator);

};

#endif /* IMGUI_HPP */
