#pragma once
#include "Entity.h"
#include "Scene.h"
#include "Timestep.h"

namespace LevyeForge {

class RuntimeScene : public Scene {
public:
  RuntimeScene() = default;
  RuntimeScene(uint32_t width, uint32_t height);
  virtual ~RuntimeScene() override;

  void OnRuntimeStart();
  void OnRuntimeStop();
  void PhysicsUpdate(float dt);
  void UpdateScripts(Timestep ts, bool runUpdate = true);
  void LateUpdateScripts(Timestep ts);
  bool IsRunning() const { return m_Running; }
  virtual void OnUpdate(Timestep ts) override;

  void ClearColor(const glm::vec4 &color) { m_ClearColor = color; }

private:
  bool m_Running = false;
  Camera &GetMainCamera();
  void FindPrimaryCamera();

  glm::vec4 m_ClearColor = {0.1f, 0.1f, 0.1f, 1};

  // Entity m_PrimaryCameraEntity;
  entt::entity m_PrimaryCameraEntity = entt::null;
};
} // namespace LevyeForge