#include <string.h>
#include <assert.h>

#include "utils.hpp"
#include "common.hpp"
#include "window.hpp"
#include "vkutils.hpp"

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::vkutils {

void findQueueFamilyIndices (VkPhysicalDevice physicalDevice, VkSurfaceKHR targetSurface, QueueFamilyIndices& output) {
  output.haveGraphics = output.havePresent = output.haveTransfer = output.haveCompute = false;

  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties (physicalDevice, &count, nullptr);

  auto available = ALLOCA (VkQueueFamilyProperties, count);
  vkGetPhysicalDeviceQueueFamilyProperties (physicalDevice, &count, available);

  for (uint32_t index = 0; index < count; ++index) {
    auto& current = available[index];

    if (current.queueCount) {
      VkBool32 supportsPresentation = VK_FALSE;
      VK_CHECK (vkGetPhysicalDeviceSurfaceSupportKHR (physicalDevice, index, targetSurface, &supportsPresentation));

      if (supportsPresentation) {
	output.havePresent = true;
	output.present = index;
      }

      if (current.queueFlags & VK_QUEUE_COMPUTE_BIT) {
	output.haveCompute = true;
        output.compute = index;
      }

      if (current.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
	output.haveGraphics = true;
	output.graphics = index;
      }

      if (current.queueFlags & VK_QUEUE_TRANSFER_BIT) {
        output.haveTransfer = true;
	output.transfer = index;
      }

      if (output.haveGraphics && output.havePresent && output.haveTransfer && output.haveCompute)
	break;
    }
  }
}

void choosePhysicalDevice (VkInstance instance, VkSurfaceKHR targetSurface, QueueFamilyIndices& outputIndices, VkPhysicalDevice& output) {
  uint32_t count = 0;
  VK_CHECK (vkEnumeratePhysicalDevices (instance, &count, nullptr));

  auto available = ALLOCA (VkPhysicalDevice, count);
  VK_CHECK (vkEnumeratePhysicalDevices (instance, &count, available));

  for (uint32_t index = 0; index < count; ++index) {
    auto& current = available[index];
    bool supportsExtensions, supportsSwapchain = false;

    { // Check support for required extensions
      uint32_t extensionsCount = 0;
      VK_CHECK (vkEnumerateDeviceExtensionProperties (current, nullptr, &extensionsCount, nullptr));

      auto availableExtensions = ALLOCA (VkExtensionProperties, extensionsCount);
      VK_CHECK (vkEnumerateDeviceExtensionProperties (current, nullptr, &extensionsCount, availableExtensions));

      for (uint32_t required = 0; required < DEVICE_EXTENSIONS_COUNT; ++required) {
	supportsExtensions = false;

	for (uint32_t present = 0; present < extensionsCount; ++present) {
	  if (!strcmp (availableExtensions[present].extensionName, vkcommon::requiredDeviceExtensions[required])) {
	    supportsExtensions = true;
	    break;
	  }
	}
      }
    }

    if (supportsExtensions) {
      uint32_t formatsCount = 0, presentModesCount = 0;
      VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR (current, targetSurface, &formatsCount, nullptr));
      VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR (current, targetSurface, &presentModesCount, nullptr));

      supportsSwapchain = formatsCount && presentModesCount;
    }

    findQueueFamilyIndices (current, targetSurface, outputIndices);

    bool hasQueueFamilies =
      outputIndices.haveGraphics &&
      outputIndices.havePresent &&
      outputIndices.haveTransfer &&
      outputIndices.haveCompute;

    if (hasQueueFamilies && supportsSwapchain) {
      output = current;
      break;
    }
  }

  assert (output);
}

