#include "game/sprite.h"
#include "core/vector.h"
#include "debug/log.h"
#include <string.h>
#include <math.h>

struct SpriteVertex {
  f32 pos[2];
  f32 uv[2];
  f32 color[4];
};

struct SpriteBatchState {
  PipelineID pipeline;
  BufferID vbo;
  BufferID ibo;
  BufferID uniformBuffer;
  u32 uniformBufferOffset;
  
  BindGroupID globalBindGroup;
  BindGroupID textureBindGroup;
  
  u32 vboOffsetBytes; 
  u32 iboOffsetBytes; 
  u32 indexOffsetCount;
  
  TextureID currentTexture;
  TextureID whiteTexture;
  SamplerID sampler;

  Vec<SpriteVertex> vertices;
  Vec<u16> indices;

  struct Uniforms { float viewProj[16]; } uniforms;
};

static SpriteBatchState s_Sprite;

const char* SPRITE_VERT = R"(
  struct Uniforms { viewProj: mat4x4<f32> };
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
    out.Position = u.viewProj * vec4<f32>(in.pos, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
  }
)";

const char* SPRITE_FRAG = R"(
  @group(0) @binding(1) var mySampler: sampler;
  @group(1) @binding(0) var myTexture: texture_2d<f32>;

  @fragment
  fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>) -> @location(0) vec4<f32> {
    let tex = textureSample(myTexture, mySampler, uv);
    if (tex.a <= 0.01) { discard; }
    return tex * color;
  }
)";

