#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>

#include "vk.hpp"
#include "common.hpp"

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

#define X(name) PFN_##name name = nullptr;
  VK_GLOBAL_FUNCTIONS
  VK_INSTANCE_FUNCTIONS
  VK_DEVICE_FUNCTIONS
#undef X

void* VulkanContext::library = nullptr;

void VulkanContext::init () {
  puts ("Loading Vulkan functions from libvulkan.so.1");
  library = dlopen ("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!library) {
    puts ("Failed, going with libvulkan.so instead");
    library = dlopen ("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  }

  assert (library);

  vkGetInstanceProcAddr = RCAST <PFN_vkGetInstanceProcAddr> (dlsym (library, "vkGetInstanceProcAddr"));

  // Load global functions
  #define X(name) name = RCAST <PFN_##name> (vkGetInstanceProcAddr (nullptr, #name));
    VK_GLOBAL_FUNCTIONS
  #undef X
}

void VulkanContext::shutdown () {
  dlclose (library);
}

void VulkanContext::loadInstance (VkInstance instance) {
  #define X(name) name = RCAST <PFN_##name> (vkGetInstanceProcAddr (instance, #name));
    VK_INSTANCE_FUNCTIONS
  #undef X
}

void VulkanContext::loadDevice (VkDevice device) {
  #define X(name) name = RCAST <PFN_##name> (vkGetDeviceProcAddr (device, #name));
    VK_DEVICE_FUNCTIONS
  #undef X
}
