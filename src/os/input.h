#pragma once

#include "core/primitive.h"

enum class Key : u16 {
  None = 0,
  
  A, B, C, D, E, F, G, H, I, J, K, L, M, 
  N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

  Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

  Space, Enter, Escape, Backspace, Tab, Shift, Control, Alt,
  ArrowUp, ArrowDown, ArrowLeft, ArrowRight,

  Count
};

// Mouse Buttons
enum class MouseButton : u8 {
  None = 0,
  Left,
  Right,
  Middle,
  Side1,
  Side2,
  Count
};

// Standard Gamepad Buttons (Xbox/PlayStation/Switch layout abstraction)
enum class GamepadButton : u8 {
  None = 0,
  FaceDown,   // A / Cross
  FaceRight,  // B / Circle
  FaceLeft,   // X / Square
  FaceUp,     // Y / Triangle
  LeftBumper, // L1 / LB
  RightBumper,// R1 / RB
		// These are only here for alignment with browser
	LeftTrigger_Button,
  RightTrigger_Button,	
  Select,     // Share / Back
  Start,      // Options / Start
  LeftStick,  // L3
  RightStick, // R3
  DPadUp,
  DPadDown,
  DPadLeft,
  DPadRight,
  Count
};

// Analog Axes
enum class GamepadAxis : u8 {
  None = 0,
  LeftX,      // -1.0 (Left) to 1.0 (Right)
  LeftY,      // -1.0 (Up)   to 1.0 (Down)
  RightX,
  RightY,
  LeftTrigger, // 0.0 to 1.0
  RightTrigger,
  Count
};


using PlayerID = u8;
using ActionID = u16;

enum class DeviceType : u8 {
  Keyboard,
  Mouse,
  GamepadButton,
  GamepadAxis
};

struct InputBinding {
  DeviceType type;
  union {
    Key key;
    MouseButton mouseBtn;
    GamepadButton gamepadBtn;
    GamepadAxis gamepadAxis;
  };
	
	bool bypassConsume = false;
  
  // For Axis: Threshold to consider "Pressed" as a button
  f32 deadzone = 0.1f; 
  
  // If true, the axis value is inverted
  bool invert = false; 

	InputBinding& bypass() { 
    bypassConsume = true; 
    return *this; 
  }

  static InputBinding FromKey(Key k) { 
    InputBinding b = {}; 
		b.type = DeviceType::Keyboard; 
		b.key = k; 
		return b; 
  }
  static InputBinding FromMouse(MouseButton m) { 
    InputBinding b = {}; 
		b.type = DeviceType::Mouse; 
		b.mouseBtn = m; 
		return b; 
  }
  static InputBinding FromButton(GamepadButton btn) { 
    InputBinding b = {}; 
		b.type = DeviceType::GamepadButton; 
		b.gamepadBtn = btn; 
		return b; 
  }
  static InputBinding FromAxis(GamepadAxis axis, bool invert = false) { 
    InputBinding b = {}; 
		b.type = DeviceType::GamepadAxis; 
		b.gamepadAxis = axis; 
		b.invert = invert; 
		return b; 
  }
};

struct Input {
  static constexpr u8 MAX_PLAYERS = 4;
  static constexpr u8 MAX_GAMEPADS = 4;

  static void Init();
  
  // Call this at the start of every frame to update state
  static void Poll(); 
  static void EndFrame(); 

  // Assign a specific gamepad index (0-3) to a Player
  // e.g. SetPlayerGamepad(0, 0) -> Player 1 uses Gamepad 1
  // e.g. SetPlayerGamepad(1, 1) -> Player 2 uses Gamepad 2
  static void SetPlayerGamepad(PlayerID player, u8 gamepadIndex);

  // Assign the Keyboard to a Player 
	static void SetPlayerKeyboard(PlayerID player);

  // Map an action to a physical input
  static void MapAction(PlayerID player, ActionID action, InputBinding binding);
  
  // Clear all bindings for a player
  static void ClearBindings(PlayerID player);

  // Returns true if the action was pressed THIS frame
  static bool IsJustPressed(PlayerID player, ActionID action);

  // Returns true if the action is currently held down
  static bool IsDown(PlayerID player, ActionID action);

  // Returns true if the action was released THIS frame
  static bool IsJustReleased(PlayerID player, ActionID action);

  // Returns the raw float value (-1.0 to 1.0)
  // Useful for movement axes or throttle
  static f32  GetValue(PlayerID player, ActionID action);
	
	// Blocks input from a device type for the current frame.
	static void Consume(DeviceType type);
	static void ConsumeAll();

  static f32 GetMouseX();
  static f32 GetMouseDeltaX();
  static f32 GetMouseY();
  static f32 GetMouseDeltaY();
  static f32 GetMouseScroll();

	static void SetCursorLock(bool lock);

  static bool s_consumeKeyboard;
  static bool s_consumeMouse;
	static bool s_consumeGamepadButtons;
	static bool s_consumeGamepadAxis;

};
