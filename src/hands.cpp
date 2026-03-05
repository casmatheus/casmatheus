#include "compiler/context.h"
#include "debug/log.h"
#include "debug/perf.h"
#include "core/primitive.h"
#include "os/window.h"
#include "os/time.h"
#include "os/input.h"
#include "gpu/renderer.h"
#include "js/texture.h"
#include "js/utility.h"
#include "game/text.h"
#include "game/shape.h"
#include "game/sprite.h"
#include <math.h>
#include <stdio.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunknown-pragmas" 

#include <emscripten.h>
#include <emscripten/html5.h>
#include <glm/gtc/matrix_transform.hpp>

#pragma clang diagnostic pop

constexpr i32 LANDMARKS_PER_HAND = 21;
constexpr i32 MAX_HANDS = 2;

const int BONE_PAIRS[21][2] = {
  {0, 1}, {1, 2}, {2, 3}, {3, 4},         // Thumb
  {1, 5}, {5, 6}, {6, 7}, {7, 8},         // Index Finger
  {5, 9}, {9, 10}, {10, 11}, {11, 12},    // Middle Finger
  {9, 13}, {13, 14}, {14, 15}, {15, 16},  // Ring Finger
  {13, 17}, {17, 18}, {18, 19}, {19, 20}, // Pinky
  {0, 17}                                 // Connect pinky base to wrist
};

struct HandData {
  bool isActive = false;
  f32 targetLandmarks[LANDMARKS_PER_HAND * 3];
	f32 renderLandmarks[LANDMARKS_PER_HAND * 3];
};

HandData g_Hands[MAX_HANDS];


extern "C" {
  EMSCRIPTEN_KEEPALIVE
  void ForceExit() {
    logInfo("Page unloading, forcing cleanup...");
		emscripten_cancel_main_loop();
    Renderer::Destroy();
  }

EMSCRIPTEN_KEEPALIVE
  f32* GetHandLandmarksPointer(i32 handIndex) {
    if (handIndex >= 0 && handIndex < MAX_HANDS) {
      return g_Hands[handIndex].targetLandmarks;
    }
    return nullptr;
  }

  EMSCRIPTEN_KEEPALIVE
  void SetHandActive(i32 handIndex, bool active) {
    if (handIndex >= 0 && handIndex < MAX_HANDS) {
      g_Hands[handIndex].isActive = active;
    }
  }
}


const f32 TARGET_WIDTH = 1920.0f;
const f32 TARGET_HEIGHT = 1080.0f;

struct GameState {
  bool initialized = false;
	bool active = false;

	Instant startTime;

	Font* font;
};

GameState game;

void initGame() {
  logInfo("Initializing Game Resources...");

	ShapeRenderer::Init();
	SpriteRenderer::Init();

  game.startTime = Instant::Now();
  game.initialized = true;
  logInfo("Game Initialized!");
}

void updateGame(f32 dt, f32 aspect) {
	(void)dt;
	(void)aspect;
}

