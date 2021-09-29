#include "imgui.hpp"
#include "common.hpp"

#include <glm/vec2.hpp>
#include <imgui/imgui.h>
#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::extra::imgui {

void create (const ContextCreateInfo& createInfo, Context& output) {
  output.handle = ImGui::CreateContext ();
  output.device = createInfo.device;
  output.allocator = createInfo.allocator;
  output.transferContext = createInfo.transferContext;

  { // Create font texture
    int width, height;
    unsigned char* pixels;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32 (&pixels, &width, &height);

    vkutils::TextureAllocationInfo allocationInfo;
    allocationInfo.width = SCAST <uint32_t> (width);
    allocationInfo.height = SCAST <uint32_t> (height);
    allocationInfo.pixels = RCAST <const char*> (pixels);

    vkutils::allocateTexture (
      output.device,
      output.allocator,
      allocationInfo,
      *output.transferContext,
      output.fontTexture
    );
  }

  { // Create font sampler
    VkSamplerCreateInfo samplerInfo {SAMPLER_CREATE_INFO};
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VK_CHECK (vkCreateSampler (output.device, &samplerInfo, nullptr, &output.fontSampler));
  }

  { // Create descriptor set layout for font sampler
    VkDescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorCount = 1;
    binding.pImmutableSamplers = nullptr;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    VkDescriptorSetLayoutCreateInfo layoutInfo {DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    VK_CHECK (vkCreateDescriptorSetLayout (output.device, &layoutInfo, nullptr, &output.descriptorSetLayout));
  }

  { // Allocate descriptor set for font sampler
    VkDescriptorSetAllocateInfo allocationInfo {DESCRIPTOR_SET_ALLOCATE_INFO};
    allocationInfo.descriptorSetCount = 1;
    allocationInfo.pSetLayouts = &output.descriptorSetLayout;
    allocationInfo.descriptorPool = createInfo.descriptorPool;

    VK_CHECK (vkAllocateDescriptorSets (output.device, &allocationInfo, &output.descriptorSet));
  }

  {
    VkDescriptorImageInfo fontImageInfo;
    fontImageInfo.sampler = output.fontSampler;
    fontImageInfo.imageView = output.fontTexture.view;
    fontImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write {WRITE_DESCRIPTOR_SET};
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.pImageInfo = &fontImageInfo;
    write.dstSet = output.descriptorSet;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    vkUpdateDescriptorSets (output.device, 1, &write, 0, nullptr);
  }

  {
    VkPushConstantRange pushConstantRange;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof (glm::vec2) * 2;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo createInfo {PIPELINE_LAYOUT_CREATE_INFO};
    createInfo.setLayoutCount = 1;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstantRange;
    createInfo.pSetLayouts = &output.descriptorSetLayout;

    VK_CHECK (vkCreatePipelineLayout (output.device, &createInfo, nullptr, &output.pipelineLayout));
  }

  {
    vkutils::GraphicsPipelineCreateInfo createInfos[1];

    VkVertexInputBindingDescription binding;
    binding.binding = 0;
    binding.stride = sizeof (ImDrawVert);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3];

    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = offsetof (ImDrawVert, pos);

    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = offsetof (ImDrawVert, uv);

    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attributes[2].offset = offsetof (ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo {PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &binding;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;
    vertexInputInfo.vertexAttributeDescriptionCount = ARRAY_SIZE (attributes);

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo {PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportInfo {PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportInfo.scissorCount = 1;
    viewportInfo.viewportCount = 1;
    viewportInfo.pScissors = nullptr;
    viewportInfo.pViewports = nullptr;

    VkDynamicState dynamicStates[2] {
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VIEWPORT
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo {PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicInfo.pDynamicStates = dynamicStates;
    dynamicInfo.dynamicStateCount = ARRAY_SIZE (dynamicStates);

    VkPipelineRasterizationStateCreateInfo rasterizationInfo {PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationInfo.lineWidth = 1.f;
    rasterizationInfo.depthBiasEnable = VK_FALSE;
    rasterizationInfo.depthClampEnable = VK_FALSE;
    rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampleInfo {PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleInfo.minSampleShading = 1.f;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo {PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilInfo.minDepthBounds = 0.f;
    depthStencilInfo.maxDepthBounds = 1.f;

    depthStencilInfo.depthTestEnable = VK_FALSE;
    depthStencilInfo.depthWriteEnable = VK_FALSE;
    depthStencilInfo.stencilTestEnable = VK_FALSE;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachmentInfo;
    blendAttachmentInfo.blendEnable = VK_TRUE;
    blendAttachmentInfo.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentInfo.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    blendAttachmentInfo.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    blendAttachmentInfo.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    blendAttachmentInfo.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    blendAttachmentInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo {PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendInfo.pAttachments = &blendAttachmentInfo;

    createInfos[0].layout = output.pipelineLayout;
    createInfos[0].renderPass = createInfo.renderPass;
    createInfos[0].pixelShaderPath = "assets/shaders/imgui.frag.spv";
    createInfos[0].vertexShaderPath = "assets/shaders/imgui.vert.spv";

    createInfos[0].vertexInputInfo = &vertexInputInfo;
    createInfos[0].inputAssemblyInfo = &inputAssemblyInfo;
    createInfos[0].viewportInfo = &viewportInfo;
    createInfos[0].dynamicInfo = &dynamicInfo;
    createInfos[0].rasterizationInfo = &rasterizationInfo;
    createInfos[0].multisampleInfo = &multisampleInfo;
    createInfos[0].colorBlendInfo = &colorBlendInfo;
    createInfos[0].depthStencilInfo = &depthStencilInfo;

    rei::vkutils::createGraphicsPipelines (
      output.device,
      VK_NULL_HANDLE,
      ARRAY_SIZE (createInfos),
      createInfos,
      &output.pipeline
    );
  }
}

void destroy (VkDevice device, VmaAllocator allocator, Context& context) {
  vkDestroyPipelineLayout (device, context.pipelineLayout, nullptr);
  vkDestroyPipeline (device, context.pipeline, nullptr);
  vkDestroyDescriptorSetLayout (device, context.descriptorSetLayout, nullptr);
  vkDestroySampler (device, context.fontSampler, nullptr);
  vkDestroyImageView (device, context.fontTexture.view, nullptr);
  vmaDestroyImage (allocator, context.fontTexture.handle, context.fontTexture.allocation);
  ImGui::DestroyContext (context.handle);
}

}
