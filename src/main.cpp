#include "vk.hpp"
#include "vkinit.hpp"
#include "vkcommon.hpp"

int main () {
  VulkanContext::init ();

  VkInstance instance;
  #ifndef NDEBUG
  VkDebugUtilsMessengerEXT debugMessenger;
  #endif

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

  #ifndef NDEBUG
  vkDestroyDebugUtilsMessengerEXT (instance, debugMessenger, nullptr);
  #endif

  vkDestroyInstance (instance, nullptr);
  VulkanContext::shutdown ();
}