void createSwapchain (const SwapchainCreateInfo& createInfo, Swapchain& output) {
  {
    // Choose swapchain extent
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VK_CHECK (vkGetPhysicalDeviceSurfaceCapabilitiesKHR (createInfo.physicalDevice, createInfo.windowSurface, &surfaceCapabilities));

    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
      output.extent = surfaceCapabilities.currentExtent;
    } else {
      output.extent.width = utils::clamp <uint32_t> (
        SCAST <uint32_t> (createInfo.window->width),
        surfaceCapabilities.minImageExtent.width,
        surfaceCapabilities.maxImageExtent.width
      );

      output.extent.height = utils::clamp <uint32_t> (
        SCAST <uint32_t> (createInfo.window->height),
        surfaceCapabilities.minImageExtent.height,
        surfaceCapabilities.maxImageExtent.height
      );
    }

    uint32_t minImagesCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount && minImagesCount > surfaceCapabilities.maxImageCount)
      minImagesCount = surfaceCapabilities.maxImageCount;

    // Choose surface format
    VkSurfaceFormatKHR surfaceFormat;
    {
      uint32_t count = 0;
      VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, nullptr));

      auto available = ALLOCA (VkSurfaceFormatKHR, count);
      VK_CHECK (vkGetPhysicalDeviceSurfaceFormatsKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, available));

      surfaceFormat = available[0];
      for (uint32_t index = 0; index < count; ++index) {
        bool rgba8 = available[index].format == VK_FORMAT_R8G8B8A8_SRGB;
        bool nonLinear = available[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        if (rgba8 && nonLinear) {
          surfaceFormat = available[index];
          break;
        }
      }
    }

    output.format = surfaceFormat.format;

    // Choose present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
      uint32_t count = 0;
      VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, nullptr));

      auto available = ALLOCA (VkPresentModeKHR, count);
      VK_CHECK (vkGetPhysicalDeviceSurfacePresentModesKHR (createInfo.physicalDevice, createInfo.windowSurface, &count, available));

      for (uint32_t index = 0; index < count; ++index) {
        if (available[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
          presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	  break;
	}
      }
    }

    VkSwapchainCreateInfoKHR info {SWAPCHAIN_CREATE_INFO_KHR};
    info.clipped = VK_TRUE;
    info.presentMode = presentMode;
    info.surface = createInfo.windowSurface;
    info.oldSwapchain = createInfo.oldSwapchain;

    info.imageArrayLayers = 1;
    info.imageExtent = output.extent;
    info.minImageCount = minImagesCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    info.preTransform = surfaceCapabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VK_CHECK (vkCreateSwapchainKHR (createInfo.device, &info, nullptr, &output.handle));
  }

  { // Create swapchain images and views
    vkGetSwapchainImagesKHR (createInfo.device, output.handle, &output.imagesCount, nullptr);

    output.images = MALLOC (VkImage, output.imagesCount);
    vkGetSwapchainImagesKHR (createInfo.device, output.handle, &output.imagesCount, output.images);

    output.views = MALLOC (VkImageView, output.imagesCount);

    VkImageSubresourceRange subresourceRange;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    for (uint32_t index = 0; index < output.imagesCount; ++index) {
      VkImageViewCreateInfo info {IMAGE_VIEW_CREATE_INFO};
      info.format = output.format;
      info.image = output.images[index];
      info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      info.subresourceRange = subresourceRange;

      VK_CHECK (vkCreateImageView (createInfo.device, &info, nullptr, &output.views[index]));
    }
  }

  { // Create depth image
    output.depthImage.format = VK_FORMAT_D24_UNORM_S8_UINT;

    VkImageCreateInfo info {IMAGE_CREATE_INFO};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.format = output.depthImage.format;

    info.extent.depth = 1;
    info.extent.width = output.extent.width;
    info.extent.height = output.extent.height;

    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VmaAllocationCreateInfo allocationInfo {};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK (vmaCreateImage (
      createInfo.allocator,
      &info,
      &allocationInfo,
      &output.depthImage.handle,
      &output.depthImage.allocation,
      nullptr
    ));
  }

  VkImageSubresourceRange subresourceRange;
  subresourceRange.levelCount = 1;
  subresourceRange.layerCount = 1;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.baseArrayLayer = 0;
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  // Create depth image view
  VkImageViewCreateInfo info {IMAGE_VIEW_CREATE_INFO};
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = output.depthImage.handle;
  info.format = output.depthImage.format;
  info.subresourceRange = subresourceRange;

  VK_CHECK (vkCreateImageView (createInfo.device, &info, nullptr, &output.depthImage.view));
}

void destroySwapchain (VkDevice device, VmaAllocator allocator, Swapchain& swapchain) {
  vkDestroyImageView (device, swapchain.depthImage.view, nullptr);
  vmaDestroyImage (allocator, swapchain.depthImage.handle, swapchain.depthImage.allocation);

  for (uint32_t index = 0; index < swapchain.imagesCount; ++index)
    vkDestroyImageView (device, swapchain.views[index], nullptr);

  free (swapchain.views);
  free (swapchain.images);

  vkDestroySwapchainKHR (device, swapchain.handle, nullptr);
}

void createShaderModule (VkDevice device, const char* relativePath, VkShaderModule& output) {
  utils::File shaderFile;
  utils::readFile (relativePath, "rb", shaderFile);

  VkShaderModuleCreateInfo createInfo {SHADER_MODULE_CREATE_INFO};
  createInfo.codeSize = shaderFile.size;
  createInfo.pCode = RCAST <uint32_t*> (shaderFile.contents);

  VK_CHECK (vkCreateShaderModule (device, &createInfo, nullptr, &output));
  free (shaderFile.contents);
}

