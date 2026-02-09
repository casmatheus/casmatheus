#include "os/audio.h"
#include "debug/log.h"
#include <emscripten.h>

static SoundID s_NextSoundID = 1;

EM_JS(void, InitAudioJS, (), {
  Module.Sound = {
    ctx: null,
    masterGain: null,
    buffers: {},       // Map<SoundID, AudioBuffer>
    sources: {},       // Map<SourceID, { sourceNode, gainNode }>
    nextSourceID: 1,   // Counter for playing instances
    
    // Browser AudioContexts usually start "suspended". 
    unlock: function() {
      if (Module.Sound.ctx && Module.Sound.ctx.state === 'suspended') {
        Module.Sound.ctx.resume()
      }
      // Remove listeners once unlocked
      window.removeEventListener('click', Module.Sound.unlock);
      window.removeEventListener('keydown', Module.Sound.unlock);
    }
  };

  try {
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    Module.Sound.ctx = new AudioContext();
    
    // Create Master Gain Node
    Module.Sound.masterGain = Module.Sound.ctx.createGain();
    Module.Sound.masterGain.gain.value = 0.5; // Default volume
    Module.Sound.masterGain.connect(Module.Sound.ctx.destination);

    // Setup Unlock Listeners
    window.addEventListener('click', Module.Sound.unlock);
    window.addEventListener('keydown', Module.Sound.unlock);
    
  } catch (e) {
    console.error("[Audio] Web Audio API not supported:", e);
  }
});

EM_JS(void, LoadSoundJS, (u32 id, const char* url), {
  const src = UTF8ToString(url);
  
  if (!Module.Sound.ctx) return;

  fetch(src)
    .then(response => response.arrayBuffer())
    .then(arrayBuffer => Module.Sound.ctx.decodeAudioData(arrayBuffer))
    .then(audioBuffer => {
      Module.Sound.buffers[id] = audioBuffer;
    })
    .catch(e => console.error("[Audio] Failed to load " + src, e));
});

EM_JS(u32, PlaySoundJS, (u32 soundID, f32 volume, f32 pitch, bool loop), {
  if (!Module.Sound.ctx) return 0;
  
  const buffer = Module.Sound.buffers[soundID];
  if (!buffer) {
    return 0;
  }

  // Create Nodes
  const source = Module.Sound.ctx.createBufferSource();
  source.buffer = buffer;
  source.playbackRate.value = pitch;
  source.loop = loop;

  const gainNode = Module.Sound.ctx.createGain();
  gainNode.gain.value = volume;

  // Connect: Source -> InstanceGain -> MasterGain -> Output
  source.connect(gainNode);
  gainNode.connect(Module.Sound.masterGain);

  source.start(0);

  // Track the source
  const sourceID = Module.Sound.nextSourceID++;
  Module.Sound.sources[sourceID] = { source: source, gain: gainNode };

  // Cleanup when sound finishes (if not looping)
  source.onended = function() {
    delete Module.Sound.sources[sourceID];
  };

  return sourceID;
});

EM_JS(void, StopSoundJS, (u32 sourceID), {
  const s = Module.Sound.sources[sourceID];
  if (s && s.source) {
    try {
      s.source.stop();
    } catch(e) {} // Ignore errors if already stopped
    delete Module.Sound.sources[sourceID];
  }
});

EM_JS(void, StopAllSoundsJS, (), {
  for (const key in Module.Sound.sources) {
    const s = Module.Sound.sources[key];
    if (s && s.source) {
      try { s.source.stop(); } catch(e) {}
    }
  }
  Module.Sound.sources = {};
});

EM_JS(void, SetMasterVolumeJS, (f32 vol), {
  if (Module.Sound.masterGain) {
    Module.Sound.masterGain.gain.value = vol;
  }
});


namespace Audio {

void Init() {
  InitAudioJS();
  logInfo("Audio System Initialized");
}

SoundID LoadFromURL(const char* path) {
  if (!path) return INVALID_SOUND_ID;
  
  SoundID id = s_NextSoundID++;
  LoadSoundJS(id, path);
  return id;
}

AudioSourceID Play(SoundID sound, f32 volume, f32 pitch, bool loop) {
  if (sound == INVALID_SOUND_ID) return INVALID_SOURCE_ID;
  return (AudioSourceID)PlaySoundJS(sound, volume, pitch, loop);
}

void Stop(AudioSourceID source) {
  if (source == INVALID_SOURCE_ID) return;
  StopSoundJS(source);
}

void StopAll() {
  StopAllSoundsJS();
}

void SetMasterVolume(f32 volume) {
  SetMasterVolumeJS(volume);
}

}
