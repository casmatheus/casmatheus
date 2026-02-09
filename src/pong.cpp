#include "compiler/context.h"
#include "debug/log.h"
#include "debug/perf.h"
#include "core/primitive.h"
#include "core/utility.h"
#include "core/math.h"
#include "core/vector.h"
#include "os/window.h"
#include "os/time.h"
#include "os/input.h"
#include "os/audio.h"
#include "gpu/renderer.h"
#include "js/texture.h"
#include "js/utility.h"
#include "game/ui.h"
#include "game/text.h"
#include "game/shape.h"
#include "game/sprite.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunknown-pragmas" 

#include <emscripten.h>
#include <emscripten/html5.h>
#include <glm/gtc/matrix_transform.hpp>
#include <math.h>
#include <stdio.h>

extern "C" {
  EMSCRIPTEN_KEEPALIVE
  void ForceExit() {
    logInfo("Page unloading, forcing cleanup...");
		emscripten_cancel_main_loop();
    Renderer::Destroy();
  }
}

#pragma clang diagnostic pop

constexpr ActionID ACTION_PROFILE   = 100;
constexpr ActionID ACTION_MOVE_UP   = 10;
constexpr ActionID ACTION_MOVE_DOWN = 11;
constexpr ActionID ACTION_ROT_RIGHT = 12;
constexpr ActionID ACTION_ROT_LEFT  = 13;
constexpr ActionID ACTION_PAUSE     = 14;

struct GameState {
  bool initialized = false;
	Instant startTime;

	// Textures
  TextureID texBg;     
  TextureID texPad1;
  TextureID texPad2;
  TextureID texBall;

	// UI Themes
	Font* font;
	UITheme startTheme;
	UITheme optionsTheme;
	UITheme exitTheme;

	enum class Scene: u8 { Menu, Playing, Pause };
	Scene currentScene = Scene::Menu;

  f32 playerY   = 0.0f;
	f32 playerRot = 0.0f;
  f32 cpuY      = 0.0f;
  f32 cpuRot    = 0.0f;

	int scorePlayer = 0;
  int scoreCpu = 0;

	f32 screenShake = 0.0f;
	f32 animTime = 0.0f;

	SoundID cheer;
	SoundID cheer2;
  
  struct Ball {
    f32 x, y;
    f32 velX, velY;
		f32 radius;
  } ball;

	struct { f32 x, y; } trail[10];
  int trailIdx = 0;

	const f32 PADDLE_HEIGHT = 0.4f; // 20% of screen height
  const f32 PADDLE_WIDTH  = 0.05f;
  const f32 PADDLE_X      = 1.75f; // Distance from center
	static constexpr f32 INITIAL_VELX = 0.8f;
	static constexpr f32 INITIAL_VELY = 0.5f;
	static constexpr f32 MAX_VELOCITY  = 2.5f;
	static constexpr f32 SMASH_VELOCITY = 3.5f;
};

GameState game;

TextureID CreateSolidTexture(u8 r, u8 g, u8 b, u8 a) {
  u8 data[4] = {r, g, b, a};
  return Renderer::CreateTexture(data, 1, 1, 4);
}

