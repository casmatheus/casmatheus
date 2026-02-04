#include "compiler/context.h"
#include "debug/log.h"
#include "debug/perf.h"
#include "core/primitive.h"
#include "core/utility.h"
#include "os/window.h"
#include "os/time.h"
#include "os/input.h"
#include "gpu/renderer.h"
#include "js/texture.h"
#include "game/ui.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunknown-pragmas" 

#include <emscripten.h>
#include <emscripten/html5.h>

extern "C" {
  EMSCRIPTEN_KEEPALIVE
  void ForceExit() {
    logInfo("Page unloading, forcing cleanup...");
		emscripten_cancel_main_loop();
    Renderer::Destroy();
  }
}

#pragma clang diagnostic pop

const char* VERTEX_SHADER = R"(
  struct GlobalUniforms {
    viewProj: mat4x4<f32>,
    time: f32,
  };
  @group(0) @binding(0) var<uniform> globals : GlobalUniforms;

  struct VertexInput {
    @location(0) pos: vec3<f32>,
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
    // Simple rotation based on time
    let c = cos(globals.time);
    let s = sin(globals.time);
    let x = in.pos.x * c - in.pos.y * s;
    let y = in.pos.x * s + in.pos.y * c;
    
    out.Position = globals.viewProj * vec4<f32>(x, y, in.pos.z, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
  }
)";

const char* FRAGMENT_SHADER = R"(
  @group(0) @binding(1) var mySampler: sampler;
  @group(1) @binding(0) var myTexture: texture_2d<f32>;

  @fragment
  fn main(@location(0) uv: vec2<f32>, @location(1) color: vec4<f32>) -> @location(0) vec4<f32> {
    return textureSample(myTexture, mySampler, uv) * color;
  }
)";

const char* BLIT_VERT = R"(
  struct VSOut { @builtin(position) pos: vec4f, @location(0) uv: vec2f };
  @vertex fn main(@builtin(vertex_index) i: u32) -> VSOut {
    var pos = array<vec2f, 6>(
      vec2f(-1.0, -1.0), vec2f( 1.0, -1.0), vec2f( 1.0,  1.0),
      vec2f(-1.0, -1.0), vec2f( 1.0,  1.0), vec2f(-1.0,  1.0)
    );
    var out: VSOut;
    out.pos = vec4f(pos[i], 0.0, 1.0);
    out.uv = pos[i] * vec2f(0.5, -0.5) + 0.5; // Map -1..1 to 0..1
    return out;
  }
)";

const char* BLIT_FRAG = R"(
  @group(0) @binding(0) var t: texture_2d<f32>;
  @group(0) @binding(1) var s: sampler;
  @fragment fn main(@location(0) uv: vec2f) -> @location(0) vec4f {
    return textureSample(t, s, uv);
  }
)";

#define ACTION_PROFILE 100

// Simple Quad Data
struct Vertex {
  f32 pos[3];
  f32 uv[2];
  f32 color[4];
};

