#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "gltf.hpp"
#include "common.hpp"
#include "gltf_model.hpp"
#include "asset_baker.hpp"

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::gltf {

// Sort primitives by material index so that it would be easier to merge
// ones with the same material into batches.
static void sortPrimitives (assets::gltf::Primitive* primitives, Int32 low, Int32 high) {
  if (low >= 0 && high >= 0 && low < high) {
    Int32 pivotIndex = 0;
    // NOTE The cast of (low + high) to unsigned is made because division of
    // an unsigned integer by a constant is faster than that of a signed one.
    // After that, unsigned is cast back into signed, because conversion to Float32
    // is faster with signed integers. Also, casting signed->unsigned and vice versa is free.
    // Reference: Agner Fog's Optimization Manual 1, page 30 and 40.
    Uint32 middle = (Uint32) floorf ((Float32) ((Int32) ((Uint32) (low + high) / 2u)));
    Uint32 pivot = primitives[middle].material;

    Int32 left = low - 1;
    Int32 right = high + 1;

    for (;;) {
      do ++left; while (primitives[left].material < pivot);
      do --right; while (primitives[right].material > pivot);

      if (left >= right) {
        pivotIndex = right;
	break;
      }

      SWAP (&primitives[left], &primitives[right]);
    }

    sortPrimitives (primitives, low, pivotIndex);
    sortPrimitives (primitives, pivotIndex + 1, high);
  }
}

void load (
  VkDevice device,
  VmaAllocator allocator,
  const vku::TransferContext* transferContext,
  const char* relativePath,
  Model* output) {

  assets::gltf::Data gltf;
  assets::gltf::load (relativePath, &gltf);
  sortPrimitives (gltf.mesh.primitives, 0, (Int32) gltf.mesh.primitivesCount - 1);

  Uint32 vertexCount = 0, indexCount = 0;

  for (size_t index = 0; index < gltf.mesh.primitivesCount; ++index) {
    const auto current = &gltf.mesh.primitives[index];
    indexCount += gltf.accessors[current->indices].count;
    vertexCount += gltf.accessors[current->attributes.position].count;
  }

  Uint32 vertexOffset = 0, indexOffset = 0;
  auto vertices = MALLOC (Vertex, vertexCount);
  auto indices = MALLOC (Uint32, indexCount);

  output->batches = MALLOC (Batch, gltf.materialsCount);

  #define GET_ACCESSOR(attribute, result) do {                                               \
    const auto accessor = &gltf.accessors[currentPrimitive->attributes.attribute];           \
    const auto bufferView = &gltf.bufferViews[accessor->bufferView];                         \
    result = (const Float32*) (&gltf.buffer[accessor->byteOffset + bufferView->byteOffset]); \
  } while (0)

  Uint32 firstIndex = 0;
  Uint32 batchOffset = 0;
  Uint32 currentMaterial = 0;
  Uint32 currentIndexCount = 0;

  const size_t vec2Size = sizeof (Float32) * 2;
  const size_t vec3Size = sizeof (Float32) * 3;

  for (size_t primitive = 0; primitive < gltf.mesh.primitivesCount; ++primitive) {
    const auto currentPrimitive = &gltf.mesh.primitives[primitive];

    Uint32 vertexStart = vertexOffset;

    const Float32* uvAccessor = nullptr;
    const Float32* normalAccessor = nullptr;
    const Float32* positionAccessor = nullptr;

    GET_ACCESSOR (uv, uvAccessor);
    GET_ACCESSOR (normal, normalAccessor);
    GET_ACCESSOR (position, positionAccessor);

    Uint32 currentVertexCount = gltf.accessors[currentPrimitive->attributes.position].count;

    for (Uint32 vertex = 0; vertex < currentVertexCount; ++vertex) {
      auto newVertex = &vertices[vertexOffset++];

      memcpy (&newVertex->u, &uvAccessor[vertex * 2], vec2Size);
      memcpy (&newVertex->nx, &normalAccessor[vertex * 3], vec3Size);
      memcpy (&newVertex->x, &positionAccessor[vertex * 3], vec3Size);
    }

    const auto accessor = &gltf.accessors[currentPrimitive->indices];
    const auto bufferView = &gltf.bufferViews[accessor->bufferView];
    const auto indexAccessor = (const Uint16*) &gltf.buffer[accessor->byteOffset + bufferView->byteOffset];

    if (currentMaterial == currentPrimitive->material) {
      currentIndexCount += accessor->count;
    } else {
      auto newBatch = &output->batches[batchOffset++];
      newBatch->firstIndex = firstIndex;
      newBatch->indexCount = currentIndexCount;
      newBatch->materialIndex = currentMaterial;

      firstIndex = indexOffset;
      currentIndexCount = accessor->count;
      currentMaterial = currentPrimitive->material;
    }

    // FIXME This is ugly
    if (batchOffset == (gltf.materialsCount - 1)) {
      auto newBatch = &output->batches[batchOffset];
      newBatch->firstIndex = indexOffset;
      newBatch->indexCount = currentIndexCount;
      newBatch->materialIndex = currentMaterial;
    }

    for (Uint32 index = 0; index < accessor->count; ++index)
      indices[indexOffset++] = indexAccessor[index] + vertexStart;
  }

  #undef GET_ACCESSOR

  {
    vku::Buffer stagingBuffer;
    VkDeviceSize vertexBufferSize = sizeof (Vertex) * vertexCount;
    vku::allocateStagingBuffer (allocator, vertexBufferSize, &stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, vertices, vertexBufferSize);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      vku::BufferAllocationInfo allocationInfo;
      allocationInfo.size = vertexBufferSize;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

      vku::allocateBuffer (allocator, &allocationInfo, &output->vertexBuffer);
    }

    vku::copyBuffer (device, transferContext, &stagingBuffer, &output->vertexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  free (vertices);

  {
    vku::Buffer stagingBuffer;
    VkDeviceSize indexBufferSize = sizeof (Uint32) * indexCount;
    vku::allocateStagingBuffer (allocator, indexBufferSize, &stagingBuffer);

    VK_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));
    memcpy (stagingBuffer.mapped, indices, indexBufferSize);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);

    {
      vku::BufferAllocationInfo allocationInfo;
      allocationInfo.size = indexBufferSize;
      allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
      allocationInfo.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      vku::allocateBuffer (allocator, &allocationInfo, &output->indexBuffer);
    }

    vku::copyBuffer (device, transferContext, &stagingBuffer, &output->indexBuffer);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  free (indices);

  output->modelMatrix = {1.f};
  math::Matrix4::scale (&output->modelMatrix, &gltf.scaleVector);

  output->texturesCount = gltf.imagesCount;
  output->textures = MALLOC (vku::Image, gltf.imagesCount);

  char texturePath[256] {};
  strcpy (texturePath, relativePath);
  char* fileName = strrchr (texturePath, '/');

  for (size_t index = 0; index < gltf.imagesCount; ++index) {
    const auto current = &gltf.images[index];

    strcpy (fileName + 1, current->uri);
    char* extension = strrchr (texturePath, '.');
    memcpy (extension + 1, "rtex", 5);

    vku::TextureAllocationInfo allocationInfo;
    REI_CHECK (assets::readImage (texturePath, &allocationInfo));

    vku::allocateTexture (
      device,
      allocator,
      &allocationInfo,
      transferContext,
      &output->textures[index]
    );

    free (allocationInfo.pixels);
  }

  output->materialsCount = gltf.materialsCount;
  output->materials = MALLOC (Material, gltf.materialsCount);

  for (size_t index = 0; index < gltf.materialsCount; ++index)
    output->materials[index].albedoIndex = gltf.materials[index].baseColorTexture;

  assets::gltf::destroy (&gltf);
}

