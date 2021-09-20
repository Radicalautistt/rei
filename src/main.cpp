#include <stdio.h>
#include "vk.hpp"
#include "vkinit.hpp"
#include "window.hpp"
#include "vkutils.hpp"
#include "vkcommon.hpp"

int main () {
  VulkanContext::init ();

  rei::extra::xcb::Window window;

  { // Create window
    rei::extra::xcb::WindowCreateInfo createInfo;
    createInfo.x = 0;
    createInfo.y = 0;
    createInfo.width = 640;
    createInfo.height = 480;
    createInfo.name = "Rei playground";

    rei::extra::xcb::createWindow (createInfo, window);
  }

  VkInstance instance;
  #ifndef NDEBUG
  VkDebugUtilsMessengerEXT debugMessenger;
  #endif

  VkSurfaceKHR windowSurface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  uint32_t queueFamilyIndex;
  VkQueue graphicsQueue, presentQueue, transferQueue, computeQueue;

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

  { // Create window surface
    VkXcbSurfaceCreateInfoKHR createInfo {XCB_SURFACE_CREATE_INFO_KHR};
    createInfo.window = window.handle;
    createInfo.connection = window.connection;

    VK_CHECK (vkCreateXcbSurfaceKHR (instance, &createInfo, nullptr, &windowSurface));
  }

  { // Choose physical device, create logical device and swapchain
    rei::vkutils::QueueFamilyIndices indices;
    rei::vkutils::choosePhysicalDevice (instance, windowSurface, indices, physicalDevice);

    { // Logical device
      VkPhysicalDeviceFeatures enabledFeatures {};

      float queuePriority = 1.f;
      // NOTE All required queues have the same index on my device,
      // so I need only one queue create info. Perhaps, I might
      // handle them more appropriately in the future (so that this can work on different devices),
      // but for this will do.
      queueFamilyIndex = indices.graphics;

      VkDeviceQueueCreateInfo queueInfo {DEVICE_QUEUE_CREATE_INFO};
      queueInfo.pQueuePriorities = &queuePriority;
      queueInfo.queueCount = 1;
      queueInfo.queueFamilyIndex = queueFamilyIndex;

      VkDeviceCreateInfo createInfo {DEVICE_CREATE_INFO};
      createInfo.queueCreateInfoCount = 1;
      createInfo.pQueueCreateInfos = &queueInfo;
      createInfo.pEnabledFeatures = &enabledFeatures;
      createInfo.enabledExtensionCount = DEVICE_EXTENSIONS_COUNT;
      createInfo.ppEnabledExtensionNames = rei::vkcommon::requiredDeviceExtensions;

      VK_CHECK (vkCreateDevice (physicalDevice, &createInfo, nullptr, &device));

      VulkanContext::loadDevice (device);
      vkGetDeviceQueue (device, queueFamilyIndex, 0, &presentQueue);
      vkGetDeviceQueue (device, queueFamilyIndex, 0, &computeQueue);
      vkGetDeviceQueue (device, queueFamilyIndex, 0, &graphicsQueue);
      vkGetDeviceQueue (device, queueFamilyIndex, 0, &transferQueue);
    }
  }

  vkDestroyDevice (device, nullptr);

  vkDestroySurfaceKHR (instance, windowSurface, nullptr);

  #ifndef NDEBUG
  vkDestroyDebugUtilsMessengerEXT (instance, debugMessenger, nullptr);
  #endif

  vkDestroyInstance (instance, nullptr);
  rei::extra::xcb::destroyWindow (window);
  VulkanContext::shutdown ();
}
