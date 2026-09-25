#pragma once
namespace LevyeForge {
enum class SoundCue { Jump, Collect, Complete };
class Audio {
public:
  static bool Initialize();
  static void Shutdown();
  static void Play(SoundCue cue);
  static void SetMuted(bool muted);
  static bool IsMuted();
  static bool IsAvailable();
  // Pure synthesis function used by the mixer and offline regression tests.
  static float Sample(SoundCue cue, float seconds);
};
}
