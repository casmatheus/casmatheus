#include "game/shape.h"
#include "core/vector.h"
#include "core/math.h"
#include "debug/log.h"
#include <string.h>

struct ShapeVertex {
  f32 pos[2];
  f32 uv[2];
  f32 color[4];
  f32 borderColor[4];
  f32 size[2];
  f32 settings[4]; // [0]=Radius, [1]=Border, [2]=Softness, [3]=Unused
};

struct ShapeBatchState {
	PipelineID pipeline;
  BufferID vbo;
  BufferID ibo;
  BufferID uniformBuffer;
  
  BindGroupID globalBindGroup;
  BindGroupID textureBindGroup;

	u32 uniformBufferOffset;
  u32 vboOffsetBytes; 
  u32 iboOffsetBytes; 
  u32 indexOffsetCount;
  
  TextureID currentTexture;
  TextureID whiteTexture;
  SamplerID sampler;

  Vec<ShapeVertex> vertices;
  Vec<u16> indices;

  struct Uniforms {
		float viewProj[16];
  } uniforms;
};

static ShapeBatchState s_Shape;

const char* SHAPE_VERT = R"(
  struct Uniforms {
		viewProj: mat4x4<f32>,
  };
  @group(0) @binding(0) var<uniform> u : Uniforms;

  struct VertexInput {
    @location(0) pos: vec2<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>,
    @location(3) borderColor: vec4<f32>,
    @location(4) size: vec2<f32>,
    @location(5) settings: vec4<f32>,
  };

  struct VertexOutput {
    @builtin(position) Position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
    @location(2) borderColor: vec4<f32>,
    @location(3) size: vec2<f32>,
    @location(4) settings: vec4<f32>,
  };

  @vertex
  fn main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;

		out.Position = u.viewProj * vec4<f32>(in.pos, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    out.borderColor = in.borderColor;
    out.size = in.size;
    out.settings = in.settings;
    return out;
  }
)";

const char* SHAPE_FRAG = R"(
  @group(0) @binding(1) var mySampler: sampler;
  @group(1) @binding(0) var myTexture: texture_2d<f32>;

  @fragment
  fn main(
    @location(0) uv: vec2<f32>, 
    @location(1) bodyColor: vec4<f32>,
    @location(2) borderColor: vec4<f32>,
    @location(3) size: vec2<f32>,
    @location(4) settings: vec4<f32>
  ) -> @location(0) vec4<f32> {
    
    let radius = settings.x;
    let border = settings.y;
    let softness = settings.z;

    let pos = (uv - 0.5) * size;
    let halfSize = size * 0.5;
    
    let d_vec = abs(pos) - halfSize + radius;
    let dist = length(max(d_vec, vec2<f32>(0.0))) + min(max(d_vec.x, d_vec.y), 0.0) - radius;

    // Use softness for the smoothstep width
    let feather = max(softness, 0.001); 
    
    let outerAlpha = 1.0 - smoothstep(0.0, feather, dist);

    // Border logic
    let t = smoothstep(-border - feather, -border, dist); 

    // Visuals (Gradient + Fake Light)
    let gradient = 1.0 + (0.5 - uv.y) * 0.15; 
    let finalBodyColor = bodyColor.rgb * gradient;

    let innerRect = halfSize - radius;
    let closest = clamp(pos, -innerRect, innerRect);
    let normal = normalize(pos - closest + vec2<f32>(0.0001, 0.0001));
    let lightDir = normalize(vec2<f32>(0.0, -1.0));
    let lightingFactor = 0.5 + ((dot(normal, lightDir) + 1.0) * 0.5);
    
    let litBorder = borderColor.rgb * lightingFactor;
    let finalRGB = mix(finalBodyColor, litBorder, t);
    
    let tex = textureSample(myTexture, mySampler, uv);

    if ((outerAlpha * tex.a) <= 0.001) { discard; }
    
    return vec4<f32>(finalRGB * tex.rgb, bodyColor.a * outerAlpha * tex.a);
  }
)";

