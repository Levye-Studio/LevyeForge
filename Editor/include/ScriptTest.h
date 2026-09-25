#pragma once
#include <LevyeForge.h>

// A selectable, compiled example. Works without a camera or a model.
class PlayerController : public LevyeForge::ScriptableEntity {
public:
  void OnUpdate(Timestep ts) override {
    using namespace LevyeForge;
    glm::vec3 direction(0);
    if (Input::IsKeyPressed(Key::W)) direction.z -= 1;
    if (Input::IsKeyPressed(Key::S)) direction.z += 1;
    if (Input::IsKeyPressed(Key::A)) direction.x -= 1;
    if (Input::IsKeyPressed(Key::D)) direction.x += 1;
    if (glm::length(direction) > 0) direction = glm::normalize(direction);
    const auto velocity = direction * m_Speed;
    if (HasComponent<RigidbodyComponent>()) {
      auto &body = GetComponent<RigidbodyComponent>();
      body.LinearVelocity.x = velocity.x;
      body.LinearVelocity.z = velocity.z;
      if (body.Type == RigidbodyComponent::BodyType::Kinematic)
        GetComponent<TransformComponent>().Translation += velocity * (float)ts;
      if (body.RuntimeCreated && body.Type == RigidbodyComponent::BodyType::Dynamic) {
        auto current = GetBodyInterface().GetLinearVelocity(JPH::BodyID(body.BodyID));
        GetBodyInterface().SetLinearVelocity(JPH::BodyID(body.BodyID),
            {velocity.x, current.GetY(), velocity.z});
      }
    } else {
      GetComponent<TransformComponent>().Translation += velocity * (float)ts;
    }
  }
  void OnImGuiRender() override { ImGui::DragFloat("Move Speed", &m_Speed, 0.1f, 0, 100); }
private:
  float m_Speed = 3;
};
