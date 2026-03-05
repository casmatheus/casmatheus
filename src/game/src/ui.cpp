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

	Input::MapAction(playerIdx, ACTION_LEFT, InputBinding::FromKey(Key::ArrowLeft).bypass());
	Input::MapAction(playerIdx, ACTION_LEFT, InputBinding::FromButton(GamepadButton::DPadLeft).bypass());

	Input::MapAction(playerIdx, ACTION_RIGHT, InputBinding::FromKey(Key::ArrowRight).bypass());
	Input::MapAction(playerIdx, ACTION_RIGHT, InputBinding::FromButton(GamepadButton::DPadRight).bypass());
}

void UI::Begin(f32 deltaTime, u32 winWidth, u32 winHeight, f32 vWidth, f32 vHeight) {
	PERF_SCOPE("[UI] Begin");

  (void)deltaTime;
	s_State.widgetCounter = 0;
	s_State.anyHot = false;

	f32 scaleX = vWidth / (f32)winWidth;
  f32 scaleY = vHeight / (f32)winHeight;

	s_State.mouseX = Input::GetMouseX() * scaleX;
  s_State.mouseY = Input::GetMouseY() * scaleY;

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
  
	glm::mat4 uiProj = glm::ortho(0.0f, vWidth, vHeight, 0.0f, -1.0f, 1.0f);
  ShapeRenderer::BeginFrame(&uiProj[0][0]);
	TextRenderer::BeginFrame(vWidth, vHeight);
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
		f32 fontSize = h * 0.55f;
    f32 textW = TextRenderer::MeasureText(s_Font, label, fontSize);
    f32 textX = x + (w - textW) * 0.5f;
    f32 textY = (y + yOff) + (h * 0.5f) + (fontSize * 0.3f);
    
    TextRenderer::DrawText(s_Font, label, textX, textY, fontSize, style->text);
  }

	return pressed;
}

bool UI::Slider(const char* label, f32* value, f32 min, f32 max, f32 x, f32 y, f32 w, f32 h) {
  i32 index = s_State.widgetCounter++;
  bool hot = false;

	f32 textH   = h * 0.35f;
  f32 sliderH = h * 0.65f;
  f32 knobW 	= h * 0.25f;


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
  bool active = (hot && s_State.interactDown);
  bool changed = false;

  if (active && s_State.mode == UI::InputMode::Mouse) {
		f32 trackStart = x + (knobW * 0.5f);
    f32 trackWidth = w - knobW;

    if (trackWidth < 1.0f) trackWidth = 1.0f;

		f32 mouseT = (s_State.mouseX - trackStart) / trackWidth;
    f32 newVal = min + (mouseT * (max - min));
    
    if (newVal < min) newVal = min;
    if (newVal > max) newVal = max;

    if (*value != newVal) {
      *value = newVal;
      changed = true;
    }
  }

  if (hot && s_State.mode == UI::InputMode::Gamepad) {
    f32 step = (max - min) * 0.01f; // 1% speed
    if (Input::IsDown(s_State.playerIdx, ACTION_LEFT)) {
      *value -= step;
      if (*value < min) *value = min;
      changed = true;
    }
    if (Input::IsDown(s_State.playerIdx, ACTION_RIGHT)) {
      *value += step;
      if (*value > max) *value = max;
      changed = true;
    }
  }

  f32 railY   = y + textH + (sliderH * 0.5f);
  f32 railThickness = sliderH * 0.55f; 
	f32 railRad   = railThickness * 0.5f;
	f32 railBorder= 3.0f;
  
  f32 knobH = sliderH * 0.9f;
  f32 knobY = y + textH + (sliderH - knobH) * 0.5f;

  f32 t = (*value - min) / (max - min);

	f32 knobCenterX = x + (knobW * 0.5f) + (t * (w - knobW));

  if (s_Font && s_Font->loaded) {
    f32 fontSize = textH * 0.85f;
		f32 textBaseY = y + textH - (fontSize * 0.2f);
    
    if (label) {
      TextRenderer::DrawText(s_Font, label, x, textBaseY, fontSize, s_Theme.colIdle.text);
    }

    char valBuf[16];
    int percent = (int)(t * 100.0f);
    snprintf(valBuf, 16, "%d%%", percent);
    
    f32 valW = TextRenderer::MeasureText(s_Font, valBuf, fontSize);
    TextRenderer::DrawText(s_Font, valBuf, x + w - valW, textBaseY, fontSize, active ? s_Theme.colPress.text : s_Theme.colIdle.text);
  }

  f32 railBgCol[] = {0.05f, 0.05f, 0.05f, 1.0f};
	ShapeRenderer::DrawRect(x, railY - (railThickness * 0.5f), w, railThickness, railBgCol, s_Theme.colIdle.border, railRad, railBorder);

	f32 shadowCol[] = {0.0f, 0.0f, 0.0f, 0.3f};
	ShapeRenderer::DrawRect(x + railBorder, railY - (railThickness * 0.5f) + railBorder, w - (railBorder * 2), (railThickness * 0.4f), shadowCol, nullptr, railRad * 0.5f);

	f32 fillW = knobCenterX - x;
	if (fillW < railRad) fillW = railRad;

  if (fillW > 0.0f) {
		const f32* baseCol = s_Theme.colIdle.text;
    f32 innerThick = railThickness - (railBorder * 2.0f);
    ShapeRenderer::DrawRect(x + railBorder, railY - (innerThick * 0.5f), fillW - railBorder, innerThick, baseCol, nullptr, innerThick*0.5f);

		f32 highlight[] = {1.0f, 1.0f, 1.0f, 0.2f};
		ShapeRenderer::DrawRect(x + railBorder, railY - (innerThick * 0.5f), fillW - railBorder, innerThick * 0.4f, highlight, nullptr, innerThick*0.5f);
  }

	f32 knobX = knobCenterX - (knobW * 0.5f);
  
  const UIStyle* style = &s_Theme.colIdle;
  if (active) style = &s_Theme.colPress;
  else if (hot) style = &s_Theme.colHover;

	ShapeRenderer::DrawRect(knobX, knobY, knobW, knobH, style->text, style->border, 4.0f, 2.0f);

  return changed;
}
