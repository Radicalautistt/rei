#include "imgui.hpp"
#include "common.hpp"

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
}

void destroy (VkDevice device, VmaAllocator allocator, Context& context) {
  vkDestroyDescriptorSetLayout (device, context.descriptorSetLayout, nullptr);
  vkDestroySampler (device, context.fontSampler, nullptr);
  vkDestroyImageView (device, context.fontTexture.view, nullptr);
  vmaDestroyImage (allocator, context.fontTexture.handle, context.fontTexture.allocation);
  ImGui::DestroyContext (context.handle);
}

}
