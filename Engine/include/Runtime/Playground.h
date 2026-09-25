#pragma once
#include "ScriptableEntity.h"
#include <array>

namespace LevyeForge {
struct PlaygroundInput {
  glm::vec2 Move{0};
  glm::vec2 Look{0};
  bool Jump=false, Interact=false, Restart=false, Sprint=false;
};
class PlaygroundController : public ScriptableEntity {
public:
  void Advance(const PlaygroundInput &input, float dt);
  void Reset();
  bool InputEnabled = true;
  bool Grounded = false, Complete = false;
  int Collected = 0;
  float Elapsed = 0;
  std::string Prompt;
  const char *AnimationState() const { return m_State == 2 ? "Jump" : m_State == 1 ? "Run" : "Idle"; }
protected:
  void OnCreate() override;
  void OnUpdate(Timestep ts) override;
  void OnLateUpdate(Timestep ts) override;
private:
  Entity m_Visual, m_Camera, m_Beacon;
  std::array<Entity,3> m_Cells;
  std::array<bool,3> m_Collected{};
  std::array<glm::vec3,3> m_CellPositions{};
  Animation m_Idle, m_Run, m_Jump;
  float m_Yaw=0, m_Pitch=0.38f, m_Facing=3.14159265f;
  float m_Coyote=0, m_JumpBuffer=0, m_JumpCooldown=0, m_Time=0;
  int m_State=-1;
  glm::vec2 m_LastMouse{0};
  bool m_Orbiting=false;
  bool ProbeGround();
};
namespace Playground {
void RegisterScripts();
void Populate(Scene &scene);
PlaygroundController *Controller(Scene &scene);
void DrawHUD(Scene &scene, glm::vec2 origin, glm::vec2 size);
}
}
