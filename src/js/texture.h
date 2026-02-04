#pragma once

#include "core/primitive.h"
#include "gpu/renderer.h"

// Loads an image from a URL via the Browser (HTML5 Canvas).
// Returns a texture ID immediately 
// The texture content will update asynchronously when the download completes.
TextureID CreateTextureFromURL(const char* url);
