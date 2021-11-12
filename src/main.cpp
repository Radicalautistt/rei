#include <stdio.h>

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

struct Frame {
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;

  VkFence renderFence;
  VkSemaphore renderSemaphore;
  VkSemaphore presentSemaphore;
};

int main () {
  rei::xcb::Window window;
  rei::Camera camera {{0.f, 1.f, 0.f}, {0.f, 1.f, 1.f}, -90.f, 0.f};

  VkInstance instance;
  #ifndef NDEBUG
  VkDebugUtilsMessengerEXT debugMessenger;
  #endif

  VkSurfaceKHR windowSurface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  Uint32 queueFamilyIndex;
  VkQueue graphicsQueue, presentQueue, computeQueue;

  VmaAllocator allocator;

  rei::vku::Swapchain swapchain;
  VkRenderPass renderPass;
  Uint32 frameIndex = 0;
  Frame frames[FRAMES_COUNT];
  VkFramebuffer* framebuffers;
  VkClearValue clearValues[2];

  VkPipelineCache pipelineCache;
  VkDescriptorPool mainDescriptorPool;

  rei::imgui::Context imguiContext;

  rei::vku::TransferContext transferContext;

  rei::gltf::Model sponza;

  rei::utils::Timer::init ();
  rei::VulkanContext::init ();

  { // Create window
    rei::xcb::WindowCreateInfo createInfo;
    createInfo.x = 0;
    createInfo.y = 0;
    createInfo.width = 640;
    createInfo.height = 480;
    createInfo.name = "Rei playground";

    rei::xcb::createWindow (&createInfo, &window);
  }

  { // Create instance
    const char* requiredExtensions[] {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_XCB_SURFACE_EXTENSION_NAME,
      #ifndef NDEBUG
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME
      #endif
    };

    VkApplicationInfo applicationInfo {APPLICATION_INFO};
    applicationInfo.pEngineName = "Rei";
    applicationInfo.apiVersion = VULKAN_VERSION;
    applicationInfo.pApplicationName = "Playground";

    VkInstanceCreateInfo createInfo {INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.ppEnabledExtensionNames = requiredExtensions;
    createInfo.enabledExtensionCount = ARRAY_SIZE (requiredExtensions);

    #ifndef NDEBUG
    auto debugMessengerInfo = rei::vki::debugMessengerInfo ();
    const char* validationLayers[] {"VK_LAYER_KHRONOS_validation"};

    createInfo.enabledLayerCount = 1;
    createInfo.pNext = &debugMessengerInfo;
    createInfo.ppEnabledLayerNames = validationLayers;
    #endif

    VK_CHECK (vkCreateInstance (&createInfo, nullptr, &instance));
    rei::VulkanContext::loadInstance (instance);
  }

  #ifndef NDEBUG
  { // Create debug messenger if debug mode is enabled
    auto createInfo = rei::vki::debugMessengerInfo ();
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
    rei::vku::QueueFamilyIndices indices;
    rei::vku::choosePhysicalDevice (instance, windowSurface, &indices, &physicalDevice);

    {
      VkFormatProperties formatProperties;
      vkGetPhysicalDeviceFormatProperties (physicalDevice, VK_FORMAT_R8G8B8A8_SRGB, &formatProperties);

      VkFormatFeatureFlags requiredFlags = VK_FORMAT_FEATURE_BLIT_SRC_BIT;
      requiredFlags |= VK_FORMAT_FEATURE_BLIT_DST_BIT;

      REI_ASSERT (formatProperties.optimalTilingFeatures & requiredFlags);
    }

    VkPhysicalDeviceFeatures enabledFeatures {};

    Float32 queuePriority = 1.f;
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
    createInfo.ppEnabledExtensionNames = rei::vkc::requiredDeviceExtensions;
    createInfo.enabledExtensionCount = ARRAY_SIZE (rei::vkc::requiredDeviceExtensions);

    VK_CHECK (vkCreateDevice (physicalDevice, &createInfo, nullptr, &device));

    rei::VulkanContext::loadDevice (device);
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
    rei::vku::SwapchainCreateInfo createInfo;
    createInfo.device = device;
    createInfo.window = &window;
    createInfo.allocator = allocator;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    createInfo.windowSurface = windowSurface;
    createInfo.physicalDevice = physicalDevice;

    rei::vku::createSwapchain (&createInfo, &swapchain);
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

    for (Uint32 index = 0; index < swapchain.imagesCount; ++index) {
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

    for (Uint8 index = 0; index < FRAMES_COUNT; ++index) {
      auto current = &frames[index];
      VK_CHECK (vkCreateCommandPool (device, &poolInfo, nullptr, &current->commandPool));

      bufferInfo.commandPool = current->commandPool;
      VK_CHECK (vkAllocateCommandBuffers (device, &bufferInfo, &current->commandBuffer));
      VK_CHECK (vkCreateFence (device, &fenceInfo, nullptr, &current->renderFence));
      VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, nullptr, &current->renderSemaphore));
      VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, nullptr, &current->presentSemaphore));
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
    auto result = rei::utils::readFile ("pipeline.cache", True, &cacheFile);

    switch (result) {
      case rei::Result::Success: {
	LOGS_INFO ("Reusing pipeline cache...");
        createInfo.initialDataSize = cacheFile.size;
	createInfo.pInitialData = cacheFile.contents;
      } break;

      default: {
	LOGS_INFO ("Failed to obtain pipeline cache data, creating one from scratch");
      } break;
    }

    VK_CHECK (vkCreatePipelineCache (device, &createInfo, nullptr, &pipelineCache));
    if (cacheFile.contents) free (cacheFile.contents);
  }

  { // Create imgui context
    rei::imgui::ContextCreateInfo createInfo;
    createInfo.device = device;
    createInfo.window = &window;
    createInfo.allocator = allocator;
    createInfo.renderPass = renderPass;
    createInfo.pipelineCache = pipelineCache;
    createInfo.transferContext = &transferContext;
    createInfo.descriptorPool = mainDescriptorPool;

    rei::imgui::create (&createInfo, &imguiContext);
  }

  rei::gltf::load (device, allocator, &transferContext, "assets/models/sponza-scene/Sponza.gltf", &sponza);
  sponza.initDescriptorPool (device);
  sponza.initMaterialDescriptors (device);
  sponza.initPipelines (device, renderPass, pipelineCache, &swapchain);

  { // Render loop
    Bool32 running = True;
    Float32 lastTime = 0.f;
    Float32 deltaTime = 0.f;

    xcb_generic_event_t* event = nullptr;

    while (running) {
      camera.firstMouse = True;
      Float32 currentTime = rei::utils::Timer::getCurrentTime ();
      deltaTime = currentTime - lastTime;
      lastTime = currentTime;

      // NOTE IMGUI asserts that deltaTime > 0.f, hence this check.
      Float32 imguiDeltaTime[2] {1.f / 60.f, deltaTime};
      ImGui::GetIO().DeltaTime = imguiDeltaTime[deltaTime > 0.f];

      while ((event = xcb_poll_for_event (window.connection))) {
	switch (event->response_type & ~0x80) {
	  case XCB_KEY_PRESS: {
	    const auto key = (const xcb_key_press_event_t*) event;
	    switch (key->detail) {
	      case KEY_ESCAPE: running = False; break;
	      case KEY_A: camera.move (rei::Camera::Direction::Left, deltaTime); break;
	      case KEY_D: camera.move (rei::Camera::Direction::Right, deltaTime); break;
	      case KEY_W: camera.move (rei::Camera::Direction::Forward, deltaTime); break;
	      case KEY_S: camera.move (rei::Camera::Direction::Backward, deltaTime); break;
	      default: break;
	    }
          } break;

	  case XCB_MOTION_NOTIFY: {
	    const auto data = (const xcb_motion_notify_event_t*) event;
	    camera.handleMouseMovement ((Float32) data->event_x, (Float32) data->event_y);
	  } break;

	  default: break;
	}

	imguiContext.handleEvents (event);

	free (event);
      }

      frameIndex %= FRAMES_COUNT;
      const auto currentFrame = &frames[frameIndex];

      VK_CHECK (vkWaitForFences (device, 1, &currentFrame->renderFence, VK_TRUE, ~0ull));
      VK_CHECK (vkResetFences (device, 1, &currentFrame->renderFence));

      Uint32 imageIndex = 0;
      VK_CHECK (vkAcquireNextImageKHR (
        device,
	swapchain.handle,
	~0ull,
	currentFrame->presentSemaphore,
	VK_NULL_HANDLE,
	&imageIndex
      ));

      { // Begin writing to command buffer
        VkCommandBufferBeginInfo beginInfo {COMMAND_BUFFER_BEGIN_INFO};
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK (vkBeginCommandBuffer (currentFrame->commandBuffer, &beginInfo));
      }

      { // Begin render pass
        VkRenderPassBeginInfo beginInfo {RENDER_PASS_BEGIN_INFO};
	beginInfo.renderPass = renderPass;
	beginInfo.renderArea.offset = {0, 0};
	beginInfo.pClearValues = clearValues;
	beginInfo.renderArea.extent = swapchain.extent;
	beginInfo.framebuffer = framebuffers[imageIndex];
	beginInfo.clearValueCount = ARRAY_SIZE (clearValues);

	vkCmdBeginRenderPass (currentFrame->commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
      }

      {
        rei::math::Matrix4 viewMatrix;
	auto center = camera.position + camera.front;
        rei::math::lookAt (&camera.position, &center, &camera.up, &viewMatrix);

        rei::math::Matrix4 viewProjection = camera.projection * viewMatrix;
        sponza.draw (currentFrame->commandBuffer, &viewProjection);

        imguiContext.newFrame ();
	rei::imgui::showDebugWindow (&camera.speed, allocator);
        ImGui::Render ();
        const ImDrawData* drawData = ImGui::GetDrawData ();
        imguiContext.updateBuffers (frameIndex, drawData);
        imguiContext.renderDrawData (currentFrame->commandBuffer, frameIndex, drawData);
      }

      vkCmdEndRenderPass (currentFrame->commandBuffer);
      VK_CHECK (vkEndCommandBuffer (currentFrame->commandBuffer));

      { // Submit written commands to a queue
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo {SUBMIT_INFO};
	submitInfo.commandBufferCount = 1;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.pCommandBuffers = &currentFrame->commandBuffer;
	submitInfo.pWaitSemaphores = &currentFrame->presentSemaphore;
	submitInfo.pSignalSemaphores = &currentFrame->renderSemaphore;

	VK_CHECK (vkQueueSubmit (graphicsQueue, 1, &submitInfo, currentFrame->renderFence));
      }

      // Present resulting image
      VkPresentInfoKHR presentInfo {PRESENT_INFO_KHR};
      presentInfo.swapchainCount = 1;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pImageIndices = &imageIndex;
      presentInfo.pSwapchains = &swapchain.handle;
      presentInfo.pWaitSemaphores = &currentFrame->renderSemaphore;

      VK_CHECK (vkQueuePresentKHR (presentQueue, &presentInfo));
      ++frameIndex;
    }
  }

  // Wait for gpu to finish rendering of the last frame
  vkDeviceWaitIdle (device);

  rei::gltf::destroy (device, allocator, &sponza);
  rei::imgui::destroy (&imguiContext);

  { // Save pipeline cache for future reuse
    size_t size = 0;
    VK_CHECK (vkGetPipelineCacheData (device, pipelineCache, &size, nullptr));

    Uint8* data = MALLOC (Uint8, size);
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

  for (Uint8 index = 0; index < FRAMES_COUNT; ++index) {
    auto current = &frames[index];
    vkDestroySemaphore (device, current->presentSemaphore, nullptr);
    vkDestroySemaphore (device, current->renderSemaphore, nullptr);
    vkDestroyFence (device, current->renderFence, nullptr);
    vkDestroyCommandPool (device, current->commandPool, nullptr);
  }

  for (Uint32 index = 0; index < swapchain.imagesCount; ++index)
    vkDestroyFramebuffer (device, framebuffers[index], nullptr);

  free (framebuffers);

  vkDestroyRenderPass (device, renderPass, nullptr);

  rei::vku::destroySwapchain (device, allocator, &swapchain);

  vmaDestroyAllocator (allocator);
  vkDestroyDevice (device, nullptr);
  vkDestroySurfaceKHR (instance, windowSurface, nullptr);

  #ifndef NDEBUG
  vkDestroyDebugUtilsMessengerEXT (instance, debugMessenger, nullptr);
  #endif

  vkDestroyInstance (instance, nullptr);
  rei::xcb::destroyWindow (&window);
  rei::VulkanContext::shutdown ();
}