void InitGame() {
  logInfo("Initializing Game Resources...");

	Audio::Init();
	Input::Init();
	ShapeRenderer::Init();
	SpriteRenderer::Init();
	TextRenderer::Init();

	game.font = Font::Load("Custom");

	Input::MapAction(0, ACTION_PROFILE, InputBinding::FromKey(Key::P).bypass());
	Input::MapAction(0, ACTION_MOVE_UP, InputBinding::FromKey(Key::W));
  Input::MapAction(0, ACTION_MOVE_UP, InputBinding::FromKey(Key::ArrowUp));
  Input::MapAction(0, ACTION_MOVE_DOWN, InputBinding::FromKey(Key::S));
  Input::MapAction(0, ACTION_MOVE_DOWN, InputBinding::FromKey(Key::ArrowDown));
  Input::MapAction(0, ACTION_ROT_LEFT, InputBinding::FromKey(Key::A));
  Input::MapAction(0, ACTION_ROT_LEFT, InputBinding::FromKey(Key::ArrowLeft));
  Input::MapAction(0, ACTION_ROT_RIGHT, InputBinding::FromKey(Key::D));
  Input::MapAction(0, ACTION_ROT_RIGHT, InputBinding::FromKey(Key::ArrowRight));
  Input::MapAction(0, ACTION_PAUSE, InputBinding::FromKey(Key::Escape).bypass());
  Input::MapAction(0, ACTION_PAUSE, InputBinding::FromButton(GamepadButton::Start).bypass());

	auto SetColor = [](f32* dest, f32 r, f32 g, f32 b, f32 a) {
    dest[0] = r; dest[1] = g; dest[2] = b; dest[3] = a;
  };

	UITheme theme = {};
  theme.cornerRadius = 16.0f;
  theme.borderSize   = 4.0f;
  theme.shadowSize   = 6.0f;
  SetColor(theme.shadowColor, 0.0f, 0.0f, 0.0f, 0.5f);
  
  SetColor(theme.colIdle.text,  0.9f, 0.9f, 0.9f, 1.0f);
  SetColor(theme.colHover.text, 1.0f, 1.0f, 1.0f, 1.0f);
  SetColor(theme.colPress.text, 0.8f, 0.8f, 0.8f, 1.0f);

  game.startTheme = theme;
  game.optionsTheme = theme;
  game.exitTheme = theme;

  // Idle: Bright Green / Gold Border
  SetColor(game.startTheme.colIdle.background,  0.2f, 0.7f, 0.2f, 1.0f);
  SetColor(game.startTheme.colIdle.border,      0.6f, 0.5f, 0.1f, 1.0f); 
  
  // Hover: Lighter Green / Brighter Gold Border
  SetColor(game.startTheme.colHover.background, 0.3f, 0.8f, 0.3f, 1.0f);
  SetColor(game.startTheme.colHover.border,     0.8f, 0.7f, 0.2f, 1.0f);

  // Press: Dark Green / Dark Gold Border
  SetColor(game.startTheme.colPress.background, 0.1f, 0.5f, 0.1f, 1.0f);
  SetColor(game.startTheme.colPress.border,     0.5f, 0.4f, 0.1f, 1.0f);

  // Idle: Royal Blue / Dark Purple Border
  SetColor(game.optionsTheme.colIdle.background,  0.2f, 0.4f, 0.8f, 1.0f);
  SetColor(game.optionsTheme.colIdle.border,      0.3f, 0.1f, 0.4f, 1.0f);

  // Hover: Lighter Blue
  SetColor(game.optionsTheme.colHover.background, 0.3f, 0.5f, 0.9f, 1.0f);
  SetColor(game.optionsTheme.colHover.border,     0.4f, 0.2f, 0.5f, 1.0f);

  // Press: Darker Blue
  SetColor(game.optionsTheme.colPress.background, 0.1f, 0.3f, 0.6f, 1.0f);
  SetColor(game.optionsTheme.colPress.border,     0.2f, 0.0f, 0.3f, 1.0f);

  // Idle: Red / Maroon Border
  SetColor(game.exitTheme.colIdle.background,  0.8f, 0.2f, 0.2f, 1.0f);
  SetColor(game.exitTheme.colIdle.border,      0.4f, 0.0f, 0.0f, 1.0f);

  // Hover: Bright Red
  SetColor(game.exitTheme.colHover.background, 0.9f, 0.3f, 0.3f, 1.0f);
  SetColor(game.exitTheme.colHover.border,     0.5f, 0.1f, 0.1f, 1.0f);

  // Press: Dark Red
  SetColor(game.exitTheme.colPress.background, 0.6f, 0.1f, 0.1f, 1.0f);
  SetColor(game.exitTheme.colPress.border,     0.3f, 0.0f, 0.0f, 1.0f);

  UI::Init(0, theme, game.font);
	
  game.startTime = Instant::Now();
	game.currentScene = GameState::Scene::Menu;
	game.ball = { 0.0f, 0.0f, game.INITIAL_VELX, game.INITIAL_VELY, 0.06f };

	game.cheer = Audio::LoadFromURL("/Assets/Audio/cheer.wav");
	game.cheer2 = Audio::LoadFromURL("/Assets/Audio/cheer2.wav");
  game.texBg   = CreateTextureFromURL("/Assets/checkers.png");
  game.texBall = CreateTextureFromURL("/Assets/ball.png");
  game.texPad1 = CreateTextureFromURL("/Assets/paddle.png");
  game.texPad2 = CreateTextureFromURL("/Assets/paddle.png");

  game.initialized = true;
  logInfo("Game Initialized!");
}

