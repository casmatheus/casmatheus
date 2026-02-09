#pragma once
#include "core/primitive.h"
#include "os/input.h"
#include "game/font.h"

#define ICON_GEAR "\uE000"
#define ICON_JOYSTICK "\uE001"
#define ICON_EXIT "\uE002"

struct UIStyle {
  f32 background[4];
  f32 border[4];
  f32 text[4];
};

struct UITheme {
	f32 cornerRadius;
  f32 borderSize;

  f32 shadowSize;
  f32 shadowColor[4];

	UIStyle colIdle;
  UIStyle colHover;
  UIStyle colPress;
};

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

	void Init(PlayerID playerIdx, const UITheme& theme, Font* font);
	void Begin(f32 deltaTime, u32 windowWidth, u32 windowHeight);
  void End();

	void DrawRect(f32 x, f32 y, f32 w, f32 h, const f32 color[4], const f32 borderColor[4] = nullptr, f32 radius = 0.0f, f32 border = 0.0f);
	bool Button(const char* label, f32 x, f32 y, f32 w, f32 h, const UITheme* overrideTheme = nullptr);

  struct State {
		PlayerID playerIdx;
		InputMode mode;

    i32 focusIndex;      // Current "selected" widget
    i32 widgetCounter;
		i32 lastFrameWidgetCount;

    f32 mouseX;
    f32 mouseY;
		bool interactReleased;
		bool interactDown;
		bool anyHot;
  };
}
