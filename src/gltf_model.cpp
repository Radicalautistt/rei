#include <string.h>

#include "gltf.hpp"
#include "common.hpp"
#include "rei_math.inl"
#include "gltf_model.hpp"
#include "asset_baker.hpp"

#include <VulkanMemoryAllocator/include/vk_mem_alloc.h>

namespace rei::gltf {

// Sort primitives by material index so that it would be easier to merge
// ones with the same material into batches.
static void sortPrimitives (assets::gltf::Primitive* primitives, i32 low, i32 high) {
  if (low >= 0 && high >= 0 && low < high) {
    i32 pivotIndex = 0;
    // NOTE The cast of (low + high) to unsigned is made because division of
    // an unsigned integer by a constant is faster than that of a signed one.
    // After that, unsigned is cast back into signed, because conversion to f32
    // is faster with signed integers. Also, casting signed->unsigned and vice versa is free.
    // Reference: Agner Fog's Optimization Manual 1, page 30 and 40.
    u32 middle = (u32) floorf ((f32) ((i32) ((u32) (low + high) / 2u)));
    u32 pivot = primitives[middle].material;

    i32 left = low - 1;
    i32 right = high + 1;

    for (;;) {
      do ++left; while (primitives[left].material < pivot);
      do --right; while (primitives[right].material > pivot);

      if (left >= right) {
        pivotIndex = right;
	break;
      }

      REI_SWAP (&primitives[left], &primitives[right]);
    }

    sortPrimitives (primitives, low, pivotIndex);
    sortPrimitives (primitives, pivotIndex + 1, high);
  }
}

void load (
  VkDevice device,
  VmaAllocator allocator,
  const vku::TransferContext* transferContext,
  VkDescriptorSetLayout descriptorLayout,
  const char* relativePath,
  Model* out) {

  assets::gltf::Data gltf;
  assets::gltf::load (relativePath, &gltf);
  sortPrimitives (gltf.mesh.primitives, 0, (i32) gltf.mesh.primitivesCount - 1);

  u32 vertexCount = 0, indexCount = 0;

  for (size_t index = 0; index < gltf.mesh.primitivesCount; ++index) {
    const auto current = &gltf.mesh.primitives[index];
    indexCount += gltf.accessors[current->indices].count;
    vertexCount += gltf.accessors[current->attributes.position].count;
  }

  vku::Buffer stagingBuffer;
  u32 vertexOffset = 0, indexOffset = 0;
  auto indexBufferSize = (VkDeviceSize) (sizeof (u32) * indexCount);
  auto vertexBufferSize = (VkDeviceSize) (sizeof (Vertex) * vertexCount);

  vku::allocateStagingBuffer (allocator, vertexBufferSize + indexBufferSize, &stagingBuffer);
  VKC_CHECK (vmaMapMemory (allocator, stagingBuffer.allocation, &stagingBuffer.mapped));

  auto vertices = (Vertex*) stagingBuffer.mapped;
  auto indices = (u32*) ((u8*) stagingBuffer.mapped + vertexBufferSize);

  out->batches = REI_MALLOC (Batch, gltf.materialsCount);

  #define GET_ACCESSOR(attribute, result) do {                                           \
    const auto accessor = &gltf.accessors[currentPrimitive->attributes.attribute];       \
    const auto bufferView = &gltf.bufferViews[accessor->bufferView];                     \
    result = (const f32*) (&gltf.buffer[accessor->byteOffset + bufferView->byteOffset]); \
  } while (0)

  u32 firstIndex = 0;
  u32 batchOffset = 0;
  u32 currentMaterial = 0;
  u32 currentIndexCount = 0;

  const size_t vec2Size = sizeof (f32) * 2;
  const size_t vec3Size = sizeof (f32) * 3;

