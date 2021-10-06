#include <stdio.h>

#include "vk.hpp"
#include "utils.hpp"
#include "imgui.hpp"
#include "common.hpp"
#include "camera.hpp"
#include "vkinit.hpp"
#include "window.hpp"
#include "vkutils.hpp"
#include "vkcommon.hpp"
#include "gltf_model.hpp"

#include <xcb/xcb.h>
#include <imgui/imgui.h>
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
  rei::extra::xcb::Window window;
  rei::Camera camera {{0.f, 1.f, 0.f}, {0.f, 100.f, -10.f}, -90.f, 0.f};

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

  VkPipelineCache pipelineCache;
  VkDescriptorPool mainDescriptorPool;

  rei::extra::imgui::Context imguiContext;

  rei::vkutils::TransferContext transferContext;

  rei::gltf::Model sponza;

  rei::math::Matrix4 modelMatrix {1.f};
  rei::math::Matrix4::translate (modelMatrix, {0.f, 0.f, 1.f});

  rei::utils::Timer::init ();
  VulkanContext::init ();

  { // Create window
    rei::extra::xcb::WindowCreateInfo createInfo;
    createInfo.x = 0;
    createInfo.y = 0;
    createInfo.width = 640;
    createInfo.height = 480;
    createInfo.name = "Rei playground";

    rei::extra::xcb::createWindow (createInfo, window);
  }

  { // Create instance
    VkApplicationInfo applicationInfo {APPLICATION_INFO};
    applicationInfo.pEngineName = "Rei";
    applicationInfo.apiVersion = VULKAN_VERSION;
    applicationInfo.pApplicationName = "Playground";

    VkInstanceCreateInfo createInfo {INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.ppEnabledExtensionNames = rei::vkcommon::requiredInstanceExtensions;
    createInfo.enabledExtensionCount = ARRAY_SIZE (rei::vkcommon::requiredInstanceExtensions);

    #ifndef NDEBUG
    auto debugMessengerInfo = rei::vkinit::debugMessengerInfo ();

    createInfo.pNext = &debugMessengerInfo;
    createInfo.ppEnabledLayerNames = rei::vkcommon::validationLayers;
    createInfo.enabledLayerCount = ARRAY_SIZE (rei::vkcommon::validationLayers);
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

    {
      VkFormatProperties formatProperties;
      vkGetPhysicalDeviceFormatProperties (physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);

      VkFormatFeatureFlags requiredFlags = VK_FORMAT_FEATURE_BLIT_SRC_BIT;
      requiredFlags |= VK_FORMAT_FEATURE_BLIT_DST_BIT;

      assert (formatProperties.optimalTilingFeatures & requiredFlags);
    }

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
    createInfo.ppEnabledExtensionNames = rei::vkcommon::requiredDeviceExtensions;
    createInfo.enabledExtensionCount = ARRAY_SIZE (rei::vkcommon::requiredDeviceExtensions);

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

  { // Create main descriptor pool
    VkDescriptorPoolSize sizes[1];
    sizes[0].descriptorCount = 1;
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    VkDescriptorPoolCreateInfo createInfo {DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.maxSets = 1;
    createInfo.pPoolSizes = sizes;
    createInfo.poolSizeCount = ARRAY_SIZE (sizes);
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK (vkCreateDescriptorPool (device, &createInfo, nullptr, &mainDescriptorPool));
  }

  { // Create/load pipeline cache
    VkPipelineCacheCreateInfo createInfo {PIPELINE_CACHE_CREATE_INFO};
    rei::utils::File cacheFile;
    auto result = rei::utils::readFile ("pipeline.cache", true, cacheFile);
    switch (result) {
      case rei::Result::Success: {
	puts ("Reusing pipeline cache...");
        createInfo.initialDataSize = cacheFile.size;
	createInfo.pInitialData = cacheFile.contents;
      } break;
      default: {
	puts ("Failed to obtain pipeline cache data, creating one from scratch");
      } break;
    }

    VK_CHECK (vkCreatePipelineCache (device, &createInfo, nullptr, &pipelineCache));
    if (cacheFile.contents) free (cacheFile.contents);
  }

  { // Create imgui context
    rei::extra::imgui::ContextCreateInfo createInfo;
    createInfo.device = device;
    createInfo.window = &window;
    createInfo.allocator = allocator;
    createInfo.renderPass = renderPass;
    createInfo.pipelineCache = pipelineCache;
    createInfo.transferContext = &transferContext;
    createInfo.descriptorPool = mainDescriptorPool;

    rei::extra::imgui::create (createInfo, imguiContext);
  }

  rei::gltf::loadModel (device, allocator, transferContext, "assets/models/sponza-scene/Sponza.gltf", sponza);

  sponza.initDescriptorPool (device);
  sponza.initMaterialDescriptors (device);
  sponza.initPipelines (device, renderPass, pipelineCache, swapchain);

  { // Render loop
    bool running = true;
    float lastTime = 0.f;
    float deltaTime = 0.f;

    xcb_generic_event_t* event = nullptr;

    while (running) {
      camera.firstMouse = true;
      float currentTime = rei::utils::Timer::getCurrentTime ();
      deltaTime = currentTime - lastTime;
      lastTime = currentTime;
      ImGui::GetIO().DeltaTime = deltaTime;

      while ((event = xcb_poll_for_event (window.connection))) {
	switch (event->response_type & ~0x80) {
	  case XCB_KEY_PRESS: {
	    auto key = RCAST <xcb_key_press_event_t*> (event);
	    if (key->detail == 9) running = false;
	    if (key->detail == 38) camera.move (rei::Camera::Direction::Left, deltaTime);
	    if (key->detail == 40) camera.move (rei::Camera::Direction::Right, deltaTime);
	    if (key->detail == 25) camera.move (rei::Camera::Direction::Forward, deltaTime);
	    if (key->detail == 39) camera.move (rei::Camera::Direction::Backward, deltaTime);
          } break;

	  case XCB_MOTION_NOTIFY: {
	    auto data = RCAST <xcb_motion_notify_event_t*> (event);
	    camera.handleMouseMovement (SCAST <float> (data->event_x), SCAST <float> (data->event_y));
	  } break;

	  default: break;
	}

	imguiContext.handleEvents (event);

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
	beginInfo.renderPass = renderPass;
	beginInfo.renderArea.offset = {0, 0};
	beginInfo.pClearValues = clearValues;
	beginInfo.renderArea.extent = swapchain.extent;
	beginInfo.framebuffer = framebuffers[imageIndex];
	beginInfo.clearValueCount = ARRAY_SIZE (clearValues);

	vkCmdBeginRenderPass (currentFrame.commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
      }

      rei::math::Matrix4 viewMatrix;
      rei::math::lookAt (camera.position, camera.position + camera.front, camera.up, viewMatrix);

      rei::math::Matrix4 mvp = camera.projection * viewMatrix * modelMatrix;
      sponza.draw (currentFrame.commandBuffer, mvp);

      imguiContext.newFrame ();
      ImGui::ShowMetricsWindow ();
      ImGui::Render ();
      const ImDrawData* drawData = ImGui::GetDrawData ();
      imguiContext.updateBuffers (drawData);
      imguiContext.renderDrawData (drawData, currentFrame.commandBuffer);

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
      // Wait for commands sent to the current frame's command buffer to finish,
      // so that we can safely destroy/update dynamic buffers in the next frame.
      vkQueueWaitIdle (graphicsQueue);
    }
  }

  // Wait for gpu to finish rendering of the last frame
  vkDeviceWaitIdle (device);

  rei::gltf::destroyModel (device, allocator, sponza);
  rei::extra::imgui::destroy (device, allocator, imguiContext);

  { // Save pipeline cache for future reuse
    size_t size = 0;
    VK_CHECK (vkGetPipelineCacheData (device, pipelineCache, &size, nullptr));

    uint8_t* data = MALLOC (uint8_t, size);
    VK_CHECK (vkGetPipelineCacheData (device, pipelineCache, &size, data));

    FILE* cacheFile = fopen ("pipeline.cache", "wb");
    fwrite (data, 1, size, cacheFile);
    fclose (cacheFile);

    free (data);
    vkDestroyPipelineCache (device, pipelineCache, nullptr);
  }

  vkDestroyDescriptorPool (device, mainDescriptorPool, nullptr);

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
