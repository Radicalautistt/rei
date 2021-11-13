#ifndef VKUTILS_HPP
#define VKUTILS_HPP

#include "common.hpp"
#include "vkcommon.hpp"

// Forward declarations
struct VmaAllocator_T;
struct VmaAllocation_T;
typedef VmaAllocator_T* VmaAllocator;
typedef VmaAllocation_T* VmaAllocation;

namespace rei::xcb {
struct Window;
}

namespace rei::vku {

struct QueueFamilyIndices {
  Bool haveGraphics, havePresent, haveTransfer, haveCompute;
  Uint32 graphics, present, transfer, compute;
};

struct Image {
  VkFormat format;

  VkImage handle;
  VkImageView view;
  VmaAllocation allocation;
};

struct AttachmentCreateInfo {
  VkFormat format;
  Uint32 width, height;
  VkImageUsageFlags usage;
};

struct SwapchainCreateInfo {
  VkDevice device;
  VmaAllocator allocator;
  xcb::Window* window;
  VkSurfaceKHR windowSurface;
  VkSwapchainKHR oldSwapchain;
  VkPhysicalDevice physicalDevice;
};

struct Swapchain {
  VkFormat format;

  Uint32 imagesCount;
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
  VkBufferUsageFlags bufferUsage;
  Uint32 memoryUsage;
  VkMemoryPropertyFlags requiredFlags;

  VkDeviceSize size;
};

struct TransferContext {
  VkFence fence;
  VkQueue queue;
  VkCommandPool commandPool;
};

struct Buffer {
  VkBuffer handle;
  VmaAllocation allocation;
  void* mapped;

  VkDeviceSize size;
};

struct ImageLayoutTransitionInfo {
  VkImageLayout oldLayout, newLayout;
  VkPipelineStageFlags source, destination;
  VkImageSubresourceRange* subresourceRange;
};

struct TextureAllocationInfo {
  Bool32 compressed;
  Uint32 mipLevels;
  Uint32 width, height;
  char* pixels;
  size_t compressedSize;
};

void findQueueFamilyIndices (
  VkPhysicalDevice physicalDevice,
  VkSurfaceKHR targetSurface,
  QueueFamilyIndices* output
);

void choosePhysicalDevice (
  VkInstance instance,
  VkSurfaceKHR targetSurface,
  QueueFamilyIndices* outputIndices,
  VkPhysicalDevice* output
);

void createAttachment (
  VkDevice device,
  VmaAllocator allocator,
  const AttachmentCreateInfo* createInfo,
  Image* output
);

void createSwapchain (const SwapchainCreateInfo* createInfo, Swapchain* output);
void destroySwapchain (VkDevice device, VmaAllocator allocator, Swapchain* swapchain);

void createShaderModule (VkDevice device, const char* relativePath, VkShaderModule* output);

void createGraphicsPipelines (
  VkDevice device,
  VkPipelineCache pipelineCache,
  Uint32 count,
  const GraphicsPipelineCreateInfo* createInfos,
  VkPipeline* outputs
);

[[nodiscard]] VkCommandBuffer startImmediateCommand (VkDevice device, VkCommandPool commandPool);
void submitImmediateCommand (VkDevice device, const TransferContext* transferContext, VkCommandBuffer commandBuffer);

void allocateBuffer (VmaAllocator allocator, const BufferAllocationInfo* allocationInfo, Buffer* output);
void allocateStagingBuffer (VmaAllocator allocator, VkDeviceSize size, Buffer* output);
void copyBuffer (VkDevice device, const TransferContext* transferContext, const Buffer* source, Buffer* destination);

void transitionImageLayout (
  VkCommandBuffer commandBuffer,
  const ImageLayoutTransitionInfo* transitionInfo,
  VkImage image
);

void allocateTexture (
  VkDevice device,
  VmaAllocator allocator,
  const TextureAllocationInfo* allocationInfo,
  const TransferContext* transferContext,
  Image* output
);

}

#endif /* VKUTILS_HPP */