  for (size_t primitive = 0; primitive < gltf.mesh.primitivesCount; ++primitive) {
    const auto currentPrimitive = &gltf.mesh.primitives[primitive];

    u32 vertexStart = vertexOffset;

    const f32* uvAccessor = nullptr;
    const f32* normalAccessor = nullptr;
    const f32* positionAccessor = nullptr;

    GET_ACCESSOR (uv, uvAccessor);
    GET_ACCESSOR (normal, normalAccessor);
    GET_ACCESSOR (position, positionAccessor);

    u32 currentVertexCount = gltf.accessors[currentPrimitive->attributes.position].count;

    for (u32 vertex = 0; vertex < currentVertexCount; ++vertex) {
      auto newVertex = &vertices[vertexOffset++];

      memcpy (&newVertex->u, &uvAccessor[vertex * 2], vec2Size);
      memcpy (&newVertex->nx, &normalAccessor[vertex * 3], vec3Size);
      memcpy (&newVertex->x, &positionAccessor[vertex * 3], vec3Size);
    }

    const auto accessor = &gltf.accessors[currentPrimitive->indices];
    const auto bufferView = &gltf.bufferViews[accessor->bufferView];
    const u16* indexAccessor = (const u16*) &gltf.buffer[accessor->byteOffset + bufferView->byteOffset];

    if (currentMaterial == currentPrimitive->material) {
      currentIndexCount += accessor->count;
    } else {
      auto newBatch = &out->batches[batchOffset++];
      newBatch->firstIndex = firstIndex;
      newBatch->indexCount = currentIndexCount;
      newBatch->materialIndex = currentMaterial;

      firstIndex = indexOffset;
      currentIndexCount = accessor->count;
      currentMaterial = currentPrimitive->material;
    }

    // FIXME This is ugly
    if (batchOffset == (gltf.materialsCount - 1)) {
      auto newBatch = &out->batches[batchOffset];
      newBatch->firstIndex = indexOffset;
      newBatch->indexCount = currentIndexCount;
      newBatch->materialIndex = currentMaterial;
    }

    for (u32 index = 0; index < accessor->count; ++index)
      indices[indexOffset++] = indexAccessor[index] + vertexStart;
  }

  #undef GET_ACCESSOR

  {
    vku::BufferAllocationInfo allocationInfo;
    allocationInfo.size = vertexBufferSize;
    allocationInfo.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vku::allocateBuffer (allocator, &allocationInfo, &out->vertexBuffer);

    allocationInfo.size = indexBufferSize;
    allocationInfo.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    allocationInfo.bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vku::allocateBuffer (allocator, &allocationInfo, &out->indexBuffer);

    VkCommandBuffer cmdBuffer;
    vku::startImmediateCmd (device, transferContext, &cmdBuffer);

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = vertexBufferSize;

    vkCmdCopyBuffer (cmdBuffer, stagingBuffer.handle, out->vertexBuffer.handle, 1, &copyRegion);

    copyRegion.size = indexBufferSize;
    copyRegion.srcOffset = vertexBufferSize;
    vkCmdCopyBuffer (cmdBuffer, stagingBuffer.handle, out->indexBuffer.handle, 1, &copyRegion);

    vku::submitImmediateCmd (device, transferContext, cmdBuffer);
    vmaUnmapMemory (allocator, stagingBuffer.allocation);
    vmaDestroyBuffer (allocator, stagingBuffer.handle, stagingBuffer.allocation);
  }

  out->modelMatrix = {1.f};
  math::mat4::scale (&out->modelMatrix, &gltf.scaleVector);

  out->texturesCount = gltf.imagesCount;
  out->textures = REI_MALLOC (vku::Image, gltf.imagesCount);

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

