#include "Runtime/Scene.h"
#include "Core/Log.h"
#include "Runtime/Components.h"
#include "Runtime/Entity.h"
#include "Runtime/RuntimeScene.h"
#include "LF_Assert.h"
#include "entity/fwd.hpp"
#include "lfpch.h"
#include "Runtime/ScriptableEntity.h"
#include "Runtime/LuaScript.h"
#include "Math/LFMath.h"

namespace LevyeForge {

Entity GlobHovered;

template <typename Component>
static void
CopyComponent(entt::registry &dst, entt::registry &src,
              const std::unordered_map<UUID, entt::entity> &enttMap) {
  auto view = src.view<Component>();
  for (auto e : view) {
    UUID uuid = src.get<IDComponent>(e).ID;
    LF_CORE_ASSERT(enttMap.find(uuid) != enttMap.end());
    entt::entity dstEnttID = enttMap.at(uuid);

    auto &component = src.get<Component>(e);
    dst.emplace_or_replace<Component>(dstEnttID, component);
  }
}

template <typename Component>
static void CopyComponentIfExists(Entity dst, Entity src) {
  if (src.HasComponent<Component>())
    dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
}

Ref<RuntimeScene> Scene::Copy(const Ref<Scene> &other) {
  LF_CORE_INFO("[COPY]: scene={}", (const void *)&other);

  Ref<RuntimeScene> newScene = CreateRef<RuntimeScene>();

  auto &srcSceneRegistry = other->m_Registry;
  auto &dstSceneRegistry = newScene->m_Registry;
  std::unordered_map<UUID, entt::entity> enttMap;
  newScene->m_Framebuffer = other->m_Framebuffer;
  newScene->m_ViewportWidth = other->m_ViewportWidth;
  newScene->m_ViewportHeight = other->m_ViewportHeight;

  // Create entities in new scene
  auto idView = srcSceneRegistry.view<IDComponent>();
  for (auto e : idView) {
    UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
    const auto &name = srcSceneRegistry.get<TagComponent>(e).Tag;
    Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
    enttMap[uuid] = (entt::entity)newEntity;
    auto &srcT = srcSceneRegistry.get<TransformComponent>(e);
    auto &dstT =
        dstSceneRegistry.get<TransformComponent>((entt::entity)newEntity);
  }

  // Copy components (except IDComponent and TagComponent)
  CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry,
                                    enttMap);
  CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry,
                                         enttMap);
  CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<ModelComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<CubeComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry,
                                       enttMap);
  CopyComponent<CircleComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<RectangleComponent>(dstSceneRegistry, srcSceneRegistry,
                                    enttMap);
  CopyComponent<LineComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<LightComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<SkyboxComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<RigidbodyComponent>(dstSceneRegistry, srcSceneRegistry,
                                    enttMap);
  CopyComponent<BoxColliderComponent>(dstSceneRegistry, srcSceneRegistry,
                                      enttMap);

  CopyComponent<SphereColliderComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

  // Runtime handles and script instances belong to the new scene.
  dstSceneRegistry.view<RigidbodyComponent>().each([](auto &rb) {
    rb.BodyID = 0xffffffffu;
    rb.RuntimeCreated = false;
  });
  dstSceneRegistry.view<NativeScriptComponent>().each([](auto &nsc) {
    nsc.Instance = nullptr;
  });
  CopyComponent<LuaScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<AnimatorComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<UIElement>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<ButtonComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  CopyComponent<TextUIComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
  for (auto id : srcSceneRegistry.view<RelationshipComponent, IDComponent>()) {
    const auto parent = srcSceneRegistry.get<RelationshipComponent>(id).Parent;
    if (!srcSceneRegistry.valid(parent)) continue;
    const auto childCopy = enttMap.at(srcSceneRegistry.get<IDComponent>(id).ID);
    const auto parentCopy = enttMap.at(srcSceneRegistry.get<IDComponent>(parent).ID);
    dstSceneRegistry.get<RelationshipComponent>(childCopy).Parent = parentCopy;
  }
  return newScene;
}

