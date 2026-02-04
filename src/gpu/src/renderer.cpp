#include "debug/perf.h"
#include "core/vector.h"
#include "core/result.h"
#include "gpu/renderer.h"
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>
#include <string.h>

#include "debug/log.h"

struct BindGroupRecipe {
  PipelineID pipeline;
  u32 groupIndex;
  Vec<ResourceBinding> bindings;
	
	BindGroupRecipe() = default;
  BindGroupRecipe(BindGroupRecipe&&) = default;
  BindGroupRecipe(const BindGroupRecipe&) = delete;
};

struct InternalBindGroup {
  WGPUBindGroup handle;
  BindGroupRecipe recipe; 
  bool dirty;

  InternalBindGroup() = default;
  InternalBindGroup(InternalBindGroup&&) = default;
  InternalBindGroup(const InternalBindGroup&) = delete;
};

struct RenderContext {
  WGPUInstance instance;
  WGPUSurface surface;
  WGPUDevice device;
  WGPUQueue queue;
  
  // Frame State
  WGPUCommandEncoder currentEncoder;
  WGPURenderPassEncoder currentPass;
  WGPUTextureView currentBackBufferView;
  bool inFrame = false;

  // Resources
  Vec<WGPUBuffer> buffers;
  Vec<WGPURenderPipeline> pipelines;
  Vec<WGPUTexture> textures;
  Vec<WGPUTextureView> textureViews;
  Vec<WGPUSampler> samplers;
	Vec<InternalBindGroup> bindGroups;
	Vec<WGPUTextureView> offscreenDepthViews;

	// Depth Resources
  WGPUTexture depthTexture;
  WGPUTextureView depthTextureView;
  WGPUTextureFormat depthFormat = WGPUTextureFormat_Depth24PlusStencil8;

  // Binding State (for handling BindGroups automatically)
  PipelineID currentPipelineId = INVALID_ID;

	WGPUTextureFormat surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
	
	// Storage for preferences
	RendererConfig config;
  bool ready = false;
	
	void initVectors() {
    buffers.push(nullptr);
    pipelines.push(nullptr);
    textures.push(nullptr);
    textureViews.push(nullptr);
    samplers.push(nullptr);
		offscreenDepthViews.push(nullptr);
		
		InternalBindGroup dummy = {nullptr, {}, false};
		bindGroups.push(move(dummy));
  }
};

static RenderContext ctx;

// -----------------------------------------------------------------------------
// Enum Converters
// -----------------------------------------------------------------------------

static WGPUVertexFormat ToWGPU(VertexFormat fmt) {
  switch(fmt) {
    case VertexFormat::Float32x2: return WGPUVertexFormat_Float32x2;
    case VertexFormat::Float32x3: return WGPUVertexFormat_Float32x3;
    case VertexFormat::Float32x4: return WGPUVertexFormat_Float32x4;
    default: return WGPUVertexFormat_Float32x3;
  }
}

static WGPUPrimitiveTopology ToWGPU(PrimitiveTopology topo) {
  switch(topo) {
    case PrimitiveTopology::TriangleList: return WGPUPrimitiveTopology_TriangleList;
    case PrimitiveTopology::LineList:     return WGPUPrimitiveTopology_LineList;
    case PrimitiveTopology::PointList:    return WGPUPrimitiveTopology_PointList;
    default: return WGPUPrimitiveTopology_TriangleList;
  }
}

static WGPUCullMode ToWGPU(CullMode mode) {
  switch(mode) {
    case CullMode::None:  return WGPUCullMode_None;
    case CullMode::Front: return WGPUCullMode_Front;
    case CullMode::Back:  return WGPUCullMode_Back;
    default: return WGPUCullMode_None;
  }
}

static WGPUBlendFactor ToWGPU(BlendMode mode, bool src) {
  // Simplified blend mapping
  switch (mode) {
    case BlendMode::Alpha: 
      return src ? WGPUBlendFactor_SrcAlpha : WGPUBlendFactor_OneMinusSrcAlpha;
    case BlendMode::Additive:
      return src ? WGPUBlendFactor_One : WGPUBlendFactor_One;
    case BlendMode::Opaque:
    default:
      return src ? WGPUBlendFactor_One : WGPUBlendFactor_Zero;
  }
}

static WGPUFilterMode ToWGPU(FilterType filter) {
  switch(filter) {
    case FilterType::Nearest: return WGPUFilterMode_Nearest;
    case FilterType::Linear:  return WGPUFilterMode_Linear;
    default: return WGPUFilterMode_Nearest;
  }
}

