#pragma once

#include "core/primitive.h"
#include "gpu/renderer.h"

using FontID = u32;

#pragma pack(push, 1)
struct FontHeader {
  u32 magic; 					// 0x464F4E54 "FONT"
  u32 textureWidth;
  u32 textureHeight;
  f32 lineHeight;
  f32 ascender;
  f32 descender;
  f32 underlineY;
  f32 underlineThickness;
  u32 glyphCount;
  u32 kerningCount;
	f32 pxRange;
  u32 _padding;
};

struct FontGlyph {
  u32 unicode;
  f32 advance;
  f32 planeLeft, planeBottom, planeRight, planeTop;
  f32 atlasLeft, atlasBottom, atlasRight, atlasTop;
};

struct FontKerning {
  u32 u1;
  u32 u2;
  f32 advance;
};
#pragma pack(pop)

struct Font {
  bool loaded = false;
  FontHeader header;
  
  FontGlyph* glyphs = nullptr;
  FontKerning* kernings = nullptr;
  
  void* rawData = nullptr; 
  TextureID texture = INVALID_ID;

	static Font* Load(const char* fontName);

  void destroy();
  const FontGlyph* glyph(u32 unicode) const;
  f32 kerning(u32 u1, u32 u2) const;
};
