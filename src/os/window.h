#pragma once

#include "core/result.h"
#include "core/primitive.h"

struct Window;
typedef Result<Window, ErrorCode> WindowRes;

struct Window {
  // Platform specific handle (HWND on Windows, unused on Web)
  void* handle;
  
  u32 width;
  u32 height;
  
  bool shouldClose;
  bool resized;
	
	static WindowRes Create(const char* title);

	static void Size(u32* width, u32* height);

	static void GetFramebufferSize(u32* width, u32* height);
	
	void destroy();
};
