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

#pragma clang diagnostic pop

constexpr ActionID ACTION_PROFILE   = 100;
constexpr ActionID ACTION_MOVE_UP   = 10;
constexpr ActionID ACTION_MOVE_DOWN = 11;
constexpr ActionID ACTION_ROT_RIGHT = 12;
constexpr ActionID ACTION_ROT_LEFT  = 13;
constexpr ActionID ACTION_PAUSE     = 14;

const f32 TARGET_WIDTH = 1920.0f;
const f32 TARGET_HEIGHT = 1080.0f;

struct GameState {
  bool initialized = false;
	bool active = false;

	enum class GameMode { Single, Multi, OnlineHost, OnlineClient };
  GameMode mode = GameMode::Single;

	i32 p1InputSrc = -1;
  i32 p2InputSrc = -1;

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

	enum class Scene: u8 { Menu, Playing, Pause, Settings, ModeSelect };
	Scene currentScene = Scene::Menu;
	Scene lastScene = Scene::Menu;

  f32 playerY   	 = 0.0f;
	f32 playerVelY 	 = 0.0f;
  f32 playerRotVel = 0.0f;
	f32 playerRot 	 = 0.0f;

  f32 cpuY      = 0.0f;
	f32 cpuVelY 	= 0.0f;
  f32 cpuRotVel = 0.0f;
  f32 cpuRot    = 0.0f;

	int scorePlayer = 0;
  int scoreCpu = 0;

	f32 screenShake = 0.0f;
	f32 animTime = 0.0f;

	f32 bgScrollX    = 0.0f;
	f32 bgScrollY    = 0.0f;
  f32 bgScrollVelX = 0.0f;
  f32 bgScrollVelY = 0.0f;

	SoundID cheer;
	SoundID cheer2;
	SoundID impact;
  
  struct Ball {
    f32 x, y;
    f32 velX, velY;
		f32 radius;
  } ball;

	struct { f32 x, y; } trail[10];
  int trailIdx = 0;

	const f32 PADDLE_HEIGHT = 0.4f; // 20% of screen height
  const f32 PADDLE_WIDTH  = 0.05f;
  f32 paddleX      = 1.75f; // Distance from center
	static constexpr f32 INITIAL_VELX = 0.8f;
	static constexpr f32 INITIAL_VELY = 0.5f;
	static constexpr f32 MAX_VELOCITY  = 3.5f;
	static constexpr f32 SMASH_VELOCITY = 4.5f;
};

#pragma pack(push, 1)
struct HostStatePacket {
  u8 type = 0;
  f32 ballX, ballY, ballVelX, ballVelY;
  f32 hostY, hostRot;
  i32 scoreP1, scoreP2;
};

struct ClientInputPacket {
  u8 type = 1;
  f32 clientY, clientRotVel, clientRot;
};
#pragma pack(pop)

GameState game;

extern "C" {
  EMSCRIPTEN_KEEPALIVE
  void ForceExit() {
    logInfo("Page unloading, forcing cleanup...");
		emscripten_cancel_main_loop();
    Renderer::Destroy();
  }

	EMSCRIPTEN_KEEPALIVE
  void OnNetworkPacket(u8* data, int size) {
    if (size == 0) return;
    u8 packetType = data[0];

    if (packetType == 0 && size == sizeof(HostStatePacket)) {
      HostStatePacket* packet = (HostStatePacket*)data;
      if (game.mode == GameState::GameMode::OnlineClient) {
        game.ball.x = packet->ballX;
        game.ball.y = packet->ballY;
        game.ball.velX = packet->ballVelX;
        game.ball.velY = packet->ballVelY;
        game.playerY = packet->hostY;
        game.playerRot = packet->hostRot;
        game.scorePlayer = packet->scoreP1;
        game.scoreCpu = packet->scoreP2;
      }
    } 
    else if (packetType == 1 && size == sizeof(ClientInputPacket)) {
      ClientInputPacket* packet = (ClientInputPacket*)data;
      if (game.mode == GameState::GameMode::OnlineHost) {
        game.cpuY = packet->clientY;
        game.cpuRotVel = packet->clientRotVel;
        game.cpuRot = packet->clientRot;
      }
    }
  }

	EM_JS(void, SendNetworkDataJS, (void* data, int size), {
  	if (typeof Network !== 'undefined') {
    	Network.sendPacket(data, size);
  	}
	});
}

