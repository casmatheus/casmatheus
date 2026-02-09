#pragma once
#include "core/primitive.h"
#include "gpu/renderer.h"

namespace SpriteRenderer {
  void Init();
  void BeginFrame(const f32* viewProj);
  void EndFrame();

  void Draw(f32 x, f32 y, f32 w, f32 h, 
            TextureID texture, 
            const f32 color[4],
						f32 rotation = 0.0f,
            f32 u0 = 0.0f, f32 v0 = 0.0f, f32 u1 = 1.0f, f32 v1 = 1.0f);
}
