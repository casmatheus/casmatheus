#include "game/text.h"
#include "core/vector.h"
#include "game/ui.h"
#include "debug/log.h"

struct TextVertex {
  f32 pos[2];
  f32 uv[2];
  f32 color[4];
};

struct TextBatchState {
  PipelineID pipeline;
  BufferID vbo;
  BufferID ibo;
  BufferID uniformBuffer;
  
  BindGroupID globalBindGroup;
  BindGroupID fontBindGroup;

  TextureID currentTexture;
  SamplerID sampler;

  Vec<TextVertex> vertices;
  Vec<u16> indices;

  u32 vboOffsetBytes; 
  u32 iboOffsetBytes; 
  u32 indexOffsetCount;

  struct Uniforms {
    f32 screenSize[2];
		f32 pxRange;
    f32 padding;
  } uniforms;
};

static TextBatchState s_Text;

const char* TEXT_VERT_SHADER = R"(
  struct Uniforms {
    screenSize: vec2<f32>,
		pxRange: f32,
    padding: f32,
  };
  @group(0) @binding(0) var<uniform> u : Uniforms;

  struct VertexInput {
    @location(0) pos: vec2<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>,
  };

  struct VertexOutput {
    @builtin(position) Position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
  };

  @vertex
  fn main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    // Map 0..Width to -1..1 and 0..Height to 1..-1 (Y Down)
    let x = (in.pos.x / u.screenSize.x) * 2.0 - 1.0;
    let y = 1.0 - (in.pos.y / u.screenSize.y) * 2.0;
    
    out.Position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
  }
)";

const char* TEXT_FRAG_SHADER = R"(
  struct Uniforms {
    screenSize: vec2<f32>,
		pxRange: f32,
    padding: f32,
  };

	@group(0) @binding(0) var<uniform> u : Uniforms;
  @group(0) @binding(1) var mySampler: sampler;
  @group(1) @binding(0) var myTexture: texture_2d<f32>;

  fn median(r: f32, g: f32, b: f32) -> f32 {
    return max(min(r, g), min(max(r, g), b));
  }

  @fragment
  fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>) -> @location(0) vec4<f32> {
		let tex = textureSample(myTexture, mySampler, uv);
    let msd = tex.rgb;
    
    let sd = median(msd.r, msd.g, msd.b) - 0.5;
		if (tex.a < 0.1) { discard; }

		let texSize = vec2<f32>(textureDimensions(myTexture));
    let dx = fwidth(uv.x) * texSize.x;
    let dy = fwidth(uv.y) * texSize.y;
		let toScreenRatio = length(vec2<f32>(dx, dy));

		let screenDist = (sd * u.pxRange) / toScreenRatio;
    
    // Compute Opacity (0.0 to 1.0)
    let opacity = clamp(screenDist + 0.5, 0.0, 1.0);

		let finalAlpha = opacity * color.a;

		if (finalAlpha < 0.001) { discard; }

    return vec4<f32>(color.rgb, finalAlpha);
  }
)";

static u32 DecodeUTF8(const char*& p) {
  u32 c = (unsigned char)*p++;
  if (c < 0x80) { // ASCII (1 byte)
		return c; 
	}
  if ((c & 0xE0) == 0xC0) { // 2 bytes
    u32 c2 = (unsigned char)*p++;
    return ((c & 0x1F) << 6) | (c2 & 0x3F);
  }
  if ((c & 0xF0) == 0xE0) { // 3 bytes
    u32 c2 = (unsigned char)*p++;
    u32 c3 = (unsigned char)*p++;
    return ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
  }
  if ((c & 0xF8) == 0xF0) { // 4 bytes
    u32 c2 = (unsigned char)*p++;
    u32 c3 = (unsigned char)*p++;
    u32 c4 = (unsigned char)*p++;
    return ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
  }
  return c; 
}

