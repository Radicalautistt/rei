#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "window.hpp"

namespace rei::xcb {

void Window::getMousePosition (f32* output) {
  auto result = xcb_query_pointer_unchecked (connection, handle);
  auto reply = xcb_query_pointer_reply (connection, result, nullptr);

  output[0] = (f32) reply->win_x;
  output[1] = (f32) reply->win_y;
}

void createWindow (const WindowCreateInfo* createInfo, Window* output) {
  output->width = createInfo->width;
  output->height = createInfo->height;

  output->connection = xcb_connect (nullptr, nullptr);
  xcb_screen_iterator_t rootsIterator = xcb_setup_roots_iterator (xcb_get_setup (output->connection));
  output->screen = rootsIterator.data;
  output->handle = xcb_generate_id (output->connection);

  u32 valueMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  u32 eventMask = XCB_EVENT_MASK_EXPOSURE |
    XCB_EVENT_MASK_KEY_PRESS |
    XCB_EVENT_MASK_KEY_RELEASE |

    XCB_EVENT_MASK_BUTTON_PRESS |
    XCB_EVENT_MASK_BUTTON_RELEASE |

    XCB_EVENT_MASK_POINTER_MOTION |
    XCB_EVENT_MASK_BUTTON_MOTION |

    XCB_EVENT_MASK_ENTER_WINDOW |
    XCB_EVENT_MASK_LEAVE_WINDOW;

  u32 values[] {output->screen->black_pixel, eventMask};

  xcb_create_window (
    output->connection,
    XCB_COPY_FROM_PARENT,
    output->handle,
    output->screen->root,
    createInfo->x,
    createInfo->y,
    createInfo->width,
    createInfo->height,
    0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    output->screen->root_visual,
    valueMask,
    values
  );

  xcb_map_window (output->connection, output->handle);

  xcb_change_property (
    output->connection,
    XCB_PROP_MODE_REPLACE,
    output->handle,
    XCB_ATOM_WM_NAME,
    XCB_ATOM_STRING,
    8,
    (u32) strlen (createInfo->name),
    createInfo->name
  );

  xcb_flush (output->connection);
}

void destroyWindow (Window* window) {
  xcb_destroy_window (window->connection, window->handle);
  xcb_disconnect (window->connection);
}

}
