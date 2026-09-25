#pragma once

#include "Animation/Animation.h"
#include "Log.h"
#include <entt.hpp>
#include "UUID.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/base/maths/simd_math.h"
#include <ozz/base/maths/soa_float4x4.h>
#include <ozz/base/maths/soa_transform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include "Config.h"
#include "Renderer/Camera.h"
#include "Renderer/Font.h"
#include "Renderer/Model.h"
#include "Renderer/Skybox.h"
#include "Renderer/Texture.h"
#include "SceneCamera.h"
#include <glm/gtx/quaternion.hpp>

namespace LevyeForge {
// Rigid body "role"
enum class BodyType { Static, Dynamic, Kinematic };

struct IDComponent {
  UUID ID;

  IDComponent() = default;
  IDComponent(const IDComponent &) = default;
};

struct TagComponent {
  std::string Tag;

  TagComponent() = default;
  TagComponent(const TagComponent &) = default;
  TagComponent(const std::string &tag) : Tag(tag) {}
};

// Handles are scene-local; serialization stores the parent's stable UUID.
struct RelationshipComponent {
  entt::entity Parent = entt::null;
};

struct TransformComponent {
  glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
  glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
  glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

  TransformComponent() = default;
  TransformComponent(const TransformComponent &) = default;
  TransformComponent(const glm::vec3 &translation) : Translation(translation) {}

  glm::mat4 GetTransform() const {
    glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

    return glm::translate(glm::mat4(1.0f), Translation) * rotation *
           glm::scale(glm::mat4(1.0f), Scale);
  }

  float GetRadius() const {
    // Assuming you're transforming a unit sphere
    return glm::compMax(Scale) * 0.5f; // max(Scale.x, Scale.y, Scale.z)
  }
};

struct CameraComponent {
  SceneCamera Camera;
  bool Primary = false; // TODO: think about moving to Scene
  bool FixedAspectRatio = false;
  bool ShowFrustum = false;

  CameraComponent() = default;
  CameraComponent(const CameraComponent &) = default;
};

struct SkyboxComponent {
  Ref<Skybox> skybox;
  Camera *Cam = nullptr;
  std::array<std::string, 6> Faces;
  SkyboxComponent() = default;
  SkyboxComponent(const SkyboxComponent &) = default;
};

class ScriptableEntity;

struct NativeScriptComponent {
  std::string ClassName, Error;
  bool Enabled = true;
  NativeScriptComponent() = default;
  NativeScriptComponent(const NativeScriptComponent &other)
      : ClassName(other.ClassName), Enabled(other.Enabled),
        InstantiateScript(other.InstantiateScript), DestroyScript(other.DestroyScript) {}
  NativeScriptComponent &operator=(const NativeScriptComponent &other) {
    if (this != &other) {
      ClassName = other.ClassName; Enabled = other.Enabled; Error.clear();
      Instance = nullptr; InstantiateScript = other.InstantiateScript;
      DestroyScript = other.DestroyScript;
    }
    return *this;
  }
  NativeScriptComponent(NativeScriptComponent &&) noexcept = default;
  NativeScriptComponent &operator=(NativeScriptComponent &&) noexcept = default;
  ScriptableEntity *Instance = nullptr;

  ScriptableEntity *(*InstantiateScript)() = nullptr;
  void (*DestroyScript)(NativeScriptComponent *) = nullptr;

  template <typename T> void Bind() {
    InstantiateScript = []() {
      return static_cast<ScriptableEntity *>(new T());
    };
    DestroyScript = [](NativeScriptComponent *nsc) {
      delete nsc->Instance;
      nsc->Instance = nullptr;
    };
  }
};

class LuaScriptInstance;
struct LuaScriptComponent {
  std::string Path, Error;
  bool Enabled = true;
  std::shared_ptr<LuaScriptInstance> Instance;
  LuaScriptComponent() = default;
  LuaScriptComponent(const LuaScriptComponent &other) : Path(other.Path), Enabled(other.Enabled) {}
  LuaScriptComponent &operator=(const LuaScriptComponent &other) {
    if (this != &other) { Path = other.Path; Enabled = other.Enabled; Error.clear(); Instance.reset(); }
    return *this;
  }
  LuaScriptComponent(LuaScriptComponent &&) noexcept = default;
  LuaScriptComponent &operator=(LuaScriptComponent &&) noexcept = default;
};

struct ModelComponent {
  Ref<Model> ModelData;

  ModelComponent() = default;
  ModelComponent(const ModelComponent &) = default;
};

struct CubeComponent {
  glm::vec3 Color{1.0f};
  CubeComponent() = default;
  CubeComponent(const CubeComponent &) = default;
};

struct LightComponent {
  glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
  LightComponent() = default;
  LightComponent(const LightComponent &) = default;
};

struct SpriteRendererComponent {
  glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
  Ref<Texture2D> Texture;
  float TilingFactor = 1.0f;

