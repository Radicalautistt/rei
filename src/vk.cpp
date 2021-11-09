#include <dlfcn.h>
#include <stdio.h>

#include "vk.hpp"
#include "common.hpp"

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

#define X(name) PFN_##name name = nullptr;
  VK_GLOBAL_FUNCTIONS
  VK_INSTANCE_FUNCTIONS
  VK_DEVICE_FUNCTIONS
#undef X

void* rei::VulkanContext::library = nullptr;

void rei::VulkanContext::init () {
  LOGS_INFO ("Loading Vulkan functions from " ANSI_YELLOW "libvulkan.so.1");
  library = dlopen ("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!library) {
    LOGS_WARNING ("Failed, going with " ANSI_RED "libvulkan.so" ANSI_YELLOW " instead");
    library = dlopen ("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  }

  REI_ASSERT (library);

  vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym (library, "vkGetInstanceProcAddr");

  // Load global functions
  #define X(name) name = (PFN_##name) vkGetInstanceProcAddr (nullptr, #name);
    VK_GLOBAL_FUNCTIONS
  #undef X
}

void rei::VulkanContext::shutdown () {
  dlclose (library);
}

void rei::VulkanContext::loadInstance (VkInstance instance) {
  #define X(name) name = (PFN_##name) vkGetInstanceProcAddr (instance, #name);
    VK_INSTANCE_FUNCTIONS
  #undef X
}

void rei::VulkanContext::loadDevice (VkDevice device) {
  #define X(name) name = (PFN_##name) vkGetDeviceProcAddr (device, #name);
    VK_DEVICE_FUNCTIONS
  #undef X
}
