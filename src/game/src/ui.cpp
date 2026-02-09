#include "debug/log.h"
#include "debug/perf.h"
#include "core/math.h"
#include "core/vector.h"
#include "gpu/renderer.h"
#include "game/ui.h"
#include "game/text.h"
#include "game/font.h"
#include "game/shape.h"
#include <string.h>

#include <glm/gtc/matrix_transform.hpp>

static UITheme s_Theme;
static UI::State s_State;
static Font* s_Font = nullptr;

void UI::Init(PlayerID playerIdx, const UITheme& theme, Font* font) {
  s_State.playerIdx = playerIdx;
	s_State.mode = UI::InputMode::Mouse;
	s_State.focusIndex = 0;
	s_State.lastFrameWidgetCount = 0;

	s_Theme = theme;
	s_Font = font;

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

	s_State.interactReleased = Input::IsJustReleased(s_State.playerIdx, ACTION_ACCEPT);
	s_State.interactDown	= Input::IsDown(s_State.playerIdx, ACTION_ACCEPT);
  
	glm::mat4 uiProj = glm::ortho(0.0f, (f32)width, (f32)height, 0.0f, -1.0f, 1.0f);
  ShapeRenderer::BeginFrame(&uiProj[0][0]);
	TextRenderer::BeginFrame(width, height);
}

void UI::End() {
  ShapeRenderer::EndFrame();
	TextRenderer::EndFrame();
  
	s_State.lastFrameWidgetCount = s_State.widgetCounter;

	if (s_State.interactReleased) {
    Input::ConsumeAll();
  } 
  else if (s_State.anyHot) {
    Input::Consume(DeviceType::Mouse);
  }
}

static bool regionHit(f32 x, f32 y, f32 w, f32 h) {
  return (s_State.mouseX >= x && s_State.mouseX <= x + w && s_State.mouseY >= y && s_State.mouseY <= y + h);
}

bool UI::Button(const char* label, f32 x, f32 y, f32 w, f32 h, const UITheme* overrideTheme) { 
	i32 index = s_State.widgetCounter++; // Claim an ID
  bool hot = false;

	const UITheme& theme = (overrideTheme != nullptr) ? *overrideTheme : s_Theme;

  if (s_State.mode == UI::InputMode::Mouse) {
    if (regionHit(x, y, w, h)) {
      hot = true;
      s_State.focusIndex = index;
    }
  } else {
    if (s_State.focusIndex == index) {
      hot = true;
    }
  }

	s_State.anyHot |= hot;

	bool pressed = (hot && s_State.interactReleased);
  bool held = (hot && s_State.interactDown);

	const UIStyle* style = &theme.colIdle;
	f32 scale = 1.0f;
  if (held) {
		style = &theme.colPress;
		scale = 0.95f;
	} else if (hot) {
		style = &theme.colHover;
		scale = 1.05f;
	} 
	
	w = w * scale;
	h = h * scale;
	x = x + (w/scale - w) * 0.5f;
	y = y + (h/scale - h) * 0.5f;

  if (!held && s_Theme.shadowSize > 0.0f) {
		ShapeRenderer::DrawRect(x, y + theme.shadowSize, w, h, theme.shadowColor, nullptr, theme.cornerRadius);
  }

	f32 yOff = held ? theme.shadowSize * 0.5f : 0.0f;

	ShapeRenderer::DrawRect(x, y + yOff, w, h, style->background, style->border, theme.cornerRadius, theme.borderSize);

	if (label && s_Font && s_Font->loaded) {
    f32 fontSize = 32.0f * scale;
    f32 textW = TextRenderer::MeasureText(s_Font, label, fontSize);
    f32 textX = x + (w - textW) * 0.5f;
    f32 textY = (y + yOff) + (h * 0.5f) + (fontSize * 0.3f);
    
    TextRenderer::DrawText(s_Font, label, textX, textY, fontSize, style->text);
  }

	return pressed;
}
