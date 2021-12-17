#ifndef VKUTILS_HPP
#define VKUTILS_HPP

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

struct QueueIndices {
  u32 graphics, present, transfer, compute;
};

struct Image {
  VkImage handle;
  VkImageView view;
  VmaAllocation allocation;
};

struct AttachmentCreateInfo {
  VkFormat format;
  u32 width, height;
  VkImageUsageFlags usage;
  VkImageAspectFlags aspectMask;
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

  u32 imagesCount;
  VkImage* images;
  VkImageView* views;

  VkSwapchainKHR handle;

  VkExtent2D extent;

  Image depthImage;
};

struct GraphicsPipelineCreateInfo {
  VkPipelineCache cache;
  VkRenderPass renderPass;
  VkPipelineLayout layout;

  const char* pixelShaderPath;
  const char* vertexShaderPath;

  size_t colorBlendAttachmentCount;

  VkPipelineDynamicStateCreateInfo* dynamicState;
  VkPipelineViewportStateCreateInfo* viewportState;
  VkPipelineVertexInputStateCreateInfo* vertexInputState;
  VkPipelineDepthStencilStateCreateInfo* depthStencilState;
  VkPipelineColorBlendAttachmentState* colorBlendAttachment;
  VkPipelineRasterizationStateCreateInfo* rasterizationState;
};

struct BufferAllocationInfo {
  VkBufferUsageFlags bufferUsage;
  u32 memoryUsage;
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
  char* pixels;
  u32 width, height;
  size_t compressedSize;
};

b8 findQueueIndices (VkPhysicalDevice physicalDevice, VkSurfaceKHR targetSurface, QueueIndices* out);

void choosePhysicalDevice (
  VkInstance instance,
  VkSurfaceKHR targetSurface,
  const char* const* requiredExtensions,
  u32 requiredExtensionCount,
  QueueIndices* outputIndices,
  VkPhysicalDevice* out
);

void createAttachment (VkDevice device, VmaAllocator allocator, const AttachmentCreateInfo* createInfo, Image* out);
void createSwapchain (const SwapchainCreateInfo* createInfo, Swapchain* out);
void destroySwapchain (VkDevice device, VmaAllocator allocator, Swapchain* swapchain);

void createShaderModule (VkDevice device, const char* relativePath, VkShaderModule* out);
void createGraphicsPipeline (VkDevice device, const GraphicsPipelineCreateInfo* createInfo, VkPipeline* out);

void startImmediateCmd (VkDevice device, const TransferContext* transferContext, VkCommandBuffer* out);
void submitImmediateCmd (VkDevice device, const TransferContext* transferContext, VkCommandBuffer cmdBuffer);

void allocateBuffer (VmaAllocator allocator, const BufferAllocationInfo* allocationInfo, Buffer* out);
void allocateStagingBuffer (VmaAllocator allocator, VkDeviceSize size, Buffer* out);

void transitionImageLayout (VkCommandBuffer commandBuffer, const ImageLayoutTransitionInfo* transitionInfo, VkImage image);

void allocateTexture (
  VkDevice device,
  VmaAllocator allocator,
  const TextureAllocationInfo* allocationInfo,
  const TransferContext* transferContext,
  Image* out
);

}

#endif /* VKUTILS_HPP */