void updateGame(f32 dt) {
	if (Input::IsJustPressed(0, ACTION_PAUSE)) {
    if (game.currentScene == GameState::Scene::Playing) {
      game.currentScene = GameState::Scene::Pause;
    } else if (game.currentScene == GameState::Scene::Pause) {
      game.currentScene = GameState::Scene::Playing;
    }
  }

  if (game.currentScene != GameState::Scene::Playing) {
		return;
	}

	game.animTime += dt;

	f32 input = Input::GetValue(0, ACTION_MOVE_UP) - Input::GetValue(0, ACTION_MOVE_DOWN);
  game.playerY += input * 1.5f * dt;

	f32 rotateInput = Input::GetValue(0, ACTION_ROT_LEFT) - Input::GetValue(0, ACTION_ROT_RIGHT);
  f32 rotationSpeed = 8.0f;
  game.playerRot += rotateInput * rotationSpeed * dt;

	f32 maxRot = 0.5f;
  if (game.playerRot > maxRot) game.playerRot = maxRot;
  if (game.playerRot < -maxRot) game.playerRot = -maxRot;

	f32 cpuSpeed = 1.3f;
  f32 deadZone = 0.05f;

  if (game.cpuY < game.ball.y - deadZone) {
    game.cpuY += cpuSpeed * dt;
  } else if (game.cpuY > game.ball.y + deadZone) {
  	game.cpuY -= cpuSpeed * dt;
  }

  f32 limit = 1.0f - (game.PADDLE_HEIGHT * 0.5f);
  
  if (game.playerY > limit)  game.playerY = limit;
  if (game.playerY < -limit) game.playerY = -limit;

  if (game.cpuY > limit)  game.cpuY = limit;
  if (game.cpuY < -limit) game.cpuY = -limit;

	game.trail[game.trailIdx] = { game.ball.x, game.ball.y };
	game.trailIdx = (game.trailIdx + 1) % 10;

  game.ball.x += game.ball.velX * dt;
  game.ball.y += game.ball.velY * dt;

  if (game.ball.y > (1.0f - game.ball.radius)) {
    game.ball.y = 1.0f - game.ball.radius;
    game.ball.velY *= -1.0f;
  }
  if (game.ball.y < (-1.0f + game.ball.radius)) {
    game.ball.y = -1.0f + game.ball.radius;
    game.ball.velY *= -1.0f;
  }

	game.screenShake = game.screenShake > 0.0f ? game.screenShake - dt : 0.0f;
	bool collision = false;
	bool smash = false;

	if (game.ball.x < -game.PADDLE_X + game.PADDLE_WIDTH && game.ball.x > -game.PADDLE_X - game.PADDLE_WIDTH) {
    if (Abs(game.ball.y - game.playerY) < (game.PADDLE_HEIGHT * 0.5f + game.ball.radius)) {
      
      game.ball.x = -game.PADDLE_X + game.PADDLE_WIDTH + 0.01f;
      collision = true;

      game.ball.velY += game.playerRot * 2.0f;

      bool rotatingCCW = rotateInput > 0.1f;
      bool rotatingCW  = rotateInput < -0.1f;
      
      if (rotatingCCW || rotatingCW) {
         smash = true;
      }
    }
  }

  if (game.ball.x > game.PADDLE_X - game.PADDLE_WIDTH && game.ball.x < game.PADDLE_X + game.PADDLE_WIDTH) {
    if (Abs(game.ball.y - game.cpuY) < (game.PADDLE_HEIGHT * 0.5f + game.ball.radius)) {
      game.ball.x = game.PADDLE_X - game.PADDLE_WIDTH - 0.01f;
      collision = true;
    }
  }

	if (collision) {
    game.ball.velX *= -1.0f;
    //game.ball.velY *= 1.0f;

		if (smash) {
      game.ball.velX *= 1.4f;
			game.screenShake =  0.5f;
      logInfo("SMASH!");
    }

    f32 cap = smash ? GameState::SMASH_VELOCITY : GameState::MAX_VELOCITY;

		if (Abs(game.ball.velX) > cap) {
      game.ball.velX = (game.ball.velX > 0 ? 1.0f : -1.0f) * cap;

    }
    if (Abs(game.ball.velY) > cap) {
      game.ball.velY = (game.ball.velY > 0 ? 1.0f : -1.0f) * cap;
		}

	}

  f32 aspect = (f32)GAME_WIDTH / (f32)GAME_HEIGHT; 
  if (game.ball.x < -aspect - 0.2f || game.ball.x > aspect + 0.2f) {

		f32 dir = 1.0f;
		if (game.ball.x > 0.0f) {
      game.scorePlayer++;
			dir = -1.0f;
    } else {
      game.scoreCpu++;
			dir = 1.0f;
    }

    game.ball.x = 0; 
    game.ball.y = 0;

    game.ball.velX = GameState::INITIAL_VELX * dir; 
    game.ball.velY = GameState::INITIAL_VELY;

		bool cheerPick = (int)game.startTime.elapsed().asSeconds() % 2;
		logInfo("%d", (int)game.startTime.elapsed().asSeconds() % 2);
		if (cheerPick) {
			Audio::Play(game.cheer);
		} else{
			Audio::Play(game.cheer2);
		}

  }
}

