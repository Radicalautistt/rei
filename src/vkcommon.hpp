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

#ifndef VULKAN_TEXTURE_FORMAT
#  define VULKAN_TEXTURE_FORMAT VK_FORMAT_R8G8B8A8_SRGB
#endif

#ifndef VULKAN_DEPTH_FORMAT
#  define VULKAN_DEPTH_FORMAT VK_FORMAT_X8_D24_UNORM_PACK32
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

#ifndef VKC_GET_NEXT_IMAGE
#  define VKC_GET_NEXT_IMAGE(device, swapchain, semaphore, out) \
    VK_CHECK (vkAcquireNextImageKHR (                           \
      device,                                                   \
      swapchain.handle,                                         \
      ~0ull,                                                    \
      currentFrame->presentSemaphore,                           \
      VK_NULL_HANDLE,                                           \
      &imageIndex                                               \
    ))
#endif

#ifndef VKC_BIND_DESCRIPTORS
#  define VKC_BIND_DESCRIPTORS(cmdBuffer, layout, count, descriptors) \
     vkCmdBindDescriptorSets (cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, count, descriptors, 0, nullptr)
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