TextureID CreateSolidTexture(u8 r, u8 g, u8 b, u8 a) {
  u8 data[4] = {r, g, b, a};
  return Renderer::CreateTexture(data, 1, 1, 4);
}

void resetGame() {
  game.scorePlayer = 0;
  game.scoreCpu = 0;

  game.playerY   	 = 0.0f;
	game.playerVelY 	 = 0.0f;
  game.playerRotVel = 0.0f;
	game.playerRot 	 = 0.0f;

  game.cpuY      = 0.0f;
	game.cpuVelY 	= 0.0f;
  game.cpuRotVel = 0.0f;
  game.cpuRot    = 0.0f;

  game.ball.x = 0.0f;
  game.ball.y = 0.0f;
  game.ball.velX = GameState::INITIAL_VELX;
  game.ball.velY = GameState::INITIAL_VELY;

  game.screenShake = 0.0f;
  game.bgScrollX = 0.0f;
  game.bgScrollY = 0.0f;
  game.bgScrollVelX = 0.0f;
  game.bgScrollVelY = 0.0f;

  game.trailIdx = 0;
  for (int i = 0; i < 10; ++i) {
    game.trail[i] = { 0.0f, 0.0f };
  }

	game.active = false;
}

void initGame() {
  logInfo("Initializing Game Resources...");

	Audio::Init();
	Input::Init();
	ShapeRenderer::Init();
	SpriteRenderer::Init();
	TextRenderer::Init();

	game.font = Font::Load("Custom");

	// Player 1 (Left Paddle) - WASD / Left Stick
	Input::MapAction(0, ACTION_MOVE_UP,   InputBinding::FromKey(Key::W));
  Input::MapAction(0, ACTION_MOVE_UP,   InputBinding::FromButton(GamepadButton::DPadUp));
  Input::MapAction(0, ACTION_MOVE_UP,   InputBinding::FromAxis(GamepadAxis::LeftY, true));

  Input::MapAction(0, ACTION_MOVE_DOWN, InputBinding::FromKey(Key::S));
  Input::MapAction(0, ACTION_MOVE_DOWN, InputBinding::FromButton(GamepadButton::DPadDown));
  Input::MapAction(0, ACTION_MOVE_DOWN, InputBinding::FromAxis(GamepadAxis::LeftY, false));

  Input::MapAction(0, ACTION_ROT_LEFT,  InputBinding::FromKey(Key::A));
  Input::MapAction(0, ACTION_ROT_LEFT,  InputBinding::FromButton(GamepadButton::FaceLeft));
  
  Input::MapAction(0, ACTION_ROT_RIGHT, InputBinding::FromKey(Key::D));
  Input::MapAction(0, ACTION_ROT_RIGHT, InputBinding::FromButton(GamepadButton::FaceRight));

  // Player 2 (Right Paddle) - Arrows / Left Stick (on second controller)
  Input::MapAction(1, ACTION_MOVE_UP,   InputBinding::FromKey(Key::ArrowUp));
  Input::MapAction(1, ACTION_MOVE_UP,   InputBinding::FromButton(GamepadButton::DPadUp));
  Input::MapAction(1, ACTION_MOVE_UP,   InputBinding::FromAxis(GamepadAxis::LeftY, true));

  Input::MapAction(1, ACTION_MOVE_DOWN, InputBinding::FromKey(Key::ArrowDown));
  Input::MapAction(1, ACTION_MOVE_DOWN, InputBinding::FromButton(GamepadButton::DPadDown));
  Input::MapAction(1, ACTION_MOVE_DOWN, InputBinding::FromAxis(GamepadAxis::LeftY, false));

  Input::MapAction(1, ACTION_ROT_LEFT,  InputBinding::FromKey(Key::ArrowLeft));
  Input::MapAction(1, ACTION_ROT_LEFT,  InputBinding::FromButton(GamepadButton::FaceLeft));

  Input::MapAction(1, ACTION_ROT_RIGHT, InputBinding::FromKey(Key::ArrowRight));
  Input::MapAction(1, ACTION_ROT_RIGHT, InputBinding::FromButton(GamepadButton::FaceRight));

  // Global Actions 
  Input::MapAction(0, ACTION_PAUSE, InputBinding::FromKey(Key::Escape).bypass());
  Input::MapAction(0, ACTION_PAUSE, InputBinding::FromButton(GamepadButton::Start).bypass());

	auto SetColor = [](f32* dest, f32 r, f32 g, f32 b, f32 a) {
    dest[0] = r; dest[1] = g; dest[2] = b; dest[3] = a;
  };

	UITheme theme = {};
  theme.cornerRadius = 16.0f;
  theme.borderSize   = 5.0f;
  theme.shadowSize   = 6.0f;
  SetColor(theme.shadowColor, 0.0f, 0.0f, 0.0f, 0.5f);
  
  SetColor(theme.colIdle.text,  0.9f, 0.9f, 0.9f, 1.0f);
  SetColor(theme.colHover.text, 1.0f, 1.0f, 1.0f, 1.0f);
  SetColor(theme.colPress.text, 0.8f, 0.8f, 0.8f, 1.0f);

	SetColor(theme.colIdle.background, 0.2f, 0.2f, 0.3f, 1.0f);
  SetColor(theme.colIdle.border,     0.5f, 0.5f, 0.6f, 1.0f);

  SetColor(theme.colHover.background, 0.3f, 0.3f, 0.4f, 1.0f);
  SetColor(theme.colHover.border,     0.7f, 0.7f, 0.8f, 1.0f);

  SetColor(theme.colPress.background, 0.4f, 0.4f, 0.5f, 1.0f);
  SetColor(theme.colPress.border,     0.9f, 0.9f, 1.0f, 1.0f);

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
	game.impact = Audio::LoadFromURL("/Assets/Audio/impact.wav");
  game.texBg   = CreateTextureFromURL("/Assets/checkers.png");
  game.texBall = CreateTextureFromURL("/Assets/ball.png");
  game.texPad1 = CreateTextureFromURL("/Assets/paddle.png");
  game.texPad2 = CreateTextureFromURL("/Assets/paddle.png");

  game.initialized = true;
  logInfo("Game Initialized!");
}

