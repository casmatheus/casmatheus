#pragma once

#include "core/primitive.h"
#include "gpu/renderer.h"

namespace ShapeRenderer {
  void Init();
  
  void BeginFrame(const f32* viewProj);
  void EndFrame();

  void DrawShape(f32 x, f32 y, f32 w, f32 h, 
                 const f32 color[4], 
                 const f32 borderColor[4], 
                 f32 radius, 
                 f32 borderThickness,
                 f32 edgeSoftness, 
                 TextureID textureID = INVALID_ID);

  inline void DrawRect(f32 x, f32 y, f32 w, f32 h, 
                       const f32 color[4], 
                       const f32 borderColor[4] = nullptr, 
                       f32 radius = 0.0f, 
                       f32 borderThickness = 0.0f,
                       f32 edgeSoftness = 1.0f,
                       TextureID textureID = INVALID_ID) 
  {
    const f32* bc = borderColor ? borderColor : color;
    DrawShape(x, y, w, h, color, bc, radius, borderThickness, edgeSoftness, textureID);
  }

  inline void DrawCircle(f32 x, f32 y, f32 diameter, 
                         const f32 color[4], 
                         const f32 borderColor[4] = nullptr,
                         f32 borderThickness = 0.0f,
                         f32 edgeSoftness = 1.0f,
                         TextureID textureID = INVALID_ID) 
  {
	const f32* bc = borderColor ? borderColor : color;
        
  f32 r = (diameter * 0.5f) - edgeSoftness;
  if (r < 0.0f) r = 0.0f;

  DrawShape(x, y, diameter, diameter, color, bc, r, borderThickness, edgeSoftness, textureID);
}
}










