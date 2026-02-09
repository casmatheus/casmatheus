#include "js/texture.h"
#include "gpu/renderer.h"
#include "debug/log.h"
#include <emscripten.h>
#include <stdlib.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    void Renderer_UpdateTexture(TextureID id, void* data, u32 w, u32 h, u8 c) {
      Renderer::UpdateTexture(id, data, w, h, c);
    }
}

EM_JS(void, LoadTextureJS, (u32 textureID, const char* url), {
  (async () => {
    const src = UTF8ToString(url);

    try {
      const response = await fetch(src);
      if (!response.ok) throw new Error(`Fetch failed: ${response.status}`);

      const contentType = response.headers.get("Content-Type");
      const decoder = new ImageDecoder({
        data: response.body,
        type: contentType,
        premultiplyAlpha: 'none',
        colorSpaceConversion: 'none'
      });

      const result = await decoder.decode();
      decoder.close();

      const frame = result.image;
      const w = frame.displayWidth;
      const h = frame.displayHeight;

      const options = { format: 'RGBA' };
      const byteSize = frame.allocationSize(options);
      const ptr = _malloc(byteSize);

      const destBuffer = new Uint8Array(Module.HEAPU8.buffer, ptr, byteSize);

      await frame.copyTo(destBuffer, options);

      Module['_Renderer_UpdateTexture'](textureID, ptr, w, h, 4);

      _free(ptr);
      frame.close();

    } catch (err) {
      console.error(`[JS] Failed to load texture: ${src}`, err);
    }
  })();
});

TextureID CreateTextureFromURL(const char* url) {
  TextureID id = Renderer::DefaultTexture();

  LoadTextureJS(id, url);

  return id;
}