const Vertex vertices[] = {
  {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // BL
  {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // BR
  {{ 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // TR
  {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // TL
};

const u16 indices[] = { 0, 1, 2, 2, 3, 0 };

struct GameState {
  bool initialized = false;
	Instant startTime;

  PipelineID pipeline;
  BufferID vbo;
  BufferID ibo;
  BufferID uniformBuffer;

  TextureID texture;
  SamplerID sampler;

  BindGroupID globalBindGroup;
  BindGroupID materialBindGroup;

	TextureID renderTarget;
	PipelineID blitPipeline;
	BindGroupID blitBindGroup;

  struct UniformData {
    glm::mat4 viewProj;
    float time;
    float _padding[3];
  } uniforms;
};

GameState game;

void InitGame() {
  logInfo("Initializing Game Resources...");

	Input::Init();
	UI::Init(0); 	// Initialize UI for Player 0
	
	Input::MapAction(0, ACTION_PROFILE, InputBinding::FromKey(Key::P).bypass());

  game.startTime = Instant::Now();
  game.vbo = Renderer::CreateBuffer(vertices, sizeof(vertices), BufferType::Vertex);
  game.ibo = Renderer::CreateBuffer(indices, sizeof(indices), BufferType::Index);
  game.uniformBuffer = Renderer::CreateBuffer(nullptr, sizeof(GameState::UniformData), BufferType::Uniform);

	game.texture = CreateTextureFromURL("checkers.png");
  game.sampler = Renderer::CreateSampler(FilterType::Nearest, WrapMode::ClampToEdge);

  PipelineConfig pCfg = PipelineConfig::Default();
  pCfg.vertexShaderSource = VERTEX_SHADER;
  pCfg.fragmentShaderSource = FRAGMENT_SHADER;
  
  pCfg.vertexLayout.stride = sizeof(Vertex);
  pCfg.vertexLayout.attributes[0] = {0, VertexFormat::Float32x3, offsetof(Vertex, pos)};
  pCfg.vertexLayout.attributes[1] = {1, VertexFormat::Float32x2, offsetof(Vertex, uv)};
  pCfg.vertexLayout.attributes[2] = {2, VertexFormat::Float32x4, offsetof(Vertex, color)};
  pCfg.vertexLayout.attributeCount = 3;

  game.pipeline = Renderer::CreatePipeline(pCfg);

  ResourceBinding globalBinds[] = {
      ResourceBinding::BindUniform(0, game.uniformBuffer),
      ResourceBinding::BindSampler(1, game.sampler)
  };
  game.globalBindGroup = Renderer::CreateBindGroup(game.pipeline, 0, globalBinds);

  ResourceBinding matBinds[] = {
      ResourceBinding::BindTexture(0, game.texture)
  };
  game.materialBindGroup = Renderer::CreateBindGroup(game.pipeline, 1, matBinds);

	game.renderTarget = Renderer::CreateRenderTexture(GAME_WIDTH, GAME_HEIGHT);

  PipelineConfig bCfg = PipelineConfig::Default();
  bCfg.vertexShaderSource = BLIT_VERT;
  bCfg.fragmentShaderSource = BLIT_FRAG;
  bCfg.depthTest = false;
  bCfg.vertexLayout.attributeCount = 0; 
  bCfg.vertexLayout.stride = 0;
  
  game.blitPipeline = Renderer::CreatePipeline(bCfg);
  
  ResourceBinding blitBinds[] = {
    ResourceBinding::BindTexture(0, game.renderTarget),
    ResourceBinding::BindSampler(1, game.sampler)
  };
  game.blitBindGroup = Renderer::CreateBindGroup(game.blitPipeline, 0, blitBinds);

  game.initialized = true;
  logInfo("Game Initialized!");
}

void RenderLoop(void* arg) {
	PERF_SCOPE("[Main] Render Loop");
  (void)arg;

	if (Input::IsJustPressed(0, ACTION_PROFILE)) {
    PERF_REPORT();
	}

  if (!Renderer::Initialized()) return;

  if (!game.initialized) {
    InitGame();
  }

	Input::Poll();

	game.uniforms.time = game.startTime.elapsed().asSeconds();
  
  float aspect = (float)GAME_WIDTH / (float)GAME_HEIGHT;
  game.uniforms.viewProj = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
  
  Renderer::UpdateBuffer(game.uniformBuffer, &game.uniforms, sizeof(game.uniforms));

	u32 winW, winH;
  Window::Size(&winW, &winH);
  static u32 lastW = 0, lastH = 0;
  if (winW != lastW || winH != lastH) {
    Renderer::OnResize(winW, winH);
    lastW = winW;
    lastH = winH;
  }


	Renderer::BeginOffscreenPass(game.renderTarget);
    
  Renderer::BindPipeline(game.pipeline);
  Renderer::BindVertexBuffer(game.vbo);
  Renderer::BindIndexBuffer(game.ibo);
  Renderer::BindGroup(0, game.globalBindGroup);
  if (game.materialBindGroup != INVALID_ID) { 
    Renderer::BindGroup(1, game.materialBindGroup);
	}
      
  Renderer::DrawIndexed(6);
  
  Renderer::EndOffscreenPass();

  Renderer::BeginFrame();

	Renderer::BindPipeline(game.blitPipeline);
  Renderer::BindGroup(0, game.blitBindGroup);
	Renderer::Draw(6);

  UI::Begin(0.016f, winW, winH);

  constexpr f32 btnW = 400.0f;
  constexpr f32 btnH = 80.0f;
  constexpr f32 spaceBetween = 100.0f;
	f32 centerX = (f32)winW / 2.0f - (btnW / 2.0f);

  f32 totalHeight = spaceBetween * 2 + btnH; 
  f32 yPos = (f32)winH / 2.0f - (totalHeight / 2.0f);

  if (UI::Button("Start Game", centerX, yPos, btnW, btnH)) {
    logInfo(">> CLICKED: Start Game");
  }
  yPos += spaceBetween;

  if (UI::Button("Options", centerX, yPos, btnW, btnH)) {
    logInfo(">> CLICKED: Options");
  }
  yPos += spaceBetween;

  if (UI::Button("Exit", centerX, yPos, btnW, btnH)) {
    logInfo(">> CLICKED: Exit");
  }

  UI::End();

  Renderer::EndFrame();
	Input::EndFrame();
}

int main() {
  Window window = Window::Create(WINDOW_WIDTH, WINDOW_HEIGHT, "WebGPU C++").unwrap();

  EM_ASM(
		window.addEventListener('beforeunload', function(event) {
      Module._ForceExit();
    });

		window.addEventListener('pagehide', function(event) {
      Module._ForceExit();
    });
  );

  RendererConfig cfg = RendererConfig::Default(window);
  Renderer::New(cfg);

  emscripten_set_main_loop_arg(RenderLoop, nullptr, 0, true);

  return 0;
}
