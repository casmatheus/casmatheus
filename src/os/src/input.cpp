#include "debug/log.h"
#include "core/math.h"
#include "core/vector.h"
#include "os/input.h"
#include <emscripten/html5.h>
#include <emscripten.h>

constexpr u32 MAX_ACTIONS = 256;

bool Input::s_consumeKeyboard = false;
bool Input::s_consumeMouse    = false;
bool Input::s_consumeGamepadButtons = false;
bool Input::s_consumeGamepadAxis    = false;

struct ActionState {
  bool down;
  bool pressed;  // Just Pressed
  bool released; // Just Released
  f32  value;    // -1.0 to 1.0
};

struct PlayerInput {
	bool active = false;
  i8 assignedGamepad = -1; // -1 = Keyboard/Mouse, 0-3 = Gamepad
	bool wantsGamepad = false;
  
	ActionState actions[MAX_ACTIONS];

  // Bindings, Allows multiple bindings per Action
  struct BindingEntry {
    ActionID action;
    InputBinding bind;
  };
  Vec<BindingEntry> bindings;
};

struct RawGamepadState {
  bool connected;
  bool buttons[(usize)GamepadButton::Count];
  f32  axes[(usize)GamepadAxis::Count];
};

struct RawInputState {
  // Keyboard
  bool keys[(usize)Key::Count];

  // Mouse
  bool mouseButtons[(usize)MouseButton::Count];
  f32 mouseX;
	f32 deltaX; 
  f32 mouseY;
  f32 deltaY;
  f32 wheel;

	RawGamepadState gamepads[Input::MAX_GAMEPADS];

};

static PlayerInput   g_Players[Input::MAX_PLAYERS];
static RawInputState g_Raw;

/*
// Maps DOM strings to our Key enum
static Key MapEmscriptenKey(const char* code) {
  // Letters
  if (strcmp(code, "KeyA") == 0) return Key::A;
  if (strcmp(code, "KeyB") == 0) return Key::B;
  if (strcmp(code, "KeyC") == 0) return Key::C;
  if (strcmp(code, "KeyD") == 0) return Key::D;
  if (strcmp(code, "KeyE") == 0) return Key::E;
  if (strcmp(code, "KeyF") == 0) return Key::F;
  if (strcmp(code, "KeyG") == 0) return Key::G;
  if (strcmp(code, "KeyH") == 0) return Key::H;
  if (strcmp(code, "KeyI") == 0) return Key::I;
  if (strcmp(code, "KeyJ") == 0) return Key::J;
  if (strcmp(code, "KeyK") == 0) return Key::K;
  if (strcmp(code, "KeyL") == 0) return Key::L;
  if (strcmp(code, "KeyM") == 0) return Key::M;
  if (strcmp(code, "KeyN") == 0) return Key::N;
  if (strcmp(code, "KeyO") == 0) return Key::O;
  if (strcmp(code, "KeyP") == 0) return Key::P;
  if (strcmp(code, "KeyQ") == 0) return Key::Q;
  if (strcmp(code, "KeyR") == 0) return Key::R;
  if (strcmp(code, "KeyS") == 0) return Key::S;
  if (strcmp(code, "KeyT") == 0) return Key::T;
  if (strcmp(code, "KeyU") == 0) return Key::U;
  if (strcmp(code, "KeyV") == 0) return Key::V;
  if (strcmp(code, "KeyW") == 0) return Key::W;
  if (strcmp(code, "KeyX") == 0) return Key::X;
  if (strcmp(code, "KeyY") == 0) return Key::Y;
  if (strcmp(code, "KeyZ") == 0) return Key::Z;

  // Numbers, Top Row and Numpad
  if (strcmp(code, "Digit0") == 0 || strcmp(code, "Numpad0") == 0) return Key::Num0;
  if (strcmp(code, "Digit1") == 0 || strcmp(code, "Numpad1") == 0) return Key::Num1;
  if (strcmp(code, "Digit2") == 0 || strcmp(code, "Numpad2") == 0) return Key::Num2;
  if (strcmp(code, "Digit3") == 0 || strcmp(code, "Numpad3") == 0) return Key::Num3;
  if (strcmp(code, "Digit4") == 0 || strcmp(code, "Numpad4") == 0) return Key::Num4;
  if (strcmp(code, "Digit5") == 0 || strcmp(code, "Numpad5") == 0) return Key::Num5;
  if (strcmp(code, "Digit6") == 0 || strcmp(code, "Numpad6") == 0) return Key::Num6;
  if (strcmp(code, "Digit7") == 0 || strcmp(code, "Numpad7") == 0) return Key::Num7;
  if (strcmp(code, "Digit8") == 0 || strcmp(code, "Numpad8") == 0) return Key::Num8;
  if (strcmp(code, "Digit9") == 0 || strcmp(code, "Numpad9") == 0) return Key::Num9;

  // Controls
  if (strcmp(code, "Space") == 0)       return Key::Space;
  if (strcmp(code, "Enter") == 0)       return Key::Enter;
  if (strcmp(code, "Escape") == 0)      return Key::Escape;
  if (strcmp(code, "Backspace") == 0)   return Key::Backspace;
  if (strcmp(code, "Tab") == 0)         return Key::Tab;
  if (strcmp(code, "ShiftLeft") == 0)   return Key::Shift;
  if (strcmp(code, "ShiftRight") == 0)  return Key::Shift;
  if (strcmp(code, "ControlLeft") == 0) return Key::Control;
  if (strcmp(code, "ControlRight") == 0) return Key::Control;
  if (strcmp(code, "AltLeft") == 0)     return Key::Alt;
  if (strcmp(code, "AltRight") == 0)    return Key::Alt;

  // Arrows
  if (strcmp(code, "ArrowUp") == 0)     return Key::ArrowUp;
  if (strcmp(code, "ArrowDown") == 0)   return Key::ArrowDown;
  if (strcmp(code, "ArrowLeft") == 0)   return Key::ArrowLeft;
  if (strcmp(code, "ArrowRight") == 0)  return Key::ArrowRight;

  return Key::None;
}
*/

