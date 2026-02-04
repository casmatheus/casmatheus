#pragma once
#include "core/primitive.h"
#include "os/input.h"
#include "gpu/renderer.h"

struct UIVertex {
  f32 pos[2];   // x, y
  f32 uv[2];    // u, v
  f32 color[4]; // r, g, b, a
};

namespace UIRenderer {
  void Init();
  void Shutdown();
  
  void BeginFrame(u32 width, u32 height);
	void Flush();

	void PushQuad(f32 x, f32 y, f32 w, f32 h, f32 colorTop[4], f32 colorBottom[4], BindGroupID bindGroup = INVALID_ID);
  
  void EndFrame();
}

namespace UI {

	enum class InputMode: u8 {
  	Mouse,
  	Gamepad
	};

	constexpr ActionID ACTION_ACCEPT  = 250; // Enter, MouseLeft, Gamepad A
  constexpr ActionID ACTION_BACK    = 251; // Escape, Gamepad B
  constexpr ActionID ACTION_UP      = 252;
  constexpr ActionID ACTION_DOWN    = 253;
  constexpr ActionID ACTION_LEFT    = 254; 
  constexpr ActionID ACTION_RIGHT   = 255;

	void Init(PlayerID uiPlayerIndex);
	void Begin(f32 deltaTime, u32 windowWidth, u32 windowHeight);
  void End();

  bool Button(const char* label, f32 x, f32 y, f32 w, f32 h);

  struct State {
		PlayerID playerIdx;
		InputMode mode;

    i32 focusIndex;      // Current "selected" widget
    i32 widgetCounter;
		i32 lastFrameWidgetCount;

    f32 mouseX;
    f32 mouseY;
		bool interactPressed;
		bool anyHot;
  };
}