void TextRenderer::Init() {
  s_Text.uniformBuffer = Renderer::CreateBuffer(nullptr, sizeof(TextBatchState::Uniforms), BufferType::Uniform);
  s_Text.vbo = Renderer::CreateBuffer(nullptr, sizeof(TextVertex) * 2048, BufferType::Vertex); // ~500 chars per batch
  s_Text.ibo = Renderer::CreateBuffer(nullptr, sizeof(u16) * 6 * 2048, BufferType::Index);

  // Linear filtering for SDFs
  s_Text.sampler = Renderer::CreateSampler(FilterType::Linear, WrapMode::ClampToEdge);
  s_Text.currentTexture = INVALID_ID;

  PipelineConfig pCfg = PipelineConfig::Default();
  pCfg.vertexShaderSource = TEXT_VERT_SHADER;
  pCfg.fragmentShaderSource = TEXT_FRAG_SHADER;
  pCfg.blendMode = BlendMode::Alpha;
  pCfg.depthTest = false;
  pCfg.depthWrite = false;
	pCfg.cull = CullMode::None;

  pCfg.vertexLayout.stride = sizeof(TextVertex);
  pCfg.vertexLayout.attributeCount = 3;
  pCfg.vertexLayout.attributes[0] = {0, VertexFormat::Float32x2, offsetof(TextVertex, pos)};
  pCfg.vertexLayout.attributes[1] = {1, VertexFormat::Float32x2, offsetof(TextVertex, uv)};
  pCfg.vertexLayout.attributes[2] = {2, VertexFormat::Float32x4, offsetof(TextVertex, color)};

  s_Text.pipeline = Renderer::CreatePipeline(pCfg);

  ResourceBinding globalBinds[] = {
    ResourceBinding::BindUniform(0, s_Text.uniformBuffer),
    ResourceBinding::BindSampler(1, s_Text.sampler)
  };
  s_Text.globalBindGroup = Renderer::CreateBindGroup(s_Text.pipeline, 0, globalBinds);
}

void TextRenderer::BeginFrame(u32 width, u32 height) {
  s_Text.vertices.clear();
  s_Text.indices.clear();
  s_Text.vboOffsetBytes = 0;
  s_Text.iboOffsetBytes = 0;
  s_Text.indexOffsetCount = 0;
  s_Text.currentTexture = INVALID_ID;

  s_Text.uniforms.screenSize[0] = (f32)width;
  s_Text.uniforms.screenSize[1] = (f32)height;
  Renderer::UpdateBuffer(s_Text.uniformBuffer, &s_Text.uniforms, sizeof(s_Text.uniforms));
}

static void FlushTextBatch() {
  if (s_Text.indices.empty()) return;

  u32 vSize = (u32)(s_Text.vertices.size * sizeof(TextVertex));
  u32 iSize = (u32)(s_Text.indices.size * sizeof(u16));
  Renderer::UpdateBuffer(s_Text.vbo, s_Text.vertices.data, vSize, s_Text.vboOffsetBytes);
  Renderer::UpdateBuffer(s_Text.ibo, s_Text.indices.data, iSize, s_Text.iboOffsetBytes);

  Renderer::BindPipeline(s_Text.pipeline);
  Renderer::BindVertexBuffer(s_Text.vbo);
  Renderer::BindIndexBuffer(s_Text.ibo);
  Renderer::BindGroup(0, s_Text.globalBindGroup);

  if (s_Text.fontBindGroup != INVALID_ID) {
    Renderer::BindGroup(1, s_Text.fontBindGroup);
  }

  i32 vertexBase = (i32)(s_Text.vboOffsetBytes / sizeof(TextVertex));
  Renderer::DrawIndexed((u32)s_Text.indices.size, 1, s_Text.indexOffsetCount, vertexBase);

  s_Text.vboOffsetBytes += vSize;
  s_Text.iboOffsetBytes += iSize;
  s_Text.indexOffsetCount += (u32)s_Text.indices.size;

  s_Text.vertices.clear();
  s_Text.indices.clear();
}