extern "C" {
  EMSCRIPTEN_KEEPALIVE
  void JSOnKeyDown(u16 keyCode) {
    if (keyCode < (u16)Key::Count) {
      g_Raw.keys[keyCode] = true;
    }
  }

  EMSCRIPTEN_KEEPALIVE
  void JSOnKeyUp(u16 keyCode) {
    if (keyCode < (u16)Key::Count) {
      g_Raw.keys[keyCode] = false;
    }
  }
}

static EM_BOOL OnMouseDown(int /*eventType*/, const EmscriptenMouseEvent *e, void* /*userData*/) {
  if (e->button == 0) g_Raw.mouseButtons[(usize)MouseButton::Left] = true;
  if (e->button == 1) g_Raw.mouseButtons[(usize)MouseButton::Middle] = true;
  if (e->button == 2) g_Raw.mouseButtons[(usize)MouseButton::Right] = true;
  return EM_TRUE;
}

static EM_BOOL OnMouseUp(int /*eventType*/, const EmscriptenMouseEvent *e, void* /*userData*/) {
  if (e->button == 0) g_Raw.mouseButtons[(usize)MouseButton::Left] = false;
  if (e->button == 1) g_Raw.mouseButtons[(usize)MouseButton::Middle] = false;
  if (e->button == 2) g_Raw.mouseButtons[(usize)MouseButton::Right] = false;
  return EM_TRUE;
}

static EM_BOOL OnMouseMove(int /*eventType*/, const EmscriptenMouseEvent *e, void* /*userData*/) {
  g_Raw.mouseX = (f32)e->targetX;
  g_Raw.mouseY = (f32)e->targetY;

	g_Raw.deltaX += (f32)e->movementX;
  g_Raw.deltaY += (f32)e->movementY;
  return EM_TRUE;
}

static EM_BOOL OnWheel(int /*eventType*/, const EmscriptenWheelEvent *e, void* /*userData*/) {
  g_Raw.wheel += (f32)e->deltaY;
  return EM_TRUE;
}

