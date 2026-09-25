#pragma once

#include <LevyeForge.h>

using namespace LevyeForge;

class EditorScene : public Scene {
public:
  EditorScene(uint32_t width, uint32_t height);
  virtual ~EditorScene() override;

  virtual void OnUpdate(Timestep ts) override;

  bool CameraInputEnabled = false;
  EditorCamera &GetCamera() { return m_EditorCamera; }

private:
  EditorCamera m_EditorCamera;
};