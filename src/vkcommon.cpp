#include "vkcommon.hpp"

namespace rei::vkc {

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
  [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
  [[maybe_unused]] void* userData) {

  fprintf (stderr, "%s\n\n", callbackData->pMessage);
  return VK_FALSE;
}

}
