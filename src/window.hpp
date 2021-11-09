#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "common.hpp"

struct xcb_screen_t;
struct xcb_connection_t;

namespace rei::xcb {

struct WindowCreateInfo {
  Int16 x, y;
  Uint16 width, height;
  const char* name;
};

struct Window {
  Uint16 width, height;
  Uint32 handle;
  xcb_connection_t* connection;
  xcb_screen_t* screen;

  void getMousePosition (Float32* output);
};

void createWindow (const WindowCreateInfo* createInfo, Window* output);
void destroyWindow (Window* window);

}

#endif /* WINDOW_HPP */
