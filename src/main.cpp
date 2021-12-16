#include <stdio.h>

#include "imgui.hpp"
#include "common.hpp"
#include "camera.hpp"
#include "window.hpp"
#include "vkutils.hpp"
#include "vkcommon.hpp"
#include "gltf_model.hpp"

#include <xcb/xcb.h>
#include <imgui/imgui.h>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

#define GBUFFER_ATTACHMENT_COUNT 3u

struct Frame {
  VkCommandPool commandPool;
  VkCommandBuffer offscreenCmd;
  VkCommandBuffer compositionCmd;

  VkFence offscreenFence;
  VkFence compositionFence;
  VkSemaphore compositionSemaphore;
  VkSemaphore presentSemaphore;
  VkSemaphore offscreenSemaphore;
};

struct Light {
  rei::math::Vector4 position;
  // x, y, z = color, w = radius
  // This is done to remove padding L.o.L
  rei::math::Vector4 colorRadius;
};

struct GBufferCreateInfo {
  Uint32 width, height;
  VkPipelineCache pipelineCache;
  VkDescriptorPool descriptorPool;
  VkRenderPass lightRenderPass;
};

struct GBuffer {
  VkSampler sampler;

  struct {
    VkDescriptorSetLayout descriptorLayout;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkRenderPass renderPass;
    VkFramebuffer framebuffer;
    rei::vku::Image depthAttachment;
    rei::vku::Image attachments[GBUFFER_ATTACHMENT_COUNT];
    VkClearValue clearValues[GBUFFER_ATTACHMENT_COUNT + 1];
  } geometryPass;

  struct {
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorLayout;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    VkRenderPass renderPass;
    VkClearValue clearValues[2];
  } lightPass;
};

struct LightPassPushConstants {
  Light light;
  // Which target to present
  // 0 - default
  // 1 - albedo
  // 2 - normal
  // 3 - position
  Uint32 target;
  rei::math::Vector4 viewPosition;
};

