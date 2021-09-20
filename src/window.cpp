#include <string.h>

#include <xcb/xcb.h>

#include "common.hpp"
#include "window.hpp"

namespace rei::extra::xcb {

void createWindow (const WindowCreateInfo& createInfo, Window& output) {
  output.width = createInfo.width;
  output.height = createInfo.height;

  output.connection = xcb_connect (nullptr, nullptr);
  xcb_screen_iterator_t rootsIterator = xcb_setup_roots_iterator (xcb_get_setup (output.connection));
  output.screen = rootsIterator.data;
  output.handle = xcb_generate_id (output.connection);

  uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  uint32_t values[] {output.screen->black_pixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS};

  xcb_create_window (
    output.connection,
    XCB_COPY_FROM_PARENT,
    output.handle,
    output.screen->root,
    createInfo.x,
    createInfo.y,
    createInfo.width,
    createInfo.height,
    0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    output.screen->root_visual,
    mask,
    values
  );

  xcb_map_window (output.connection, output.handle);

  xcb_change_property (
    output.connection,
    XCB_PROP_MODE_REPLACE,
    output.handle,
    XCB_ATOM_WM_NAME,
    XCB_ATOM_STRING,
    8,
    SCAST <uint32_t> (strlen (createInfo.name)),
    createInfo.name
  );

  xcb_flush (output.connection);
}

void destroyWindow (Window& window) {
  xcb_destroy_window (window.connection, window.handle);
  xcb_disconnect (window.connection);
}

}