static EM_BOOL OnGamepadConnected(int /*eventType*/, const EmscriptenGamepadEvent *e, void * /*userData*/) {
  logInfo("Gamepad Connected [Index %d]: %s", e->index, e->id);

  for (int i = 0; i < Input::MAX_PLAYERS; ++i) {
    if (g_Players[i].assignedGamepad == (i8)e->index) {
      return EM_TRUE;
    }
  }

  for (int i = 0; i < Input::MAX_PLAYERS; ++i) {
    if (g_Players[i].assignedGamepad == -1) {
      g_Players[i].assignedGamepad = (i8)e->index;
      g_Players[i].wantsGamepad = true; // Force enable
      logInfo(" -> Auto-assigned Gamepad %d to Player %d", e->index, i);
      break; 
    }
  }
  return EM_TRUE;
}

static EM_BOOL OnGamepadDisconnected(int /*eventType*/, const EmscriptenGamepadEvent *e, void * /*userData*/) {
  logInfo("Gamepad Disconnected [Index %d]: %s", e->index, e->id);

  for (int i = 0; i < Input::MAX_PLAYERS; ++i) {
    if (g_Players[i].assignedGamepad == (i8)e->index) {
      g_Players[i].assignedGamepad = -1; 
      logInfo("Player %d waiting for reconnect...", i);
    }
  }
  return EM_TRUE;
}

void Input::Init() {
  memset(&g_Raw, 0, sizeof(g_Raw));
  for (int i = 0; i < MAX_PLAYERS; ++i) {
    memset(g_Players[i].actions, 0, sizeof(g_Players[i].actions));
    g_Players[i].assignedGamepad = -1;
		g_Players[i].wantsGamepad = false;
  }

  emscripten_set_mousedown_callback("#canvas", nullptr, true, OnMouseDown);
  emscripten_set_mouseup_callback("#canvas", nullptr, true, OnMouseUp);
  emscripten_set_mousemove_callback("#canvas", nullptr, true, OnMouseMove);
  emscripten_set_wheel_callback("#canvas", nullptr, true, OnWheel);


	emscripten_set_gamepadconnected_callback(nullptr, true, OnGamepadConnected);
  emscripten_set_gamepaddisconnected_callback(nullptr, true, OnGamepadDisconnected);
}

static f32 EvaluateBinding(const InputBinding& bind, i8 gamepadIdx) {
	if (!bind.bypassConsume) {
	  if (bind.type == DeviceType::Keyboard 
	  		&& Input::s_consumeKeyboard) { 
	  	return 0.0f;
    }
    if (bind.type == DeviceType::Mouse         
	  		&& Input::s_consumeMouse) { 
	  	return 0.0f;
    }
    if (bind.type == DeviceType::GamepadButton 
	  		&& Input::s_consumeGamepadButtons) { 
	  	return 0.0f;
    }
    if (bind.type == DeviceType::GamepadAxis   
	  		&& Input::s_consumeGamepadAxis) {
	  	return 0.0f;
	  }
	}

  f32 value = 0.0f;

	switch (bind.type) {
		case DeviceType::Keyboard: {
			if (bind.key != Key::None) {
        return g_Raw.keys[(usize)bind.key] ? 1.0f : 0.0f;
      }
		} break;

		case DeviceType::Mouse: {
      if (bind.mouseBtn != MouseButton::None) {
        return g_Raw.mouseButtons[(usize)bind.mouseBtn] ? 1.0f : 0.0f;
      }
		} break;													
	
		case DeviceType::GamepadButton: {
			if (gamepadIdx >= 0 && gamepadIdx < Input::MAX_GAMEPADS) {
				const RawGamepadState rgp = g_Raw.gamepads[gamepadIdx];
				if (rgp.connected && bind.gamepadBtn != GamepadButton::None) {
					return rgp.buttons[(usize)bind.gamepadBtn] ? 1.0f : 0.0f;
				}
      } 
		} break;

    case DeviceType::GamepadAxis: {
			if (gamepadIdx >= 0 && gamepadIdx < Input::MAX_GAMEPADS) {
				const RawGamepadState rgp = g_Raw.gamepads[gamepadIdx];

				if (rgp.connected && bind.gamepadAxis != GamepadAxis::None) {
					f32 val = rgp.axes[(usize)bind.gamepadAxis];
          if (Abs(val) < bind.deadzone) { 
						val = 0.0f;
					} else {
						// Renormalize to 0.0 - 1.0 range to avoid jump in sensitivity
  				  float sign = (val > 0) ? 1.0f : -1.0f;
    				val = sign * ((Abs(val) - bind.deadzone) / (1.0f - bind.deadzone));
					}
          return bind.invert ? -val : val;
        }
      }
    } break;
  }
  
  return value;
}

