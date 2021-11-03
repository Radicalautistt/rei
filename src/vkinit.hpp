#ifndef VKINIT_HPP
#define VKINIT_HPP

#include "vk.hpp"
#include "vkcommon.hpp"

namespace rei::vki {

[[nodiscard]] constexpr inline auto debugMessengerInfo () noexcept {
  VkDebugUtilsMessengerCreateInfoEXT createInfo {DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  createInfo.pfnUserCallback = vkc::debugCallback;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
  createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

  return createInfo;
}

}

#endif /* VKINIT_HPP */
