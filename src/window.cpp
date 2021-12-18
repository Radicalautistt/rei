#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "window.hpp"
#include "vkcommon.hpp"

namespace rei::xcb {

void Window::getMousePosition (f32* out) {
  auto result = xcb_query_pointer_unchecked (connection, handle);
  auto reply = xcb_query_pointer_reply (connection, result, nullptr);

  out[0] = (f32) reply->win_x;
  out[1] = (f32) reply->win_y;
}

void createWindow (VkInstance instance, const WindowCreateInfo* createInfo, Window* out) {
  out->width = createInfo->width;
  out->height = createInfo->height;

  out->connection = xcb_connect (nullptr, nullptr);
  out->screen = (xcb_setup_roots_iterator (xcb_get_setup (out->connection))).data;
  out->handle = xcb_generate_id (out->connection);

  u32 values[] {
    out->screen->black_pixel,

    XCB_EVENT_MASK_EXPOSURE |
    XCB_EVENT_MASK_KEY_PRESS |
    XCB_EVENT_MASK_KEY_RELEASE |

    XCB_EVENT_MASK_BUTTON_PRESS |
    XCB_EVENT_MASK_BUTTON_RELEASE |

    XCB_EVENT_MASK_BUTTON_MOTION |
    XCB_EVENT_MASK_POINTER_MOTION |

    XCB_EVENT_MASK_ENTER_WINDOW |
    XCB_EVENT_MASK_LEAVE_WINDOW
  };

  xcb_create_window (
    out->connection,
    XCB_COPY_FROM_PARENT,
    out->handle,
    out->screen->root,
    createInfo->x,
    createInfo->y,
    createInfo->width,
    createInfo->height,
    0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    out->screen->root_visual,
    XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
    values
  );

  xcb_map_window (out->connection, out->handle);

  xcb_change_property (
    out->connection,
    XCB_PROP_MODE_REPLACE,
    out->handle,
    XCB_ATOM_WM_NAME,
    XCB_ATOM_STRING,
    8,
    (u32) strlen (createInfo->name),
    createInfo->name
  );

  xcb_flush (out->connection);

  VkXcbSurfaceCreateInfoKHR info;
  info.pNext = nullptr;
  info.flags = VKC_NO_FLAGS;
  info.window = out->handle;
  info.connection = out->connection;
  info.sType = XCB_SURFACE_CREATE_INFO_KHR;

  VKC_CHECK (vkCreateXcbSurfaceKHR (instance, &info, nullptr, &out->surface));
}

void destroyWindow (VkInstance instance, Window* window) {
  vkDestroySurfaceKHR (instance, window->surface, nullptr);
  xcb_destroy_window (window->connection, window->handle);
  xcb_disconnect (window->connection);
}

}
