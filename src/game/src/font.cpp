#include "game/font.h"
#include "os/network.h"
#include "js/texture.h"
#include "debug/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

constexpr u32 MAX_FONTS = 2;
static Font s_Fonts[MAX_FONTS];

// Callback for when the .bin file finishes downloading
static void onFontLoaded(const FetchResult& result, void* userData) {
	
	Font* font = (Font*)userData;

  if (!result) {
    logError("Failed to load font binary!");
    return;
  }

	font->rawData = malloc(result.value.size);
  if (!font->rawData) {
    logError("Failed to allocate memory for font data");
    return;
  }
  memcpy(font->rawData, result.value.data, result.value.size);

  FontHeader* header = (FontHeader*)font->rawData;
  if (header->magic != 0x464F4E54) {
    logError("Invalid Font Magic Number");
    return;
  }
  
  font->header = *header;

  u8* ptr = (u8*)font->rawData;
  ptr += sizeof(FontHeader);

  font->glyphs = (FontGlyph*)ptr;
  ptr += sizeof(FontGlyph) * header->glyphCount;

  font->kernings = (FontKerning*)ptr;

  font->loaded = true;
  logInfo("Font Binary Loaded: %u glyphs, %u kernings", header->glyphCount, header->kerningCount);
}

Font* Font::Load(const char* fontName) {
	Font* font = nullptr;

  for (u32 i = 0; i < MAX_FONTS; ++i) {
    if (s_Fonts[i].texture == INVALID_ID) {
      font = &s_Fonts[i];
      break;
    }
  }

  if (!font) {
    logError("Max Fonts (%u) Reached! Cannot load %s", MAX_FONTS, fontName);
    return nullptr;
  }

  char texUrl[128];
  snprintf(texUrl, 128, "Fonts/%s/atlas.png", fontName);
  font->texture = CreateTextureFromURL(texUrl);

  char binUrl[128];
  snprintf(binUrl, 128, "Fonts/%s/metrics.bin", fontName);
	fetchFile(binUrl, onFontLoaded, font);

	return font;
}

void Font::destroy() {
  if (rawData) {
    free(rawData);
    rawData = nullptr;
  }

	if (texture != INVALID_ID) {
    Renderer::DestroyTexture(texture);
    texture = INVALID_ID;
  }

	loaded = false;
  glyphs = nullptr;
  kernings = nullptr;
}

const FontGlyph* Font::glyph(u32 unicode) const {
  if (!loaded) { 
		logError("The Font was not Properly Loaded");
		return nullptr;
	}

	if (!glyphs) {
		logError("There are no Glyphs in this Font");
		return nullptr;
	}

  for (u32 i = 0; i < header.glyphCount; ++i) {
    if (glyphs[i].unicode == unicode) {
      return &glyphs[i];
    }
  }
  return nullptr;
}

f32 Font::kerning(u32 u1, u32 u2) const {
  if (!loaded) { 
		logError("The Font was not Properly Loaded");
		return 0.0f;
	}

  if (!kernings) { 
		return 0.0f;
	}

  for (u32 i = 0; i < header.kerningCount; ++i) {
    if (kernings[i].u1 == u1 && kernings[i].u2 == u2) {
      return kernings[i].advance;
    }
  }
  return 0.0f;
}