void updateGame(f32 dt, f32 aspect) {
  if (Input::IsJustPressed(0, ACTION_PAUSE)) {
    if (game.currentScene == GameState::Scene::Playing) {
      game.currentScene = GameState::Scene::Pause;
    } else if (game.currentScene == GameState::Scene::Pause) {
      game.currentScene = GameState::Scene::Playing;
    }
  }

  if (game.currentScene != GameState::Scene::Playing) return;

  game.animTime += dt;

  constexpr f32 moveAcceleration = 15.0f;
  constexpr f32 moveFriction = 10.0f;    	
  constexpr f32 rotAcceleration = 80.0f;
  constexpr f32 rotFriction = 10.0f;    	
  constexpr f32 maxRot = 0.75f;

  // Player 1 (Left Paddle) - Handled by Host or Local
  if (game.mode != GameState::GameMode::OnlineClient) {
    f32 input = Input::GetValue(0, ACTION_MOVE_UP) - Input::GetValue(0, ACTION_MOVE_DOWN);
    game.playerVelY += input * moveAcceleration * dt;
    game.playerVelY -= game.playerVelY * moveFriction * dt;
    game.playerY += game.playerVelY * dt;

    f32 rotateInput = Input::GetValue(0, ACTION_ROT_LEFT) - Input::GetValue(0, ACTION_ROT_RIGHT);
    game.playerRotVel += rotateInput * rotAcceleration * dt;
    game.playerRotVel -= game.playerRotVel * rotFriction * dt;
    game.playerRot += game.playerRotVel * dt;

    if (game.playerRot > maxRot)  { game.playerRot = maxRot; game.playerRotVel = 0.0f; }
    if (game.playerRot < -maxRot) { game.playerRot = -maxRot; game.playerRotVel = 0.0f; }
  }

  // Player 2 / CPU (Right Paddle)
  if (game.mode == GameState::GameMode::Single) {
    // CPU AI Logic
    f32 cpuSpeed = 1.3f;
    f32 deadZone = 0.05f;
    if (game.cpuY < game.ball.y - deadZone) game.cpuY += cpuSpeed * dt;
    else if (game.cpuY > game.ball.y + deadZone) game.cpuY -= cpuSpeed * dt;
    game.cpuRot = 0.0f;
    game.cpuRotVel = 0.0f;
    
  } else if (game.mode == GameState::GameMode::Multi || game.mode == GameState::GameMode::OnlineClient) {
    // Local P2 OR Online Client Input
    // The Client reads P1 input mappings, but applies them to the Right Paddle, cpuY
    int inputIdx = (game.mode == GameState::GameMode::OnlineClient) ? 0 : 1; 

    f32 inputP2 = Input::GetValue(inputIdx, ACTION_MOVE_UP) - Input::GetValue(inputIdx, ACTION_MOVE_DOWN);
    game.cpuVelY += inputP2 * moveAcceleration * dt;
    game.cpuVelY -= game.cpuVelY * moveFriction * dt;
    game.cpuY += game.cpuVelY * dt;

    f32 rotP2 = Input::GetValue(inputIdx, ACTION_ROT_LEFT) - Input::GetValue(inputIdx, ACTION_ROT_RIGHT);
    game.cpuRotVel += rotP2 * rotAcceleration * dt;
    game.cpuRotVel -= game.cpuRotVel * rotFriction * dt;
    game.cpuRot += game.cpuRotVel * dt;

    if (game.cpuRot > maxRot)  { game.cpuRot = maxRot; game.cpuRotVel = 0.0f; }
    if (game.cpuRot < -maxRot) { game.cpuRot = -maxRot; game.cpuRotVel = 0.0f; }
    
    if (game.mode == GameState::GameMode::OnlineClient) {
      ClientInputPacket packet;
      packet.clientY = game.cpuY;
      packet.clientRotVel = game.cpuRotVel;
      packet.clientRot = game.cpuRot;
      SendNetworkDataJS(&packet, sizeof(packet));
    }
  }

  game.paddleX = aspect - 0.2f;

  f32 limit = 1.0f - (game.PADDLE_HEIGHT * 0.5f);
  if (game.cpuY > limit) { game.cpuY = limit; game.cpuVelY = 0.0f; }
  if (game.cpuY < -limit) { game.cpuY = -limit; game.cpuVelY = 0.0f; }
  if (game.playerY > limit) { game.playerY = limit; game.playerVelY = 0.0f; }
  if (game.playerY < -limit) { game.playerY = -limit; game.playerVelY = 0.0f; }

  game.trail[game.trailIdx] = { game.ball.x, game.ball.y };
  game.trailIdx = (game.trailIdx + 1) % 10;
  
  game.screenShake = game.screenShake > 0.0f ? game.screenShake - dt : 0.0f;

  if (game.mode != GameState::GameMode::OnlineClient) {
    game.ball.x += game.ball.velX * aspect * dt;
    game.ball.y += game.ball.velY * dt;

    if (game.ball.y > (1.0f - game.ball.radius)) {
      game.ball.y = 1.0f - game.ball.radius;
      game.ball.velY *= -1.0f;
      Audio::Play(game.impact);
    }
    if (game.ball.y < (-1.0f + game.ball.radius)) {
      game.ball.y = -1.0f + game.ball.radius;
      game.ball.velY *= -1.0f;
      Audio::Play(game.impact);
    }

    bool collision = false;
    bool smash = false;
    
    if (game.ball.x < -game.paddleX + game.PADDLE_WIDTH && game.ball.x > -game.paddleX - game.PADDLE_WIDTH) {
      if (Abs(game.ball.y - game.playerY) < (game.PADDLE_HEIGHT * 0.5f + game.ball.radius)) {
        game.ball.x = -game.paddleX + game.PADDLE_WIDTH + 0.01f;
        collision = true;
        game.ball.velY += game.playerRot * 2.0f;
        smash = (game.playerRotVel > 0.2f) || (game.playerRotVel < -0.2f);
      }
    }

    // P2 Paddle Collision
    if (game.ball.x > game.paddleX - game.PADDLE_WIDTH && game.ball.x < game.paddleX + game.PADDLE_WIDTH) {
      if (Abs(game.ball.y - game.cpuY) < (game.PADDLE_HEIGHT * 0.5f + game.ball.radius)) {
        game.ball.x = game.paddleX - game.PADDLE_WIDTH - 0.01f;
        collision = true;
        game.ball.velY += game.cpuRot * 2.0f;
        smash = (game.cpuRotVel > 0.2f) || (game.cpuRotVel < -0.2f);
      }
    }

    // Paddle Collision
    if (collision) {
      game.ball.velX *= -1.0f;
      if (smash) {
        game.ball.velX *= 1.4f;
        game.screenShake = 0.5f;
        f32 speed = sqrt(game.ball.velX * game.ball.velX + game.ball.velY * game.ball.velY);
        if (speed > 0.001f) {
          game.bgScrollVelX = -(game.ball.velX / speed) * 1.1f;
          game.bgScrollVelY = -(game.ball.velY / speed) * 1.1f;
        }
      }

      f32 cap = smash ? GameState::SMASH_VELOCITY : GameState::MAX_VELOCITY;
      if (Abs(game.ball.velX) > cap) game.ball.velX = (game.ball.velX > 0 ? 1.0f : -1.0f) * cap;
      if (Abs(game.ball.velY) > cap) game.ball.velY = (game.ball.velY > 0 ? 1.0f : -1.0f) * cap;

      Audio::Play(game.impact);
    }
    
    // Point
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
      Audio::Play(cheerPick ? game.cheer : game.cheer2);
    }

    if (game.mode == GameState::GameMode::OnlineHost) {
      HostStatePacket packet;
      packet.ballX = game.ball.x;
      packet.ballY = game.ball.y;
      packet.ballVelX = game.ball.velX;
      packet.ballVelY = game.ball.velY;
      packet.hostY = game.playerY;
      packet.hostRot = game.playerRot;
      packet.scoreP1 = game.scorePlayer;
      packet.scoreP2 = game.scoreCpu;
      SendNetworkDataJS(&packet, sizeof(packet));
    }
  }

  game.bgScrollX += game.bgScrollVelX * dt;
  game.bgScrollY += game.bgScrollVelY * dt;
  game.bgScrollVelX *= 0.95f;
  game.bgScrollVelY *= 0.95f;
  if (Abs(game.bgScrollVelX) < 0.001f) game.bgScrollVelX = 0.0f;
  if (Abs(game.bgScrollVelY) < 0.001f) game.bgScrollVelY = 0.0f;
}

