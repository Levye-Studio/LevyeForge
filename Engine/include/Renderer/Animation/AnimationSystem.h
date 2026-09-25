#pragma once

#include "Timestep.h"

namespace LevyeForge {

class Scene;

class AnimationSystem {
public:
  static void Update(Scene *scene, Timestep ts);
};

} // namespace LevyeForge