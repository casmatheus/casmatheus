#pragma once

#include "core/primitive.h"
#include "game/font.h"
#include "gpu/renderer.h"

namespace TextRenderer {
  void Init();
  
  void BeginFrame(u32 width, u32 height);
  
  void DrawText(const Font* font, const char* text, f32 x, f32 y, f32 fontSize, const f32 color[4]);
	f32 MeasureText(const Font* font, const char* text, f32 fontSize);

  void EndFrame();
}