static WGPUAddressMode ToWGPU(WrapMode wrap) {
  switch(wrap) {
    case WrapMode::Repeat: return WGPUAddressMode_Repeat;
    case WrapMode::ClampToEdge: return WGPUAddressMode_ClampToEdge;
    case WrapMode::MirroredRepeat: return WGPUAddressMode_MirrorRepeat;
    default: return WGPUAddressMode_Repeat;
  }
}

static WGPUPresentMode ToWGPU(PresentMode mode) {
  switch(mode) {
  	case PresentMode::Immediate: return WGPUPresentMode_Immediate;
    case PresentMode::FIFO:      return WGPUPresentMode_Fifo;
    default: return WGPUPresentMode_Fifo;
	}
}

inline WGPUStringView SV(const char* s) {
  return { s, strlen(s) };
}

static void createDepthTexture(u32 width, u32 height) {
  if (ctx.depthTexture) { 
		wgpuTextureDestroy(ctx.depthTexture);
		wgpuTextureRelease(ctx.depthTexture);
	}

  WGPUTextureDescriptor desc = {};
  desc.usage = WGPUTextureUsage_RenderAttachment;
  desc.size = {width, height, 1};
  desc.format = ctx.depthFormat;
  desc.sampleCount = 1;
	desc.mipLevelCount = 1;
  desc.dimension = WGPUTextureDimension_2D;
  desc.viewFormatCount = 0;
  desc.viewFormats = nullptr;

	ctx.depthTexture = wgpuDeviceCreateTexture(ctx.device, &desc);
  ctx.depthTextureView = wgpuTextureCreateView(ctx.depthTexture, nullptr);
}