namespace ShapeRenderer {

void Init() {
	s_Shape.uniformBuffer = Renderer::CreateBuffer(nullptr, 65536, BufferType::Uniform);
  s_Shape.uniformBufferOffset = 0;
  s_Shape.vbo = Renderer::CreateBuffer(nullptr, sizeof(ShapeVertex) * 4096, BufferType::Vertex);
  s_Shape.ibo = Renderer::CreateBuffer(nullptr, sizeof(u16) * 6 * 4096, BufferType::Index);

  s_Shape.whiteTexture = Renderer::DefaultTexture();
  s_Shape.currentTexture = INVALID_ID;

	s_Shape.sampler  = Renderer::CreateSampler(FilterType::Linear, WrapMode::ClampToEdge);

	PipelineConfig cfg = PipelineConfig::Default();
  cfg.vertexShaderSource = SHAPE_VERT;
  cfg.fragmentShaderSource = SHAPE_FRAG;
  cfg.blendMode = BlendMode::Alpha;
  cfg.depthTest = false;
  cfg.depthWrite = false;
	cfg.cull = CullMode::None;

  cfg.vertexLayout.stride = sizeof(ShapeVertex);
  cfg.vertexLayout.attributeCount = 6;
  cfg.vertexLayout.attributes[0] = {0, VertexFormat::Float32x2, offsetof(ShapeVertex, pos)};
  cfg.vertexLayout.attributes[1] = {1, VertexFormat::Float32x2, offsetof(ShapeVertex, uv)};
  cfg.vertexLayout.attributes[2] = {2, VertexFormat::Float32x4, offsetof(ShapeVertex, color)};
  cfg.vertexLayout.attributes[3] = {3, VertexFormat::Float32x4, offsetof(ShapeVertex, borderColor)};
  cfg.vertexLayout.attributes[4] = {4, VertexFormat::Float32x2, offsetof(ShapeVertex, size)};
  cfg.vertexLayout.attributes[5] = {5, VertexFormat::Float32x4, offsetof(ShapeVertex, settings)}; 
  s_Shape.pipeline = Renderer::CreatePipeline(cfg);

	s_Shape.globalBindGroup = INVALID_ID;
}

void Flush() {
  if (s_Shape.indices.empty()) return;

  u32 vSize = (u32)(s_Shape.vertices.size * sizeof(ShapeVertex));
  u32 iSize = (u32)(s_Shape.indices.size * sizeof(u16));

  Renderer::UpdateBuffer(s_Shape.vbo, s_Shape.vertices.data, vSize, s_Shape.vboOffsetBytes);
  Renderer::UpdateBuffer(s_Shape.ibo, s_Shape.indices.data, iSize, s_Shape.iboOffsetBytes);

  Renderer::BindPipeline(s_Shape.pipeline);
  Renderer::BindVertexBuffer(s_Shape.vbo);
  Renderer::BindIndexBuffer(s_Shape.ibo);
  
  Renderer::BindGroup(0, s_Shape.globalBindGroup);
  
  if (s_Shape.textureBindGroup != INVALID_ID) {
    Renderer::BindGroup(1, s_Shape.textureBindGroup);
  }

  i32 vertexBase = (i32)(s_Shape.vboOffsetBytes / sizeof(ShapeVertex));
  Renderer::DrawIndexed((u32)s_Shape.indices.size, 1, s_Shape.indexOffsetCount, vertexBase);

  s_Shape.vboOffsetBytes += vSize;
  s_Shape.iboOffsetBytes += iSize;
  s_Shape.indexOffsetCount += (u32)s_Shape.indices.size;

  s_Shape.vertices.clear();
  s_Shape.indices.clear();
}

void BeginFrame(const f32* viewProj) {
  s_Shape.vertices.clear();
  s_Shape.indices.clear();
  s_Shape.vboOffsetBytes = 0;
  s_Shape.iboOffsetBytes = 0;
  s_Shape.indexOffsetCount = 0;
  s_Shape.currentTexture = INVALID_ID; 

  s_Shape.uniformBufferOffset += 256;
  if (s_Shape.uniformBufferOffset >= 65536) {
    s_Shape.uniformBufferOffset = 0;
  }

  memcpy(s_Shape.uniforms.viewProj, viewProj, sizeof(f32) * 16);
  Renderer::UpdateBuffer(s_Shape.uniformBuffer, &s_Shape.uniforms, sizeof(s_Shape.uniforms), s_Shape.uniformBufferOffset);

  if (s_Shape.globalBindGroup != INVALID_ID) {
    Renderer::DestroyBindGroup(s_Shape.globalBindGroup);
  }

  ResourceBinding globals[] = { 
    ResourceBinding::BindUniform(0, s_Shape.uniformBuffer, sizeof(ShapeBatchState::Uniforms), s_Shape.uniformBufferOffset),
    ResourceBinding::BindSampler(1, s_Shape.sampler)
  };
  s_Shape.globalBindGroup = Renderer::CreateBindGroup(s_Shape.pipeline, 0, globals);
}

void EndFrame() {
  Flush();
}

void DrawShape(f32 x, f32 y, f32 w, f32 h, 
               const f32 color[4], 
               const f32 borderColor[4], 
               f32 radius, 
               f32 border,
               f32 edgeSoftness,
               TextureID textureID) 
{
  TextureID targetTex = (textureID == INVALID_ID) ? s_Shape.whiteTexture : textureID;

  if (s_Shape.currentTexture != targetTex || s_Shape.vertices.size + 4 > 4096) {
    Flush();
    if (s_Shape.currentTexture != targetTex) {
      if (s_Shape.textureBindGroup != INVALID_ID) {
        Renderer::DestroyBindGroup(s_Shape.textureBindGroup);
      }
      ResourceBinding texBind[] = { ResourceBinding::BindTexture(0, targetTex) };
      s_Shape.textureBindGroup = Renderer::CreateBindGroup(s_Shape.pipeline, 1, texBind);
      s_Shape.currentTexture = targetTex;
    }
  }

  u16 start = (u16)s_Shape.vertices.size;
  ShapeVertex v;
  v.size[0] = w; v.size[1] = h;
  
  v.settings[0] = radius; 
  v.settings[1] = border;
  v.settings[2] = edgeSoftness; 
  v.settings[3] = 0.0f;

  memcpy(v.color, color, sizeof(f32) * 4);
  
  if (borderColor) {
    memcpy(v.borderColor, borderColor, sizeof(f32) * 4);
  } else {
    memcpy(v.borderColor, color, sizeof(f32) * 4);
  }

  v.pos[0] = x;     v.pos[1] = y + h; v.uv[0] = 0.0f; v.uv[1] = 1.0f; s_Shape.vertices.push(v);
  v.pos[0] = x + w; v.pos[1] = y + h; v.uv[0] = 1.0f; v.uv[1] = 1.0f; s_Shape.vertices.push(v);
  v.pos[0] = x + w; v.pos[1] = y;     v.uv[0] = 1.0f; v.uv[1] = 0.0f; s_Shape.vertices.push(v);
  v.pos[0] = x;     v.pos[1] = y;     v.uv[0] = 0.0f; v.uv[1] = 0.0f; s_Shape.vertices.push(v);

  s_Shape.indices.push(start); 
  s_Shape.indices.push(start + 1); 
  s_Shape.indices.push(start + 2);
  s_Shape.indices.push(start + 2); 
  s_Shape.indices.push(start + 3); 
  s_Shape.indices.push(start);
}

}