void destroy (VkDevice device, VmaAllocator allocator, Model* model) {
  vmaDestroyBuffer (allocator, model->indexBuffer.handle, model->indexBuffer.allocation);
  vmaDestroyBuffer (allocator, model->vertexBuffer.handle, model->vertexBuffer.allocation);

  vkDestroySampler (device, model->sampler, nullptr);
  free (model->materials);

  vkDestroyDescriptorPool (device, model->descriptorPool, nullptr);

  for (size_t index = 0; index < model->texturesCount; ++index) {
    auto current = &model->textures[index];
    vkDestroyImageView (device, current->view, nullptr);
    vmaDestroyImage (allocator, current->handle, current->allocation);
  }

  free (model->textures);
  free (model->batches);
}

void Model::initDescriptors (VkDevice device, VkDescriptorSetLayout descriptorLayout) {
  {
    VkDescriptorPoolSize poolSize {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (Uint32) texturesCount};

    VkDescriptorPoolCreateInfo createInfo {DESCRIPTOR_POOL_CREATE_INFO};
    createInfo.poolSizeCount = 1;
    createInfo.pPoolSizes = &poolSize;
    createInfo.maxSets = (Uint32) materialsCount;

    VK_CHECK (vkCreateDescriptorPool (device, &createInfo, nullptr, &descriptorPool));
  }

  {
    VkSamplerCreateInfo createInfo {SAMPLER_CREATE_INFO};
    createInfo.minLod = 0.f;
    // FIXME Don't hardcode number of mip levels
    createInfo.maxLod = 11.f;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VK_CHECK (vkCreateSampler (device, &createInfo, nullptr, &sampler));
  }

  // Batch all descriptor writes to make a single vkUpdateDescriptorSets call.
  auto writes = MALLOC (VkWriteDescriptorSet, materialsCount);
  auto imageInfos = MALLOC (VkDescriptorImageInfo, materialsCount);

  memset (writes, 0, sizeof (VkWriteDescriptorSet) * materialsCount);

  for (size_t index = 0; index < materialsCount; ++index) {
    auto current = &materials[index];

    {
      VkDescriptorSetAllocateInfo allocationInfo {DESCRIPTOR_SET_ALLOCATE_INFO};
      allocationInfo.descriptorSetCount = 1;
      allocationInfo.pSetLayouts = &descriptorLayout;
      allocationInfo.descriptorPool = descriptorPool;

      VK_CHECK (vkAllocateDescriptorSets (device, &allocationInfo, &current->descriptorSet));
    }

    auto albedoInfo = &imageInfos[index];
    albedoInfo->sampler = sampler;
    albedoInfo->imageView = textures[current->albedoIndex].view;
    albedoInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto write = &writes[index];
    write->dstBinding = 0;
    write->descriptorCount = 1;
    write->sType = WRITE_DESCRIPTOR_SET;
    write->pImageInfo = &imageInfos[index];
    write->dstSet = current->descriptorSet;
    write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  }

  vkUpdateDescriptorSets (device, (Uint32) materialsCount, writes, 0, nullptr);
  free (writes);
  free (imageInfos);
}

void Model::draw (VkCommandBuffer commandBuffer, VkPipelineLayout layout, const math::Matrix4* viewProjection) {
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers (commandBuffer, 0, 1, &vertexBuffer.handle, &offset);
  vkCmdBindIndexBuffer (commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

  math::Matrix4 matrices[2];
  math::Matrix4::mul (viewProjection, &modelMatrix, &matrices[0]);
  matrices[1] = modelMatrix;
  vkCmdPushConstants (commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (math::Matrix4) * 2, matrices);

  for (size_t index = 0; index < materialsCount; ++index) {
    const auto current = &batches[index];

    VKC_BIND_DESCRIPTORS (commandBuffer, layout, 1, &materials[current->materialIndex].descriptorSet);
    vkCmdDrawIndexed (commandBuffer, current->indexCount, 1, current->firstIndex, 0, 0);
  }
}

}
