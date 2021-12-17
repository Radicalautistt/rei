#include <dlfcn.h>

#include "vkcommon.hpp"

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

// Zero initialize function pointers
#define X(name) PFN_##name name = nullptr;
  VK_GLOBAL_FUNCTIONS
  VK_INSTANCE_FUNCTIONS
  VK_DEVICE_FUNCTIONS
#undef X

namespace rei::vkc {

void* Context::handle = nullptr;

void Context::init () {
  REI_LOGS_INFO ("Loading Vulkan functions from " ANSI_YELLOW "libvulkan.so.1");
  handle = dlopen ("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    REI_LOGS_WARN ("Failed, going with " ANSI_RED "libvulkan.so" ANSI_YELLOW " instead");
    handle = dlopen ("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
  }

  REI_ASSERT (handle);

  vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym (handle, "vkGetInstanceProcAddr");

  // Load global functions
  #define X(name) name = (PFN_##name) vkGetInstanceProcAddr (nullptr, #name);
    VK_GLOBAL_FUNCTIONS
  #undef X
}

void Context::shutdown () {
  dlclose (handle);
}

void Context::loadInstance (VkInstance instance) {
  #define X(name) name = (PFN_##name) vkGetInstanceProcAddr (instance, #name);
    VK_INSTANCE_FUNCTIONS
  #undef X
}

void Context::loadDevice (VkDevice device) {
  #define X(name) name = (PFN_##name) vkGetDeviceProcAddr (device, #name);
    VK_DEVICE_FUNCTIONS
  #undef X
}

const char* getError (VkResult error) noexcept {
  #define GET_ERROR(name) case VK_##name: return #name

  switch (error) {
    GET_ERROR (TIMEOUT);
    GET_ERROR (NOT_READY);
    GET_ERROR (EVENT_SET);
    GET_ERROR (INCOMPLETE);
    GET_ERROR (EVENT_RESET);
    GET_ERROR (SUBOPTIMAL_KHR);
    GET_ERROR (ERROR_DEVICE_LOST);
    GET_ERROR (ERROR_OUT_OF_DATE_KHR);
    GET_ERROR (ERROR_TOO_MANY_OBJECTS);
    GET_ERROR (ERROR_SURFACE_LOST_KHR);
    GET_ERROR (ERROR_MEMORY_MAP_FAILED);
    GET_ERROR (ERROR_LAYER_NOT_PRESENT);
    GET_ERROR (ERROR_INVALID_SHADER_NV);
    GET_ERROR (ERROR_OUT_OF_HOST_MEMORY);
    GET_ERROR (ERROR_FEATURE_NOT_PRESENT);
    GET_ERROR (ERROR_INCOMPATIBLE_DRIVER);
    GET_ERROR (ERROR_OUT_OF_DEVICE_MEMORY);
    GET_ERROR (ERROR_FORMAT_NOT_SUPPORTED);
    GET_ERROR (ERROR_INITIALIZATION_FAILED);
    GET_ERROR (ERROR_VALIDATION_FAILED_EXT);
    GET_ERROR (ERROR_EXTENSION_NOT_PRESENT);
    GET_ERROR (ERROR_NATIVE_WINDOW_IN_USE_KHR);
    GET_ERROR (ERROR_INCOMPATIBLE_DISPLAY_KHR);
    default: return "Unknown";
  }

  #undef GET_ERROR
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback (
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
  void* userData) {

  (void) messageType;
  (void) userData;

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    REI_LOG_ERROR ("%s\n", callbackData->pMessage);

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    REI_LOG_WARN ("%s\n", callbackData->pMessage);

  return VK_FALSE;
}

}
