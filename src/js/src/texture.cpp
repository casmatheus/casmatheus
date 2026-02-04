#include "js/texture.h"
#include "gpu/renderer.h"
#include "debug/log.h"
#include <emscripten.h>
#include <stdlib.h>

/*
extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void Renderer_UpdateTexture(TextureID id, void* data, u32 w, u32 h, u8 c) {
      Renderer::UpdateTexture(id, data, w, h, c);
    }
}


EM_JS(void, LoadTextureJS, (u32 textureID, const char* url), {
  var src = UTF8ToString(url);

  fetch(src).then(function(response) {
    if (!response.ok) throw new Error("Fetch failed " + response.status);
    return response.blob();
  }).then(function(blob) {
    
    return createImageBitmap(blob, {
      premultiplyAlpha: 'none',
      colorSpaceConversion: 'none',
      resizeQuality: 'pixelated'
    });
    
  }).then(function(bitmap) {
      
    var w = bitmap.width;
    var h = bitmap.height;
    var canvas = new OffscreenCanvas(w, h);
    var ctx = canvas.getContext('2d', {
      willReadFrequently: true,
      alpha: true
    });

    ctx.drawImage(bitmap, 0, 0);
    
    var imageData = ctx.getImageData(0, 0, w, h);
    var pixelData = imageData.data;

    var byteCount = pixelData.length;
    var ptr = _malloc(byteCount);

    Module.HEAPU8.set(pixelData, ptr);

    Module['_Renderer_UpdateTexture'](textureID, ptr, w, h, 4);

    _free(ptr);
    bitmap.close();

  }).catch(function(err) {
    console.error("[JS] Failed to load texture: " + src, err);
  });
});
	*/

TextureID CreateTextureFromURL(const char* url) {
  TextureID id = Renderer::DefaultTexture();

  LoadTextureJS(id, url);

  return id;
}
