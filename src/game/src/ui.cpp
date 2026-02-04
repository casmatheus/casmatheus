#include "debug/perf.h"
#include "core/math.h"
#include "core/vector.h"
#include "game/ui.h"
#include "game/text.h"
#include "game/font.h"
#include "js/texture.h"

struct UIBatchState {
  PipelineID pipeline;
  BufferID vbo;
  BufferID ibo;
  BufferID uniformBuffer;
  
  BindGroupID globalBindGroup;
  BindGroupID textureBindGroup;
	BindGroupID currentBindGroup;
	
	u32 vboOffsetBytes; 
  u32 iboOffsetBytes; 
  u32 indexOffsetCount;
  
  TextureID whiteTexture;
  SamplerID sampler;

  Vec<UIVertex> vertices;
  Vec<u16> indices;

  struct Uniforms {
    f32 screenSize[2];
    f32 padding[2];
  } uniforms;
};

static UIBatchState ui;

const char* UI_VERTEX_SHADER = R"(
  struct Uniforms {
    screenSize: vec2<f32>,
    padding: vec2<f32>,
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
    let x = (in.pos.x / u.screenSize.x) * 2.0 - 1.0;
    let y = 1.0 - (in.pos.y / u.screenSize.y) * 2.0;
    
    out.Position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
  }
)";

const char* UI_FRAGMENT_SHADER = R"(
    @group(0) @binding(1) var mySampler: sampler;
    @group(1) @binding(0) var myTexture: texture_2d<f32>;

    @fragment
    fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>) -> @location(0) vec4<f32> {
      return textureSample(myTexture, mySampler, uv) * color;
    }
)";

void UIRenderer::Init() {
  ui.uniformBuffer = Renderer::CreateBuffer(nullptr, sizeof(UIBatchState::Uniforms), BufferType::Uniform);
  
  ui.vbo = Renderer::CreateBuffer(nullptr, sizeof(UIVertex) * 1024, BufferType::Vertex);
  ui.ibo = Renderer::CreateBuffer(nullptr, sizeof(u16) * 6 * 1024, BufferType::Index);

  ui.whiteTexture = Renderer::DefaultTexture();
	ui.sampler = Renderer::CreateSampler(FilterType::Nearest, WrapMode::ClampToEdge);

  PipelineConfig pCfg = PipelineConfig::Default();
  pCfg.vertexShaderSource = UI_VERTEX_SHADER;
  pCfg.fragmentShaderSource = UI_FRAGMENT_SHADER;
  
  // Enable Alpha Blending for transparency
  pCfg.blendMode = BlendMode::Alpha;
  pCfg.depthTest = false;
  pCfg.depthWrite = false;

  // Layout: Pos(2), UV(2), Color(4)
  pCfg.vertexLayout.stride = sizeof(UIVertex);
  pCfg.vertexLayout.attributeCount = 3;
  pCfg.vertexLayout.attributes[0] = {0, VertexFormat::Float32x2, offsetof(UIVertex, pos)};
  pCfg.vertexLayout.attributes[1] = {1, VertexFormat::Float32x2, offsetof(UIVertex, uv)};
  pCfg.vertexLayout.attributes[2] = {2, VertexFormat::Float32x4, offsetof(UIVertex, color)};
  ui.pipeline = Renderer::CreatePipeline(pCfg);

  ResourceBinding globalBinds[] = {
    ResourceBinding::BindUniform(0, ui.uniformBuffer),
    ResourceBinding::BindSampler(1, ui.sampler)
  };
  ui.globalBindGroup = Renderer::CreateBindGroup(ui.pipeline, 0, globalBinds);
  
  ResourceBinding matBinds[] = { ResourceBinding::BindTexture(0, ui.whiteTexture) };
  ui.textureBindGroup = Renderer::CreateBindGroup(ui.pipeline, 1, matBinds);
}

void UIRenderer::BeginFrame(u32 width, u32 height) {
  ui.vertices.clear();
  ui.indices.clear();
	ui.currentBindGroup = INVALID_ID;

	ui.vboOffsetBytes = 0;
  ui.iboOffsetBytes = 0;
  ui.indexOffsetCount = 0;
  
  ui.uniforms.screenSize[0] = (f32)width;
  ui.uniforms.screenSize[1] = (f32)height;
  Renderer::UpdateBuffer(ui.uniformBuffer, &ui.uniforms, sizeof(ui.uniforms));
}

