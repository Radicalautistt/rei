#include <string.h>
#include <assert.h>

#include "utils.hpp"
#include "common.hpp"
#include "window.hpp"
#include "vkutils.hpp"

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::vkutils {

void findQueueFamilyIndices (VkPhysicalDevice physicalDevice, VkSurfaceKHR targetSurface, QueueFamilyIndices& output) {
  output.haveGraphics = output.havePresent = output.haveTransfer = output.haveCompute = false;

  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties (physicalDevice, &count, nullptr);

  auto available = ALLOCA (VkQueueFamilyProperties, count);
  vkGetPhysicalDeviceQueueFamilyProperties (physicalDevice, &count, available);

  for (uint32_t index = 0; index < count; ++index) {
    auto& current = available[index];

    if (current.queueCount) {
      VkBool32 supportsPresentation = VK_FALSE;
      VK_CHECK (vkGetPhysicalDeviceSurfaceSupportKHR (physicalDevice, index, targetSurface, &supportsPresentation));

      if (supportsPresentation) {
	output.havePresent = true;
	output.present = index;
      }

      if (current.queueFlags & VK_QUEUE_COMPUTE_BIT) {
	output.haveCompute = true;
        output.compute = index;
      }

      if (current.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
	output.haveGraphics = true;
	output.graphics = index;
      }

      if (current.queueFlags & VK_QUEUE_TRANSFER_BIT) {
        output.haveTransfer = true;
	output.transfer = index;
      }

      if (output.haveGraphics && output.havePresent && output.haveTransfer && output.haveCompute)
	break;
    }
  }
}

void choosePhysicalDevice (VkInstance instance, VkSurfaceKHR targetSurface, QueueFamilyIndices& outputIndices, VkPhysicalDevice& output) {
  uint32_t count = 0;
  VK_CHECK (vkEnumeratePhysicalDevices (instance, &count, nullptr));

  auto available = ALLOCA (VkPhysicalDevice, count);
  VK_CHECK (vkEnumeratePhysicalDevices (instance, &count, available));

  for (uint32_t index = 0; index < count; ++index) {
    auto& current = available[index];
    bool supportsExtensions, supportsSwapchain = false;

    { // Check support for required extensions
      uint32_t extensionsCount = 0;
      VK_CHECK (vkEnumerateDeviceExtensionProperties (current, nullptr, &extensionsCount, nullptr));

      auto availableExtensions = ALLOCA (VkExtensionProperties, extensionsCount);
      VK_CHECK (vkEnumerateDeviceExtensionProperties (current, nullptr, &extensionsCount, availableExtensions));

      for (uint32_t required = 0; required < DEVICE_EXTENSIONS_COUNT; ++required) {
	supportsExtensions = false;

	for (uint32_t present = 0; present < extensionsCount; ++present) {
	  if (!strcmp (availableExtensions[present].extensionName, vkcommon::requiredDeviceExtensions[required])) {
	    supportsExtensions = true;
	    break;
	  }
	}
      }
    }

    if (supportsExtensions) {
      uint32_t formatsCount = 0, presentModesCount = 0;
      VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR (current, targetSurface, &formatsCount, nullptr));
      VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR (current, targetSurface, &presentModesCount, nullptr));

      supportsSwapchain = formatsCount && presentModesCount;
    }

    findQueueFamilyIndices (current, targetSurface, outputIndices);

    bool hasQueueFamilies =
      outputIndices.haveGraphics &&
      outputIndices.havePresent &&
      outputIndices.haveTransfer &&
      outputIndices.haveCompute;

    if (hasQueueFamilies && supportsSwapchain) {
      output = current;
      break;
    }
  }

  assert (output);
}

