#include <stdio.h>

#include "vk.hpp"
#include "common.hpp"
#include "vkinit.hpp"
#include "window.hpp"
#include "vkutils.hpp"
#include "vkcommon.hpp"
#include "gltf_model.hpp"

#include <xcb/xcb.h>
#include <stb/stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#define FRAMES_COUNT 2u

struct Frame {
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;

  VkFence renderFence;
  VkSemaphore renderSemaphore;
  VkSemaphore presentSemaphore;
};

struct Vertex2D {
  float x, y;
  float u, v;
};

constexpr Vertex2D quadVertices[4] {
  {0.5f, 0.5f, 1.f, 1.f},
  {0.5f, -0.5f, 1.f, 0.f},
  {-0.5f, -0.5f, 0.f, 0.f},
  {-0.5f, 0.5f, 0.f, 1.f}
};

constexpr uint16_t quadIndices[6] {0, 1, 3, 1, 2, 3};

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
  VkQueue graphicsQueue, presentQueue, computeQueue;

  VmaAllocator allocator;

  rei::vkutils::Swapchain swapchain;
  VkRenderPass renderPass;
  uint32_t frameIndex = 0;
  Frame frames[FRAMES_COUNT];
  VkFramebuffer* framebuffers;
  VkClearValue clearValues[2] {};

  rei::vkutils::TransferContext transferContext;

  rei::gltf::Model sponza;

  glm::mat4 modelMatrix = glm::translate (glm::mat4 {1.f}, glm::vec3 {0.f, 0.f, 1.f});
  glm::mat4 view = glm::lookAt (glm::vec3 {0.f, 0.f, 3.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});
  glm::mat4 projection = glm::perspective (glm::radians (45.f), 1680.f / 1050.f, 0.1f, 3000.f);
  projection[1][1] *= -1;
  glm::mat4 mvp = projection * view * modelMatrix;

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
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;
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
    vkGetDeviceQueue (device, queueFamilyIndex, 0, &transferContext.queue);
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
    createInfo.window = &window;
    createInfo.allocator = allocator;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    createInfo.windowSurface = windowSurface;
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

  { // Create command pools, buffers, fences and semaphores for each frame in flight
    VkCommandPoolCreateInfo poolInfo {COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandBufferAllocateInfo bufferInfo {COMMAND_BUFFER_ALLOCATE_INFO};
    bufferInfo.commandBufferCount = 1;
    bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkFenceCreateInfo fenceInfo {FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK (vkCreateCommandPool (device, &poolInfo, nullptr, &transferContext.commandPool));

    VkSemaphoreCreateInfo semaphoreInfo {SEMAPHORE_CREATE_INFO};

    for (uint8_t index = 0; index < FRAMES_COUNT; ++index) {
      auto& current = frames[index];
      VK_CHECK (vkCreateCommandPool (device, &poolInfo, nullptr, &current.commandPool));

      bufferInfo.commandPool = current.commandPool;
      VK_CHECK (vkAllocateCommandBuffers (device, &bufferInfo, &current.commandBuffer));
      VK_CHECK (vkCreateFence (device, &fenceInfo, nullptr, &current.renderFence));
      VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, nullptr, &current.renderSemaphore));
      VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, nullptr, &current.presentSemaphore));
    }

    fenceInfo.flags = VULKAN_NO_FLAGS;
    VK_CHECK (vkCreateFence (device, &fenceInfo, nullptr, &transferContext.fence));
  }

  rei::gltf::loadModel (device, allocator, transferContext, "assets/models/sponza-scene/Sponza.gltf", sponza);

  sponza.initDescriptorPool (device);
  sponza.initMaterialDescriptors (device);
  sponza.initPipelines (device, renderPass, swapchain);

  { // Render loop
    bool running = true;
    while (running) {
      xcb_generic_event_t* event = xcb_poll_for_event (window.connection);
      if (event) {
	switch (event->response_type & ~0x80) {
	  case XCB_KEY_PRESS: {
	    auto key = RCAST <xcb_key_press_event_t*> (event);
	    if (key->detail == 9) running = false;
	    break;
          }
	}

	free (event);
      }

      auto& currentFrame = frames[frameIndex % FRAMES_COUNT];

      VK_CHECK (vkWaitForFences (device, 1, &currentFrame.renderFence, VK_TRUE, ~0ull));
      VK_CHECK (vkResetFences (device, 1, &currentFrame.renderFence));

      uint32_t imageIndex = 0;
      VK_CHECK (vkAcquireNextImageKHR (
        device,
	swapchain.handle,
	~0ull,
	currentFrame.presentSemaphore,
	VK_NULL_HANDLE,
	&imageIndex
      ));

      { // Begin writing to command buffer
        VkCommandBufferBeginInfo beginInfo {COMMAND_BUFFER_BEGIN_INFO};
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK (vkBeginCommandBuffer (currentFrame.commandBuffer, &beginInfo));
      }

      { // Begin render pass
        VkRenderPassBeginInfo beginInfo {RENDER_PASS_BEGIN_INFO};
	beginInfo.clearValueCount = 2;
	beginInfo.pClearValues = clearValues;
	beginInfo.renderPass = renderPass;
	beginInfo.renderArea.offset = {0, 0};
	beginInfo.renderArea.extent = swapchain.extent;
	beginInfo.framebuffer = framebuffers[imageIndex];

	vkCmdBeginRenderPass (currentFrame.commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
      }

      sponza.draw (currentFrame.commandBuffer, mvp);

      vkCmdEndRenderPass (currentFrame.commandBuffer);
      VK_CHECK (vkEndCommandBuffer (currentFrame.commandBuffer));

      { // Submit written commands to a queue
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo {SUBMIT_INFO};
	submitInfo.commandBufferCount = 1;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.pCommandBuffers = &currentFrame.commandBuffer;
	submitInfo.pWaitSemaphores = &currentFrame.presentSemaphore;
	submitInfo.pSignalSemaphores = &currentFrame.renderSemaphore;

	VK_CHECK (vkQueueSubmit (graphicsQueue, 1, &submitInfo, currentFrame.renderFence));
      }

      // Present resulting image
      VkPresentInfoKHR presentInfo {PRESENT_INFO_KHR};
      presentInfo.swapchainCount = 1;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pImageIndices = &imageIndex;
      presentInfo.pSwapchains = &swapchain.handle;
      presentInfo.pWaitSemaphores = &currentFrame.renderSemaphore;

      VK_CHECK (vkQueuePresentKHR (presentQueue, &presentInfo));
      ++frameIndex;
    }
  }

  // Wait for gpu to finish rendering of the last frame
  vkDeviceWaitIdle (device);

  rei::gltf::destroyModel (device, allocator, sponza);

  vkDestroyFence (device, transferContext.fence, nullptr);
  vkDestroyCommandPool (device, transferContext.commandPool, nullptr);

  for (uint8_t index = 0; index < FRAMES_COUNT; ++index) {
    auto& current = frames[index];
    vkDestroySemaphore (device, current.presentSemaphore, nullptr);
    vkDestroySemaphore (device, current.renderSemaphore, nullptr);
    vkDestroyFence (device, current.renderFence, nullptr);
    vkDestroyCommandPool (device, current.commandPool, nullptr);
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