void UIRenderer::PushQuad(f32 x, f32 y, f32 w, f32 h, f32 colorTop[4], f32 colorBottom[4], BindGroupID bindGroup) {

	if (bindGroup == INVALID_ID) {
		bindGroup = ui.textureBindGroup;
	}

	if (ui.currentBindGroup != INVALID_ID && ui.currentBindGroup != bindGroup) {
    UIRenderer::Flush();
  }

	ui.currentBindGroup = bindGroup;

  u16 startIdx = (u16)ui.vertices.size;
  
 	ui.vertices.push({{x, y + h},     {0.0f, 1.0f}, {colorBottom[0], colorBottom[1], colorBottom[2], colorBottom[3]}});
 	ui.vertices.push({{x + w, y + h}, {1.0f, 1.0f}, {colorBottom[0], colorBottom[1], colorBottom[2], colorBottom[3]}});
 	ui.vertices.push({{x + w, y},     {1.0f, 0.0f}, {colorTop[0], colorTop[1], colorTop[2], colorTop[3]}});
	ui.vertices.push({{x, y},         {0.0f, 0.0f}, {colorTop[0], colorTop[1], colorTop[2], colorTop[3]}});

  ui.indices.push(startIdx + 0); 
	ui.indices.push(startIdx + 1); 
	ui.indices.push(startIdx + 2);
  ui.indices.push(startIdx + 2); 
	ui.indices.push(startIdx + 3); 
	ui.indices.push(startIdx + 0);
}

void UIRenderer::EndFrame() {
	UIRenderer::Flush();
}

void UIRenderer::Flush() {
  if (ui.indices.empty()) return;

	u32 vSize = (u32)(ui.vertices.size * sizeof(UIVertex));
  u32 iSize = (u32)(ui.indices.size * sizeof(u16));

  Renderer::UpdateBuffer(ui.vbo, ui.vertices.data, vSize, ui.vboOffsetBytes);
  Renderer::UpdateBuffer(ui.ibo, ui.indices.data, iSize, ui.iboOffsetBytes);

  Renderer::BindPipeline(ui.pipeline);
  Renderer::BindVertexBuffer(ui.vbo);
  Renderer::BindIndexBuffer(ui.ibo);
  Renderer::BindGroup(0, ui.globalBindGroup);

  if (ui.currentBindGroup != INVALID_ID) {
     Renderer::BindGroup(1, ui.currentBindGroup);
  } else {
     Renderer::BindGroup(1, ui.textureBindGroup);
  }

	i32 vertexBase = (i32)(ui.vboOffsetBytes / sizeof(UIVertex));
  Renderer::DrawIndexed((u32)ui.indices.size, 1, ui.indexOffsetCount, vertexBase);

	ui.vboOffsetBytes += vSize;
  ui.iboOffsetBytes += iSize;
  ui.indexOffsetCount += (u32)ui.indices.size;

  ui.vertices.clear();
  ui.indices.clear();
}

static UI::State s_State;
static Font* s_Font;

void UI::Init(PlayerID playerIdx) {
  s_State.playerIdx = playerIdx;
	s_State.mode = UI::InputMode::Mouse;
	s_State.focusIndex = 0;
	s_State.lastFrameWidgetCount = 0;

  UIRenderer::Init();
	TextRenderer::Init();
	s_Font = Font::Load("Inter");

	// Accept / Interact
	Input::MapAction(playerIdx, ACTION_ACCEPT, InputBinding::FromKey(Key::Enter).bypass());
  Input::MapAction(playerIdx, ACTION_ACCEPT, InputBinding::FromKey(Key::Space).bypass());
  Input::MapAction(playerIdx, ACTION_ACCEPT, InputBinding::FromMouse(MouseButton::Left).bypass());
  Input::MapAction(playerIdx, ACTION_ACCEPT, InputBinding::FromButton(GamepadButton::FaceDown).bypass()); // Xbox 'A'

  // Back / Cancel
  Input::MapAction(playerIdx, ACTION_BACK, InputBinding::FromKey(Key::Escape).bypass());
  Input::MapAction(playerIdx, ACTION_BACK, InputBinding::FromButton(GamepadButton::FaceRight).bypass()); // Xbox 'B'

  // Navigation (Arrows + DPad)
  Input::MapAction(playerIdx, ACTION_UP,   InputBinding::FromKey(Key::ArrowUp).bypass());
  Input::MapAction(playerIdx, ACTION_UP,   InputBinding::FromButton(GamepadButton::DPadUp).bypass());
  
  Input::MapAction(playerIdx, ACTION_DOWN, InputBinding::FromKey(Key::ArrowDown).bypass());
  Input::MapAction(playerIdx, ACTION_DOWN, InputBinding::FromButton(GamepadButton::DPadDown).bypass());
}