void createSwapchain (const SwapchainCreateInfo& createInfo, Swapchain& output) {
  {
    // Choose swapchain extent
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK (vkGetPhysicalDeviceSurfaceCapabilitiesKHR (createInfo.physicalDevice, createInfo.windowSurface, &surfaceCapabilities));

    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
      output.extent = surfaceCapabilities.currentExtent;
    } else {
      output.extent.width = utils::clamp <uint32_t> (
        SCAST <uint32_t> (createInfo.window->width),
        surfaceCapabilities.minImageExtent.width,
        surfaceCapabilities.maxImageExtent.width
      );

      output.extent.height = utils::clamp <uint32_t> (
        SCAST <uint32_t> (createInfo.window->height),
        surfaceCapabilities.minImageExtent.height,
        surfaceCapabilities.maxImageExtent.height
      );
    }

    uint32_t minImagesCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount && minImagesCount > surfaceCapabilities.maxImageCount)
      minImagesCount = surfaceCapabilities.maxImageCount;

    // Choose surface format
    VkSurfaceFormatKHR surfaceFormat;
    {
      uint32_t count = 0;
      VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, nullptr));

      auto available = ALLOCA (VkSurfaceFormatKHR, count);
      VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, available));

      surfaceFormat = available[0];
      for (uint32_t index = 0; index < count; ++index) {
        bool rgba8 = available[index].format == VK_FORMAT_R8G8B8A8_SRGB;
        bool nonLinear = available[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        if (rgba8 && nonLinear) {
          surfaceFormat = available[index];
          break;
        }
      }
    }

    output.format = surfaceFormat.format;

    // Choose present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
      uint32_t count = 0;
      VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, nullptr));

      auto available = ALLOCA (VkPresentModeKHR, count);
      VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, available));

      for (uint32_t index = 0; index < count; ++index) {
        if (available[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
          presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	  break;
	}
      }
    }

    VkSwapchainCreateInfoKHR info {SWAPCHAIN_CREATE_INFO_KHR};
    info.clipped = VK_TRUE;
    info.presentMode = presentMode;
    info.surface = createInfo.windowSurface;
    info.oldSwapchain = createInfo.oldSwapchain;

    info.imageArrayLayers = 1;
    info.imageExtent = output.extent;
    info.minImageCount = minImagesCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    info.preTransform = surfaceCapabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VK_CHECK (vkCreateSwapchainKHR (createInfo.device, &info, nullptr, &output.handle));
  }

  { // Create swapchain images and views
    vkGetSwapchainImagesKHR (createInfo.device, output.handle, &output.imagesCount, nullptr);

    output.images = MALLOC (VkImage, output.imagesCount);
    vkGetSwapchainImagesKHR (createInfo.device, output.handle, &output.imagesCount, output.images);

    output.views = MALLOC (VkImageView, output.imagesCount);

    VkImageSubresourceRange subresourceRange;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    for (uint32_t index = 0; index < output.imagesCount; ++index) {
      VkImageViewCreateInfo info {IMAGE_VIEW_CREATE_INFO};
      info.format = output.format;
      info.image = output.images[index];
      info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      info.subresourceRange = subresourceRange;

      VK_CHECK (vkCreateImageView (createInfo.device, &info, nullptr, &output.views[index]));
    }
  }

  { // Create depth image
    output.depthImage.format = VK_FORMAT_D24_UNORM_S8_UINT;

    VkImageCreateInfo info {IMAGE_CREATE_INFO};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.format = output.depthImage.format;

    info.extent.depth = 1;
    info.extent.width = output.extent.width;
    info.extent.height = output.extent.height;

    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo allocationInfo {};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK (vmaCreateImage (
      createInfo.allocator,
      &info,
      &allocationInfo,
      &output.depthImage.handle,
      &output.depthImage.allocation,
      nullptr
    ));
  }

  VkImageSubresourceRange subresourceRange;
  subresourceRange.levelCount = 1;
  subresourceRange.layerCount = 1;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.baseArrayLayer = 0;
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  // Create depth image view
  VkImageViewCreateInfo info {IMAGE_VIEW_CREATE_INFO};
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = output.depthImage.handle;
  info.format = output.depthImage.format;
  info.subresourceRange = subresourceRange;

  VK_CHECK (vkCreateImageView (createInfo.device, &info, nullptr, &output.depthImage.view));
}

void destroySwapchain (VkDevice device, VmaAllocator allocator, Swapchain& swapchain) {
  vkDestroyImageView (device, swapchain.depthImage.view, nullptr);
  vmaDestroyImage (allocator, swapchain.depthImage.handle, swapchain.depthImage.allocation);

  for (uint32_t index = 0; index < swapchain.imagesCount; ++index)
    vkDestroyImageView (device, swapchain.views[index], nullptr);

  free (swapchain.views);
  free (swapchain.images);

  vkDestroySwapchainKHR (device, swapchain.handle, nullptr);
}

}
