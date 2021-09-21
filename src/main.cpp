#include <stdio.h>
#include <string.h>

#include "vk.hpp"
#include "common.hpp"
#include "vkinit.hpp"
#include "window.hpp"
#include "vkutils.hpp"
#include "vkcommon.hpp"

#include <xcb/xcb.h>
#include <stb/stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#define FRAMES_COUNT 2u
#define PIPELINES_COUNT 1u

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
  uint32_t frameIndex;
  Frame frames[FRAMES_COUNT];
  VkFramebuffer* framebuffers;
  VkClearValue clearValues[2] {};

  rei::vkutils::TransferContext transferContext;

  rei::vkutils::Buffer quadVertexBuffer;
  rei::vkutils::Buffer quadIndexBuffer;
  rei::vkutils::Image testImage;

  VkDescriptorPool descriptorPool;
  VkDescriptorSetLayout quadDescriptorLayout;
  VkDescriptorSet quadDescriptorSet;
  VkSampler quadSampler;

  glm::mat4 modelMatrix = glm::translate (glm::mat4 {1.f}, glm::vec3 {0.f, 0.f, 1.f});
  glm::mat4 mvp = glm::mat4 {1.f} * glm::mat4 {1.f} * modelMatrix;

  VkPipelineLayout quadPipelineLayout;
  VkPipeline pipelines[PIPELINES_COUNT];

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

  { // Create vertex buffer for quad
    rei::vkutils::Buffer stagingBuffer;
    rei::vkutils::allocateStagingBuffer (device, allocator, sizeof (quadVertices), stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, quadVertices, sizeof (quadVertices));
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      rei::vkutils::BufferAllocationInfo allocationInfo;
      allocationInfo.size = sizeof (quadVertices);
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      rei::vkutils::allocateBuffer (device, allocator, allocationInfo, quadVertexBuffer);
    }

    rei::vkutils::copyBuffer (device, transferContext, stagingBuffer, quadVertexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  { // Create index buffer for quad
    rei::vkutils::Buffer stagingBuffer;
    rei::vkutils::allocateStagingBuffer (device, allocator, sizeof (quadIndices), stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, quadIndices, sizeof (quadIndices));
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      rei::vkutils::BufferAllocationInfo allocationInfo;
      allocationInfo.size = sizeof (quadIndices);
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      rei::vkutils::allocateBuffer (device, allocator, allocationInfo, quadIndexBuffer);
    }

    rei::vkutils::copyBuffer (device, transferContext, stagingBuffer, quadIndexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  { // Create test texture to let quad sample from it
    int width, height, channels;
    auto pixels = stbi_load ("assets/schnoz.png", &width, &height, &channels, STBI_rgb_alpha);

    rei::vkutils::TextureAllocationInfo allocationInfo;
    allocationInfo.width = SCAST <uint32_t> (width);
    allocationInfo.height = SCAST <uint32_t> (height);
    allocationInfo.pixels = RCAST <const char*> (pixels);

    rei::vkutils::allocateTexture (device, allocator, allocationInfo, transferContext, testImage);

    stbi_image_free (pixels);
  }

  { // Create descriptor pool
    VkDescriptorPoolSize poolSize {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo createInfo {DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.maxSets = 1;
    createInfo.poolSizeCount = 1;
    createInfo.pPoolSizes = &poolSize;

    VK_CHECK (vkCreateDescriptorPool (device, &createInfo, nullptr, &descriptorPool));
  }

  { // Create layout for quad descriptor sampler
    VkDescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    VkDescriptorSetLayoutCreateInfo createInfo {DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    createInfo.bindingCount = 1;
    createInfo.pBindings = &binding;

    VK_CHECK (vkCreateDescriptorSetLayout (device, &createInfo, nullptr, &quadDescriptorLayout));
  }

  { // Create sampler
    VkSamplerCreateInfo createInfo {SAMPLER_CREATE_INFO};
    createInfo.minFilter = VK_FILTER_NEAREST;
    createInfo.magFilter = VK_FILTER_NEAREST;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VK_CHECK (vkCreateSampler (device, &createInfo, nullptr, &quadSampler));
  }

  { // Allocate quad descriptor
    VkDescriptorSetAllocateInfo allocationInfo {DESCRIPTOR_SET_ALLOCATE_INFO};
    allocationInfo.descriptorSetCount = 1;
    allocationInfo.descriptorPool = descriptorPool;
    allocationInfo.pSetLayouts = &quadDescriptorLayout;

    VK_CHECK (vkAllocateDescriptorSets (device, &allocationInfo, &quadDescriptorSet));
  }

  {
    VkDescriptorImageInfo imageInfo;
    imageInfo.sampler = quadSampler;
    imageInfo.imageView = testImage.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writeInfo {WRITE_DESCRIPTOR_SET};
    writeInfo.dstBinding = 0;
    writeInfo.descriptorCount = 1;
    writeInfo.pImageInfo = &imageInfo;
    writeInfo.dstSet = quadDescriptorSet;
    writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    vkUpdateDescriptorSets (device, 1, &writeInfo, 0, nullptr);
  }

  { // Create quad pipeline layout
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof (glm::mat4);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo createInfo {PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pSetLayouts = &quadDescriptorLayout;
    createInfo.pPushConstantRanges = &pushConstantRange;
    VK_CHECK (vkCreatePipelineLayout (device, &createInfo, nullptr, &quadPipelineLayout));
  }

  { // Create graphics pipelines
    rei::vkutils::GraphicsPipelineCreateInfo createInfos[PIPELINES_COUNT];

    VkVertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof (Vertex2D);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[2];

    // Position
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].offset = offsetof (Vertex2D, x);
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;

    // Uv
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].offset = offsetof (Vertex2D, u);
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo {PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexBindingDescriptions = &binding;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo {PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = swapchain.extent;

    VkViewport viewport;
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.minDepth = 0.f;
    viewport.minDepth = 1.f;
    viewport.width = SCAST <float> (swapchain.extent.width);
    viewport.height = SCAST <float> (swapchain.extent.height);

    VkPipelineViewportStateCreateInfo viewportInfo {PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportInfo.scissorCount = 1;
    viewportInfo.viewportCount = 1;
    viewportInfo.pScissors = &scissor;
    viewportInfo.pViewports = &viewport;

    VkPipelineRasterizationStateCreateInfo rasterizationInfo {PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationInfo.lineWidth = 1.f;
    rasterizationInfo.depthBiasEnable = VK_FALSE;
    rasterizationInfo.depthClampEnable = VK_FALSE;
    rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampleInfo {PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleInfo.minSampleShading = 1.f;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo {PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilInfo.minDepthBounds = 0.f;
    depthStencilInfo.maxDepthBounds = 1.f;

    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.stencilTestEnable = VK_FALSE;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachmentInfo {};
    blendAttachmentInfo.blendEnable = VK_FALSE,
    blendAttachmentInfo.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo {PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendInfo.pAttachments = &blendAttachmentInfo;

    // Textured quad pipeline
    createInfos[0].dynamicInfo = nullptr;
    createInfos[0].renderPass = renderPass;
    createInfos[0].layout = quadPipelineLayout;
    createInfos[0].pixelShaderPath = "assets/shaders/textured_quad.frag.spv";
    createInfos[0].vertexShaderPath = "assets/shaders/textured_quad.vert.spv";

    createInfos[0].vertexInputInfo = &vertexInputInfo;
    createInfos[0].inputAssemblyInfo = &inputAssemblyInfo;
    createInfos[0].viewportInfo = &viewportInfo;
    createInfos[0].rasterizationInfo = &rasterizationInfo;
    createInfos[0].multisampleInfo = &multisampleInfo;
    createInfos[0].colorBlendInfo = &colorBlendInfo;
    createInfos[0].depthStencilInfo = &depthStencilInfo;

    rei::vkutils::createGraphicsPipelines (device, VK_NULL_HANDLE, PIPELINES_COUNT, createInfos, pipelines);
  }

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

      VkDeviceSize offset = 0;

      vkCmdBindPipeline (currentFrame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[0]);
      vkCmdBindVertexBuffers (currentFrame.commandBuffer, 0, 1, &quadVertexBuffer.handle, &offset);
      vkCmdBindIndexBuffer (currentFrame.commandBuffer, quadIndexBuffer.handle, 0, VK_INDEX_TYPE_UINT16);
      vkCmdBindDescriptorSets (currentFrame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadPipelineLayout, 0, 1, &quadDescriptorSet, 0, nullptr);

      vkCmdPushConstants (currentFrame.commandBuffer, quadPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (glm::mat4), &mvp);

      vkCmdDrawIndexed (currentFrame.commandBuffer, 6, 1, 0, 0, 0);

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

  vkDestroyPipelineLayout (device, quadPipelineLayout, nullptr);

  for (uint8_t index = 0; index < PIPELINES_COUNT; ++index)
    vkDestroyPipeline (device, pipelines[index], nullptr);

  vkDestroyDescriptorPool (device, descriptorPool, nullptr);
  vkDestroyDescriptorSetLayout (device, quadDescriptorLayout, nullptr);
  vkDestroySampler (device, quadSampler, nullptr);

  vkDestroyImageView (device, testImage.view, nullptr);
  vmaDestroyImage (allocator, testImage.handle, testImage.allocation);

  vmaDestroyBuffer (allocator, quadIndexBuffer.handle, quadIndexBuffer.allocation);
  vmaDestroyBuffer (allocator, quadVertexBuffer.handle, quadVertexBuffer.allocation);

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
