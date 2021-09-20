#ifndef VKUTILS_HPP
#define VKUTILS_HPP

#include "vkcommon.hpp"

namespace rei::vkutils {

struct QueueFamilyIndices {
  bool haveGraphics, havePresent, haveTransfer, haveCompute;
  uint32_t graphics, present, transfer, compute;
};

void findQueueFamilyIndices (VkPhysicalDevice physicalDevice, VkSurfaceKHR targetSurface, QueueFamilyIndices& output);

void choosePhysicalDevice (
  VkInstance instance,
  VkSurfaceKHR targetSurface,
  QueueFamilyIndices& outputIndices,
  VkPhysicalDevice& output
);

}

#endif /* VKUTILS_HPP */
