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

void findQueueFamilyIndices (VkPhysicalDevice physicalDevice, VkSurfaceKHR targetSurface, QueueFamilyIndices& output);

void choosePhysicalDevice (
  VkInstance instance,
  VkSurfaceKHR targetSurface,
  QueueFamilyIndices& outputIndices,
  VkPhysicalDevice& output
);

void createSwapchain (const SwapchainCreateInfo& createInfo, Swapchain& output);
void destroySwapchain (VkDevice device, VmaAllocator allocator, Swapchain& swapchain);

}

#endif /* VKUTILS_HPP */