namespace {
bool AssignTRS(const glm::mat4 &matrix, TransformComponent &output) {
  TransformComponent candidate;
  if (!Math::DecomposeTransform(matrix, candidate.Translation, candidate.Rotation, candidate.Scale))
    return false;
  const auto reconstructed = candidate.GetTransform();
  for (int column=0;column<4;++column)
    if (glm::length(matrix[column]-reconstructed[column]) > 0.001f * std::max(1.0f,glm::length(matrix[column])))
      return false; // A local TRS cannot represent shear; never silently move the object.
  output=candidate;
  return true;
}
}
glm::mat4 Scene::GetWorldTransform(entt::entity entity) const {
  glm::mat4 world(1);
  size_t remaining=m_Registry.view<TransformComponent>().size()+1;
  while (m_Registry.valid(entity) && remaining--) {
    if (auto *transform=m_Registry.try_get<TransformComponent>(entity))
      world=transform->GetTransform()*world;
    const auto *relationship=m_Registry.try_get<RelationshipComponent>(entity);
    entity=relationship?relationship->Parent:entt::null;
  }
  return world;
}
TransformComponent Scene::GetWorldTransformComponents(entt::entity entity) const {
  const auto *relationship=m_Registry.try_get<RelationshipComponent>(entity);
  if (!relationship || !m_Registry.valid(relationship->Parent))
    if (const auto *local=m_Registry.try_get<TransformComponent>(entity)) return *local;
  TransformComponent result;
  const auto world=GetWorldTransform(entity);
  if (!Math::DecomposeTransform(world,result.Translation,result.Rotation,result.Scale)) {
    result.Translation=glm::vec3(world[3]);
    // Keep collapsed axes collapsed so physics rejects degenerate shapes.
    result.Scale={glm::length(glm::vec3(world[0])),glm::length(glm::vec3(world[1])),glm::length(glm::vec3(world[2]))};
  }
  return result;
}
bool Scene::SetWorldTransform(entt::entity entity, const glm::mat4 &world) {
  if (!m_Registry.valid(entity)) return false;
  glm::mat4 local=world;
  const auto *relationship=m_Registry.try_get<RelationshipComponent>(entity);
  if (relationship && m_Registry.valid(relationship->Parent)) {
    auto parentWorld=GetWorldTransform(relationship->Parent);
    if (std::abs(glm::determinant(parentWorld)) < 1e-8f) return false;
    local=glm::inverse(parentWorld)*world;
  }
  return AssignTRS(local,m_Registry.get<TransformComponent>(entity));
}
Entity Scene::GetParent(Entity entity) {
  if (!entity || entity.GetScene()!=this) return {};
  const auto *relationship=m_Registry.try_get<RelationshipComponent>(entity);
  return relationship && m_Registry.valid(relationship->Parent) ? Entity(relationship->Parent,this) : Entity();
}
std::vector<Entity> Scene::GetChildren(Entity parent) {
  std::vector<Entity> children;
  if (parent && parent.GetScene()!=this) return children;
  const entt::entity parentID=parent?static_cast<entt::entity>(parent):entt::null;
  for (auto id : m_Registry.view<RelationshipComponent, IDComponent>()) {
    const auto actual=m_Registry.get<RelationshipComponent>(id).Parent;
    if (actual==parentID || (parentID==entt::null && !m_Registry.valid(actual)))
      children.emplace_back(id,this);
  }
  std::sort(children.begin(),children.end(),[](Entity a,Entity b) { return uint64_t(a.GetUUID())<uint64_t(b.GetUUID()); });
  return children;
}
bool Scene::IsDescendant(Entity entity, Entity ancestor) {
  if (!entity || !ancestor || entity.GetScene()!=this || ancestor.GetScene()!=this) return false;
  size_t remaining=m_Registry.view<RelationshipComponent>().size()+1;
  while (entity && remaining--) {
    if (entity==ancestor) return true;
    entity=GetParent(entity);
  }
  return false;
}
bool Scene::SetParent(Entity child, Entity parent, bool preserveWorld) {
  m_HierarchyError.clear();
  if (!child || child.GetScene()!=this || (parent && parent.GetScene()!=this)) {
    m_HierarchyError="Entities must belong to the same scene.";
    return false;
  }
  if (parent && IsDescendant(parent,child)) {
    m_HierarchyError="An entity cannot be parented to itself or one of its descendants.";
    return false;
  }
  auto &relationship=m_Registry.get<RelationshipComponent>(child);
  const entt::entity newParent=parent?static_cast<entt::entity>(parent):entt::null;
  if (relationship.Parent==newParent) return true;
  auto local=child.GetComponent<TransformComponent>();
  if (preserveWorld) {
    const auto parentWorld=parent?GetWorldTransform(parent):glm::mat4(1);
    if (std::abs(glm::determinant(parentWorld))<1e-8f ||
        !AssignTRS(glm::inverse(parentWorld)*GetWorldTransform(child),local)) {
      m_HierarchyError="Cannot preserve this transform under the chosen parent. Avoid zero or nonuniform parent scale combined with rotation.";
      return false;
    }
  }
  relationship.Parent=newParent;
  child.GetComponent<TransformComponent>()=local;
  return true;
}
bool Scene::Unparent(Entity child) { return SetParent(child,Entity(),true); }