void Input::Poll() {
	emscripten_sample_gamepad_data();

	for (int i = 0; i < MAX_GAMEPADS; ++i) {
    RawGamepadState& raw = g_Raw.gamepads[i];
    EmscriptenGamepadEvent gp;
        
    int result = emscripten_get_gamepad_status(i, &gp);
        
    raw.connected = (result == EMSCRIPTEN_RESULT_SUCCESS && gp.connected);
		if (raw.connected) {
      raw.buttons[(u8)GamepadButton::FaceDown]    = gp.digitalButton[0];
      raw.buttons[(u8)GamepadButton::FaceRight]   = gp.digitalButton[1];
      raw.buttons[(u8)GamepadButton::FaceLeft]    = gp.digitalButton[2];
      raw.buttons[(u8)GamepadButton::FaceUp]      = gp.digitalButton[3];
      raw.buttons[(u8)GamepadButton::LeftBumper]  = gp.digitalButton[4];
      raw.buttons[(u8)GamepadButton::RightBumper] = gp.digitalButton[5];
      
      raw.buttons[(u8)GamepadButton::Select]      = gp.digitalButton[8];
      raw.buttons[(u8)GamepadButton::Start]       = gp.digitalButton[9];
      raw.buttons[(u8)GamepadButton::LeftStick]   = gp.digitalButton[10];
      raw.buttons[(u8)GamepadButton::RightStick]  = gp.digitalButton[11];
      
      raw.buttons[(u8)GamepadButton::DPadUp]      = gp.digitalButton[12];
      raw.buttons[(u8)GamepadButton::DPadDown]    = gp.digitalButton[13];
      raw.buttons[(u8)GamepadButton::DPadLeft]    = gp.digitalButton[14];
      raw.buttons[(u8)GamepadButton::DPadRight]   = gp.digitalButton[15];

      raw.axes[(u8)GamepadAxis::LeftX]  = (f32)gp.axis[0];
      raw.axes[(u8)GamepadAxis::LeftY]  = (f32)gp.axis[1];
      raw.axes[(u8)GamepadAxis::RightX] = (f32)gp.axis[2];
      raw.axes[(u8)GamepadAxis::RightY] = (f32)gp.axis[3];
      
      raw.axes[(u8)GamepadAxis::LeftTrigger]  = (f32)gp.analogButton[6];
      raw.axes[(u8)GamepadAxis::RightTrigger] = (f32)gp.analogButton[7];
    } else {
      memset(&raw, 0, sizeof(RawGamepadState));
    }
  }

  // Process each player
  for (u8 p = 0; p < MAX_PLAYERS; ++p) {
    PlayerInput& player = g_Players[p];

    // Reset transient state
    for (u32 a = 0; a < MAX_ACTIONS; ++a) {
      player.actions[a].pressed = false;
      player.actions[a].released = false;
    }

    // Accumulate new values
    f32 newValues[MAX_ACTIONS] = {0};

    for (const auto& entry : player.bindings) {
      if (entry.action >= MAX_ACTIONS) continue;

      f32 val = EvaluateBinding(entry.bind, player.assignedGamepad);
			
			newValues[entry.action] += val;
    }

		for (u32 a = 0; a < MAX_ACTIONS; ++a) {
      ActionState& state = player.actions[a];
      bool wasDown = state.down;

      // CLAMP logic: Keep the value between -1.0 and 1.0
      f32 finalValue = newValues[a];
      if (finalValue > 1.0f) finalValue = 1.0f;
      if (finalValue < -1.0f) finalValue = -1.0f;

      bool isDown = Abs(finalValue) > 0.5f; // Threshold for digital "down"

      state.value = finalValue;
      state.down  = isDown;
      
      if (isDown && !wasDown) state.pressed = true;
      if (!isDown && wasDown) state.released = true;
    }
	}
}