  SpriteRendererComponent() = default;
  SpriteRendererComponent(const SpriteRendererComponent &) = default;
  SpriteRendererComponent(const glm::vec4 &color) : Color(color) {}
};

struct CircleComponent {
  glm::vec4 Color{1.0f};
  float Radius = 0.5f;
  CircleComponent() = default;
  CircleComponent(const CircleComponent &) = default;
};

struct RectangleComponent {
  glm::vec4 Color{1.0f};
  RectangleComponent() = default;
  RectangleComponent(const RectangleComponent &) = default;
};

struct LineComponent {
  glm::vec4 Color{1.0f};
  float Thickness = 0.05f;
  glm::vec2 p0{0, 0}, p1{1, 0};
  float Order = -1;
  LineComponent() = default;
  LineComponent(const LineComponent &) = default;
};

// Animation 3D
struct AnimatorComponent {
  Ref<Model> ModelRef;
  Animation CurrentAnimation;
  float Time = 0.0f;
  float Speed = 1.0f;
  bool Loop = true;
  bool Playing = true;
  bool HasPose = false;
  bool ShowSkeleton = false;
  std::string InPlaceJoint;
  float BlendDuration = 0, BlendTime = 0;
  std::vector<ozz::math::SoaTransform> BlendFrom, BlendOutput;
  ozz::unique_ptr<ozz::animation::SamplingJob::Context> Context;
  std::vector<ozz::math::SoaTransform> LocalTransforms;
  std::vector<ozz::math::Float4x4> ModelMatrices, FinalMatrices;

  void InitFromModel(const Ref<Model> &model) {
    ModelRef = model;
    CurrentAnimation = Animation();
    Context.reset();
    Time = 0;
    HasPose = false;
    BlendFrom.clear(); BlendOutput.clear(); BlendDuration = BlendTime = 0;
    LocalTransforms.clear();
    ModelMatrices.clear();
    FinalMatrices.clear();
    if (!model || !model->GetSkeleton()) return;
    const auto *skeleton = model->GetSkeleton();
    LocalTransforms.assign(skeleton->joint_rest_poses().begin(), skeleton->joint_rest_poses().end());
    ModelMatrices.resize(skeleton->num_joints(), ozz::math::Float4x4::identity());
    FinalMatrices.resize(skeleton->num_joints(), ozz::math::Float4x4::identity());
    Context = ozz::make_unique<ozz::animation::SamplingJob::Context>(skeleton->num_joints());
  }

  bool Play(Animation animation) {
    if (!ModelRef || !animation.Get() || animation.GetSkeleton() != ModelRef->GetSkeleton() ||
        animation.Get()->num_tracks() != ModelRef->GetSkeleton()->num_joints())
      return false;
    BlendFrom.clear(); BlendDuration = BlendTime = 0;
    CurrentAnimation = std::move(animation);
    Time = 0;
    Playing = true;
    HasPose = false;
    Context->Invalidate();
    return true;
  }

  bool CrossFade(Animation animation, float duration = 0.16f) {
    const auto previousPose = LocalTransforms;
    const bool hadPose = HasPose;
    if (!Play(std::move(animation))) return false;
    if (hadPose && duration > 0) {
      BlendFrom = previousPose;
      BlendOutput.resize(previousPose.size());
      BlendDuration = duration;
    }
    return true;
  }

  AnimatorComponent() = default;
  AnimatorComponent(const AnimatorComponent &other) { *this = other; }
  AnimatorComponent &operator=(const AnimatorComponent &other) {
    if (this == &other) return *this;
    InitFromModel(other.ModelRef);
    if (other.CurrentAnimation.Get()) Play(other.CurrentAnimation);
    Time = other.Time;
    Speed = other.Speed;
    ShowSkeleton = other.ShowSkeleton;
    InPlaceJoint = other.InPlaceJoint;
    Loop = other.Loop;
    Playing = other.Playing;
    return *this;
  }
  AnimatorComponent(AnimatorComponent &&) noexcept = default;
  AnimatorComponent &operator=(AnimatorComponent &&) noexcept = default;
};

// physics 3d

struct BoxColliderComponent {
  bool ShowWireframe = false;
  glm::vec3 HalfSize = {0.5f, 0.5f, 0.5f};

  float Friction = 0.5f;
  float Restitution = 0.0f;

  bool IsTrigger = false;
};

struct SphereColliderComponent {
  bool ShowWireframe = false;
  float Radius = 0.5f;
  float Friction = 0.5f;
  float Restitution = 0.0f;
  bool IsTrigger = false;
};

struct RigidbodyComponent {
  enum class BodyType { Static = 0, Dynamic, Kinematic };

  BodyType Type = BodyType::Static;

  glm::vec3 LinearVelocity = glm::vec3(0);
  float Mass = 1.0f;
  float LinearDamping = 0.0f;
  float AngularDamping = 0.05f;

  bool UseGravity = true;
  bool LockRotation = false;

  uint32_t BodyID = 0xffffffffu;
  bool RuntimeCreated = false;
};

/// UI
struct UIElement {
  Ref<Texture2D> Texture;
  glm::vec4 Color{1.0f};

  UIElement() = default;
  UIElement(const UIElement &) = default;
};

struct ButtonComponent : public UIElement {
  std::function<void()> OnClick = nullptr;
  bool Hovered = false;
  bool ClickedLastFrame = false;
  glm::vec3 OriginalScale = {120, 50, 1};
  glm::vec3 TargetScale = {120, 50, 1};
  glm::vec4 BaseColor = {1, 1, 1, 1};
  glm::vec4 CurrentColor = {1, 1, 1, 1};

  ButtonComponent() = default;
  ButtonComponent(const ButtonComponent &) = default;
};

struct TextUIComponent : public UIElement {
  Ref<Font> m_Font;
  std::string Text;
  TextUIComponent() = default;
  TextUIComponent(const TextUIComponent &) = default;
};

} // namespace LevyeForge