    vku::allocateTexture (device, allocator, &allocationInfo, transferContext, &out->textures[index]);
    free (allocationInfo.pixels);
  }

  out->materialsCount = gltf.materialsCount;
  const u32 materialsCount = (u32) out->materialsCount;
  auto materials = REI_MALLOC (Material, materialsCount);

  for (size_t i = 0; i < materialsCount; ++i)
    materials[i].albedoIndex = gltf.materials[i].baseColorTexture;

  assets::gltf::destroy (&gltf);

  {
    VkDescriptorPoolSize poolSize {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, materialsCount};

    VkDescriptorPoolCreateInfo createInfo;
    createInfo.poolSizeCount = 1;
    createInfo.pPoolSizes = &poolSize;
    createInfo.maxSets = materialsCount;
    createInfo.sType = DESCRIPTOR_POOL_CREATE_INFO;

    createInfo.pNext = nullptr;
    createInfo.flags = VKC_NO_FLAGS;

    VKC_CHECK (vkCreateDescriptorPool (device, &createInfo, nullptr, &out->descriptorPool));
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

    VKC_CHECK (vkCreateSampler (device, &createInfo, nullptr, &out->sampler));
  }

  auto writes = REI_MALLOC (VkWriteDescriptorSet, materialsCount);
  auto imageInfos = REI_MALLOC (VkDescriptorImageInfo, materialsCount);
  out->descriptors = REI_MALLOC (VkDescriptorSet, materialsCount);

  { // Batch descriptor set allocations
    auto descriptorLayouts = REI_ALLOCA (VkDescriptorSetLayout, materialsCount);
    for (u32 i = 0; i < materialsCount; ++i) descriptorLayouts[i] = descriptorLayout;

    VkDescriptorSetAllocateInfo allocationInfo;
    allocationInfo.pNext = nullptr;
    allocationInfo.pSetLayouts = descriptorLayouts;
    allocationInfo.descriptorSetCount = materialsCount;
    allocationInfo.descriptorPool = out->descriptorPool;
    allocationInfo.sType = DESCRIPTOR_SET_ALLOCATE_INFO;

    VKC_CHECK (vkAllocateDescriptorSets (device, &allocationInfo, out->descriptors));
  }

  // Batch all descriptor writes to make a single vkUpdateDescriptorSets call.
  for (size_t index = 0; index < materialsCount; ++index) {
    auto current = &materials[index];

    auto albedoInfo = &imageInfos[index];
    albedoInfo->sampler = out->sampler;
    albedoInfo->imageView = out->textures[current->albedoIndex].view;
    albedoInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto write = &writes[index];
    write->dstBinding = 0;
    write->descriptorCount = 1;
    write->sType = WRITE_DESCRIPTOR_SET;
    write->pImageInfo = &imageInfos[index];
    write->dstSet = out->descriptors[index];
    write->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    write->pNext = nullptr;
    write->dstArrayElement = 0;
    write->pBufferInfo = nullptr;
    write->pTexelBufferView = nullptr;
  }

  vkUpdateDescriptorSets (device, materialsCount, writes, 0, nullptr);
  free (writes);
  free (materials);
  free (imageInfos);
}

void destroy (VkDevice device, VmaAllocator allocator, Model* model) {
  vmaDestroyBuffer (allocator, model->indexBuffer.handle, model->indexBuffer.allocation);
  vmaDestroyBuffer (allocator, model->vertexBuffer.handle, model->vertexBuffer.allocation);

  vkDestroySampler (device, model->sampler, nullptr);

  vkDestroyDescriptorPool (device, model->descriptorPool, nullptr);
  free (model->descriptors);

  for (size_t index = 0; index < model->texturesCount; ++index) {
    auto current = &model->textures[index];
    vkDestroyImageView (device, current->view, nullptr);
    vmaDestroyImage (allocator, current->handle, current->allocation);
  }

  free (model->textures);
  free (model->batches);
}

void Model::draw (VkCommandBuffer cmdBuffer, VkPipelineLayout layout, const math::Mat4* viewProjection) {
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers (cmdBuffer, 0, 1, &vertexBuffer.handle, &offset);
  vkCmdBindIndexBuffer (cmdBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

  math::Mat4 matrices[2];
  math::mat4::mul (viewProjection, &modelMatrix, &matrices[0]);
  matrices[1] = modelMatrix;
  vkCmdPushConstants (cmdBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof (math::Mat4) * 2, matrices);

  for (size_t index = 0; index < materialsCount; ++index) {
    const auto current = &batches[index];

    VKC_BIND_DESCRIPTORS (cmdBuffer, layout, 1, &descriptors[current->materialIndex]);
    vkCmdDrawIndexed (cmdBuffer, current->indexCount, 1, current->firstIndex, 0, 0);
  }
}

}