void TextRenderer::DrawText(const Font* font, const char* text, f32 x, f32 y, f32 fontSize, const f32 color[4]) {
	if (!font || !text) {
    logError("Invalid Pointer"); 
    return;
	}

	if (!font->loaded) {
    logError("Font has not been properly loaded"); 
    return; 
	}

  if (s_Text.currentTexture != font->texture) {
    FlushTextBatch();
    
    s_Text.currentTexture = font->texture;

		s_Text.uniforms.pxRange = font->header.pxRange;
		Renderer::UpdateBuffer(s_Text.uniformBuffer, &s_Text.uniforms, sizeof(s_Text.uniforms));
    
    if (s_Text.fontBindGroup != INVALID_ID) {
      Renderer::DestroyBindGroup(s_Text.fontBindGroup);
    }
    ResourceBinding matBinds[] = { ResourceBinding::BindTexture(0, s_Text.currentTexture) };
    s_Text.fontBindGroup = Renderer::CreateBindGroup(s_Text.pipeline, 1, matBinds);
  }

  f32 cursorX = x;
  f32 cursorY = y;
  
  // Iterate UTF-8, Assumes ASCII
  const char* p = text;
  while (*p) {
		u32 unicode = DecodeUTF8(p);

    if (unicode == '\n') {
      cursorX = x;
      cursorY += font->header.lineHeight * fontSize;
      continue;
    }

    const FontGlyph* glyph = font->glyph(unicode);
    if (!glyph) continue;

    if (*p) {
      f32 k = font->kerning(unicode, (u32)*p);
      cursorX += k * fontSize;
    }

		f32 texW = (f32)font->header.textureWidth;
    f32 texH = (f32)font->header.textureHeight;

    f32 qL = cursorX + glyph->planeLeft * fontSize;
    f32 qR = cursorX + glyph->planeRight * fontSize;
    f32 qB = cursorY + glyph->planeBottom * fontSize;
    f32 qT = cursorY + glyph->planeTop * fontSize;
    
		f32 uL = glyph->atlasLeft / texW;
    f32 uR = glyph->atlasRight / texW;
    f32 vB = glyph->atlasBottom / texH;
    f32 vT = glyph->atlasTop / texH;

		f32 off = fontSize * 0.06f; 
    f32 sR = color[0] * 0.1f;
    f32 sG = color[1] * 0.1f;
    f32 sB = color[2] * 0.1f;
    f32 sA = color[3] * 0.5f;;

		if (s_Text.vertices.size + 8 > 2048) {
      FlushTextBatch();
		}

		s_Text.vertices.reserve(s_Text.vertices.size + 8);
    s_Text.indices.reserve(s_Text.indices.size + 12);

    u16 startIdx = (u16)s_Text.vertices.size;
    
		s_Text.vertices.push({{qL + off, qT + off}, {uL, vT}, {sR, sG, sB, sA}});
    s_Text.vertices.push({{qR + off, qT + off}, {uR, vT}, {sR, sG, sB, sA}});
    s_Text.vertices.push({{qR + off, qB + off}, {uR, vB}, {sR, sG, sB, sA}});
    s_Text.vertices.push({{qL + off, qB + off}, {uL, vB}, {sR, sG, sB, sA}});

    s_Text.indices.push(startIdx + 0); s_Text.indices.push(startIdx + 1); s_Text.indices.push(startIdx + 2);
    s_Text.indices.push(startIdx + 2); s_Text.indices.push(startIdx + 3); s_Text.indices.push(startIdx + 0);

    s_Text.vertices.push({{qL, qT}, {uL, vT}, {color[0], color[1], color[2], color[3]}});
    s_Text.vertices.push({{qR, qT}, {uR, vT}, {color[0], color[1], color[2], color[3]}});
    s_Text.vertices.push({{qR, qB}, {uR, vB}, {color[0], color[1], color[2], color[3]}});
    s_Text.vertices.push({{qL, qB}, {uL, vB}, {color[0], color[1], color[2], color[3]}});

		s_Text.indices.push(startIdx + 4); s_Text.indices.push(startIdx + 5); s_Text.indices.push(startIdx + 6);
    s_Text.indices.push(startIdx + 6); s_Text.indices.push(startIdx + 7); s_Text.indices.push(startIdx + 4);

    cursorX += glyph->advance * fontSize;
  }
}

f32 TextRenderer::MeasureText(const Font* font, const char* text, f32 fontSize) {
	if (!font || !text) {
    logError("Invalid Pointer"); 
    return 0.0f;
	}

	if (!font->loaded) {
    logError("Font has not been properly loaded"); 
    return 0.0f; 
	}

  f32 width = 0.0f;
  
  const char* p = text;
  while (*p) {
		u32 unicode = DecodeUTF8(p);

    if (unicode == '\n') {
      continue;
    }

    const FontGlyph* glyph = font->glyph(unicode);
    if (!glyph) continue;

    if (*p) {
      f32 k = font->kerning(unicode, (u32)*p);
      width += k * fontSize;
    }

    width += glyph->advance * fontSize;
  }

	return width;
}

void TextRenderer::EndFrame() {
  FlushTextBatch();
}