Entity Scene::CreateEntity(const std::string &name) {
  return CreateEntityWithUUID(UUID(), name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string &name) {
  Entity entity = {m_Registry.create(), this};
  entity.AddComponent<IDComponent>(uuid);
  entity.AddComponent<TransformComponent>();
  entity.AddComponent<RelationshipComponent>();
  auto &tag = entity.AddComponent<TagComponent>();
  tag.Tag = name.empty() ? "Entity" : name;
  return entity;
}

void Scene::DestroyScriptInstance(Entity entity) {
  auto *script = m_Registry.try_get<NativeScriptComponent>(entity);
  if (!script || !script->Instance) return;
  auto *instance = script->Instance;
  script->Instance = nullptr;
  try { instance->OnDestroy(); }
  catch (const std::exception &e) { LF_CORE_ERROR("Native OnDestroy: {}", e.what()); }
  catch (...) { LF_CORE_ERROR("Native OnDestroy failed"); }
  delete instance;
}

void Scene::DestroyLuaScriptInstance(Entity entity) {
  auto *script = m_Registry.try_get<LuaScriptComponent>(entity);
  if (!script || !script->Instance) return;
  auto instance = std::move(script->Instance);
  instance->Stop();
}

void Scene::DestroyEntityNow(Entity entity) {
  if (!m_Registry.valid((entt::entity)entity))
    return;
  // Deleting a hierarchy node deletes its descendants and their runtime state.
  for (auto child : GetChildren(entity)) if (child) DestroyEntityNow(child);
  DestroyScriptInstance(entity);
  DestroyLuaScriptInstance(entity);
  if (auto *runtime = dynamic_cast<RuntimeScene *>(this))
    Physics::DestroyBody(entity, runtime);
  m_Registry.destroy(entity);
}

void Scene::DestroyEntity(Entity entity) {
  // Defer destruction until the current update finishes.
  m_DestroyQueue.push_back((entt::entity)entity);
}

void Scene::ReadPixelEntity(int &mouseX, int &mouseY, glm::vec2 &viewportSize) {
  if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x &&
      mouseY < (int)viewportSize.y) {
    int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
    if (pixelData != -1 && !m_Registry.valid(static_cast<entt::entity>(pixelData)))
      pixelData = -1;
    GlobHovered =
        pixelData == -1 ? Entity() : Entity((entt::entity)pixelData, this);
  }
}

