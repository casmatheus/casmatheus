#include "os/audio.h"
#include "debug/log.h"
#include <emscripten.h>

static SoundID s_NextSoundID = 1;

EM_JS(void, InitAudioJS, (), {
  Engine.Sound = {
    ctx: null,
    masterGain: null,
    buffers: {},       // Map<SoundID, AudioBuffer>
    sources: {},       // Map<SourceID, { sourceNode, gainNode }>
    nextSourceID: 1,   // Counter for playing instances
    
    unlock: function() {
      if (Engine.Sound.ctx && Engine.Sound.ctx.state === 'suspended') {
        Engine.Sound.ctx.resume()
      }
      window.removeEventListener('click', Engine.Sound.unlock);
      window.removeEventListener('keydown', Engine.Sound.unlock);
    }
  };

  try {
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    Engine.Sound.ctx = new AudioContext();
    
    Engine.Sound.masterGain = Engine.Sound.ctx.createGain();
    Engine.Sound.masterGain.gain.value = 0.5;
    Engine.Sound.masterGain.connect(Engine.Sound.ctx.destination);

    window.addEventListener('click', Engine.Sound.unlock);
    window.addEventListener('keydown', Engine.Sound.unlock);
    
  } catch (e) {
    console.error("[Audio] Web Audio API not supported:", e);
  }
});

EM_JS(void, LoadSoundJS, (u32 id, const char* url), {
  const src = UTF8ToString(url);
  
  if (!Engine.Sound.ctx) return;

  fetch(src)
    .then(response => response.arrayBuffer())
    .then(arrayBuffer => Engine.Sound.ctx.decodeAudioData(arrayBuffer))
    .then(audioBuffer => {
      Engine.Sound.buffers[id] = audioBuffer;
    })
    .catch(e => console.error("[Audio] Failed to load " + src, e));
});

EM_JS(u32, PlaySoundJS, (u32 soundID, f32 volume, f32 pitch, bool loop), {
  if (!Engine.Sound.ctx) return 0;
  
  const buffer = Engine.Sound.buffers[soundID];
  if (!buffer) {
    return 0;
  }

  const source = Engine.Sound.ctx.createBufferSource();
  source.buffer = buffer;
  source.playbackRate.value = pitch;
  source.loop = loop;

  const gainNode = Engine.Sound.ctx.createGain();
  gainNode.gain.value = volume;

  source.connect(gainNode);
  gainNode.connect(Engine.Sound.masterGain);

  source.start(0);

  const sourceID = Engine.Sound.nextSourceID++;
  Engine.Sound.sources[sourceID] = { source: source, gain: gainNode };

  source.onended = function() {
    delete Engine.Sound.sources[sourceID];
  };

  return sourceID;
});

EM_JS(void, StopSoundJS, (u32 sourceID), {
  const s = Engine.Sound.sources[sourceID];
  if (s && s.source) {
    try {
      s.source.stop();
    } catch(e) {}
    delete Engine.Sound.sources[sourceID];
  }
});

EM_JS(void, StopAllSoundsJS, (), {
  for (const key in Engine.Sound.sources) {
    const s = Engine.Sound.sources[key];
    if (s && s.source) {
      try { s.source.stop(); } catch(e) {}
    }
  }
  Engine.Sound.sources = {};
});

EM_JS(void, SetMasterVolumeJS, (f32 vol), {
  if (Engine.Sound.masterGain) {
    Engine.Sound.masterGain.gain.value = vol;
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