void createGraphicsPipelines (
  VkDevice device,
  VkPipelineCache pipelineCache,
  uint32_t count,
  const GraphicsPipelineCreateInfo* createInfos,
  VkPipeline* outputs) {

  auto shaders = MALLOC (Shaders, count);
  auto infos = MALLOC (VkGraphicsPipelineCreateInfo, count);
  memset (infos, 0, sizeof (VkGraphicsPipelineCreateInfo) * count);

  for (uint32_t index = 0; index < count; ++index) {
    // This naming is so horrible
    auto& info = infos[index];
    auto& currentShader = shaders[index];
    auto& currentInfo = createInfos[index];

    createShaderModule (device, currentInfo.vertexShaderPath, currentShader.vertex);
    createShaderModule (device, currentInfo.pixelShaderPath, currentShader.pixel);

    currentShader.stages[0].pName = "main";
    currentShader.stages[0].pNext = nullptr;
    currentShader.stages[0].flags = VULKAN_NO_FLAGS;
    currentShader.stages[0].module = currentShader.vertex;
    currentShader.stages[0].pSpecializationInfo = nullptr;
    currentShader.stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    currentShader.stages[0].sType = PIPELINE_SHADER_STAGE_CREATE_INFO;

    currentShader.stages[1].pName = "main";
    currentShader.stages[1].pNext = nullptr;
    currentShader.stages[1].flags = VULKAN_NO_FLAGS;
    currentShader.stages[1].module = currentShader.pixel;
    currentShader.stages[1].pSpecializationInfo = nullptr;
    currentShader.stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    currentShader.stages[1].sType = PIPELINE_SHADER_STAGE_CREATE_INFO;

    info.stageCount = 2;
    info.pStages = currentShader.stages;
    info.layout = currentInfo.layout;
    info.renderPass = currentInfo.renderPass;
    info.pVertexInputState = currentInfo.vertexInputInfo;
    info.pInputAssemblyState = currentInfo.inputAssemblyInfo;
    info.pDynamicState = currentInfo.dynamicInfo;
    info.pViewportState = currentInfo.viewportInfo;
    info.pRasterizationState = currentInfo.rasterizationInfo;
    info.pDepthStencilState = currentInfo.depthStencilInfo;
    info.pMultisampleState = currentInfo.multisampleInfo;
    info.pColorBlendState = currentInfo.colorBlendInfo;
    info.sType = GRAPHICS_PIPELINE_CREATE_INFO;
  }

  VK_CHECK (vkCreateGraphicsPipelines (device, pipelineCache, count, infos, nullptr, outputs));

  for (uint32_t index = 0; index < count; ++index) {
    vkDestroyShaderModule (device, shaders[index].pixel, nullptr);
    vkDestroyShaderModule (device, shaders[index].vertex, nullptr);
  }

  free (shaders);
  free (infos);
}

[[nodiscard]] VkCommandBuffer startImmediateCommand (VkDevice device, VkCommandPool commandPool) {
  VkCommandBuffer commandBuffer;

  VkCommandBufferAllocateInfo allocationInfo {COMMAND_BUFFER_ALLOCATE_INFO};
  allocationInfo.commandBufferCount = 1;
  allocationInfo.commandPool = commandPool;

  VK_CHECK (vkAllocateCommandBuffers (device, &allocationInfo, &commandBuffer));

  VkCommandBufferBeginInfo beginInfo {COMMAND_BUFFER_BEGIN_INFO};
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VK_CHECK (vkBeginCommandBuffer (commandBuffer, &beginInfo));
  return commandBuffer;
}

void submitImmediateCommand (
  VkDevice device,
  VkCommandBuffer commandBuffer,
  VkCommandPool commandPool,
  VkFence waitFence,
  VkQueue submitQueue) {

  VK_CHECK (vkEndCommandBuffer (commandBuffer));

  VkSubmitInfo submitInfo {SUBMIT_INFO};
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VK_CHECK (vkQueueSubmit (submitQueue, 1, &submitInfo, waitFence));
  VK_CHECK (vkWaitForFences (device, 1, &waitFence, VK_TRUE, ~0ull));

  VK_CHECK (vkResetFences (device, 1, &waitFence));
  VK_CHECK (vkResetCommandPool (device, commandPool, VULKAN_NO_FLAGS));
}


