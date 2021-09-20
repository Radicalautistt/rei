#include <stdio.h>

#include "vk.hpp"
#include "common.hpp"
#include "vkinit.hpp"
#include "window.hpp"
#include "vkutils.hpp"
#include "vkcommon.hpp"

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#define FRAMES_COUNT 2u

int main () {
  VulkanContext::init ();

  rei::extra::xcb::Window window;

  { // Create window
    rei::extra::xcb::WindowCreateInfo createInfo;
    createInfo.x = 0;
    createInfo.y = 0;
    createInfo.width = 640;
    createInfo.height = 480;
    createInfo.name = "Rei playground";

    rei::extra::xcb::createWindow (createInfo, window);
  }

  VkInstance instance;
  #ifndef NDEBUG
  VkDebugUtilsMessengerEXT debugMessenger;
  #endif

  VkSurfaceKHR windowSurface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  uint32_t queueFamilyIndex;
  VkQueue graphicsQueue, presentQueue, transferQueue, computeQueue;

  VmaAllocator allocator;

  rei::vkutils::Swapchain swapchain;
  VkRenderPass renderPass;
  VkFramebuffer* framebuffers;
  VkClearValue clearValues[2] {};

  { // Create instance
    VkApplicationInfo applicationInfo {APPLICATION_INFO};
    applicationInfo.apiVersion = VULKAN_VERSION;
    applicationInfo.pEngineName = "Rei";
    applicationInfo.pApplicationName = "Playground";

    VkInstanceCreateInfo createInfo {INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = INSTANCE_EXTENSIONS_COUNT;
    createInfo.ppEnabledExtensionNames = rei::vkcommon::requiredInstanceExtensions;

    #ifndef NDEBUG
    auto debugMessengerInfo = rei::vkinit::debugMessengerInfo ();

    createInfo.enabledLayerCount = 1;
    createInfo.pNext = &debugMessengerInfo;
    createInfo.ppEnabledLayerNames = rei::vkcommon::validationLayers;
    #endif

    VK_CHECK (vkCreateInstance (&createInfo, nullptr, &instance));
    VulkanContext::loadInstance (instance);
  }

  #ifndef NDEBUG
  { // Create debug messenger if debug mode is enabled
    auto createInfo = rei::vkinit::debugMessengerInfo ();
    VK_CHECK (vkCreateDebugUtilsMessengerEXT (instance, &createInfo, nullptr, &debugMessenger));
  }
  #endif

  { // Create window surface
    VkXcbSurfaceCreateInfoKHR createInfo {XCB_SURFACE_CREATE_INFO_KHR};
    createInfo.window = window.handle;
    createInfo.connection = window.connection;

    VK_CHECK (vkCreateXcbSurfaceKHR (instance, &createInfo, nullptr, &windowSurface));
  }

  { // Choose physical device, create logical device
    rei::vkutils::QueueFamilyIndices indices;
    rei::vkutils::choosePhysicalDevice (instance, windowSurface, indices, physicalDevice);

    VkPhysicalDeviceFeatures enabledFeatures {};

    float queuePriority = 1.f;
    // NOTE All required queues have the same index on my device,
    // so I need only one queue create info. Perhaps, I might
    // handle them more appropriately in the future (so that this can work on different devices),
    // but for this will do.
    queueFamilyIndex = indices.graphics;

    VkDeviceQueueCreateInfo queueInfo {DEVICE_QUEUE_CREATE_INFO};
    queueInfo.pQueuePriorities = &queuePriority;
    queueInfo.queueCount = 1;
    queueInfo.queueFamilyIndex = queueFamilyIndex;

    VkDeviceCreateInfo createInfo {DEVICE_CREATE_INFO};
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.pEnabledFeatures = &enabledFeatures;
    createInfo.enabledExtensionCount = DEVICE_EXTENSIONS_COUNT;
    createInfo.ppEnabledExtensionNames = rei::vkcommon::requiredDeviceExtensions;

    VK_CHECK (vkCreateDevice (physicalDevice, &createInfo, nullptr, &device));

    VulkanContext::loadDevice (device);
    vkGetDeviceQueue (device, queueFamilyIndex, 0, &presentQueue);
    vkGetDeviceQueue (device, queueFamilyIndex, 0, &computeQueue);
    vkGetDeviceQueue (device, queueFamilyIndex, 0, &graphicsQueue);
    vkGetDeviceQueue (device, queueFamilyIndex, 0, &transferQueue);
  }

  { // Create allocator
    // Since I'm loading Vulkan functions dynamically,
    // I need to manually provide pointers to them to VMA
    VmaVulkanFunctions vulkanFunctions;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;

    VmaAllocatorCreateInfo createInfo {};
    createInfo.device = device;
    createInfo.instance = instance;
    createInfo.physicalDevice = physicalDevice;
    createInfo.vulkanApiVersion = VULKAN_VERSION;
    createInfo.pVulkanFunctions = &vulkanFunctions;

    VK_CHECK (vmaCreateAllocator (&createInfo, &allocator));
  }

  { // Create swapchain
    rei::vkutils::SwapchainCreateInfo createInfo;
    createInfo.device = device;
    createInfo.allocator = allocator;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    createInfo.windowSurface = windowSurface;
    createInfo.window = &window;
    createInfo.physicalDevice = physicalDevice;

    rei::vkutils::createSwapchain (createInfo, swapchain);
  }

  { // Create main render pass
    clearValues[0].color.float32[0] = 30.f / 255.f;
    clearValues[0].color.float32[1] = 7.f / 255.f;
    clearValues[0].color.float32[2] = 50.f / 255.f;
    clearValues[0].color.float32[3] = 1.f;

    clearValues[1].depthStencil.depth = 1.f;

    VkAttachmentDescription attachments[2];

    // Color attachment
    attachments[0].flags = VULKAN_NO_FLAGS;
    attachments[0].format = swapchain.format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Depth attachment
    attachments[1].flags = VULKAN_NO_FLAGS;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].format = swapchain.depthImage.format;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference;
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference;
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkRenderPassCreateInfo createInfo {RENDER_PASS_CREATE_INFO};
    createInfo.subpassCount = 1;
    createInfo.attachmentCount = 2;
    createInfo.pSubpasses = &subpass;
    createInfo.pAttachments = attachments;

    VK_CHECK (vkCreateRenderPass (device, &createInfo, nullptr, &renderPass));
  }

  { // Create framebuffers
    framebuffers = MALLOC (VkFramebuffer, swapchain.imagesCount);

    VkFramebufferCreateInfo createInfo {FRAMEBUFFER_CREATE_INFO};
    createInfo.layers = 1;
    createInfo.attachmentCount = 2;
    createInfo.renderPass = renderPass;
    createInfo.width = swapchain.extent.width;
    createInfo.height = swapchain.extent.height;

    for (uint32_t index = 0; index < swapchain.imagesCount; ++index) {
      VkImageView attachments[2] {swapchain.views[index], swapchain.depthImage.view};
      createInfo.pAttachments = attachments;

      VK_CHECK (vkCreateFramebuffer (device, &createInfo, nullptr, &framebuffers[index]));
    }
  }

  for (uint32_t index = 0; index < swapchain.imagesCount; ++index)
    vkDestroyFramebuffer (device, framebuffers[index], nullptr);

  free (framebuffers);

  vkDestroyRenderPass (device, renderPass, nullptr);

  rei::vkutils::destroySwapchain (device, allocator, swapchain);

  vmaDestroyAllocator (allocator);
  vkDestroyDevice (device, nullptr);
  vkDestroySurfaceKHR (instance, windowSurface, nullptr);

  #ifndef NDEBUG
  vkDestroyDebugUtilsMessengerEXT (instance, debugMessenger, nullptr);
  #endif

  vkDestroyInstance (instance, nullptr);
  rei::extra::xcb::destroyWindow (window);
  VulkanContext::shutdown ();
}
