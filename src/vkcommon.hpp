#ifndef VKCOMMON_HPP
#define VKCOMMON_HPP

#include <stdlib.h>

#include "vk.hpp"
#include "common.hpp"

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
#    define VK_CHECK(call) do {                                                             \
       VkResult error = call;                                                               \
       if (error) {                                                                         \
         LOG_ERROR (                                                                        \
	   "%s:%d Vulkan error " ANSI_YELLOW "%s" ANSI_RED " occured in " ANSI_YELLOW "%s", \
	   __FILE__,                                                                        \
	   __LINE__,                                                                        \
	   rei::vkc::getError (error),                                                      \
	   __FUNCTION__                                                                     \
	 );                                                                                 \
                                                                                            \
         abort ();                                                                          \
       }                                                                                    \
     } while (0)
#  endif
#endif

namespace rei::vkc {

constexpr const char* requiredDeviceExtensions[] {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

[[nodiscard]] const char* getError (VkResult) noexcept;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback (
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
  void* userData
);

}

#endif /* VKCOMMON_HPP */
