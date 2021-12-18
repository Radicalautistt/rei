#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "rei_types.hpp"

struct xcb_screen_t;
struct xcb_connection_t;

struct VkInstance_T;
struct VkSurfaceKHR_T;
typedef VkInstance_T* VkInstance;
typedef VkSurfaceKHR_T* VkSurfaceKHR;

#define KEY_W 25
#define KEY_A 38
#define KEY_S 39
#define KEY_D 40
#define MOUSE_LEFT 1
#define KEY_ESCAPE 9
#define MOUSE_RIGHT 3

namespace rei::xcb {

struct WindowCreateInfo {
  i16 x, y;
  u16 width, height;
  const char* name;
};

struct Window {
  u16 width, height;
  u32 handle;
  xcb_connection_t* connection;
  xcb_screen_t* screen;
  VkSurfaceKHR surface;

  void getMousePosition (f32* out);
};

void createWindow (VkInstance instance, const WindowCreateInfo* createInfo, Window* out);
void destroyWindow (VkInstance instance, Window* window);

}

#endif /* WINDOW_HPP */
