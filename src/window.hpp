#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "common.hpp"

struct xcb_screen_t;
struct xcb_connection_t;

#define KEY_W 25
#define KEY_A 38
#define KEY_S 39
#define KEY_D 40
#define MOUSE_LEFT 1
#define KEY_ESCAPE 9
#define MOUSE_RIGHT 3

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
