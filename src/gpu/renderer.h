#pragma once

#include "core/primitive.h"
#include "core/result.h"
#include "os/window.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

constexpr u32 MAX_BINDINGS = 8;

using BufferID    = u32;
using TextureID   = u32;
using SamplerID  	= u32;
using PipelineID  = u32;
using BindGroupID = u32;

constexpr u32 INVALID_ID = 0;

enum class VertexFormat : u8 {
  Float32x2, // vec2 (UVs)
  Float32x3, // vec3 (Position)
  Float32x4  // vec4 (Color/Weights)
};

enum class FilterType : u8 {
  Nearest, // Pixelated
  Linear   // Smooth
};

enum class WrapMode : u8 {
  Repeat,
  ClampToEdge,
  MirroredRepeat
};

enum class BlendMode : u8 {
  Opaque,      // No transparency (Default, Fastest)
  Alpha,       // Standard transparency
  Additive     // Additive blending
};

struct VertexAttribute {
  u32          location;
  VertexFormat format;
  u64          offset;
};

struct VertexLayout {
  u64 stride; // sizeof(Vertex)
  VertexAttribute attributes[8];
  u32 attributeCount;
};

enum class PrimitiveTopology : u8 {
  TriangleList,
  LineList,
  PointList
};

enum class PresentMode : u8 {
  Immediate = 0, // Uncapped FPS
  FIFO = 1   	   // FPS Capped to monitor refresh rate
};


enum class AntiAliasing : u8 {
    None = 1,		// No AA (1 sample)
    MSAAx4 = 4
};

enum class CullMode : u8 {
  None,
  Front,
  Back
};

struct RendererConfig {
  Window* 			window;				// Target window for rendering
	u32           renderWidth;  // Internal Resolution
  u32           renderHeight; // Internal Resolution
  PresentMode 	present;			// Presentation Mode
	AntiAliasing  antiAliasing; // Anti-Aliasing Mode
	glm::vec4 		clearColor; 	// RGBA

	static RendererConfig Default(Window& win) {
    return { &win, 
						 win.width, win.height,
             PresentMode::FIFO, 
             AntiAliasing::None, 
             {0.1f, 0.1f, 0.1f, 1.0f} 
           };
  }

};

struct PipelineConfig {
  const char* vertexShaderSource;
  const char* fragmentShaderSource;

  VertexLayout      vertexLayout;
	PrimitiveTopology topology;
  CullMode 	 cull;

	BlendMode         blendMode;

  bool depthTest;
  bool depthWrite;

  static PipelineConfig Default() {
  	return { nullptr, nullptr,
             {}, PrimitiveTopology::TriangleList, CullMode::Back,
             BlendMode::Opaque,
             true, true
        	 };
  }

};

enum class BindingType : u8 {
  None = 0,
  UniformBuffer,
  StorageBuffer,
  Texture,
  Sampler
};

struct ResourceBinding {
  u32 bindingSlot;
  BindingType type;
  
  union {
    struct {
      BufferID id;
      u64 offset;
      u64 size;
    } buffer;

    struct {
      TextureID id;
    } texture;

    struct {
      SamplerID id;
    } sampler;
  };

  static ResourceBinding BindUniform(u32 slot, BufferID buffer, u64 size = 0, u64 offset = 0) {
    ResourceBinding mb;
    mb.bindingSlot = slot;
    mb.type = BindingType::UniformBuffer;
    mb.buffer = { buffer, offset, size };
    return mb;
  }

  static ResourceBinding BindTexture(u32 slot, TextureID texture) {
    ResourceBinding mb;
    mb.bindingSlot = slot;
    mb.type = BindingType::Texture;
    mb.texture = { texture };
    return mb;
  }

  static ResourceBinding BindSampler(u32 slot, SamplerID sampler) {
    ResourceBinding mb;
    mb.bindingSlot = slot;
    mb.type = BindingType::Sampler;
    mb.sampler = { sampler };
    return mb;
  }
};

enum class BufferType : u8 {
  Vertex,
  Index,
  Uniform
};

namespace Renderer {
	// Initializes the GPU backend.
	ErrorCode New(const RendererConfig& config);

	bool Initialized();

  // Tears down the backend and frees all Resources
  void Destroy();

  // Call this when the window is resized. 
  void OnResize(u32 width, u32 height);

	BufferID CreateBuffer(const void* data, u64 size, BufferType type);
	void UpdateBuffer(BufferID buffer, const void* data, u64 size, u64 offset = 0);

	void BindVertexBuffer(BufferID buffer);
  void BindIndexBuffer(BufferID buffer);
  void DestroyBuffer(BufferID buffer);

	PipelineID CreatePipeline(const PipelineConfig& config);
	void BindPipeline(PipelineID pipeline);
	void DestroyPipeline(PipelineID pipeline);

	TextureID CreateRenderTexture(u32 width, u32 height);
  void BeginOffscreenPass(TextureID target);
  // Must be called before BeginFrame
  void EndOffscreenPass();

	void BeginFrame();
  void EndFrame();
	
	TextureID CreateTexture(const void* data, u32 width, u32 height, u8 channels = 4);
	TextureID DefaultTexture();
	void UpdateTexture(TextureID, const void* data, u32 width, u32 height, u8 channels = 4);
  void DestroyTexture(TextureID texture);

  SamplerID CreateSampler(FilterType filter, WrapMode wrap = WrapMode::Repeat); 
  void DestroySampler(SamplerID sampler);

	BindGroupID CreateBindGroup(PipelineID pipeline, u32 groupIndex, u32 bindingCount, const ResourceBinding* bindings);
	template<usize N>
	BindGroupID CreateBindGroup(PipelineID pipeline, u32 groupIndex, const ResourceBinding (&bindings)[N]) {
    return CreateBindGroup(pipeline, groupIndex, (u32)N, bindings);
	}
	void BindGroup(u32 groupIndex, BindGroupID bindGroup);
	void DestroyBindGroup(BindGroupID bindGroup);
	
	void Draw(u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0, u32 firstInstance = 0);
	void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 firstIndex = 0, i32 vertexOffset = 0, u32 firstInstance = 0);
}
