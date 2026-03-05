#include "os/window.h"

#include <emscripten/html5.h>
#include <emscripten.h>

static const char* CANVAS_ID = "#canvas";

EM_JS(void, SetupResizeHandler, (), {
  function resizeCanvas() {
    var canvas = document.getElementById('canvas');
    if (!canvas) return;

    var dpr = window.devicePixelRatio || 1;
    var w = window.innerWidth;
    var h = window.innerHeight;

    canvas.style.width = w + 'px';
    canvas.style.height = h + 'px';
  }

  window.addEventListener('resize', resizeCanvas);
  
  if (window.ResizeObserver) {
    new ResizeObserver(resizeCanvas).observe(document.body);
  }

  resizeCanvas();
})

WindowRes Window::Create(const char* title) {
	SetupResizeHandler();


	f64 dpr = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1; });
	double w, h;
  emscripten_get_element_css_size(CANVAS_ID, &w, &h);

  u32 width  = (u32)(w * dpr);
  u32 height = (u32)(h * dpr);

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

void Window::GetFramebufferSize(u32* width, u32* height) {
	double w, h;
  emscripten_get_element_css_size(CANVAS_ID, &w, &h);
  double dpr = emscripten_get_device_pixel_ratio();
  
  if (width)  *width  = (u32)(w * dpr);
  if (height) *height = (u32)(h * dpr);
}

void Window::destroy() {}
