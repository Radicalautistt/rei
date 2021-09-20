#ifndef VKUTILS_HPP
#define VKUTILS_HPP

#include "vkcommon.hpp"

// Forward declarations
struct VmaAllocator_T;
struct VmaAllocation_T;
typedef VmaAllocator_T* VmaAllocator;
typedef VmaAllocation_T* VmaAllocation;

namespace rei::extra::xcb {
struct Window;
}

namespace rei::vkutils {

struct QueueFamilyIndices {
  bool haveGraphics, havePresent, haveTransfer, haveCompute;
  uint32_t graphics, present, transfer, compute;
};

struct Image {
  VkFormat format;

  VkImage handle;
  VkImageView view;
  VmaAllocation allocation;
};

struct SwapchainCreateInfo {
  VkDevice device;
  VmaAllocator allocator;
  extra::xcb::Window* window;
  VkSurfaceKHR windowSurface;
  VkSwapchainKHR oldSwapchain;
  VkPhysicalDevice physicalDevice;
};

struct Swapchain {
  VkFormat format;

  uint32_t imagesCount;
  VkImage* images;
  VkImageView* views;

  VkSwapchainKHR handle;

  VkExtent2D extent;

  Image depthImage;
};

struct Shaders {
  VkShaderModule vertex, pixel;
  VkPipelineShaderStageCreateInfo stages[2];
};

struct GraphicsPipelineCreateInfo {
  VkRenderPass renderPass;
  VkPipelineLayout layout;

  const char* vertexShaderPath;
  const char* pixelShaderPath;

  VkPipelineDynamicStateCreateInfo* dynamicInfo;
  VkPipelineViewportStateCreateInfo* viewportInfo;
  VkPipelineColorBlendStateCreateInfo* colorBlendInfo;
  VkPipelineMultisampleStateCreateInfo* multisampleInfo;
  VkPipelineVertexInputStateCreateInfo* vertexInputInfo;
  VkPipelineDepthStencilStateCreateInfo* depthStencilInfo;
  VkPipelineInputAssemblyStateCreateInfo* inputAssemblyInfo;
  VkPipelineRasterizationStateCreateInfo* rasterizationInfo;
};

struct BufferAllocationInfo {
  VkDevice device;
  VmaAllocator allocator;
  VkDeviceSize size;

  VkBufferUsageFlags bufferUsage;
  uint32_t memoryUsage;
  VkMemoryPropertyFlags requiredFlags;
};

struct BufferCopyInfo {
  VkDevice device;
  VkFence waitFence;
  VkCommandPool commandPool;
  VkQueue submitQueue;
};

struct Buffer {
  VkBuffer handle;
  VmaAllocation allocation;
  void* mapped;

  VkDeviceSize size;
};

void findQueueFamilyIndices (VkPhysicalDevice physicalDevice, VkSurfaceKHR targetSurface, QueueFamilyIndices& output);

void choosePhysicalDevice (
  VkInstance instance,
  VkSurfaceKHR targetSurface,
  QueueFamilyIndices& outputIndices,
  VkPhysicalDevice& output
);

void createSwapchain (const SwapchainCreateInfo& createInfo, Swapchain& output);
void destroySwapchain (VkDevice device, VmaAllocator allocator, Swapchain& swapchain);

void createShaderModule (VkDevice device, const char* relativePath, VkShaderModule& output);

void createGraphicsPipelines (
  VkDevice device,
  VkPipelineCache pipelineCache,
  uint32_t count,
  const GraphicsPipelineCreateInfo* createInfos,
  VkPipeline* outputs
);

[[nodiscard]] VkCommandBuffer startImmediateCommand (VkDevice device, VkCommandPool commandPool);
void submitImmediateCommand (
  VkDevice device,
  VkCommandBuffer commandBuffer,
  VkCommandPool commandPool,
  VkFence waitFence,
  VkQueue submitQueue
);

void allocateBuffer (const BufferAllocationInfo& allocationInfo, Buffer& output);
void copyBuffer (const BufferCopyInfo& copyInfo, const Buffer& source, Buffer& destination);

}

#endif /* VKUTILS_HPP */