void renderLoop(void* arg) {
	PERF_SCOPE("[Main] Render Loop");
  (void)arg;

	if (Input::IsJustPressed(0, ACTION_PROFILE)) {
    PERF_REPORT();
	}

  if (!Renderer::Initialized()) return;

  if (!game.initialized) {
    initGame();
  }

	Input::Poll();

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
  f32 aspect = (f32)winW / (f32)winH;


	static Instant lastTime = Instant::Now();

  Instant now = Instant::Now();
  Duration elapsed = now - lastTime;
  lastTime = now;

  f32 dt = elapsed.asSeconds();

  if (dt > 0.05f) {
    dt = 0.05f;
  }

  updateGame(dt, aspect);

  Renderer::BeginFrame();

	f32 shakeX = 0.0f, shakeY = 0.0f;
	if (game.screenShake > 0.0f) {
    shakeX = (sin(game.startTime.elapsed().asSeconds() * 50.0f) * game.screenShake) * 0.1f;
    shakeY = (cos(game.startTime.elapsed().asSeconds() * 43.0f) * game.screenShake) * 0.1f;
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

	glm::mat4 gameProj = glm::ortho(
    -aspect + shakeX, aspect + shakeX,
    -1.0f + shakeY, 1.0f + shakeY,
    -1.0f, 1.0f
  );
  
	SpriteRenderer::BeginFrame(&gameProj[0][0]);

  f32 u0 = (-aspect * 0.5f) + game.bgScrollX; 
  f32 u1 = ( aspect * 0.5f) + game.bgScrollX;

  f32 v0 = 0.0f + game.bgScrollY;
  f32 v1 = 1.0f + game.bgScrollY;

	SpriteRenderer::Draw(
    -aspect, -1.0f,    	 
    aspect * 2.0f, 2.0f, 
    game.texBg,          
    (f32[]){1,1,1,1},    
    0.0f, 
		u0, v0,          
    u1, v1 
  );


	constexpr f32 scoreFontSize = 150.0f;
	constexpr f32 scoreY = 140.0f;
  constexpr f32 scoreAlpha = 0.75f;

  if (game.currentScene == GameState::Scene::Playing 
	 || game.currentScene == GameState::Scene::Pause 
	 || (game.currentScene == GameState::Scene::Settings && game.lastScene == GameState::Scene::Pause)) {


		f32 bounceSpeed = 1.0f; 
    f32 bounce = Abs(sinf(game.animTime * bounceSpeed)); 
    f32 ballShadowScale = 1.0f - (bounce * 0.3f); 
    f32 ballScale   = 1.0f + (bounce * 0.2f);

		f32 shadowOff = 0.03f;
		f32 shadowScale = 0.9f;
    f32 shadowCol[] = {0,0,0,0.5f};

		// Paddle 1 Shadow
		SpriteRenderer::Draw(
      -game.paddleX - game.PADDLE_WIDTH + shadowOff, 
      game.playerY - (game.PADDLE_HEIGHT * 0.5f) - shadowOff, 
      game.PADDLE_WIDTH * 2 * shadowScale, 
			game.PADDLE_HEIGHT * shadowScale, 
      game.texPad1, shadowCol,
			game.playerRot
    );

		// Paddle 2 Shadow 
    SpriteRenderer::Draw(
      game.paddleX - game.PADDLE_WIDTH  + shadowOff, 
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
      -game.paddleX - game.PADDLE_WIDTH, game.playerY - (game.PADDLE_HEIGHT * 0.5f), 
      game.PADDLE_WIDTH * 2, game.PADDLE_HEIGHT, 
      game.texPad1, (f32[]){0,0,1,1},
			game.playerRot
    );
		
		// Paddle 2
    SpriteRenderer::Draw(
      game.paddleX - game.PADDLE_WIDTH, game.cpuY - (game.PADDLE_HEIGHT * 0.5f), 
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
	
    TextRenderer::BeginFrame(virtualW, virtualH);

    char scoreBuf[32];
    snprintf(scoreBuf, 32, "%d", game.scorePlayer);
    f32 w = TextRenderer::MeasureText(game.font, scoreBuf, scoreFontSize);
    TextRenderer::DrawText(game.font, scoreBuf, (virtualW * 0.25f) - (w * 0.5f), scoreY, scoreFontSize, (f32[]){1,1,1,scoreAlpha});

    snprintf(scoreBuf, 32, "%d", game.scoreCpu);
    w = TextRenderer::MeasureText(game.font, scoreBuf, scoreFontSize);
    TextRenderer::DrawText(game.font, scoreBuf, (virtualW * 0.75f) - (w * 0.5f), scoreY, scoreFontSize, (f32[]){1,1,1,scoreAlpha});

    TextRenderer::EndFrame();
  }


	if (game.currentScene == GameState::Scene::Menu) {
		SpriteRenderer::EndFrame();

		UI::Begin(dt, winW, winH, virtualW, virtualH);
    f32 btnW = 400.0f;
    f32 btnH = 80.0f;
    f32 spaceBetween = 150.0f;
	  f32 centerX = (f32)virtualW / 2.0f - (btnW / 2.0f);

    f32 totalHeight = spaceBetween * 2 + btnH; 
    f32 yPos = (f32)virtualH / 2.0f - (totalHeight / 2.0f);

    f32 dim[] = {0,0,0,0.7f};
		ShapeRenderer::DrawRect(0, 0, (f32)virtualW, (f32)virtualH, dim);

	  if (UI::Button("Start Game " ICON_JOYSTICK, centerX, yPos, btnW, btnH, &game.startTheme)) {
			resetGame();
			game.currentScene = GameState::Scene::ModeSelect;
			game.lastScene = GameState::Scene::Menu;
			game.active = true;
    }
    yPos += spaceBetween;

    if (UI::Button("Options " ICON_GEAR, centerX, yPos, btnW, btnH, &game.optionsTheme)) {
      game.currentScene = GameState::Scene::Settings;
			game.lastScene = GameState::Scene::Menu;
    }
    yPos += spaceBetween;

    if (UI::Button("Exit " ICON_EXIT, centerX, yPos, btnW, btnH, &game.exitTheme)) {
			game.lastScene = GameState::Scene::Menu;
			JS::Close();
    }

	  UI::End();
	} else if (game.currentScene == GameState::Scene::Pause) {
		UI::Begin(dt, winW, winH, virtualW, virtualH);

    char scoreBuf[32];
    snprintf(scoreBuf, 32, "%d", game.scorePlayer);
    f32 w = TextRenderer::MeasureText(game.font, scoreBuf, scoreFontSize);
    TextRenderer::DrawText(game.font, scoreBuf, (virtualW * 0.25f) - (w * 0.5f), scoreY, scoreFontSize, (f32[]){1,1,1,scoreAlpha});

    snprintf(scoreBuf, 32, "%d", game.scoreCpu);
    w = TextRenderer::MeasureText(game.font, scoreBuf, scoreFontSize);
    TextRenderer::DrawText(game.font, scoreBuf, (virtualW * 0.75f) - (w * 0.5f), scoreY, scoreFontSize, (f32[]){1,1,1,scoreAlpha});

    f32 btnW = 400.0f;
    f32 btnH = 80.0f;
    f32 spaceBetween = 150.0f;
	  f32 centerX = (f32)virtualW / 2.0f - (btnW / 2.0f);

    f32 totalHeight = spaceBetween * 2 + btnH; 
    f32 yPos = (f32)virtualH / 2.0f - (totalHeight / 2.0f);

    f32 dim[] = {0,0,0,0.7f};
		ShapeRenderer::DrawRect(0, 0, (f32)virtualW, (f32)virtualH, dim);

    if (UI::Button("Resume " ICON_JOYSTICK, centerX, yPos, btnW, btnH, &game.startTheme)) {
      game.currentScene = GameState::Scene::Playing;
			game.lastScene = GameState::Scene::Pause;
    }
    yPos += spaceBetween;

    if (UI::Button("Options " ICON_GEAR, centerX, yPos, btnW, btnH, &game.optionsTheme)) {
      game.currentScene = GameState::Scene::Settings;
			game.lastScene = GameState::Scene::Pause;
    }
    yPos += spaceBetween;

    if (UI::Button("Exit to Menu " ICON_EXIT, centerX, yPos, btnW, btnH, &game.exitTheme)) {
			resetGame();
      game.currentScene = GameState::Scene::Menu;
			game.lastScene = GameState::Scene::Pause;
    }

	  UI::End();
	} else if (game.currentScene == GameState::Scene::Settings) {

		if (game.active) {
      char scoreBuf[32];
      snprintf(scoreBuf, 32, "%d", game.scorePlayer);
      f32 w = TextRenderer::MeasureText(game.font, scoreBuf, scoreFontSize);
      TextRenderer::DrawText(game.font, scoreBuf, (virtualW * 0.25f) - (w * 0.5f), scoreY, scoreFontSize, (f32[]){1,1,1,scoreAlpha});

      snprintf(scoreBuf, 32, "%d", game.scoreCpu);
      w = TextRenderer::MeasureText(game.font, scoreBuf, scoreFontSize);
      TextRenderer::DrawText(game.font, scoreBuf, (virtualW * 0.75f) - (w * 0.5f), scoreY, scoreFontSize, (f32[]){1,1,1,scoreAlpha});
		} else {
			SpriteRenderer::EndFrame();
		}

		UI::Begin(dt, winW, winH, virtualW, virtualH);

    f32 btnW = 400.0f;
    f32 btnH = 80.0f;
    f32 spaceBetween = 150.0f;
	  f32 centerX = (f32)virtualW / 2.0f - (btnW / 2.0f);


		f32 totalHeight = 0.0f;
		if (game.mode == GameState::GameMode::Multi) {
    	totalHeight = spaceBetween * 3 + btnH; 
		} else {
    	totalHeight = spaceBetween + btnH; 
		}
    f32 yPos = (f32)virtualH / 2.0f - (totalHeight / 2.0f);

    f32 dim[] = {0,0,0,0.7f};
		ShapeRenderer::DrawRect(0, 0, (f32)virtualW, (f32)virtualH, dim);

		static f32 masterVolume = 0.5f;

		if (UI::Slider("Volume", &masterVolume, 0.0f, 1.0f, centerX, yPos, btnW, btnH)) {
		  Audio::SetMasterVolume(masterVolume);
		}
		yPos += spaceBetween;

		if (game.mode == GameState::GameMode::Multi) {
      auto CycleInput = [](int current, int playerIdx) -> int {
				int next = current;

				for (int i = 0; i < 5; i++) {
					next++;
        
        	if (next > 3) next = -1;
        
          if (next == -1) { 
			    	Input::SetPlayerKeyboard(playerIdx);
						return next;
					}

					if (Input::IsGamepadConnected(next)) {
            Input::SetPlayerGamepad(playerIdx, next);
            return next;
          }
        }
          
        return current;
      };

      // P1 Selector
      char p1Label[32];
      const char* p1SrcName = (game.p1InputSrc == -1) ? "Keyboard" : 
                              (game.p1InputSrc == 0) ? "Gamepad 1" : "Gamepad ?";
      if (game.p1InputSrc > 0) snprintf(p1Label, 32, "P1: Gamepad %d", game.p1InputSrc + 1);
      else snprintf(p1Label, 32, "P1: %s", p1SrcName);

      if (UI::Button(p1Label, centerX, yPos, btnW, btnH)) {
          game.p1InputSrc = CycleInput(game.p1InputSrc, 0);
      }

      yPos += spaceBetween;

      // P2 Selector
      char p2Label[32];
      const char* p2SrcName = (game.p2InputSrc == -1) ? "Keyboard" : 
                              (game.p2InputSrc == 0) ? "Gamepad 1" : "Gamepad ?";
      if (game.p2InputSrc > 0) snprintf(p2Label, 32, "P2: Gamepad %d", game.p2InputSrc + 1);
      else snprintf(p2Label, 32, "P2: %s", p2SrcName);

      if (UI::Button(p2Label, centerX, yPos, btnW, btnH)) {
          game.p2InputSrc = CycleInput(game.p2InputSrc, 1);
      }

      yPos += spaceBetween;
    }

    if (UI::Button("Back " ICON_EXIT, centerX, yPos, btnW, btnH, &game.exitTheme)) {
      game.currentScene = game.lastScene;
			game.lastScene = GameState::Scene::Settings;
    }
	  UI::End();
	}  else if (game.currentScene == GameState::Scene::ModeSelect) {
		SpriteRenderer::EndFrame();
		UI::Begin(dt, winW, winH, virtualW, virtualH);

    f32 btnW = 400.0f;
    f32 btnH = 80.0f;
    f32 spaceBetween = 150.0f;
	  f32 centerX = (f32)virtualW / 2.0f - (btnW / 2.0f);

    f32 totalHeight = spaceBetween * 2 + btnH; 
    f32 yPos = (f32)virtualH / 2.0f - (totalHeight / 2.0f);

    f32 dim[] = {0,0,0,0.7f};
		ShapeRenderer::DrawRect(0, 0, (f32)virtualW, (f32)virtualH, dim);

    if (UI::Button("Singleplayer " ICON_JOYSTICK, centerX, yPos, btnW, btnH, &game.startTheme)) {
			resetGame();
			game.mode = GameState::GameMode::Single;
      game.currentScene = GameState::Scene::Playing;
			game.lastScene = GameState::Scene::ModeSelect;
			game.active = true;
    }

		yPos += spaceBetween;

    if (UI::Button("Multiplayer " ICON_USER2, centerX, yPos, btnW, btnH, &game.startTheme)) {
			resetGame();
			game.mode = GameState::GameMode::Multi;
      game.currentScene = GameState::Scene::Playing;
			game.lastScene = GameState::Scene::ModeSelect;
			game.active = true;
    }

		yPos += spaceBetween;

    if (UI::Button("Back " ICON_EXIT, centerX, yPos, btnW, btnH, &game.exitTheme)) {
      game.currentScene = game.lastScene;
			game.lastScene = GameState::Scene::ModeSelect;
    }


	  UI::End();
	}

  Renderer::EndFrame();
	Input::EndFrame();
}

int main() {
  Window window = Window::Create("Pong").unwrap();

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