void renderLoop(void* arg) {
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

	f32 dt = 0.016f;
  updateGame(dt);

	u32 winW, winH;
  Window::Size(&winW, &winH);
  static u32 lastW = 0, lastH = 0;
  if (winW != lastW || winH != lastH) {
    Renderer::OnResize(winW, winH);
    lastW = winW;
    lastH = winH;
  }

  Renderer::BeginFrame();
	f32 aspect = (f32)winW / (f32)winH;
	f32 shakeX = 0.0f, shakeY = 0.0f;
	if (game.screenShake > 0.0f) {
    shakeX = (sin(game.startTime.elapsed().asSeconds() * 50.0f) * game.screenShake) * 0.1f;
    shakeY = (cos(game.startTime.elapsed().asSeconds() * 43.0f) * game.screenShake) * 0.1f;
	}

	glm::mat4 gameProj = glm::ortho(-aspect + shakeX, aspect + shakeX, -1.0f + shakeY, 1.0f + shakeY, -1.0f, 1.0f);
  
	SpriteRenderer::BeginFrame(&gameProj[0][0]);
	SpriteRenderer::Draw(
    -aspect, -1.0f,    	 
    aspect * 2.0f, 2.0f, 
    game.texBg,          
    (f32[]){1,1,1,1},    
    0.0f, 0.0f, 0.0f,          
    aspect, 1.0f         
  );

  if (game.currentScene == GameState::Scene::Playing || game.currentScene == GameState::Scene::Pause) {


		f32 bounceSpeed = 1.0f; 
    f32 bounce = Abs(sinf(game.animTime * bounceSpeed)); 
    f32 ballShadowScale = 1.0f - (bounce * 0.3f); 
    f32 ballScale   = 1.0f + (bounce * 0.2f);

		f32 shadowOff = 0.03f;
		f32 shadowScale = 0.9f;
    f32 shadowCol[] = {0,0,0,0.5f};

		// Paddle 1 Shadow
		SpriteRenderer::Draw(
      -game.PADDLE_X - game.PADDLE_WIDTH + shadowOff, 
      game.playerY - (game.PADDLE_HEIGHT * 0.5f) - shadowOff, 
      game.PADDLE_WIDTH * 2 * shadowScale, 
			game.PADDLE_HEIGHT * shadowScale, 
      game.texPad1, shadowCol,
			game.playerRot
    );

		// Paddle 2 Shadow 
    SpriteRenderer::Draw(
      game.PADDLE_X - game.PADDLE_WIDTH  + shadowOff, 
			game.cpuY - (game.PADDLE_HEIGHT * 0.5f) - shadowOff, 
      game.PADDLE_WIDTH * 2 * shadowScale, 
			game.PADDLE_HEIGHT * shadowScale, 
      game.texPad2, shadowCol,
			game.cpuRot
    );

		// Ball Shadow
		f32 sRad = game.ball.radius * ballShadowScale;
    SpriteRenderer::Draw(
      (game.ball.x + shadowOff) - sRad, 
      (game.ball.y + shadowOff) - sRad, 
      sRad * 2.0f, sRad * 2.0f, 
      game.texBall, shadowCol
    );

		// Ball trail
		for (int i = 0; i < 10; ++i) {
      int idx = (game.trailIdx + i) % 10;
      f32 scale = (f32)i / 10.0f;
      f32 alpha = scale * 0.3f;
      
      f32 tRad = game.ball.radius * scale;
      f32 trailCol[] = {1.0f, 1.0f, 1.0f, alpha}; 

      SpriteRenderer::Draw(
          game.trail[idx].x - tRad, 
          game.trail[idx].y - tRad, 
          tRad * 2, tRad * 2, 
          game.texBall, trailCol
      );
   	}

		// Paddle 1
    SpriteRenderer::Draw(
      -game.PADDLE_X - game.PADDLE_WIDTH, game.playerY - (game.PADDLE_HEIGHT * 0.5f), 
      game.PADDLE_WIDTH * 2, game.PADDLE_HEIGHT, 
      game.texPad1, (f32[]){0,0,1,1},
			game.playerRot
    );
		
		// Paddle 2
    SpriteRenderer::Draw(
      game.PADDLE_X - game.PADDLE_WIDTH, game.cpuY - (game.PADDLE_HEIGHT * 0.5f), 
      game.PADDLE_WIDTH * 2, game.PADDLE_HEIGHT, 
      game.texPad2, (f32[]){1,0,0,1},
			game.cpuRot
    );

		// Ball
		f32 bRad = game.ball.radius * ballScale;
    SpriteRenderer::Draw(
      game.ball.x - bRad, 
      game.ball.y - bRad, 
      bRad * 2.0f, bRad * 2.0f, 
      game.texBall, (f32[]){1,1,1,1}
    );

		SpriteRenderer::EndFrame();
	
		if (game.font && game.font->loaded) {
      TextRenderer::BeginFrame(winW, winH);

      char scoreBuf[32];
      f32 fontSize = 144.0f;
      f32 textY = 140.0f;
      f32 textAlpha = 0.75f;

      snprintf(scoreBuf, 32, "%d", game.scorePlayer);
      f32 w = TextRenderer::MeasureText(game.font, scoreBuf, fontSize);
      TextRenderer::DrawText(game.font, scoreBuf, (winW * 0.25f) - (w * 0.5f), textY, fontSize, (f32[]){1,1,1,textAlpha});

      snprintf(scoreBuf, 32, "%d", game.scoreCpu);
      w = TextRenderer::MeasureText(game.font, scoreBuf, fontSize);
      TextRenderer::DrawText(game.font, scoreBuf, (winW * 0.75f) - (w * 0.5f), textY, fontSize, (f32[]){1,1,1,textAlpha});

      TextRenderer::EndFrame();
		}
  }

	if (game.currentScene == GameState::Scene::Menu) {
		SpriteRenderer::EndFrame();

	  UI::Begin(dt, winW, winH);
    constexpr f32 btnW = 300.0f;
    constexpr f32 btnH = 60.0f;
    constexpr f32 spaceBetween = 100.0f;
	  f32 centerX = (f32)winW / 2.0f - (btnW / 2.0f);

    f32 totalHeight = spaceBetween * 2 + btnH; 
    f32 yPos = (f32)winH / 2.0f - (totalHeight / 2.0f);

    f32 dim[] = {0,0,0,0.7f};
		ShapeRenderer::DrawRect(0, 0, (f32)winW, (f32)winH, dim);

	  if (UI::Button("Start Game " ICON_JOYSTICK, centerX, yPos, btnW, btnH, &game.startTheme)) {
			game.currentScene = GameState::Scene::Playing;
    }
    yPos += spaceBetween;

    if (UI::Button("Options " ICON_GEAR, centerX, yPos, btnW, btnH, &game.optionsTheme)) {
      logInfo(">> CLICKED: Options");
    }
    yPos += spaceBetween;

    if (UI::Button("Exit " ICON_EXIT, centerX, yPos, btnW, btnH, &game.exitTheme)) {
			JS::Close();
    }

	  UI::End();
	} else if (game.currentScene == GameState::Scene::Pause) {
	  UI::Begin(dt, winW, winH);

    char scoreBuf[32];
    f32 fontSize = 144.0f;
    f32 textY = 140.0f;
    f32 textAlpha = 0.75f;

    snprintf(scoreBuf, 32, "%d", game.scorePlayer);
    f32 w = TextRenderer::MeasureText(game.font, scoreBuf, fontSize);
    TextRenderer::DrawText(game.font, scoreBuf, (winW * 0.25f) - (w * 0.5f), textY, fontSize, (f32[]){1,1,1,textAlpha});

    snprintf(scoreBuf, 32, "%d", game.scoreCpu);
    w = TextRenderer::MeasureText(game.font, scoreBuf, fontSize);
    TextRenderer::DrawText(game.font, scoreBuf, (winW * 0.75f) - (w * 0.5f), textY, fontSize, (f32[]){1,1,1,textAlpha});

    constexpr f32 btnW = 300.0f;
    constexpr f32 btnH = 60.0f;
    constexpr f32 spaceBetween = 100.0f;
	  f32 centerX = (f32)winW / 2.0f - (btnW / 2.0f);

    f32 totalHeight = spaceBetween + btnH; 
    f32 yPos = (f32)winH / 2.0f - (totalHeight / 2.0f);

    f32 dim[] = {0,0,0,0.7f};
		ShapeRenderer::DrawRect(0, 0, (f32)winW, (f32)winH, dim);

    if (UI::Button("Resume " ICON_JOYSTICK, centerX, yPos, btnW, btnH, &game.startTheme)) {
      game.currentScene = GameState::Scene::Playing;
    }
    yPos += spaceBetween;

    if (UI::Button("Options " ICON_GEAR, centerX, yPos, btnW, btnH, &game.optionsTheme)) {
      logInfo(">> CLICKED: Options");
    }
    yPos += spaceBetween;

    if (UI::Button("Exit to Menu " ICON_EXIT, centerX, yPos, btnW, btnH, &game.exitTheme)) {
      game.currentScene = GameState::Scene::Menu;
    }

	  UI::End();
	}

  Renderer::EndFrame();
	Input::EndFrame();
}

int main() {
  Window window = Window::Create(WINDOW_WIDTH, WINDOW_HEIGHT, "Pong").unwrap();

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

  emscripten_set_main_loop_arg(renderLoop, nullptr, 0, true);

  return 0;
}
