#include "Runtime/RuntimeScene.h"
#include "Animation/AnimationSystem.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "Runtime/Components.h"
#include "Runtime/Entity.h"
#include "Runtime/ScriptableEntity.h"
#include "Runtime/NativeScriptRegistry.h"
#include "Runtime/LuaScript.h"

namespace LevyeForge {

RuntimeScene::RuntimeScene(uint32_t width, uint32_t height) {
  LF_PROFILE_FUNCTION();
  m_ViewportWidth = width;
  m_ViewportHeight = height;

  FramebufferSpecification fbSpec;
  fbSpec.Attachments = {FramebufferTextureFormat::RGBA8,
                        FramebufferTextureFormat::RED_INTEGER,
                        FramebufferTextureFormat::Depth};
  fbSpec.Width = width;
  fbSpec.Height = height;
  m_Framebuffer = Framebuffer::Create(fbSpec);
}
RuntimeScene::~RuntimeScene() { OnRuntimeStop(); }

// ------------------------------
// Scene::OnRuntimeStart
// ------------------------------

void RuntimeScene::OnRuntimeStart() {
  if (m_Running)
    return;
  Physics::Init();
  m_Running = true;
  FindPrimaryCamera();
  auto view = m_Registry.view<RigidbodyComponent, TransformComponent>();
  for (auto entity : view)
    Physics::CreateBody(entity, this);
  Physics::GetSystem().OptimizeBroadPhase();
  m_Registry.view<NativeScriptComponent>().each([](auto &script) { script.Error.clear(); });
  m_Registry.view<LuaScriptComponent>().each([](auto &script) { script.Error.clear(); });
  UpdateScripts(0, false);
}

void RuntimeScene::OnRuntimeStop() {
  if (!m_Running)
    return;
  m_Running = false;
  std::vector<entt::entity> scripts;
  for (auto entity : m_Registry.view<NativeScriptComponent>()) scripts.push_back(entity);
  for (auto entity : scripts) if (m_Registry.valid(entity)) DestroyScriptInstance(Entity(entity, this));
  scripts.clear();
  for (auto entity : m_Registry.view<LuaScriptComponent>()) scripts.push_back(entity);
  for (auto entity : scripts) if (m_Registry.valid(entity)) DestroyLuaScriptInstance(Entity(entity, this));
  FlushEntityDestruction();
  Physics::Shutdown();
  m_Registry.view<RigidbodyComponent>().each([](auto &rb) {
    rb.RuntimeCreated = false;
    rb.BodyID = 0xffffffffu;
  });
}

void RuntimeScene::UpdateScripts(Timestep ts, bool runUpdate) {
  if (!m_Running) return;
  // Snapshot IDs: callbacks may add scripts or queue entity destruction.
  std::vector<entt::entity> entities;
  for (auto id : m_Registry.view<NativeScriptComponent>()) entities.push_back(id);
  for (auto id : entities) {
    if (!m_Registry.valid(id)) continue;
    auto *script = m_Registry.try_get<NativeScriptComponent>(id);
    if (!script) continue;
    if (!script->Enabled) { DestroyScriptInstance(Entity(id, this)); continue; }
    if (!script->Error.empty()) continue;
    if (!script->InstantiateScript && !script->ClassName.empty())
      NativeScriptRegistry::Bind(*script, script->ClassName);
    if (!script->InstantiateScript) {
      if (!script->ClassName.empty()) script->Error = "Native script class is not registered: " + script->ClassName;
      continue;
    }
    try {
      if (!script->Instance) {
        script->Instance = script->InstantiateScript();
        if (!script->Instance) throw std::runtime_error("Native script factory returned null");
        script->Instance->m_Entity = Entity(id, this);
        script->Instance->m_Scene = this;
        script->Instance->m_BodyInterface = &Physics::GetSystem().GetBodyInterface();
        script->Instance->OnCreate();
      }
      // Reacquire after OnCreate: a callback may change component storage.
      script = m_Registry.try_get<NativeScriptComponent>(id);
      if (runUpdate && script && script->Instance) script->Instance->OnUpdate(ts);
    } catch (const std::exception &error) {
      if (auto *current = m_Registry.try_get<NativeScriptComponent>(id)) current->Error = error.what();
      LF_CORE_ERROR("Native script failed: {}", error.what());
      DestroyScriptInstance(Entity(id, this));
    } catch (...) {
      if (auto *current = m_Registry.try_get<NativeScriptComponent>(id)) current->Error = "Unknown native script exception";
      DestroyScriptInstance(Entity(id, this));
    }
  }
  entities.clear();
  for (auto id : m_Registry.view<LuaScriptComponent>()) entities.push_back(id);
  for (auto id : entities) {
    auto *script = m_Registry.try_get<LuaScriptComponent>(id);
    if (!script) continue;
    if (!script->Enabled) { DestroyLuaScriptInstance(Entity(id, this)); continue; }
    if (!script->Error.empty() || script->Path.empty()) continue;
    if (!script->Instance) {
      script->Instance = std::make_shared<LuaScriptInstance>(Entity(id, this));
      if (!script->Instance->Load(script->Path)) {
        script->Error = script->Instance->GetError();
        DestroyLuaScriptInstance(Entity(id, this));
        continue;
      }
    }
    if (runUpdate && !script->Instance->Update(std::max(0.0f, (float)ts))) {
      script->Error = script->Instance->GetError();
      DestroyLuaScriptInstance(Entity(id, this));
    }
  }
}

void RuntimeScene::LateUpdateScripts(Timestep ts) {
  if (!m_Running) return;
  std::vector<entt::entity> entities;
  for (auto id : m_Registry.view<NativeScriptComponent>()) entities.push_back(id);
  for (auto id : entities) {
    auto *script = m_Registry.try_get<NativeScriptComponent>(id);
    if (!script || !script->Enabled || !script->Instance || !script->Error.empty()) continue;
    try { script->Instance->OnLateUpdate(ts); }
    catch (const std::exception &e) {
      if (auto *current = m_Registry.try_get<NativeScriptComponent>(id)) current->Error=e.what();
      DestroyScriptInstance(Entity(id,this));
    }
    catch (...) { DestroyScriptInstance(Entity(id,this)); }
  }
}

void RuntimeScene::PhysicsUpdate(float dt) {
  if (m_Running)
    Physics::Step(this, dt);
}

void RuntimeScene::FindPrimaryCamera() {
  auto view = m_Registry.view<CameraComponent>();
  for (auto entity : view) {
    if (view.get<CameraComponent>(entity).Primary) {
      m_PrimaryCameraEntity = entity;
      return;
    }
  }

  m_PrimaryCameraEntity = entt::null;
}

Camera &RuntimeScene::GetMainCamera() {

  LF_CORE_ASSERT(m_PrimaryCameraEntity != entt::null, "No Primary Camera!");

  auto &camComp = m_Registry.get<CameraComponent>(m_PrimaryCameraEntity);
  auto transform = GetWorldTransformComponents(m_PrimaryCameraEntity);

  camComp.Camera.SetPosition(transform.Translation);
  camComp.Camera.SetRotation2(transform.Rotation);

  return camComp.Camera;
}

void RuntimeScene::OnUpdate(Timestep ts) {
  LF_PROFILE_FUNCTION();
  if (!m_Running)
    return;
  m_Framebuffer->Bind();
  // Clear our entity ID attachment to -1
  RenderCommand::SetClearColor(m_ClearColor);
  RenderCommand::Clear();
  m_Framebuffer->ClearAttachment(1, -1);

  UpdateScripts(ts);

  PhysicsUpdate(ts);
  LateUpdateScripts(ts);
  AnimationSystem::Update(this, ts);

  FindPrimaryCamera();
  if (m_PrimaryCameraEntity == entt::null) {
    m_Framebuffer->Unbind();
    FlushEntityDestruction();
    return;
  }

  // Render 3D

  Renderer3D::BeginCamera(GetMainCamera());

  GroupEntity<SkyboxComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        if (comp.skybox)
          Renderer3D::DrawSkybox(comp.skybox, GetMainCamera());
      });

  GroupEntity<LightComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        Renderer3D::RenderLight(GetWorldTransformComponents(entity).Translation, comp.Color);
      });

  GroupEntity<ModelComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        std::vector<ozz::math::Float4x4> *bones = nullptr;

        if (entity.template HasComponent<AnimatorComponent>()) {
          auto &animator = entity.template GetComponent<AnimatorComponent>();
          if (animator.HasPose) bones = &animator.FinalMatrices;
        }

        Renderer3D::DrawModel(comp.ModelData, GetWorldTransform(entity), bones,
                              (int)id);
      });

  auto CubeGroup =
      m_Registry.group<CubeComponent>(entt::get<TransformComponent>);
  for (auto entity : CubeGroup) {
    auto [transform, CubeComp] =
        CubeGroup.get<TransformComponent, CubeComponent>(entity);

    Renderer3D::DrawCube(GetWorldTransform(entity), CubeComp.Color, 1.0f, (int)entity);
  }

  GroupEntity<BoxColliderComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        if (!comp.ShowWireframe) return;
        const auto box = glm::translate(glm::mat4(1), GetWorldTransformComponents(entity).Translation) *
            glm::mat4_cast(glm::quat(GetWorldTransformComponents(entity).Rotation)) *
            glm::scale(glm::mat4(1), glm::abs(GetWorldTransformComponents(entity).Scale) * comp.HalfSize * 2.0f);
        Renderer3D::DrawWireCube(box, {0.2f, 0.9f, 0.2f}, 1.0f);
      });
  GroupEntity<SphereColliderComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        if (!comp.ShowWireframe) return;
        const auto scale = glm::abs(GetWorldTransformComponents(entity).Scale);
        const float radius = comp.Radius * std::max({scale.x, scale.y, scale.z});
        Renderer3D::DrawWireSphere(GetWorldTransformComponents(entity).Translation, glm::vec3(radius), {0.2f,0.9f,0.2f}, 1);
      });

  Renderer3D::EndCamera();

  Renderer2D::BeginCamera(GetMainCamera());
  ViewEntity<Entity, UIElement>([this](auto entity, auto &comp) {
    auto &transform = entity.template GetComponent<TransformComponent>();
    if (comp.Texture && comp.Texture->IsLoaded())
      Renderer2D::DrawQuad(GetWorldTransform(entity), comp.Texture, 1.0f, comp.Color, (int)(uint32_t)entity);
    else Renderer2D::DrawQuad(GetWorldTransform(entity), comp.Color, (int)(uint32_t)entity);
  });

  auto group1 =
      m_Registry.group<ButtonComponent>(entt::get<TransformComponent>);
  for (auto entity : group1) {

    auto [transform, ui] =
        group1.get<TransformComponent, ButtonComponent>(entity);
    if (ui.Texture && ui.Texture->IsLoaded())
      Renderer2D::DrawQuad(GetWorldTransform(entity), ui.Texture, 1.0f, ui.CurrentColor, (int)entity);
    else Renderer2D::DrawQuad(GetWorldTransform(entity), ui.CurrentColor, (int)entity);
  }

  ViewEntity<Entity, TextUIComponent>([this](auto entity, auto &comp) {
    auto &transform = entity.template GetComponent<TransformComponent>();
    if (comp.m_Font && comp.m_Font->GetTexture())
      Renderer2D::DrawText(comp.Text, GetWorldTransformComponents(entity).Translation, comp.m_Font, comp.Color);
  });

  auto Spritegroup =
      m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
  for (auto entity : Spritegroup) {
    auto [transform, sprite] =
        Spritegroup.get<TransformComponent, SpriteRendererComponent>(entity);
    if (sprite.Texture && sprite.Texture->IsLoaded())
      Renderer2D::DrawQuad(GetWorldTransform(entity), sprite.Texture, sprite.TilingFactor, sprite.Color, (int)entity);
    else
      Renderer2D::DrawQuad(GetWorldTransform(entity), sprite.Color, (int)entity);
  }

  GroupEntity<CircleComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        LF_CORE_TRACE("entity: {}, id: {}", (uint32_t)entity, (uint32_t)id);
        Renderer2D::DrawCircle(GetWorldTransformComponents(entity).Translation, comp.Radius, comp.Color,
                               entity, 1);
      });

  GroupEntity<RectangleComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        Renderer2D::DrawQuad(GetWorldTransform(entity), comp.Color, entity);
      });

  GroupEntity<LineComponent>([this](auto entity, auto &comp, auto &transform,
                                    auto id) {
    Renderer2D::DrawLine(glm::vec2(GetWorldTransform(entity)*glm::vec4(comp.p0,0,1)), glm::vec2(GetWorldTransform(entity)*glm::vec4(comp.p1,0,1)), comp.Thickness, comp.Color, entity);
  });

  // ViewEntity<Entity, SpriteRendererComponent>([this] (auto entity, auto&
  // comp){

  // 	auto& transform = entity.template GetComponent<TransformComponent>();
  // 	Renderer2D::DrawSprite(GetWorldTransform(entity), comp, (int)entity);
  // });

  // Renderer2D::DrawQuad({0, 0}, {10, 10}, {0, 1, 0, 1});

  Renderer2D::EndCamera();

  // ReadPixelEntity(mouseX, mouseY, viewportSize);

  m_Framebuffer->Unbind();

  RenderCommand::SetClearColor({0.1f, 0.1f, 0.1f, 1});
  RenderCommand::Clear();

  FlushEntityDestruction();
}
} // namespace LevyeForge