void Scene::OnViewportResize(uint32_t width, uint32_t height) {
  LF_PROFILE_FUNCTION();
  m_ViewportWidth = width;
  m_ViewportHeight = height;

  if (width == 0 || height == 0)
    return;
  if (m_Framebuffer)
    m_Framebuffer->Resize(width, height);

  // Resize our non-FixedAspectRatio cameras
  auto view = m_Registry.view<CameraComponent>();
  for (auto entity : view) {
    auto &cameraComponent = view.get<CameraComponent>(entity);
    if (!cameraComponent.FixedAspectRatio)
      cameraComponent.Camera.SetViewportSize(width, height);
  }
}

void Scene::OnMouseInput(float mouseX, float mouseY, bool mousePressed,
                         Timestep ts) {
  auto group1 =
      m_Registry.group<ButtonComponent>(entt::get<TransformComponent>);
  for (auto entity : group1) {

    auto [transform, button] =
        group1.get<TransformComponent, ButtonComponent>(entity);

    const auto world=GetWorldTransform(entity);
    bool hovered=false;
    if (std::abs(glm::determinant(world))>1e-8f) {
      auto local=glm::inverse(world)*glm::vec4(mouseX,mouseY,world[3].z,1);
      hovered=std::abs(local.x)<=0.5f && std::abs(local.y)<=0.5f;
    }

    button.Hovered = hovered;

    if (hovered && mousePressed && !button.ClickedLastFrame) {
      if (button.OnClick)
        button.OnClick();
      button.ClickedLastFrame = true;
    } else if (!mousePressed) {
      button.ClickedLastFrame = false;
    }

    // Animate scale
    if (button.ClickedLastFrame)
      button.TargetScale = button.OriginalScale * 0.95f;
    else if (button.Hovered)
      button.TargetScale = button.OriginalScale * 1.05f;
    else
      button.TargetScale = button.OriginalScale;

    transform.Scale = glm::mix(transform.Scale, button.TargetScale, 0.2f);

    button.BaseColor = button.Color;
    button.CurrentColor = button.Color;

    // Animate Color
    glm::vec4 hoverColor = button.BaseColor * 1.2f;
    hoverColor.a = button.BaseColor.a; // preserve alpha

    button.CurrentColor =
        glm::mix(button.CurrentColor,
                 button.Hovered ? hoverColor : button.BaseColor, 0.2f);
    button.Color = button.CurrentColor;
  }

  m_MouseX = mouseX;
  m_MouseY = mouseY;
}

Entity Scene::GetHoveredEntity() {

  if (GlobHovered)
    return GlobHovered;
  else
    return Entity();
}

void Scene::FlushEntityDestruction() {
  if (m_DestroyQueue.empty())
    return;
  auto pending = std::move(m_DestroyQueue);
  m_DestroyQueue.clear();
  for (auto e : pending)
    if (m_Registry.valid(e))
      DestroyEntityNow(Entity(e, this));

}

Entity Scene::DuplicateEntity(Entity entity) {
  const auto children = GetChildren(entity);
  std::string name = entity.GetName();
  Entity newEntity = CreateEntity(name);

  CopyComponentIfExists<TransformComponent>(newEntity, entity);
  CopyComponentIfExists<SpriteRendererComponent>(newEntity, entity);
  CopyComponentIfExists<CameraComponent>(newEntity, entity);
  CopyComponentIfExists<NativeScriptComponent>(newEntity, entity);
  CopyComponentIfExists<LuaScriptComponent>(newEntity, entity);
  CopyComponentIfExists<ModelComponent>(newEntity, entity);
  CopyComponentIfExists<CubeComponent>(newEntity, entity);
  CopyComponentIfExists<LightComponent>(newEntity, entity);
  CopyComponentIfExists<AnimatorComponent>(newEntity, entity);
  CopyComponentIfExists<SkyboxComponent>(newEntity, entity);
  CopyComponentIfExists<CircleComponent>(newEntity, entity);
  CopyComponentIfExists<RectangleComponent>(newEntity, entity);
  CopyComponentIfExists<LineComponent>(newEntity, entity);
  CopyComponentIfExists<UIElement>(newEntity, entity);
  CopyComponentIfExists<ButtonComponent>(newEntity, entity);
  CopyComponentIfExists<TextUIComponent>(newEntity, entity);
  CopyComponentIfExists<BoxColliderComponent>(newEntity, entity);
  CopyComponentIfExists<SphereColliderComponent>(newEntity, entity);
  CopyComponentIfExists<RigidbodyComponent>(newEntity, entity);
  if (newEntity.HasComponent<RigidbodyComponent>()) {
    auto &rb = newEntity.GetComponent<RigidbodyComponent>();
    rb.BodyID = 0xffffffffu;
    rb.RuntimeCreated = false;
  }  SetParent(newEntity, GetParent(entity), false);
  for (auto child : children) {
    auto childCopy = DuplicateEntity(child);
    SetParent(childCopy, newEntity, false);
  }
  return newEntity;
}