namespace SpriteRenderer {

void Init() {
  s_Sprite.uniformBuffer = Renderer::CreateBuffer(nullptr, 65536, BufferType::Uniform);
  s_Sprite.uniformBufferOffset = 0;
  s_Sprite.vbo = Renderer::CreateBuffer(nullptr, sizeof(SpriteVertex) * 4096, BufferType::Vertex);
  s_Sprite.ibo = Renderer::CreateBuffer(nullptr, sizeof(u16) * 6 * 4096, BufferType::Index);

  s_Sprite.whiteTexture = Renderer::DefaultTexture();
  s_Sprite.currentTexture = INVALID_ID;
  
  s_Sprite.sampler = Renderer::CreateSampler(FilterType::Nearest, WrapMode::Repeat);

  PipelineConfig cfg = PipelineConfig::Default();
  cfg.vertexShaderSource = SPRITE_VERT;
  cfg.fragmentShaderSource = SPRITE_FRAG;
  cfg.blendMode = BlendMode::Alpha;
  cfg.depthTest = false;
  cfg.depthWrite = false;
  cfg.cull = CullMode::None;
  
  cfg.vertexLayout.stride = sizeof(SpriteVertex);
  cfg.vertexLayout.attributeCount = 3;
  cfg.vertexLayout.attributes[0] = {0, VertexFormat::Float32x2, offsetof(SpriteVertex, pos)};
  cfg.vertexLayout.attributes[1] = {1, VertexFormat::Float32x2, offsetof(SpriteVertex, uv)};
  cfg.vertexLayout.attributes[2] = {2, VertexFormat::Float32x4, offsetof(SpriteVertex, color)};

  s_Sprite.pipeline = Renderer::CreatePipeline(cfg);
  s_Sprite.globalBindGroup = INVALID_ID;
}

void Flush() {
  if (s_Sprite.indices.empty()) return;

  u32 vSize = (u32)(s_Sprite.vertices.size * sizeof(SpriteVertex));
  u32 iSize = (u32)(s_Sprite.indices.size * sizeof(u16));
  
  Renderer::UpdateBuffer(s_Sprite.vbo, s_Sprite.vertices.data, vSize, s_Sprite.vboOffsetBytes);
  Renderer::UpdateBuffer(s_Sprite.ibo, s_Sprite.indices.data, iSize, s_Sprite.iboOffsetBytes);

  Renderer::BindPipeline(s_Sprite.pipeline);
  Renderer::BindVertexBuffer(s_Sprite.vbo);
  Renderer::BindIndexBuffer(s_Sprite.ibo);
  Renderer::BindGroup(0, s_Sprite.globalBindGroup);
  if (s_Sprite.textureBindGroup != INVALID_ID) Renderer::BindGroup(1, s_Sprite.textureBindGroup);

  i32 vertexBase = (i32)(s_Sprite.vboOffsetBytes / sizeof(SpriteVertex));
  Renderer::DrawIndexed((u32)s_Sprite.indices.size, 1, s_Sprite.indexOffsetCount, vertexBase);

  s_Sprite.vboOffsetBytes += vSize;
  s_Sprite.iboOffsetBytes += iSize;
  s_Sprite.indexOffsetCount += (u32)s_Sprite.indices.size;
  s_Sprite.vertices.clear();
  s_Sprite.indices.clear();
}

void BeginFrame(const f32* viewProj) {
  s_Sprite.vertices.clear();
  s_Sprite.indices.clear();
  s_Sprite.vboOffsetBytes = 0;
  s_Sprite.iboOffsetBytes = 0;
  s_Sprite.indexOffsetCount = 0;
  s_Sprite.currentTexture = INVALID_ID;

  s_Sprite.uniformBufferOffset += 256;
  if (s_Sprite.uniformBufferOffset >= 65536) s_Sprite.uniformBufferOffset = 0;

  memcpy(s_Sprite.uniforms.viewProj, viewProj, sizeof(f32) * 16);
  Renderer::UpdateBuffer(s_Sprite.uniformBuffer, &s_Sprite.uniforms, sizeof(s_Sprite.uniforms), s_Sprite.uniformBufferOffset);

  if (s_Sprite.globalBindGroup != INVALID_ID) Renderer::DestroyBindGroup(s_Sprite.globalBindGroup);
  ResourceBinding globals[] = { 
      ResourceBinding::BindUniform(0, s_Sprite.uniformBuffer, sizeof(SpriteBatchState::Uniforms), s_Sprite.uniformBufferOffset),
      ResourceBinding::BindSampler(1, s_Sprite.sampler)
  };
  s_Sprite.globalBindGroup = Renderer::CreateBindGroup(s_Sprite.pipeline, 0, globals);
}

void EndFrame() { Flush(); }

void Draw(f32 x, f32 y, f32 w, f32 h, TextureID tex, const f32 color[4], f32 rotation, f32 u0, f32 v0, f32 u1, f32 v1) {
  TextureID targetTex = (tex == INVALID_ID) ? s_Sprite.whiteTexture : tex;

  if (s_Sprite.currentTexture != targetTex || s_Sprite.vertices.size + 4 > 4096) {
    Flush();
    if (s_Sprite.currentTexture != targetTex) {
      if (s_Sprite.textureBindGroup != INVALID_ID) Renderer::DestroyBindGroup(s_Sprite.textureBindGroup);
      ResourceBinding texBind[] = { ResourceBinding::BindTexture(0, targetTex) };
      s_Sprite.textureBindGroup = Renderer::CreateBindGroup(s_Sprite.pipeline, 1, texBind);
      s_Sprite.currentTexture = targetTex;
    }
  }

  u16 start = (u16)s_Sprite.vertices.size;
  SpriteVertex v;
  memcpy(v.color, color, sizeof(f32) * 4);

	f32 cx = x + w * 0.5f;
  f32 cy = y + h * 0.5f;
  
  f32 c = cosf(rotation);
  f32 s = sinf(rotation);

  f32 lx[] = { -w * 0.5f,  w * 0.5f,  w * 0.5f, -w * 0.5f };
  f32 ly[] = {  h * 0.5f,  h * 0.5f, -h * 0.5f, -h * 0.5f };
  
  f32 u[] = { u0, u1, u1, u0 };
  f32 v_uv[] = { v1, v1, v0, v0 };

  for (int i = 0; i < 4; ++i) {
    f32 rx = lx[i] * c - ly[i] * s;
    f32 ry = lx[i] * s + ly[i] * c;

    v.pos[0] = cx + rx;
    v.pos[1] = cy + ry;
    v.uv[0]  = u[i];
    v.uv[1]  = v_uv[i];
    s_Sprite.vertices.push(v);
  }

  s_Sprite.indices.push(start); s_Sprite.indices.push(start + 1); s_Sprite.indices.push(start + 2);
  s_Sprite.indices.push(start + 2); s_Sprite.indices.push(start + 3); s_Sprite.indices.push(start);
}

}