namespace Renderer {

void OnAdapterRequest(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata, void* userdata2);
void OnDeviceRequest(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata, void* userdata2);

ErrorCode New(const RendererConfig& config) {
	ctx.config = config;

  WGPUInstanceDescriptor instanceDesc = {};
	instanceDesc.nextInChain = nullptr;
  ctx.instance = wgpuCreateInstance(&instanceDesc);
  if (!ctx.instance) {
    logError("Failed to create WebGPU Instance");
    return ErrorCode::PLATFORM_ERROR;
  }

	WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
	canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
  canvasDesc.selector = SV("#canvas");
  
	WGPUSurfaceDescriptor surfaceDesc = {};
	surfaceDesc.nextInChain = (WGPUChainedStruct*)&canvasDesc;
	ctx.surface = wgpuInstanceCreateSurface(ctx.instance, &surfaceDesc);

  WGPURequestAdapterOptions options = {};
  options.compatibleSurface = ctx.surface;

	WGPURequestAdapterCallbackInfo cbInfo = {};
  cbInfo.nextInChain = nullptr;
  cbInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  cbInfo.callback = OnAdapterRequest;
  cbInfo.userdata1 = nullptr;
  cbInfo.userdata2 = nullptr;

	wgpuInstanceRequestAdapter(ctx.instance, &options, cbInfo);

	return ErrorCode::OK;
}

void OnAdapterRequest(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata, void* userdata2) {
	(void)userdata; (void)userdata2; (void)message;
	if (status != WGPURequestAdapterStatus_Success) {
    logError("Failed to get adapter: %s", message.data);
    return;
  }

  WGPUDeviceDescriptor deviceDesc = {};
  deviceDesc.label = SV("WebGPU Device");
  
  WGPUSurfaceCapabilities capabilities = {};
  wgpuSurfaceGetCapabilities(ctx.surface, adapter, &capabilities);
  ctx.surfaceFormat = capabilities.formats[0];

	WGPURequestDeviceCallbackInfo cbInfo = {};
  cbInfo.nextInChain = nullptr;
  cbInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  cbInfo.callback = OnDeviceRequest;
  cbInfo.userdata1 = nullptr;
  cbInfo.userdata2 = nullptr;

  wgpuAdapterRequestDevice(adapter, &deviceDesc, cbInfo);
}

void OnDeviceRequest(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata, void* userdata2) {
	(void)userdata; (void)userdata2; (void)message;
  if (status != WGPURequestDeviceStatus_Success) {
    logError("Failed to get device: %s", message.data);
    return;
  }
  
  ctx.device = device;
  ctx.queue = wgpuDeviceGetQueue(ctx.device);
  
  WGPUSurfaceConfiguration surfConfig = {};
  surfConfig.device = ctx.device;
  surfConfig.format = ctx.surfaceFormat;
  surfConfig.width = ctx.config.renderWidth;
  surfConfig.height = ctx.config.renderHeight;
  surfConfig.usage = WGPUTextureUsage_RenderAttachment;
  surfConfig.presentMode = ToWGPU(ctx.config.present);
  surfConfig.alphaMode = WGPUCompositeAlphaMode_Auto; 
  
  wgpuSurfaceConfigure(ctx.surface, &surfConfig);
  createDepthTexture(surfConfig.width, surfConfig.height);

  ctx.initVectors();    
	
  ctx.ready = true;
  logInfo("WebGPU Renderer Initialized");
}

bool Initialized() {
  return ctx.ready;
}

void Destroy() {
  ctx.ready = false;
  ctx.buffers.clear();
  ctx.pipelines.clear();
  ctx.textures.clear();
  ctx.textureViews.clear();
  ctx.samplers.clear();
	ctx.bindGroups.clear();

  if (ctx.device) {
		wgpuDeviceDestroy(ctx.device);
    wgpuDeviceRelease(ctx.device);
  }
  
  ctx.device = nullptr;
  ctx.instance = nullptr;
}

TextureID DefaultTexture() {
  constexpr u32 w = 64;
  constexpr u32 h = 64;
  u8 data[w * h * 4];
  
  for (u32 i = 0; i < w * h; ++i) {
    data[i * 4 + 0] = 255;   // R
    data[i * 4 + 1] = 255;   // G
    data[i * 4 + 2] = 255;   // B
    data[i * 4 + 3] = 255;   // A
  }

  return Renderer::CreateTexture(data, w, h, 4);
}

void OnResize(u32 width, u32 height) {
	if (!ctx.ready) {
    logError("Renderer not ready");
    return;
  }
  if (width == 0 || height == 0) {
    logError("Invalid dimensions %ux%u", width, height);
    return;
  }

	emscripten_set_canvas_element_size("#canvas", width, height);

	ctx.config.renderWidth = width;
	ctx.config.renderHeight = height;
  
  // Configure Surface
  WGPUSurfaceConfiguration surfConfig = {};
  surfConfig.device = ctx.device;
  surfConfig.format = ctx.surfaceFormat;
  surfConfig.width = ctx.config.renderWidth;
  surfConfig.height = ctx.config.renderHeight;
  surfConfig.usage = WGPUTextureUsage_RenderAttachment;
  surfConfig.presentMode = ToWGPU(ctx.config.present);
  surfConfig.alphaMode = WGPUCompositeAlphaMode_Auto; 
  
	wgpuSurfaceConfigure(ctx.surface, &surfConfig);
	createDepthTexture(surfConfig.width, surfConfig.height);
}

BufferID CreateBuffer(const void* data, u64 size, BufferType type) {
	if (!ctx.ready) {
    logError("Renderer not ready");
    return INVALID_ID;
  }

  if (size == 0) {
    logError("Size cannot be 0");
    return INVALID_ID;
  }

  WGPUBufferUsage usage = WGPUBufferUsage_CopyDst;
  if (type == BufferType::Vertex) usage |= WGPUBufferUsage_Vertex;
  else if (type == BufferType::Index) usage |= WGPUBufferUsage_Index;
  else if (type == BufferType::Uniform) usage |= WGPUBufferUsage_Uniform;

  WGPUBufferDescriptor desc = {};
  desc.size = (size + 3) & ~3;
  desc.usage = usage;
  desc.mappedAtCreation = true;

	WGPUBuffer buffer = wgpuDeviceCreateBuffer(ctx.device, &desc);
  
  if (data) {
		memcpy(wgpuBufferGetMappedRange(buffer, 0, desc.size), data, size);
  }
	wgpuBufferUnmap(buffer);

  ctx.buffers.push(buffer);
  return (BufferID)(ctx.buffers.size - 1);
}

void UpdateBuffer(BufferID bufferId, const void* data, u64 size, u64 offset) {
	PERF_SCOPE("[Renderer] Update Buffer");

	if (bufferId >= ctx.buffers.size || !ctx.buffers[bufferId]) {
    logError("Invalid buffer ID %u", bufferId);
    return;
	}
	if (size % 4 != 0) {
  	logError("UpdateBuffer size must be a multiple of 4 bytes");
    return;
	}

	wgpuQueueWriteBuffer(ctx.queue, ctx.buffers[bufferId], offset, data, size);
}

void BindVertexBuffer(BufferID bufferId) {
	if (!ctx.inFrame) {
  	logError("Called outside of frame");
    return;
  }
  if (bufferId >= ctx.buffers.size) {
    logError("Invalid buffer ID %u", bufferId);
    return;
  }
	
	wgpuRenderPassEncoderSetVertexBuffer(ctx.currentPass, 0, ctx.buffers[bufferId], 0, wgpuBufferGetSize(ctx.buffers[bufferId]));
}

void BindIndexBuffer(BufferID bufferId) {
	if (!ctx.inFrame) {
  	logError("Called outside of frame");
    return;
  }
  if (bufferId >= ctx.buffers.size) {
    logError("Invalid buffer ID %u", bufferId);
    return;
  }
	
	wgpuRenderPassEncoderSetIndexBuffer(ctx.currentPass, ctx.buffers[bufferId], WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(ctx.buffers[bufferId]));
}

void DestroyBuffer(BufferID buffer) {
  if (buffer < ctx.buffers.size) {
		wgpuBufferDestroy(ctx.buffers[buffer]);
    wgpuBufferRelease(ctx.buffers[buffer]);
    ctx.buffers[buffer] = nullptr;
  }
}

static WGPUBindGroup BuildBindGroup(const BindGroupRecipe& recipe) {
  if (recipe.pipeline >= ctx.pipelines.size || !ctx.pipelines[recipe.pipeline]) {
    logError("Invalid Pipeline ID %u", recipe.pipeline);
    return nullptr;
  }
	
	WGPUBindGroupLayout layout = wgpuRenderPipelineGetBindGroupLayout(ctx.pipelines[recipe.pipeline], recipe.groupIndex);
  
  Vec<WGPUBindGroupEntry> entries;
  entries.reserve(recipe.bindings.size);

  for (const auto& b : recipe.bindings) {
    WGPUBindGroupEntry entry{};
    entry.binding = b.bindingSlot;

    switch (b.type) {
      case BindingType::UniformBuffer:
      case BindingType::StorageBuffer:
        if (b.buffer.id < ctx.buffers.size && ctx.buffers[b.buffer.id]) {
          entry.buffer = ctx.buffers[b.buffer.id];
          entry.offset = b.buffer.offset;
					entry.size = (b.buffer.size > 0) ? b.buffer.size : wgpuBufferGetSize(ctx.buffers[b.buffer.id]);
        } else {
          logError("Invalid Buffer ID %u at slot %u", b.buffer.id, b.bindingSlot);
        } break;

      case BindingType::Texture:
        if (b.texture.id < ctx.textureViews.size && ctx.textureViews[b.texture.id]) {
          entry.textureView = ctx.textureViews[b.texture.id];
        } else {
          logError("Invalid Texture ID %u at slot %u", b.texture.id, b.bindingSlot);
        } break;

      case BindingType::Sampler:
        if (b.sampler.id < ctx.samplers.size && ctx.samplers[b.sampler.id]) {
          entry.sampler = ctx.samplers[b.sampler.id];
        } else {
          logError("Invalid Sampler ID %u at slot %u", b.sampler.id, b.bindingSlot);
        } break;

      default: break;
    }
    entries.push(entry);
  }

  WGPUBindGroupDescriptor desc = {};
  desc.layout = layout;
  desc.entryCount = (uint32_t)entries.size;
  desc.entries = entries.data;

  WGPUBindGroup bg = wgpuDeviceCreateBindGroup(ctx.device, &desc);
	wgpuBindGroupLayoutRelease(layout);

  return bg;
}

BindGroupID CreateBindGroup(PipelineID pipelineId, u32 groupIndex, u32 bindingCount, const ResourceBinding* bindings) {
  if (!ctx.ready) {
    logError("Rendere not Ready");
    return INVALID_ID;
  }
	if (pipelineId >= ctx.pipelines.size) {
    logError("Invalid Pipeline ID %u", pipelineId);
		return INVALID_ID;
	}
	if (bindingCount > MAX_BINDINGS) {
    logError("Binding count %u exceeds limit of %u", bindingCount, MAX_BINDINGS);
    return INVALID_ID;
	}

	InternalBindGroup bg;
  bg.recipe.pipeline = pipelineId;
  bg.recipe.groupIndex = groupIndex;
	for (u32 i = 0; i < bindingCount; ++i) {
    bg.recipe.bindings.push(bindings[i]);
  }

  bg.handle = BuildBindGroup(bg.recipe);
  bg.dirty = false;

	ctx.bindGroups.push(move(bg));

  return (BindGroupID)(ctx.bindGroups.size - 1);
}

void BindGroup(u32 groupIndex, BindGroupID id) {
  if (ctx.inFrame && id < ctx.bindGroups.size) {
		InternalBindGroup& bg = ctx.bindGroups[id];

		if (bg.dirty) {
			if (bg.handle) { 
				wgpuBindGroupRelease(bg.handle);
			}

      bg.handle = BuildBindGroup(bg.recipe);
      bg.dirty = false;
      logInfo("Rebuilt BindGroup %u", id);
    }

		wgpuRenderPassEncoderSetBindGroup(ctx.currentPass, groupIndex, bg.handle, 0, nullptr);
	}
}

void DestroyBindGroup(BindGroupID bindGroup) {
  if (bindGroup < ctx.bindGroups.size) {
    InternalBindGroup& bg = ctx.bindGroups[bindGroup];

		if (bg.handle) { 
			wgpuBindGroupRelease(bg.handle);
		}
    
    bg.handle = nullptr; 
    bg.recipe.bindings.clear();
    bg.recipe.groupIndex = 0;
    bg.recipe.pipeline = INVALID_ID;
    bg.dirty = false;
  }
}

// -------------------------------------------------------------------------
// Pipelines
// -------------------------------------------------------------------------

PipelineID CreatePipeline(const PipelineConfig& config) {
	if (!ctx.ready) {
    logError("Renderer not ready");
    return INVALID_ID;
  }

  if (!config.vertexShaderSource || !config.fragmentShaderSource) {
    logError("Shader source cannot be null");
    return INVALID_ID;
	}

	// Shader Module
  WGPUShaderSourceWGSL wgslVert{};
	wgslVert.chain.sType = WGPUSType_ShaderSourceWGSL;
  wgslVert.code = SV(config.vertexShaderSource);
  WGPUShaderModuleDescriptor vertDesc{};
	vertDesc.nextInChain = (WGPUChainedStruct*)&wgslVert;
	WGPUShaderModule vertMod = wgpuDeviceCreateShaderModule(ctx.device, &vertDesc);

  WGPUShaderSourceWGSL wgslFrag{};
	wgslFrag.chain.sType = WGPUSType_ShaderSourceWGSL;
  wgslFrag.code = SV(config.fragmentShaderSource);
  WGPUShaderModuleDescriptor fragDesc{};
	fragDesc.nextInChain = (WGPUChainedStruct*)&wgslFrag;
	WGPUShaderModule fragMod = wgpuDeviceCreateShaderModule(ctx.device, &fragDesc);

  // Vertex Layout
  Vec<WGPUVertexAttribute> attributes;
  for(u32 i=0; i < config.vertexLayout.attributeCount; ++i) {
    const auto& attr = config.vertexLayout.attributes[i];
    WGPUVertexAttribute a{};
    a.format = ToWGPU(attr.format);
    a.offset = attr.offset;
    a.shaderLocation = attr.location;
    attributes.push(a);
  }

  WGPUVertexBufferLayout vbLayout{};
  vbLayout.arrayStride = config.vertexLayout.stride;
  vbLayout.attributeCount = attributes.size;
  vbLayout.attributes = attributes.data;
  vbLayout.stepMode = WGPUVertexStepMode_Vertex;

  // Render Pipeline Descriptor
  WGPURenderPipelineDescriptor desc{};
  
  // Fragment
  WGPUColorTargetState colorTarget{};
  colorTarget.format = ctx.surfaceFormat; 
  colorTarget.writeMask = WGPUColorWriteMask_All;

	WGPUDepthStencilState depthStencil{};
	depthStencil.format = ctx.depthFormat; // Always match the RenderPass format
  depthStencil.stencilFront.compare = WGPUCompareFunction_Always;
  depthStencil.stencilBack.compare = WGPUCompareFunction_Always;

  if (config.depthTest) {
    depthStencil.depthWriteEnabled = config.depthWrite ? WGPUOptionalBool_True : WGPUOptionalBool_False;
    depthStencil.depthCompare = WGPUCompareFunction_Less;
  } else {
    depthStencil.depthWriteEnabled = WGPUOptionalBool_False;
    depthStencil.depthCompare = WGPUCompareFunction_Always;
  }

  desc.depthStencil = &depthStencil;
		
  // Blend
  WGPUBlendState blend{};
  if (config.blendMode != BlendMode::Opaque) {
    blend.color.srcFactor = ToWGPU(config.blendMode, true);
    blend.color.dstFactor = ToWGPU(config.blendMode, false);
    blend.color.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = ToWGPU(config.blendMode, true);
    blend.alpha.dstFactor = ToWGPU(config.blendMode, false);
    blend.alpha.operation = WGPUBlendOperation_Add;
    colorTarget.blend = &blend;
  }
	
	// Fragment
  WGPUFragmentState fragment{};
  fragment.module = fragMod;
  fragment.entryPoint = SV("main");
  fragment.targetCount = 1;
  fragment.targets = &colorTarget;
  desc.fragment = &fragment;

  // Vertex
  desc.vertex.module = vertMod;
  desc.vertex.entryPoint = SV("main");
	if (config.vertexLayout.attributeCount > 0) {
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &vbLayout;
  } else {
    desc.vertex.bufferCount = 0;
    desc.vertex.buffers = nullptr;
	}

  // Primitive
  desc.primitive.topology = ToWGPU(config.topology);
  desc.primitive.cullMode = ToWGPU(config.cull);
  desc.primitive.frontFace = WGPUFrontFace_CCW;

  // Multisample
  desc.multisample.count = 1;
  desc.multisample.mask = ~0u;

  desc.layout = nullptr;
	
	WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(ctx.device, &desc);

	wgpuShaderModuleRelease(vertMod);
  wgpuShaderModuleRelease(fragMod);
  
  ctx.pipelines.push(pipeline);
  return (PipelineID)(ctx.pipelines.size - 1);
}

void BindPipeline(PipelineID pipeline) {
	if (!ctx.inFrame) {
    logError("Called outside of frame");
    return;
  }

  if (pipeline >= ctx.pipelines.size) {
    logError("Invalid pipeline ID %u", pipeline);
    return;
  }
  
	wgpuRenderPassEncoderSetPipeline(ctx.currentPass, ctx.pipelines[pipeline]);
  ctx.currentPipelineId = pipeline;
}

void DestroyPipeline(PipelineID pipeline) {
  if (pipeline < ctx.pipelines.size) {
		if (ctx.pipelines[pipeline]) { 
			wgpuRenderPipelineRelease(ctx.pipelines[pipeline]);
		}
    ctx.pipelines[pipeline] = nullptr;
  }
}

// -------------------------------------------------------------------------
// Texture / Sampler
// -------------------------------------------------------------------------

using texRes = Result<WGPUTexture, bool>;
static 
texRes
createTextureBase(const void* data, u32 width, u32 height, u8 channels) {
	if (!ctx.ready) {
    logError("Renderer not ready");
    return texRes::Err(false);
  }
	if (width == 0 || height == 0) {
    logError("Invalid texture dimensions %ux%u", width, height);
    return texRes::Err(false);
  }
	
	u32 unalignedBytes = width * channels;
	u32 alignedBytes = (unalignedBytes + 255) & ~255;
	// Number of Bytes per Row of a texture must be a multiple of 256

  WGPUTextureDescriptor desc{};
  desc.size = {width, height, 1};
	desc.mipLevelCount = 1;
  desc.sampleCount = 1;
  desc.dimension = WGPUTextureDimension_2D;
  
  if (channels == 1) {
    desc.format = WGPUTextureFormat_R8Unorm;
  } else if (channels == 4) {
    desc.format = WGPUTextureFormat_RGBA8Unorm;
  } else {
    logError("Unsupported channel count: %d. Only 1 and 4 are currently supported.", channels);
    return texRes::Err(false);
  }

  desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
	WGPUTexture texture = wgpuDeviceCreateTexture(ctx.device, &desc);

  if (data) {
		WGPUTexelCopyTextureInfo dest{};
		dest.texture = texture;
    dest.mipLevel = 0;
    dest.origin = {0, 0, 0};
    dest.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout layout{};
		layout.offset = 0;
    layout.rowsPerImage = height;
		layout.bytesPerRow = alignedBytes;

		if (unalignedBytes == alignedBytes) {
      wgpuQueueWriteTexture(ctx.queue, &dest, data, width * height * channels, &layout, &desc.size);
    } else {
      u32 paddedSize = alignedBytes * height;
      u8* paddedBuffer = (u8*)malloc(paddedSize);
      
      const u8* src = (const u8*)data;
      
      for (u32 y = 0; y < height; ++y) {
        memcpy(paddedBuffer + (y * alignedBytes), src + (y * unalignedBytes), unalignedBytes);
      }
      
      wgpuQueueWriteTexture(ctx.queue, &dest, paddedBuffer, paddedSize, &layout, &desc.size);
      
      free(paddedBuffer);
    }
  }

	return texRes::Ok(texture);
}

TextureID CreateTexture(const void* data, u32 width, u32 height, u8 channels) {

	texRes res = createTextureBase(data, width, height, channels);
	if (!res) {
		return INVALID_ID;
	}
	WGPUTexture texture = res.unwrap(); 

  ctx.textures.push(texture);
	ctx.textureViews.push(wgpuTextureCreateView(texture, nullptr));
	ctx.offscreenDepthViews.push(nullptr);

  return (TextureID)(ctx.textures.size - 1);
}

void UpdateTexture(TextureID targetID, const void* data, u32 width, u32 height, u8 channels) {
	if (!ctx.ready) {
    logError("Renderer not ready");
    return;
  }

	if (targetID >= ctx.textures.size || targetID == INVALID_ID) {
		logError("Invalid Texture ID: %u", targetID);
    logError("Invalid Texture ID");
		return;
	}

	texRes res = createTextureBase(data, width, height, channels);
	if (!res) {
			return;
	}
	WGPUTexture texture = res.unwrap(); 

  if (ctx.textures[targetID]) {
    wgpuTextureDestroy(ctx.textures[targetID]);
    wgpuTextureRelease(ctx.textures[targetID]);
    wgpuTextureViewRelease(ctx.textureViews[targetID]);
  }

  ctx.textures[targetID] = texture;
  ctx.textureViews[targetID] = wgpuTextureCreateView(texture, nullptr);

  for (auto& bg : ctx.bindGroups) {
    for (const auto& binding : bg.recipe.bindings) {
      if (binding.type == BindingType::Texture && binding.texture.id == targetID) {
        bg.dirty = true;
        break;
      }
    }
  }
}

TextureID CreateRenderTexture(u32 width, u32 height) {
	if (!ctx.ready) {
    logError("Renderer not ready");
  	return INVALID_ID;
  }

  WGPUTextureDescriptor desc = {};
  desc.size = {width, height, 1};
  desc.mipLevelCount = 1;
  desc.sampleCount = 1;
  desc.dimension = WGPUTextureDimension_2D;
  desc.format = WGPUTextureFormat_BGRA8Unorm;
  desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment;

  WGPUTexture texture = wgpuDeviceCreateTexture(ctx.device, &desc);
  WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

  WGPUTextureDescriptor depthDesc = {};
  depthDesc.size = {width, height, 1};
  depthDesc.mipLevelCount = 1;
  depthDesc.sampleCount = 1;
  depthDesc.dimension = WGPUTextureDimension_2D;
  depthDesc.format = ctx.depthFormat;
  depthDesc.usage = WGPUTextureUsage_RenderAttachment;
  
  WGPUTexture depthTex = wgpuDeviceCreateTexture(ctx.device, &depthDesc);
  WGPUTextureView depthView = wgpuTextureCreateView(depthTex, nullptr);
  wgpuTextureRelease(depthTex);

  ctx.textures.push(texture);
  ctx.textureViews.push(view);
  ctx.offscreenDepthViews.push(depthView);

  return (TextureID)(ctx.textures.size - 1);
}

SamplerID CreateSampler(FilterType filter, WrapMode wrap) {
	if (!ctx.ready) {
    logError("Renderer not ready");
    return INVALID_ID;
  }

  WGPUSamplerDescriptor desc{};
  desc.minFilter = ToWGPU(filter);
  desc.magFilter = ToWGPU(filter);
  desc.addressModeU = ToWGPU(wrap);
  desc.addressModeV = ToWGPU(wrap);
	desc.lodMinClamp = 0.0f;
  desc.lodMaxClamp = 32.0f;
  desc.maxAnisotropy = 1;
  
	WGPUSampler sampler = wgpuDeviceCreateSampler(ctx.device, &desc);
  ctx.samplers.push(sampler);
  return (SamplerID)(ctx.samplers.size - 1);
}

void DestroyTexture(TextureID texture) {
  if(texture < ctx.textures.size) {
		if (ctx.textures[texture]) {
			wgpuTextureDestroy(ctx.textures[texture]);
			wgpuTextureRelease(ctx.textures[texture]);
		}
		if (ctx.textureViews[texture]) {
			wgpuTextureViewRelease(ctx.textureViews[texture]);
		}
    ctx.textures[texture] = nullptr;
    ctx.textureViews[texture] = nullptr;
  }
}

void DestroySampler(SamplerID sampler) {
  if(sampler < ctx.samplers.size) {
		if (ctx.samplers[sampler]) { 
			wgpuSamplerRelease(ctx.samplers[sampler]);
		}
  	ctx.samplers[sampler] = nullptr;
  }
}

// -------------------------------------------------------------------------
// Execution
// -------------------------------------------------------------------------

void BeginOffscreenPass(TextureID target) {
  if (!ctx.ready) { 
    logError("Renderer not ready");
		return;
	}

  if (target >= ctx.textures.size || !ctx.offscreenDepthViews[target]) {
    logError("Invalid Render Target ID %u", target);
    return;
  }

  if (!ctx.currentEncoder) {
    ctx.currentEncoder = wgpuDeviceCreateCommandEncoder(ctx.device, nullptr);
  }

  WGPURenderPassColorAttachment attachment = {};
  attachment.view = ctx.textureViews[target];
	attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  attachment.loadOp = WGPULoadOp_Clear;
  attachment.storeOp = WGPUStoreOp_Store;
  attachment.clearValue = {0.1f, 0.1f, 0.1f, 1.0f};

  WGPURenderPassDepthStencilAttachment depthAttachment = {};
  depthAttachment.view = ctx.offscreenDepthViews[target];
  depthAttachment.depthClearValue = 1.0f;
  depthAttachment.depthLoadOp = WGPULoadOp_Clear;
  depthAttachment.depthStoreOp = WGPUStoreOp_Store;
	depthAttachment.stencilLoadOp = WGPULoadOp_Clear;
  depthAttachment.stencilStoreOp = WGPUStoreOp_Store;

  WGPURenderPassDescriptor passDesc = {};
  passDesc.colorAttachmentCount = 1;
  passDesc.colorAttachments = &attachment;
  passDesc.depthStencilAttachment = &depthAttachment;

  ctx.currentPass = wgpuCommandEncoderBeginRenderPass(ctx.currentEncoder, &passDesc);
  ctx.inFrame = true;
}

void EndOffscreenPass() {
  if (ctx.currentPass) {
    wgpuRenderPassEncoderEnd(ctx.currentPass);
    wgpuRenderPassEncoderRelease(ctx.currentPass);
    ctx.currentPass = nullptr;
  }

  ctx.inFrame = false; 
}

void BeginFrame() {
  if (!ctx.ready) return;

  WGPUSurfaceTexture surfaceTexture;
	wgpuSurfaceGetCurrentTexture(ctx.surface, &surfaceTexture);
	if (!surfaceTexture.texture) {
  	return;
  }

  ctx.currentBackBufferView = wgpuTextureCreateView(surfaceTexture.texture, nullptr);
	if (!ctx.currentEncoder) {
    ctx.currentEncoder = wgpuDeviceCreateCommandEncoder(ctx.device, nullptr);
  }

  WGPURenderPassColorAttachment attachment = {};
  attachment.view = ctx.currentBackBufferView;
	attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  attachment.loadOp = WGPULoadOp_Clear;
  attachment.storeOp = WGPUStoreOp_Store;
	attachment.clearValue = {
    ctx.config.clearColor.r, 
    ctx.config.clearColor.g, 
    ctx.config.clearColor.b, 
    ctx.config.clearColor.a
  };

	WGPURenderPassDepthStencilAttachment depthAttachment = {};
  depthAttachment.view = ctx.depthTextureView;
  depthAttachment.depthClearValue = 1.0f;
  depthAttachment.depthLoadOp = WGPULoadOp_Clear;
  depthAttachment.depthStoreOp = WGPUStoreOp_Store;
  depthAttachment.stencilLoadOp = WGPULoadOp_Clear; 
  depthAttachment.stencilStoreOp = WGPUStoreOp_Store;

  WGPURenderPassDescriptor passDesc{};
  passDesc.colorAttachmentCount = 1;
  passDesc.colorAttachments = &attachment;
	
	if (ctx.depthTextureView) {
    passDesc.depthStencilAttachment = &depthAttachment;
  }

  ctx.currentPass = wgpuCommandEncoderBeginRenderPass(ctx.currentEncoder, &passDesc);
  ctx.inFrame = true;
}

void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
  if (!ctx.inFrame) {
    logError("Called outside of frame");
    return;
  }
  if (!ctx.currentPipelineId) {
    logError("No pipeline bound");
    return;
  }

