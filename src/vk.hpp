#ifndef VK_HPP
#define VK_HPP

// Tell vulkan.h not to include function prototypes,
// so we can generate and load them ourselves.
#ifndef VK_NO_PROTOTYPES
#  define VK_NO_PROTOTYPES
#endif

#ifndef VK_USE_PLATFORM_XCB_KHR
#  define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

// Used to load global and instance level functions.
// After that vkGetDeviceProcAddr is being used instead.
// It loads functions directly from VkDevice to reduce function call overhead.
extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

#define VK_GLOBAL_FUNCTIONS                     \
  X (vkCreateInstance)                          \
  X (vkEnumerateInstanceLayerProperties)        \
  X (vkEnumerateInstanceExtensionProperties)    \

#define VK_INSTANCE_FUNCTIONS                   \
  X (vkDestroyInstance)                         \
                                                \
  X (vkCreateDebugUtilsMessengerEXT)            \
  X (vkDestroyDebugUtilsMessengerEXT)           \
                                                \
  X (vkCreateXcbSurfaceKHR)                     \
  X (vkDestroySurfaceKHR)                       \
                                                \
  X (vkEnumeratePhysicalDevices)                \
  X (vkEnumerateDeviceExtensionProperties)      \
                                                \
  X (vkGetPhysicalDeviceFeatures)               \
  X (vkGetPhysicalDeviceProperties)             \
  X (vkGetPhysicalDeviceMemoryProperties)       \
  X (vkGetPhysicalDeviceSurfaceSupportKHR)      \
  X (vkGetPhysicalDeviceSurfaceFormatsKHR)      \
  X (vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
  X (vkGetPhysicalDeviceQueueFamilyProperties)  \
  X (vkGetPhysicalDeviceSurfacePresentModesKHR) \
                                                \
  X (vkCreateDevice)                            \
  X (vkDestroyDevice)                           \
  X (vkGetDeviceProcAddr)                       \

#define VK_DEVICE_FUNCTIONS                     \
  X (vkDeviceWaitIdle)                          \
                                                \
  X (vkGetDeviceQueue)                          \
  X (vkQueueSubmit)                             \
  X (vkQueueWaitIdle)                           \
  X (vkQueuePresentKHR)                         \
                                                \
  X (vkCreateSwapchainKHR)                      \
  X (vkDestroySwapchainKHR)                     \
  X (vkGetSwapchainImagesKHR)                   \
  X (vkAcquireNextImageKHR)                     \
                                                \
  X (vkCreateImage)                             \
  X (vkDestroyImage)                            \
  X (vkCreateImageView)                         \
  X (vkDestroyImageView)                        \
                                                \
  X (vkMapMemory)                               \
  X (vkFreeMemory)                              \
  X (vkUnmapMemory)                             \
  X (vkCreateBuffer)                            \
  X (vkDestroyBuffer)                           \
  X (vkCmdCopyBuffer)                           \
  X (vkCmdCopyBufferToImage)                    \
  X (vkAllocateMemory)                          \
  X (vkBindImageMemory)                         \
  X (vkBindBufferMemory)                        \
  X (vkFlushMappedMemoryRanges)                 \
  X (vkGetImageMemoryRequirements)              \
  X (vkGetBufferMemoryRequirements)             \
  X (vkInvalidateMappedMemoryRanges)            \
                                                \
  X (vkCreateRenderPass)                        \
  X (vkDestroyRenderPass)                       \
  X (vkCmdBeginRenderPass)                      \
  X (vkCmdEndRenderPass)                        \
  X (vkCreateFramebuffer)                       \
  X (vkDestroyFramebuffer)                      \
                                                \
  X (vkCreateCommandPool)                       \
  X (vkDestroyCommandPool)                      \
  X (vkAllocateCommandBuffers)                  \
  X (vkResetCommandPool)                        \
  X (vkResetCommandBuffer)                      \
  X (vkBeginCommandBuffer)                      \
  X (vkEndCommandBuffer)                        \
                                                \
  X (vkCreateFence)                             \
  X (vkDestroyFence)                            \
  X (vkResetFences)                             \
  X (vkWaitForFences)                           \
  X (vkCreateSemaphore)                         \
  X (vkDestroySemaphore)                        \
                                                \
  X (vkCreateShaderModule)                      \
  X (vkDestroyShaderModule)                     \
  X (vkCreateGraphicsPipelines)                 \
  X (vkDestroyPipeline)                         \
  X (vkCmdBindPipeline)                         \
  X (vkCmdPipelineBarrier)                      \
  X (vkCreatePipelineCache)                     \
  X (vkDestroyPipelineCache)                    \
  X (vkCreatePipelineLayout)                    \
  X (vkDestroyPipelineLayout)                   \
  X (vkCmdSetViewport)                          \
  X (vkCmdSetScissor)                           \
                                                \
  X (vkCreateDescriptorPool)                    \
  X (vkDestroyDescriptorPool)                   \
  X (vkCreateDescriptorSetLayout)               \
  X (vkDestroyDescriptorSetLayout)              \
  X (vkAllocateDescriptorSets)                  \
  X (vkUpdateDescriptorSets)                    \
  X (vkCmdBindDescriptorSets)                   \
  X (vkCreateSampler)                           \
  X (vkDestroySampler)                          \
  X (vkCmdPushConstants)                        \
                                                \
  X (vkCmdBindVertexBuffers)                    \
  X (vkCmdBindIndexBuffer)                      \
                                                \
  X (vkCmdDraw)                                 \
  X (vkCmdDrawIndexed)                          \
  X (vkCmdDrawIndirect)                         \
  X (vkCmdDrawIndexedIndirect)                  \

#ifndef X
// en.wikipedia.org/wiki/X_Macro
#  define X(name) extern PFN_##name name;
     VK_GLOBAL_FUNCTIONS
     VK_INSTANCE_FUNCTIONS
     VK_DEVICE_FUNCTIONS
#  undef X
#endif

struct VulkanContext {
  static void init ();
  static void shutdown ();
  static void loadInstance (VkInstance instance);
  static void loadDevice (VkDevice device);

  static void* library;
};

// NOTE just in case anyone ever sees this: I haven't written this by hand
#define SUBMIT_INFO VK_STRUCTURE_TYPE_SUBMIT_INFO
#define MEMORY_BARRIER VK_STRUCTURE_TYPE_MEMORY_BARRIER
#define APPLICATION_INFO VK_STRUCTURE_TYPE_APPLICATION_INFO
#define BIND_SPARSE_INFO VK_STRUCTURE_TYPE_BIND_SPARSE_INFO
#define PRESENT_INFO_KHR VK_STRUCTURE_TYPE_PRESENT_INFO_KHR
#define EVENT_CREATE_INFO VK_STRUCTURE_TYPE_EVENT_CREATE_INFO
#define FENCE_CREATE_INFO VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
#define IMAGE_CREATE_INFO VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
#define BUFFER_CREATE_INFO VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO
#define DEVICE_CREATE_INFO VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
#define COPY_DESCRIPTOR_SET VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET
#define MAPPED_MEMORY_RANGE VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE
#define SAMPLER_CREATE_INFO VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO
#define IMAGE_MEMORY_BARRIER VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER
#define INSTANCE_CREATE_INFO VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
#define MEMORY_ALLOCATE_INFO VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
#define WRITE_DESCRIPTOR_SET VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET
#define BUFFER_MEMORY_BARRIER VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER
#define SEMAPHORE_CREATE_INFO VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
#define IMAGE_VIEW_CREATE_INFO VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
#define QUERY_POOL_CREATE_INFO VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO
#define RENDER_PASS_BEGIN_INFO VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO
#define BUFFER_VIEW_CREATE_INFO VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO
#define FRAMEBUFFER_CREATE_INFO VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO
#define RENDER_PASS_CREATE_INFO VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO
#define COMMAND_POOL_CREATE_INFO VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
#define DEVICE_QUEUE_CREATE_INFO VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
#define SWAPCHAIN_CREATE_INFO_KHR VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR
#define COMMAND_BUFFER_BEGIN_INFO VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
#define LOADER_DEVICE_CREATE_INFO VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO
#define SHADER_MODULE_CREATE_INFO VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO
#define PIPELINE_CACHE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
#define DESCRIPTOR_POOL_CREATE_INFO VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO
#define LOADER_INSTANCE_CREATE_INFO VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO
#define XCB_SURFACE_CREATE_INFO_KHR VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR
#define PIPELINE_LAYOUT_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
#define COMMAND_BUFFER_ALLOCATE_INFO VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
#define COMPUTE_PIPELINE_CREATE_INFO VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO
#define DESCRIPTOR_SET_ALLOCATE_INFO VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO
#define GRAPHICS_PIPELINE_CREATE_INFO VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
#define COMMAND_BUFFER_INHERITANCE_INFO VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO
#define DESCRIPTOR_SET_LAYOUT_CREATE_INFO VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
#define PIPELINE_SHADER_STAGE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
#define PIPELINE_DYNAMIC_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
#define PIPELINE_VIEWPORT_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
#define DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT
#define PIPELINE_COLOR_BLEND_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
#define PIPELINE_MULTISAMPLE_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
#define PIPELINE_TESSELLATION_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO
#define PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
#define PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
#define PIPELINE_RASTERIZATION_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
#define PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO

#endif /* VK_HPP */
