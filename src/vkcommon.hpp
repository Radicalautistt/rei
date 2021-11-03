#ifndef VKCOMMON_HPP
#define VKCOMMON_HPP

#include <stdio.h>
#include <stdlib.h>

#include "vk.hpp"

#ifndef VULKAN_VERSION
#  define VULKAN_VERSION VK_API_VERSION_1_0
#endif

#ifndef VULKAN_NO_FLAGS
#  define VULKAN_NO_FLAGS 0u
#endif

#ifdef NDEBUG
#  ifndef VK_CHECK
#    define VK_CHECK(call) call
#  endif
#else
#  ifndef VK_CHECK
#    define VK_CHECK(call) do {                     \
       if (VkResult error = call; error) {          \
         fprintf (                                  \
	   stderr,                                  \
	   "%s:%d Vulkan error %s occured in %s\n", \
	   __FILE__,                                \
	   __LINE__,                                \
	   rei::vkc::getError (error),              \
	   __FUNCTION__                             \
	 );                                         \
                                                    \
         abort ();                                  \
       }                                            \
     } while (false)
#  endif
#endif

namespace rei::vkc {

constexpr const char* requiredDeviceExtensions[] {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

[[nodiscard]] const char* getError (VkResult) noexcept;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback (
  [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
  [[maybe_unused]] void* userData
);

}

#endif /* VKCOMMON_HPP */
