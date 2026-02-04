#include "os/window.h"

#include <emscripten/html5.h>
#include <emscripten.h>

static const char* CANVAS_ID = "#canvas";

WindowRes Window::Create(u32 width, u32 height, const char* title) {
  EMSCRIPTEN_RESULT res = emscripten_set_canvas_element_size(CANVAS_ID, width, height);

  if (res != EMSCRIPTEN_RESULT_SUCCESS) {
    logError("Failed to find canvas element '%s'", CANVAS_ID);
    return WindowRes::Err(ErrorCode::PLATFORM_ERROR);
  }

  if (title) {
    EM_ASM({
      document.title = UTF8ToString($0);
    }, title);
  }

  Window window = {};
  window.handle = nullptr;
  window.width = width;
  window.height = height;
  window.shouldClose = false;
  window.resized = false;

  logInfo("Canvas configured successfully.");
  return WindowRes::Ok(window);
}

void Window::Size(u32* width, u32* height) {
  double w, h;
  emscripten_get_element_css_size(CANVAS_ID, &w, &h);
  if (width)  *width  = (u32)w;
  if (height) *height = (u32)h;
}

void Window::destroy() {}