static void createGBuffer (VkDevice device, VmaAllocator allocator, const GBufferCreateInfo* createInfo, GBuffer* out) {
  for (Uint8 index = 0; index < GBUFFER_ATTACHMENT_COUNT; ++index) {
    rei::vku::AttachmentCreateInfo info;
    info.width = createInfo->width;
    info.height = createInfo->height;
    info.format = VULKAN_TEXTURE_FORMAT;
    info.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    rei::vku::createAttachment (device, allocator, &info, &out->geometryPass.attachments[index]);
  }

  {
    rei::vku::AttachmentCreateInfo info;
    info.width = createInfo->width;
    info.height = createInfo->height;
    info.format = VK_FORMAT_D24_UNORM_S8_UINT;
    info.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    info.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    rei::vku::createAttachment (device, allocator, &info, &out->geometryPass.depthAttachment);
  }

  {
    VkAttachmentReference references[GBUFFER_ATTACHMENT_COUNT];
    VkAttachmentDescription attachments[GBUFFER_ATTACHMENT_COUNT + 1];

    for (Uint8 index = 0; index < GBUFFER_ATTACHMENT_COUNT; ++index) {
      attachments[index].flags = VULKAN_NO_FLAGS;
      attachments[index].format = VULKAN_TEXTURE_FORMAT;
      attachments[index].samples = VK_SAMPLE_COUNT_1_BIT;
      attachments[index].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachments[index].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachments[index].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachments[index].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[index].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachments[index].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      references[index].attachment = index;
      references[index].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    attachments[GBUFFER_ATTACHMENT_COUNT].flags = VULKAN_NO_FLAGS;
    attachments[GBUFFER_ATTACHMENT_COUNT].format = VK_FORMAT_D24_UNORM_S8_UINT;
    attachments[GBUFFER_ATTACHMENT_COUNT].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[GBUFFER_ATTACHMENT_COUNT].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[GBUFFER_ATTACHMENT_COUNT].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[GBUFFER_ATTACHMENT_COUNT].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[GBUFFER_ATTACHMENT_COUNT].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[GBUFFER_ATTACHMENT_COUNT].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[GBUFFER_ATTACHMENT_COUNT].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference;
    depthReference.attachment = GBUFFER_ATTACHMENT_COUNT;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    subpass.flags = VULKAN_NO_FLAGS;
    subpass.inputAttachmentCount = 0;
    subpass.preserveAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.pResolveAttachments = nullptr;
    subpass.pPreserveAttachments = nullptr;
    subpass.pColorAttachments = references;
    subpass.pDepthStencilAttachment = &depthReference;
    subpass.colorAttachmentCount = GBUFFER_ATTACHMENT_COUNT;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    VkRenderPassCreateInfo info;
    info.pNext = nullptr;
    info.subpassCount = 1;
    info.dependencyCount = 0;
    info.pSubpasses = &subpass;
    info.flags = VULKAN_NO_FLAGS;
    info.pDependencies = nullptr;
    info.pAttachments = attachments;
    info.sType = RENDER_PASS_CREATE_INFO;
    info.attachmentCount = GBUFFER_ATTACHMENT_COUNT + 1;

    VK_CHECK (vkCreateRenderPass (device, &info, nullptr, &out->geometryPass.renderPass));
  }

  out->lightPass.renderPass = createInfo->lightRenderPass;
  out->lightPass.clearValues[0].color = {{0.f, 0.f, 0.f, 0.f}};
  out->lightPass.clearValues[1].depthStencil = {1.f, 0};

  for (Uint8 index = 0; index < GBUFFER_ATTACHMENT_COUNT; ++index)
    out->geometryPass.clearValues[index].color = {{0.f, 0.f, 0.f, 0.f}};

  out->geometryPass.clearValues[GBUFFER_ATTACHMENT_COUNT].depthStencil = {1.f, 0};

  {
    VkImageView attachments[GBUFFER_ATTACHMENT_COUNT + 1] {
      out->geometryPass.attachments[0].view,
      out->geometryPass.attachments[1].view,
      out->geometryPass.attachments[2].view,
      out->geometryPass.depthAttachment.view,
    };

    VkFramebufferCreateInfo info;
    info.layers = 1;
    info.pNext = nullptr;
    info.flags = VULKAN_NO_FLAGS;
    info.width = createInfo->width;
    info.pAttachments = attachments;
    info.height = createInfo->height;
    info.sType = FRAMEBUFFER_CREATE_INFO;
    info.renderPass = out->geometryPass.renderPass;
    info.attachmentCount = GBUFFER_ATTACHMENT_COUNT + 1;

    VK_CHECK (vkCreateFramebuffer (device, &info, nullptr, &out->geometryPass.framebuffer));
  }

  {
    VkSamplerCreateInfo info;
    info.minLod = 0.f;
    info.maxLod = 1.f;
    info.pNext = nullptr;
    info.mipLodBias = 0.f;
    info.maxAnisotropy = 1.f;
    info.flags = VULKAN_NO_FLAGS;
    info.compareEnable = VK_FALSE;
    info.sType = SAMPLER_CREATE_INFO;
    info.anisotropyEnable = VK_FALSE;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.compareOp = VK_COMPARE_OP_NEVER;
    info.unnormalizedCoordinates = VK_FALSE;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VK_CHECK (vkCreateSampler (device, &info, nullptr, &out->sampler));
  }

  {
    VkDescriptorSetLayoutBinding bindings[GBUFFER_ATTACHMENT_COUNT];
    for (Uint8 index = 0; index < GBUFFER_ATTACHMENT_COUNT; ++index) {
      bindings[index].binding = index;
      bindings[index].descriptorCount = 1;
      bindings[index].pImmutableSamplers = nullptr;
      bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
      bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }

    VkDescriptorSetLayoutCreateInfo info;
    info.pNext = nullptr;
    info.pBindings = bindings;
    info.flags = VULKAN_NO_FLAGS;
    info.bindingCount = GBUFFER_ATTACHMENT_COUNT;
    info.sType = DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    VK_CHECK (vkCreateDescriptorSetLayout (device, &info, nullptr, &out->lightPass.descriptorLayout));

    info.bindingCount = 1;
    info.pBindings = &bindings[0];

    VK_CHECK (vkCreateDescriptorSetLayout (device, &info, nullptr, &out->geometryPass.descriptorLayout));
  }

  {
    VkDescriptorSetAllocateInfo allocationInfo;
    allocationInfo.pNext = nullptr;
    allocationInfo.descriptorSetCount = 1;
    allocationInfo.sType = DESCRIPTOR_SET_ALLOCATE_INFO;
    allocationInfo.descriptorPool = createInfo->descriptorPool;
    allocationInfo.pSetLayouts = &out->lightPass.descriptorLayout;

    VK_CHECK (vkAllocateDescriptorSets (device, &allocationInfo, &out->lightPass.descriptorSet));
  }

  {
    VkWriteDescriptorSet writes[GBUFFER_ATTACHMENT_COUNT];
    VkDescriptorImageInfo imageInfos[GBUFFER_ATTACHMENT_COUNT];

    for (Uint8 index = 0; index < GBUFFER_ATTACHMENT_COUNT; ++index) {
      imageInfos[index].sampler = out->sampler;
      imageInfos[index].imageView = out->geometryPass.attachments[index].view;
      imageInfos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      writes[index].pNext = nullptr;
      writes[index].dstBinding = index;
      writes[index].dstArrayElement = 0;
      writes[index].descriptorCount = 1;
      writes[index].pBufferInfo = nullptr;
      writes[index].pTexelBufferView = nullptr;
      writes[index].sType = WRITE_DESCRIPTOR_SET;
      writes[index].pImageInfo = &imageInfos[index];
      writes[index].dstSet = out->lightPass.descriptorSet;
      writes[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }

    vkUpdateDescriptorSets (device, GBUFFER_ATTACHMENT_COUNT, writes, 0, nullptr);
  }

  {
    VkPushConstantRange pushConstant;
    pushConstant.offset = 0;
    pushConstant.size = sizeof (rei::math::Matrix4) * 2;
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo info;
    info.pNext = nullptr;
    info.setLayoutCount = 1;
    info.flags = VULKAN_NO_FLAGS;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushConstant;
    info.sType = PIPELINE_LAYOUT_CREATE_INFO;
    info.pSetLayouts = &out->geometryPass.descriptorLayout;

    VK_CHECK (vkCreatePipelineLayout (device, &info, nullptr, &out->geometryPass.pipelineLayout));

    info.pSetLayouts = &out->lightPass.descriptorLayout;
    pushConstant.size = sizeof (LightPassPushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VK_CHECK (vkCreatePipelineLayout (device, &info, nullptr, &out->lightPass.pipelineLayout));
  }

  {
    VkVertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof (rei::Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3];
    attributes[0].location = 0;
    attributes[0].binding = binding.binding;
    attributes[0].offset = REI_OFFSET_OF (rei::Vertex, x);
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;

    attributes[1].location = 1;
    attributes[1].binding = binding.binding;
    attributes[1].offset = REI_OFFSET_OF (rei::Vertex, nx);
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;

    attributes[2].location = 2;
    attributes[2].binding = binding.binding;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = REI_OFFSET_OF (rei::Vertex, u);

    VkPipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.pNext = nullptr;
    vertexInputState.flags = VULKAN_NO_FLAGS;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.vertexAttributeDescriptionCount = 3;
    vertexInputState.pVertexBindingDescriptions = &binding;
    vertexInputState.pVertexAttributeDescriptions = attributes;
    vertexInputState.sType = PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent.width = createInfo->width;
    scissor.extent.height = createInfo->height;

    VkViewport viewport;
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    viewport.width = (Float32) createInfo->width;
    viewport.height = (Float32) createInfo->height;

    VkPipelineViewportStateCreateInfo viewportState;
    viewportState.pNext = nullptr;
    viewportState.scissorCount = 1;
    viewportState.viewportCount = 1;
    viewportState.pScissors = &scissor;
    viewportState.pViewports = &viewport;
    viewportState.flags = VULKAN_NO_FLAGS;
    viewportState.sType = PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    VkPipelineRasterizationStateCreateInfo rasterizationState;
    rasterizationState.pNext = nullptr;
    rasterizationState.lineWidth = 1.f;
    rasterizationState.depthBiasClamp = 0.f;
    rasterizationState.flags = VULKAN_NO_FLAGS;
    rasterizationState.depthBiasSlopeFactor = 0.f;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.depthBiasConstantFactor = 0.f;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.sType = PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    depthStencilState.back = {};
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.front = {};
    depthStencilState.pNext = nullptr;
    depthStencilState.minDepthBounds = 0.f;
    depthStencilState.maxDepthBounds = 1.f;
    depthStencilState.flags = VULKAN_NO_FLAGS;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.sType = PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendAttachmentState colorBlendAttachments[GBUFFER_ATTACHMENT_COUNT];

    for (Uint8 index = 0; index < GBUFFER_ATTACHMENT_COUNT; ++index) {
      colorBlendAttachments[index].colorWriteMask = 0xF;
      colorBlendAttachments[index].blendEnable = VK_FALSE;
      colorBlendAttachments[index].colorBlendOp = VK_BLEND_OP_ADD;
      colorBlendAttachments[index].alphaBlendOp = VK_BLEND_OP_ADD;
      colorBlendAttachments[index].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      colorBlendAttachments[index].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      colorBlendAttachments[index].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      colorBlendAttachments[index].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    }

    rei::vku::GraphicsPipelineCreateInfo info;
    info.dynamicState = nullptr;
    info.colorBlendAttachmentCount = 3;
    info.cache = createInfo->pipelineCache;
    info.layout = out->geometryPass.pipelineLayout;
    info.renderPass = out->geometryPass.renderPass;

    info.viewportState = &viewportState;
    info.vertexInputState = &vertexInputState;
    info.depthStencilState = &depthStencilState;
    info.rasterizationState = &rasterizationState;
    info.colorBlendAttachment = colorBlendAttachments;

    info.pixelShaderPath = "assets/shaders/deferred_geometry.frag.spv";
    info.vertexShaderPath = "assets/shaders/deferred_geometry.vert.spv";

    rei::vku::createGraphicsPipeline (device, &info, &out->geometryPass.pipeline);

    vertexInputState.vertexBindingDescriptionCount = 0;
    vertexInputState.vertexAttributeDescriptionCount = 0;
    vertexInputState.pVertexBindingDescriptions = nullptr;
    vertexInputState.pVertexAttributeDescriptions = nullptr;

    rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask = 0xF;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    info.colorBlendAttachmentCount = 1;
    info.renderPass = out->lightPass.renderPass;
    info.layout = out->lightPass.pipelineLayout;
    info.colorBlendAttachment = &colorBlendAttachment;
    info.pixelShaderPath = "assets/shaders/deferred_light.frag.spv";
    info.vertexShaderPath = "assets/shaders/deferred_light.vert.spv";

    rei::vku::createGraphicsPipeline (device, &info, &out->lightPass.pipeline);
  }
}

static void destroyGBuffer (VkDevice device, VmaAllocator allocator, GBuffer* gbuffer) {
  vkDestroyPipeline (device, gbuffer->lightPass.pipeline, nullptr);
  vkDestroyPipeline (device, gbuffer->geometryPass.pipeline, nullptr);
  vkDestroyPipelineLayout (device, gbuffer->lightPass.pipelineLayout, nullptr);
  vkDestroyPipelineLayout (device, gbuffer->geometryPass.pipelineLayout, nullptr);

  vkDestroyDescriptorSetLayout (device, gbuffer->geometryPass.descriptorLayout, nullptr);
  vkDestroyDescriptorSetLayout (device, gbuffer->lightPass.descriptorLayout, nullptr);
  vkDestroySampler (device, gbuffer->sampler, nullptr);

  vkDestroyFramebuffer (device, gbuffer->geometryPass.framebuffer, nullptr);
  vkDestroyRenderPass (device, gbuffer->geometryPass.renderPass, nullptr);

  vkDestroyImageView (device, gbuffer->geometryPass.depthAttachment.view, nullptr);
  vmaDestroyImage (allocator, gbuffer->geometryPass.depthAttachment.handle, gbuffer->geometryPass.depthAttachment.allocation);

  for (Uint8 index = 0; index < GBUFFER_ATTACHMENT_COUNT; ++index) {
    auto current = &gbuffer->geometryPass.attachments[index];
    vkDestroyImageView (device, current->view, nullptr);
    vmaDestroyImage (allocator, current->handle, current->allocation);
  }
}

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
  VkRenderPass defaultRenderPass;

  Uint32 frameIndex = 0;
  Frame frames[FRAMES_COUNT];
  VkFramebuffer* framebuffers;

  VkPipelineCache pipelineCache;
  VkDescriptorPool mainDescriptorPool;
  GBuffer gbuffer;

  rei::imgui::Context imguiContext;
  rei::vku::TransferContext transferContext;

  rei::gltf::Model sponza;

  rei::Timer::init ();
  rei::vkc::Context::init ();

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

    VkApplicationInfo applicationInfo;
    applicationInfo.pNext = nullptr;
    applicationInfo.engineVersion = 0;
    applicationInfo.pEngineName = "Rei";
    applicationInfo.applicationVersion = 0;
    applicationInfo.sType = APPLICATION_INFO;
    applicationInfo.apiVersion = VULKAN_VERSION;
    applicationInfo.pApplicationName = "Playground";

    VkInstanceCreateInfo createInfo;
    createInfo.pNext = nullptr;
    createInfo.enabledLayerCount = 0;
    createInfo.flags = VULKAN_NO_FLAGS;
    createInfo.sType = INSTANCE_CREATE_INFO;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.ppEnabledExtensionNames = requiredExtensions;
    createInfo.enabledExtensionCount = ARRAY_SIZE (requiredExtensions);

    #ifndef NDEBUG
    const char* validationLayers[] {"VK_LAYER_KHRONOS_validation"};

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo;
    debugMessengerInfo.pNext = nullptr;
    debugMessengerInfo.pUserData = nullptr;
    debugMessengerInfo.flags = VULKAN_NO_FLAGS;
    debugMessengerInfo.pfnUserCallback = rei::vkc::debugCallback;
    debugMessengerInfo.sType = DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugMessengerInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    debugMessengerInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

    createInfo.enabledLayerCount = 1;
    createInfo.pNext = &debugMessengerInfo;
    createInfo.ppEnabledLayerNames = validationLayers;
    #endif

    VK_CHECK (vkCreateInstance (&createInfo, nullptr, &instance));
    rei::vkc::Context::loadInstance (instance);
  }

  #ifndef NDEBUG
  { // Create debug messenger if debug mode is enabled
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    createInfo.pNext = nullptr;
    createInfo.pUserData = nullptr;
    createInfo.flags = VULKAN_NO_FLAGS;
    createInfo.pfnUserCallback = rei::vkc::debugCallback;
    createInfo.sType = DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    createInfo.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
    createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

    VK_CHECK (vkCreateDebugUtilsMessengerEXT (instance, &createInfo, nullptr, &debugMessenger));
  }
  #endif

  { // Create window surface
    VkXcbSurfaceCreateInfoKHR createInfo;
    createInfo.pNext = nullptr;
    createInfo.flags = VULKAN_NO_FLAGS;
    createInfo.window = window.handle;
    createInfo.connection = window.connection;
    createInfo.sType = XCB_SURFACE_CREATE_INFO_KHR;

    VK_CHECK (vkCreateXcbSurfaceKHR (instance, &createInfo, nullptr, &windowSurface));
  }

  { // Choose physical device, create logical device
    rei::vku::QueueFamilyIndices indices;
    rei::vku::choosePhysicalDevice (instance, windowSurface, &indices, &physicalDevice);

    {
      // Make sure that VULKAN_TEXTURE_FORMAT supports image blitting (which is needed for mipmap generation) on the chosen device
      VkFormatProperties formatProperties;
      vkGetPhysicalDeviceFormatProperties (physicalDevice, VULKAN_TEXTURE_FORMAT, &formatProperties);

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

    rei::vkc::Context::loadDevice (device);
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

  { // Create default render pass
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
    attachments[1].format = VULKAN_DEPTH_FORMAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
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
    createInfo.dependencyCount = 0;
    createInfo.pDependencies = nullptr;

    VK_CHECK (vkCreateRenderPass (device, &createInfo, nullptr, &defaultRenderPass));
  }

  { // Create framebuffers
    framebuffers = MALLOC (VkFramebuffer, swapchain.imagesCount);

    VkFramebufferCreateInfo createInfo {FRAMEBUFFER_CREATE_INFO};
    createInfo.layers = 1;
    createInfo.attachmentCount = 2;
    createInfo.renderPass = defaultRenderPass;
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
    bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkFenceCreateInfo fenceInfo {FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK (vkCreateCommandPool (device, &poolInfo, nullptr, &transferContext.commandPool));

    VkSemaphoreCreateInfo semaphoreInfo {SEMAPHORE_CREATE_INFO};

    bufferInfo.commandBufferCount = 1;
    for (Uint8 index = 0; index < FRAMES_COUNT; ++index) {
      auto current = &frames[index];
      VK_CHECK (vkCreateCommandPool (device, &poolInfo, nullptr, &current->commandPool));

      bufferInfo.commandPool = current->commandPool;

      VK_CHECK (vkAllocateCommandBuffers (device, &bufferInfo, &current->compositionCmd));
      VK_CHECK (vkAllocateCommandBuffers (device, &bufferInfo, &current->offscreenCmd));
      VK_CHECK (vkCreateFence (device, &fenceInfo, nullptr, &current->compositionFence));
      VK_CHECK (vkCreateFence (device, &fenceInfo, nullptr, &current->offscreenFence));
      VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, nullptr, &current->compositionSemaphore));
      VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, nullptr, &current->presentSemaphore));
      VK_CHECK (vkCreateSemaphore (device, &semaphoreInfo, nullptr, &current->offscreenSemaphore));
    }

    fenceInfo.flags = VULKAN_NO_FLAGS;
    VK_CHECK (vkCreateFence (device, &fenceInfo, nullptr, &transferContext.fence));
  }

  { // Create main descriptor pool
    VkDescriptorPoolSize sizes[1];
    sizes[0].descriptorCount = 1 + GBUFFER_ATTACHMENT_COUNT;
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    VkDescriptorPoolCreateInfo createInfo {DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.pPoolSizes = sizes;
    createInfo.poolSizeCount = ARRAY_SIZE (sizes);
    createInfo.maxSets = 1 + GBUFFER_ATTACHMENT_COUNT;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VK_CHECK (vkCreateDescriptorPool (device, &createInfo, nullptr, &mainDescriptorPool));
  }

  { // Create/load pipeline cache
    VkPipelineCacheCreateInfo createInfo {PIPELINE_CACHE_CREATE_INFO};
    rei::File cacheFile;
    auto result = rei::readFile ("pipeline.cache", True, &cacheFile);

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

  {
    GBufferCreateInfo createInfo;
    createInfo.lightRenderPass = defaultRenderPass;
    createInfo.pipelineCache = pipelineCache;
    createInfo.width = swapchain.extent.width;
    createInfo.height = swapchain.extent.height;
    createInfo.descriptorPool = mainDescriptorPool;

    createGBuffer (device, allocator, &createInfo, &gbuffer);
  }

  { // Create imgui context
    rei::imgui::ContextCreateInfo createInfo;
    createInfo.window = &window;
    createInfo.renderPass = defaultRenderPass;
    createInfo.pipelineCache = pipelineCache;
    createInfo.transferContext = &transferContext;
    createInfo.descriptorPool = mainDescriptorPool;

    rei::imgui::create (device, allocator, &createInfo, &imguiContext);
  }

  rei::gltf::load (device, allocator, &transferContext, "assets/models/sponza-scene/Sponza.gltf", &sponza);
  sponza.initDescriptors (device, gbuffer.geometryPass.descriptorLayout);

  Float32 lastTime = 0.f;
  Float32 deltaTime = 0.f;
  const Float32 defaultDelta = 1.f / 60.f;

  xcb_generic_event_t* event = nullptr;

  VkCommandBufferBeginInfo cmdBeginInfo;
  cmdBeginInfo.pNext = nullptr;
  cmdBeginInfo.pInheritanceInfo = nullptr;
  cmdBeginInfo.sType = COMMAND_BUFFER_BEGIN_INFO;
  cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  VkRenderPassBeginInfo offscreenBeginInfo;
  offscreenBeginInfo.pNext = nullptr;
  offscreenBeginInfo.renderArea.offset = {0, 0};
  offscreenBeginInfo.sType = RENDER_PASS_BEGIN_INFO;
  offscreenBeginInfo.renderArea.extent = swapchain.extent;
  offscreenBeginInfo.renderPass = gbuffer.geometryPass.renderPass;
  offscreenBeginInfo.framebuffer = gbuffer.geometryPass.framebuffer;
  offscreenBeginInfo.pClearValues = gbuffer.geometryPass.clearValues;
  offscreenBeginInfo.clearValueCount = ARRAY_SIZE (gbuffer.geometryPass.clearValues);

  VkRenderPassBeginInfo compositionBeginInfo;
  compositionBeginInfo.pNext = nullptr;
  compositionBeginInfo.renderArea.offset = {0, 0};
  compositionBeginInfo.sType = RENDER_PASS_BEGIN_INFO;
  compositionBeginInfo.renderPass = defaultRenderPass;
  compositionBeginInfo.renderArea.extent = swapchain.extent;
  compositionBeginInfo.pClearValues = gbuffer.lightPass.clearValues;
  compositionBeginInfo.clearValueCount = ARRAY_SIZE (gbuffer.lightPass.clearValues);

  LightPassPushConstants lightPushConstants;
  lightPushConstants.target = 0;

  lightPushConstants.viewPosition.x = 0.f;
  lightPushConstants.viewPosition.y = 0.f;
  lightPushConstants.viewPosition.z = 0.f;
  lightPushConstants.viewPosition.w = 1.f;

  lightPushConstants.light.position.x = 0.f;
  lightPushConstants.light.position.y = 0.f;
  lightPushConstants.light.position.z = 0.5f;
  lightPushConstants.light.position.w = 0.f;

  lightPushConstants.light.colorRadius.x = 1.f;
  lightPushConstants.light.colorRadius.y = 31.f / 255.f;
  lightPushConstants.light.colorRadius.z = 31.f / 255.f;
  lightPushConstants.light.colorRadius.w = 0.5f;

  VkPipelineStageFlags pipelineWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  for (;;) {
    camera.firstMouse = True;
    Float32 currentTime = rei::Timer::getCurrentTime ();
    deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    // NOTE IMGUI asserts that deltaTime > 0.f, hence this check.
    Float32 imguiDeltaTime[2] {defaultDelta, deltaTime};
    ImGui::GetIO().DeltaTime = imguiDeltaTime[deltaTime > 0.f];

    while ((event = xcb_poll_for_event (window.connection))) {
      switch (event->response_type & ~0x80) {
        case XCB_KEY_PRESS: {
          const auto key = (const xcb_key_press_event_t*) event;
          switch (key->detail) {
            case KEY_ESCAPE: goto RESOURCE_CLEANUP;
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
    auto offscreenCmd = currentFrame->offscreenCmd;
    auto compositionCmd = currentFrame->compositionCmd;

    VkFence fences[2] {currentFrame->compositionFence, currentFrame->offscreenFence};

    VK_CHECK (vkWaitForFences (device, 2, fences, VK_TRUE, ~0ull));
    VK_CHECK (vkResetFences (device, 2, fences));

    Uint32 imageIndex = 0;
    VKC_GET_NEXT_IMAGE (device, swapchain, currentFrame->presentSemaphore, &imageIndex);

    // Geometry pass of deferred renderer
    VK_CHECK (vkBeginCommandBuffer (offscreenCmd, &cmdBeginInfo));
    vkCmdBeginRenderPass (offscreenCmd, &offscreenBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (offscreenCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gbuffer.geometryPass.pipeline);

    {
      rei::math::Vector3 center;
      rei::math::Matrix4 viewMatrix;
      rei::math::Vector3::add (&camera.position, &camera.front, &center);
      rei::math::lookAt (&camera.position, &center, &camera.up, &viewMatrix);

      rei::math::Matrix4 viewProjection;
      rei::math::Matrix4::mul (&camera.projection, &viewMatrix, &viewProjection);
      sponza.draw (offscreenCmd, gbuffer.geometryPass.pipelineLayout, &viewProjection);
    }

    vkCmdEndRenderPass (offscreenCmd);
    VK_CHECK (vkEndCommandBuffer (offscreenCmd));

    { // Submit written commands to a queue
      VkSubmitInfo submitInfo;
      submitInfo.pNext = nullptr;
      submitInfo.sType = SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pCommandBuffers = &offscreenCmd;
      submitInfo.pWaitDstStageMask = &pipelineWaitStage;
      submitInfo.pWaitSemaphores = &currentFrame->presentSemaphore;
      submitInfo.pSignalSemaphores = &currentFrame->offscreenSemaphore;

      VK_CHECK (vkQueueSubmit (graphicsQueue, 1, &submitInfo, currentFrame->offscreenFence));
    }

    VK_CHECK (vkBeginCommandBuffer (compositionCmd, &cmdBeginInfo));

    // Light pass of deferred renderer
    compositionBeginInfo.framebuffer = framebuffers[imageIndex];
    vkCmdBeginRenderPass (compositionCmd, &compositionBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (compositionCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gbuffer.lightPass.pipeline);

    vkCmdPushConstants (
      compositionCmd,
      gbuffer.lightPass.pipelineLayout,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      0,
      sizeof (LightPassPushConstants),
      &lightPushConstants
    );

    VKC_BIND_DESCRIPTORS (compositionCmd, gbuffer.lightPass.pipelineLayout, 1, &gbuffer.lightPass.descriptorSet);
    vkCmdDraw (compositionCmd, 3, 1, 0, 0);

    imguiContext.newFrame ();
    rei::imgui::showDebugWindow (&camera.speed, &lightPushConstants.target, allocator);
    ImGui::Render ();
    const ImDrawData* drawData = ImGui::GetDrawData ();
    imguiContext.updateBuffers (frameIndex, drawData);

    imguiContext.renderDrawData (compositionCmd, frameIndex, drawData);

    vkCmdEndRenderPass (compositionCmd);
    VK_CHECK (vkEndCommandBuffer (compositionCmd));

    { // Submit written commands to a queue
      VkSubmitInfo submitInfo;
      submitInfo.pNext = nullptr;
      submitInfo.sType = SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pCommandBuffers = &compositionCmd;
      submitInfo.pWaitDstStageMask = &pipelineWaitStage;
      submitInfo.pWaitSemaphores = &currentFrame->offscreenSemaphore;
      submitInfo.pSignalSemaphores = &currentFrame->compositionSemaphore;

      VK_CHECK (vkQueueSubmit (graphicsQueue, 1, &submitInfo, currentFrame->compositionFence));
    }

    // Present resulting image
    VkPresentInfoKHR presentInfo;
    presentInfo.pNext = nullptr;
    presentInfo.pResults = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.sType = PRESENT_INFO_KHR;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pSwapchains = &swapchain.handle;
    presentInfo.pWaitSemaphores = &currentFrame->compositionSemaphore;

    VK_CHECK (vkQueuePresentKHR (presentQueue, &presentInfo));
    ++frameIndex;
  }

RESOURCE_CLEANUP:

  // Wait for gpu to finish rendering of the last frame
  vkDeviceWaitIdle (device);

  rei::gltf::destroy (device, allocator, &sponza);
  rei::imgui::destroy (device, &imguiContext);
  destroyGBuffer (device, allocator, &gbuffer);

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
    vkDestroySemaphore (device, current->offscreenSemaphore, nullptr);
    vkDestroySemaphore (device, current->presentSemaphore, nullptr);
    vkDestroySemaphore (device, current->compositionSemaphore, nullptr);
    vkDestroyFence (device, current->offscreenFence, nullptr);
    vkDestroyFence (device, current->compositionFence, nullptr);
    vkDestroyCommandPool (device, current->commandPool, nullptr);
  }

  for (Uint32 index = 0; index < swapchain.imagesCount; ++index)
    vkDestroyFramebuffer (device, framebuffers[index], nullptr);

  free (framebuffers);

  vkDestroyRenderPass (device, defaultRenderPass, nullptr);
  rei::vku::destroySwapchain (device, allocator, &swapchain);

  vmaDestroyAllocator (allocator);
  vkDestroyDevice (device, nullptr);
  vkDestroySurfaceKHR (instance, windowSurface, nullptr);

  #ifndef NDEBUG
  vkDestroyDebugUtilsMessengerEXT (instance, debugMessenger, nullptr);
  #endif

  vkDestroyInstance (instance, nullptr);
  rei::xcb::destroyWindow (&window);
  rei::vkc::Context::shutdown ();
}