void allocateBuffer (const BufferAllocationInfo& allocationInfo, Buffer& output) {
  VkBufferCreateInfo createInfo {BUFFER_CREATE_INFO};
  createInfo.size = allocationInfo.size;
  createInfo.usage = allocationInfo.bufferUsage;

  output.size = allocationInfo.size;

  VmaAllocationCreateInfo vmaAllocationInfo {};
  vmaAllocationInfo.requiredFlags = allocationInfo.requiredFlags;
  vmaAllocationInfo.usage = SCAST <VmaMemoryUsage> (allocationInfo.memoryUsage);

  VK_CHECK (vmaCreateBuffer (
    allocationInfo.allocator,
    &createInfo,
    &vmaAllocationInfo,
    &output.handle,
    &output.allocation,
    nullptr
  ));
}

void allocateStagingBuffer (VkDevice device, VmaAllocator allocator, VkDeviceSize size, Buffer& output) {
  rei::vkutils::BufferAllocationInfo allocationInfo;
  allocationInfo.size = size;
  allocationInfo.device = device;
  allocationInfo.allocator = allocator;
  allocationInfo.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;
  allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  allocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  rei::vkutils::allocateBuffer (allocationInfo, output);
}

void copyBuffer (const BufferCopyInfo& copyInfo, const Buffer& source, Buffer& destination) {
  auto commandBuffer = startImmediateCommand (copyInfo.device, copyInfo.commandPool);

  VkBufferCopy copyRegion;
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = source.size;

  vkCmdCopyBuffer (commandBuffer, source.handle, destination.handle, 1, &copyRegion);

  submitImmediateCommand (
    copyInfo.device,
    commandBuffer,
    copyInfo.commandPool,
    copyInfo.waitFence,
    copyInfo.submitQueue
  );
}

void allocateTexture (const TextureAllocationInfo& allocationInfo, Image& output) {
  output.format = VK_FORMAT_R8G8B8A8_SRGB;

  VkExtent3D extent {allocationInfo.width, allocationInfo.height, 1};
  VkDeviceSize size = SCAST <VkDeviceSize> (extent.width * extent.height * 4);

  Buffer stagingBuffer;
  allocateStagingBuffer (allocationInfo.device, allocationInfo.allocator, size, stagingBuffer);

  VK_CHECK (vmaMapMemory (allocationInfo.allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
  memcpy (stagingBuffer.mapped, allocationInfo.pixels, size);
  vmaUnmapMemory (allocationInfo.allocator, stagingBuffer.allocation);

  {
    VkImageCreateInfo createInfo {IMAGE_CREATE_INFO};
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.extent = extent;
    createInfo.format = output.format;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo vmaAllocationInfo {};
    vmaAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaAllocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK (vmaCreateImage (
      allocationInfo.allocator,
      &createInfo,
      &vmaAllocationInfo,
      &output.handle,
      &output.allocation,
      nullptr
    ));
  }

  VkImageSubresourceRange subresourceRange;
  subresourceRange.levelCount = 1;
  subresourceRange.layerCount = 1;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.baseArrayLayer = 0;
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  {
    auto commandBuffer = startImmediateCommand (allocationInfo.device, allocationInfo.commandPool);

    VkImageMemoryBarrier transferBarrier {IMAGE_MEMORY_BARRIER};
    transferBarrier.image = output.handle;
    transferBarrier.srcAccessMask = VULKAN_NO_FLAGS;
    transferBarrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    transferBarrier.subresourceRange = subresourceRange;
    transferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    vkCmdPipelineBarrier (
      commandBuffer,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VULKAN_NO_FLAGS,
      0, nullptr,
      0, nullptr,
      1, &transferBarrier
    );

    VkBufferImageCopy copyRegion;
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.imageExtent = extent;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdCopyBufferToImage (
      commandBuffer,
      stagingBuffer.handle,
      output.handle,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1, &copyRegion
    );

    auto& readBarrier = transferBarrier;
    readBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    readBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    readBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    readBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier (
      commandBuffer,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VULKAN_NO_FLAGS,
      0, nullptr,
      0, nullptr,
      1, &readBarrier
    );

    submitImmediateCommand (
      allocationInfo.device,
      commandBuffer,
      allocationInfo.commandPool,
      allocationInfo.waitFence,
      allocationInfo.submitQueue
    );
  }

  vmaDestroyBuffer (allocationInfo.allocator, stagingBuffer.handle, stagingBuffer.allocation);

  VkImageViewCreateInfo createInfo {IMAGE_VIEW_CREATE_INFO};
  createInfo.image = output.handle;
  createInfo.format = output.format;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.subresourceRange = subresourceRange;

  VK_CHECK (vkCreateImageView (allocationInfo.device, &createInfo, nullptr, &output.view));
}

}
