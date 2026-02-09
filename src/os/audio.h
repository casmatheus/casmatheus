#pragma once

#include "core/primitive.h"

using SoundID = u32;

using AudioSourceID = u32;

constexpr SoundID INVALID_SOUND_ID = 0;
constexpr AudioSourceID INVALID_SOURCE_ID = 0;

namespace Audio {
  // Initializes the Audio Context
  void Init();

  // Loads an audio file (mp3, wav, ogg) from a URL/Path.
  // Returns an ID immediately; the sound becomes playable once loaded asynchronously.
  SoundID LoadFromURL(const char* path);

  // Returns a SourceID handle you can use to stop/modify this specific instance later.
  AudioSourceID Play(SoundID sound, f32 volume = 1.0f, f32 pitch = 1.0f, bool loop = false);

  // Stops a specific playing instance
  void Stop(AudioSourceID source);

  // Stops all currently playing sounds
  void StopAll();

  // Updates parameters of a currently playing sound
  void SetVolume(AudioSourceID source, f32 volume);
  void SetPitch(AudioSourceID source, f32 pitch);

  // Master volume control (0.0f to 1.0f)
  void SetMasterVolume(f32 volume);
}