  wgpuRenderPassEncoderDraw(ctx.currentPass, vertexCount, instanceCount, firstVertex, firstInstance);
}

void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset, u32 firstInstance) {
	if (!ctx.inFrame) {
    logError("Called outside of frame");
    return;
  }

  if (!ctx.currentPipelineId) {
    logError("No pipeline bound");
    return;
  }

	wgpuRenderPassEncoderDrawIndexed(ctx.currentPass, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void EndFrame() {
	if (!ctx.inFrame) {
    logError("Called outside of frame");
    return;
  }

	if (ctx.currentPass) {
    wgpuRenderPassEncoderEnd(ctx.currentPass);
    wgpuRenderPassEncoderRelease(ctx.currentPass);
    ctx.currentPass = nullptr;
	}

	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(ctx.currentEncoder, nullptr);

	if (ctx.currentEncoder) {
    wgpuCommandEncoderRelease(ctx.currentEncoder);
    ctx.currentEncoder = nullptr;
  }

	wgpuQueueSubmit(ctx.queue, 1, &commands);

  ctx.inFrame = false;

  wgpuCommandBufferRelease(commands);

	if (ctx.currentBackBufferView) {
    wgpuTextureViewRelease(ctx.currentBackBufferView);
    ctx.currentBackBufferView = nullptr;
  }

}

}