void Input::EndFrame() {
	g_Raw.deltaX = 0.0f;
  g_Raw.deltaY = 0.0f;
  g_Raw.wheel = 0.0f;

	s_consumeKeyboard = false;
  s_consumeMouse    = false;
  s_consumeGamepadButtons = false;
  s_consumeGamepadAxis    = false;
}


void Input::SetPlayerGamepad(PlayerID player, u8 gamepadIndex) {
  if (player >= MAX_PLAYERS) {
    logError("SetPlayerGamepad: Invalid player ID %u", player);
    return;
  }
  g_Players[player].assignedGamepad = gamepadIndex;
	g_Players[player].wantsGamepad = true;
}

void Input::SetPlayerKeyboard(PlayerID player) {
  if (player >= MAX_PLAYERS) {
    logError("SetPlayerKeyboard: Invalid player ID %u", player);
    return;
  }
  g_Players[player].assignedGamepad = -1;
	g_Players[player].wantsGamepad = false;
}

void Input::MapAction(PlayerID player, ActionID action, InputBinding binding) {
  if (player >= MAX_PLAYERS) {
    logError("MapAction: Invalid player ID %u", player);
    return;
  }
  if (action >= MAX_ACTIONS) {
    logError("MapAction: Invalid action ID %u (Max: %u)", action, MAX_ACTIONS);
    return;
  }
  
  PlayerInput::BindingEntry entry;
  entry.action = action;
  entry.bind = binding;
  g_Players[player].bindings.push(entry);
}

void Input::ClearBindings(PlayerID player) {
  if (player >= MAX_PLAYERS) {
    logError("ClearBindings: Invalid player ID %u", player);
    return;
  }
  g_Players[player].bindings.clear();
}

bool Input::IsJustPressed(PlayerID player, ActionID action) {
  if (player >= MAX_PLAYERS || action >= MAX_ACTIONS) {
    logError("IsJustPressed: Invalid access Player %u, Action %u", player, action);
    return false;
  }
  return g_Players[player].actions[action].pressed;
}

bool Input::IsDown(PlayerID player, ActionID action) {
  if (player >= MAX_PLAYERS || action >= MAX_ACTIONS) {
    logError("IsDown: Invalid access Player %u, Action %u", player, action);
    return false;
  }
  return g_Players[player].actions[action].down;
}

bool Input::IsJustReleased(PlayerID player, ActionID action) {
  if (player >= MAX_PLAYERS || action >= MAX_ACTIONS) {
    logError("IsJustReleased: Invalid access Player %u, Action %u", player, action);
    return false;
  }
  return g_Players[player].actions[action].released;
}

f32 Input::GetValue(PlayerID player, ActionID action) {
  if (player >= MAX_PLAYERS || action >= MAX_ACTIONS) {
    logError("GetValue: Invalid access Player %u, Action %u", player, action);
    return 0.0f;
  }
  return g_Players[player].actions[action].value;
}

f32 Input::GetMouseX() { return g_Raw.mouseX; }
f32 Input::GetMouseDeltaX() { return g_Raw.deltaX; }
f32 Input::GetMouseY() { return g_Raw.mouseY; }
f32 Input::GetMouseDeltaY() { return g_Raw.deltaY; }
f32 Input::GetMouseScroll() { return g_Raw.wheel; }

void Input::SetCursorLock(bool lock) {
  if (lock) {
    emscripten_request_pointerlock("#canvas", true);
  } else {
    emscripten_exit_pointerlock();
  }
}

void Input::Consume(DeviceType type) {
  switch (type) {
    case DeviceType::Keyboard:      s_consumeKeyboard = true; break;
    case DeviceType::Mouse:         s_consumeMouse    = true; break;
    case DeviceType::GamepadButton: s_consumeGamepadButtons = true; break;
    case DeviceType::GamepadAxis:   s_consumeGamepadAxis    = true; break;
  }
}

void Input::ConsumeAll() {
  s_consumeKeyboard = true;
  s_consumeMouse    = true;
  s_consumeGamepadButtons = true;
  s_consumeGamepadAxis    = true;
}