void UI::Begin(f32 deltaTime, u32 width, u32 height) {
	PERF_SCOPE("[UI] Frame");

  (void)deltaTime;
	s_State.widgetCounter = 0;
	s_State.anyHot = false;

	s_State.mouseX = Input::GetMouseX();
  s_State.mouseY = Input::GetMouseY();

	bool mouseMoved = Abs(Input::GetMouseDeltaX()) > 0.1f || Abs(Input::GetMouseDeltaY()) > 0.1f;
  if (mouseMoved) {
    s_State.mode = UI::InputMode::Mouse;
  }
  
  bool navUp   = Input::IsJustPressed(s_State.playerIdx, ACTION_UP);
  bool navDown = Input::IsJustPressed(s_State.playerIdx, ACTION_DOWN);

  if (navUp || navDown) {
    s_State.mode = UI::InputMode::Gamepad;
  }

  if (s_State.mode == UI::InputMode::Gamepad) {
    if (navDown) s_State.focusIndex++;
    if (navUp)   s_State.focusIndex--;

    if (s_State.lastFrameWidgetCount > 0) {
			// Wrap Around
      if (s_State.focusIndex >= s_State.lastFrameWidgetCount) {
				s_State.focusIndex = 0;
			}
      if (s_State.focusIndex < 0) {
				s_State.focusIndex = s_State.lastFrameWidgetCount - 1;
			}
    } else {
      s_State.focusIndex = 0;
    }
  }

	s_State.interactPressed = Input::IsJustPressed(s_State.playerIdx, ACTION_ACCEPT);
  
	UIRenderer::BeginFrame(width, height);
	TextRenderer::BeginFrame(width, height);
}

void UI::End() {
  UIRenderer::EndFrame();
	TextRenderer::EndFrame();
  
	s_State.lastFrameWidgetCount = s_State.widgetCounter;

	if (s_State.interactPressed) {
    Input::ConsumeAll();
  } 
  else if (s_State.anyHot) {
    Input::Consume(DeviceType::Mouse);
  }
}

static bool RegionHit(f32 x, f32 y, f32 w, f32 h) {
  return (s_State.mouseX >= x && s_State.mouseX <= x + w && s_State.mouseY >= y && s_State.mouseY <= y + h);
}

bool UI::Button(const char* label, f32 x, f32 y, f32 w, f32 h) {
	i32 index = s_State.widgetCounter++; // Claim an ID
  bool hot = false;

  if (s_State.mode == UI::InputMode::Mouse) {
    if (RegionHit(x, y, w, h)) {
      hot = true;
      s_State.focusIndex = index;
    }
  } else {
    if (s_State.focusIndex == index) {
      hot = true;
    }
  }

	s_State.anyHot |= hot;

	f32 topColor[4]    = {0.28f, 0.28f, 0.32f, 1.0f};
	f32 bottomColor[4] = {0.20f, 0.20f, 0.24f, 1.0f};
  f32 textColor[4] = {0.9f, 0.9f, 0.9f, 1.0f}; 
	if (hot) {
    if (s_State.interactPressed) {
      topColor[0] = 0.05f; topColor[1] = 0.05f; topColor[2] = 0.05f;
      bottomColor[0] = 0.1f; bottomColor[1] = 0.1f; bottomColor[2] = 0.1f;
    } else {
      topColor[0] = 0.35f; topColor[1] = 0.35f; topColor[2] = 0.45f;
      bottomColor[0] = 0.25f; bottomColor[1] = 0.25f; bottomColor[2] = 0.35f;
      textColor[0] = 1.0f; textColor[1] = 1.0f; textColor[2] = 1.0f;
    }
	}

  UIRenderer::PushQuad(x, y, w, h, topColor, bottomColor);

	if (s_Font && s_Font->loaded) {
    f32 fontSize = 48.0f;
    f32 textW = TextRenderer::MeasureText(s_Font, label, fontSize);
    f32 textX = x + (w - textW) * 0.5f;
    f32 textY = y + (h * 0.5f) + (fontSize * 0.3f); 
    TextRenderer::DrawText(s_Font, label, textX, textY, fontSize, textColor);
  }

	return (hot && s_State.interactPressed);
}
