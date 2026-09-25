#include "lfpch.h"
#include "Audio/Audio.h"
#include <miniaudio.h>
#include <atomic>
#include <cmath>

namespace LevyeForge {
namespace {
ma_device device;
bool available = false;
std::atomic<bool> muted{false};
std::atomic<unsigned> pending{0};
float clocks[3] = {10,10,10};
void Callback(ma_device *, void *output, const void *, ma_uint32 frames) {
  const unsigned requested = pending.exchange(0);
  for (int i=0;i<3;++i) if (requested & (1u<<i)) clocks[i]=0;
  auto *samples = static_cast<float *>(output);
  for (ma_uint32 frame=0;frame<frames;++frame) {
    float value=0;
    for (int i=0;i<3;++i) {
      value += Audio::Sample(static_cast<SoundCue>(i), clocks[i]);
      clocks[i] += 1.0f/48000;
    }
    value = muted.load() ? 0 : std::clamp(value, -0.7f, 0.7f);
    samples[frame*2]=samples[frame*2+1]=value;
  }
}
}
float Audio::Sample(SoundCue cue, float t) {
  const float duration = cue == SoundCue::Jump ? 0.18f : cue == SoundCue::Collect ? 0.35f : 1.0f;
  if (t < 0 || t >= duration) return 0;
  float frequency = 330;
  if (cue == SoundCue::Jump) frequency = 300 + 850*t;
  if (cue == SoundCue::Collect) frequency = t < 0.14f ? 660 : 990;
  if (cue == SoundCue::Complete) {
    const float notes[] = {523.25f,659.25f,783.99f,1046.5f};
    frequency = notes[std::min(3, int(t*4))];
  }
  const float envelope = std::min(1.0f, t/0.01f) * (1-t/duration);
  return 0.16f * envelope * std::sin(6.2831853f * frequency * t);
}
bool Audio::Initialize() {
  if (available) return true;
  auto config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = 48000;
  config.dataCallback = Callback;
  if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
    LF_CORE_WARN("Audio output unavailable; continuing without sound");
    return false;
  }
  pending.store(0);
  for (auto &clock : clocks) clock=10;
  if (ma_device_start(&device) != MA_SUCCESS) { ma_device_uninit(&device); return false; }
  available=true;
  return true;
}
void Audio::Shutdown() { if (available) { ma_device_uninit(&device); available=false; } pending=0; }
void Audio::Play(SoundCue cue) { if (available) pending.fetch_or(1u << static_cast<unsigned>(cue)); }
void Audio::SetMuted(bool value) { muted=value; }
bool Audio::IsMuted() { return muted; }
bool Audio::IsAvailable() { return available; }
}
