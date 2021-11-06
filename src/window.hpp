#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <stdint.h>

struct xcb_screen_t;
struct xcb_connection_t;

namespace rei::xcb {

struct WindowCreateInfo {
  int16_t x, y;
  uint16_t width, height;
  const char* name;
};

struct Window {
  uint16_t width, height;
  uint32_t handle;
  xcb_connection_t* connection;
  xcb_screen_t* screen;

  void getMousePosition (float* output);
};

void createWindow (const WindowCreateInfo* createInfo, Window* output);
void destroyWindow (Window* window);

}

#endif /* WINDOW_HPP */