template <typename T>
void Scene::OnComponentAdded(Entity entity, T &component) {
}

template <>
void Scene::OnComponentAdded<IDComponent>(Entity entity,
                                          IDComponent &component) {}

template <>
void Scene::OnComponentAdded<TransformComponent>(
    Entity entity, TransformComponent &component) {}

template <>
void Scene::OnComponentAdded<SpriteRendererComponent>(
    Entity entity, SpriteRendererComponent &component) {}

template <>
void Scene::OnComponentAdded<UIElement>(Entity entity, UIElement &component) {}

template <>
void Scene::OnComponentAdded<ButtonComponent>(Entity entity,
                                              ButtonComponent &component) {}

template <>
void Scene::OnComponentAdded<TextUIComponent>(Entity entity,
                                              TextUIComponent &component) {}

template <>
void Scene::OnComponentAdded<TagComponent>(Entity entity,
                                           TagComponent &component) {}

template <>
void Scene::OnComponentAdded<ModelComponent>(Entity entity,
                                             ModelComponent &component) {}

template <>
void Scene::OnComponentAdded<CubeComponent>(Entity entity,
                                            CubeComponent &component) {}

template <>
void Scene::OnComponentAdded<LightComponent>(Entity entity,
                                             LightComponent &component) {}

template <>
void Scene::OnComponentAdded<NativeScriptComponent>(
    Entity entity, NativeScriptComponent &component) {}

template <>
void Scene::OnComponentAdded<CameraComponent>(Entity entity,
                                              CameraComponent &component) {
  if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
    component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
}

template <>
void Scene::OnComponentAdded<SkyboxComponent>(Entity entity,
                                              SkyboxComponent &component) {}

template <>
void Scene::OnComponentAdded<CircleComponent>(Entity entity,
                                              CircleComponent &component) {}

template <>
void Scene::OnComponentAdded<RectangleComponent>(
    Entity entity, RectangleComponent &component) {}

template <>
void Scene::OnComponentAdded<LineComponent>(Entity entity,
                                            LineComponent &component) {}

template <>
void Scene::OnComponentAdded<AnimatorComponent>(Entity entity,
                                                AnimatorComponent &component) {}

template <>
void Scene::OnComponentAdded<BoxColliderComponent>(
    Entity entity, BoxColliderComponent &component) {}

template <>
void Scene::OnComponentAdded<RigidbodyComponent>(
    Entity entity, RigidbodyComponent &component) {}
} // namespace LevyeForge

namespace LevyeForge {
template <>
void Scene::OnComponentAdded<SphereColliderComponent>(Entity entity, SphereColliderComponent &component) {}
}

namespace LevyeForge {
template <> void Scene::OnComponentAdded<LuaScriptComponent>(Entity, LuaScriptComponent &) {}
}

namespace LevyeForge {
template <> void Scene::OnComponentAdded<RelationshipComponent>(Entity, RelationshipComponent &) {}
}
