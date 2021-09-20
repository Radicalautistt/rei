#include <string.h>
#include <assert.h>

#include "common.hpp"
#include "vkutils.hpp"

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

}