void renderLoop(void* arg) {
  (void)arg;

  if (!Renderer::Initialized()) return;

  if (!game.initialized) {
    initGame();
  }

	u32 winW, winH;
  Window::Size(&winW, &winH);

  u32 phyW, phyH;
  Window::GetFramebufferSize(&phyW, &phyH);

  static u32 lastW = 0, lastH = 0;
  if (phyW != lastW || phyH != lastH) {
    Renderer::OnResize(phyW, phyH);
    lastW = phyW;
    lastH = phyH;
  }

	f32 targetAspect = TARGET_WIDTH / TARGET_HEIGHT;
	f32 windowAspect = (f32)winW / (f32)winH;

	f32 virtualW, virtualH;
	if (windowAspect < targetAspect) {
    virtualW = TARGET_WIDTH;
    virtualH = TARGET_WIDTH / windowAspect;
	} else {
    virtualH = TARGET_HEIGHT;
    virtualW = TARGET_HEIGHT * windowAspect;
	}

	static Instant lastTime = Instant::Now();

  Instant now = Instant::Now();
  Duration elapsed = now - lastTime;
  lastTime = now;

  f32 dt = elapsed.asSeconds();

  if (dt > 0.05f) {
    dt = 0.05f;
  }

  updateGame(dt, windowAspect);

  Renderer::BeginFrame();

	glm::mat4 gameProj = glm::ortho(-windowAspect, windowAspect, -1.0f, 1.0f, -1.0f, 1.0f);

	ShapeRenderer::BeginFrame(&gameProj[0][0]);
	SpriteRenderer::BeginFrame(&gameProj[0][0]);

	f32 jointColorLeft[] = {0.0f, 1.0f, 0.5f, 1.0f};
	f32 jointColorRight[] = {1.0f, 0.25f, 0.25f, 1.0f};

	f32 boneColorLeft[] = {0.0f, 1.0f, 0.5f, 0.6f};
  f32 boneColorRight[] = {1.0f, 0.25f, 0.25f, 0.6f};

  for (int i = 0; i < MAX_HANDS; ++i) {
    if (!g_Hands[i].isActive) continue;

		constexpr f32 lerpFactor = 0.4f; 
    for (int j = 0; j < LANDMARKS_PER_HAND * 3; ++j) {
      g_Hands[i].renderLandmarks[j] += (g_Hands[i].targetLandmarks[j] - g_Hands[i].renderLandmarks[j]) * lerpFactor;
    }

		f32* jointColor = (i == 0) ? jointColorLeft : jointColorRight;
		f32* boneColor = (i == 0) ? boneColorLeft : boneColorRight;

		for (int b = 0; b < 21; ++b) {
      int idxA = BONE_PAIRS[b][0];
      int idxB = BONE_PAIRS[b][1];

      f32 xA = ((1.0f - g_Hands[i].renderLandmarks[idxA * 3 + 0]) * 2.0f - 1.0f) * windowAspect;
      f32 yA = (g_Hands[i].renderLandmarks[idxA * 3 + 1] * 2.0f - 1.0f);
      
      f32 xB = ((1.0f - g_Hands[i].renderLandmarks[idxB * 3 + 0]) * 2.0f - 1.0f) * windowAspect;
      f32 yB = (g_Hands[i].renderLandmarks[idxB * 3 + 1] * 2.0f - 1.0f);

      f32 dx = xB - xA;
      f32 dy = yB - yA;
      f32 dist = sqrtf(dx*dx + dy*dy);
      f32 rot = atan2f(dy, dx);
      
      f32 cx = (xA + xB) * 0.5f;
      f32 cy = (yA + yB) * 0.5f;
      f32 thickness = 0.012f;

      SpriteRenderer::Draw(cx - dist * 0.5f, cy - thickness * 0.5f, dist, thickness, INVALID_ID, boneColor, rot);
    }

    for (int j = 0; j < LANDMARKS_PER_HAND; ++j) {
      f32 normX = g_Hands[i].renderLandmarks[j * 3 + 0];
      f32 normY = g_Hands[i].renderLandmarks[j * 3 + 1];
			f32 normZ = g_Hands[i].renderLandmarks[j * 3 + 2];
			
      f32 worldX = ((1.0f - normX) * 2.0f - 1.0f) * windowAspect;
      f32 worldY = (normY * 2.0f - 1.0f);

			f32 diameter = 0.035f - (normZ * 0.15f);

			if (diameter < 0.01f) diameter = 0.01f;

      f32 radius = diameter * 0.5f;

      ShapeRenderer::DrawCircle(worldX - radius, worldY - radius, diameter, jointColor);
    }
  }

	SpriteRenderer::EndFrame();
	ShapeRenderer::EndFrame();

  Renderer::EndFrame();
}

int main() {
  Window window = Window::Create("Hands").unwrap();

  EM_ASM(
		window.addEventListener('beforeunload', function(event) {
      Engine._ForceExit();
    });

		window.addEventListener('pagehide', function(event) {
      Engine._ForceExit();
    });
  );

  RendererConfig cfg = RendererConfig::Default(window);
  Renderer::New(cfg);

  emscripten_set_main_loop_arg(renderLoop, nullptr, 0, true);

  return 0;
}
