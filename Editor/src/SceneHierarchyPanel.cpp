#include "Panels/SceneHierarchyPanel.h"
#include "Runtime/Components.h"
#include "lfpch.h"
#include "Runtime/NativeScriptRegistry.h"
#include "Runtime/LuaScript.h"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <stb_image.h>
#include "Runtime/ScriptableEntity.h"

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
#define _CRT_SECURE_NO_WARNINGS
#endif

namespace LevyeForge {

static bool DrawVec3Control(const char *label, glm::vec3 &values,
                            float resetValue = 0.0f,
                            float columnWidth = 100.0f) {
  bool changed = false;

  ImGui::PushID(label);
  ImGui::PushID(&values);

  ImGui::Columns(2);
  ImGui::SetColumnWidth(0, columnWidth);
  ImGui::TextUnformatted(label);
  ImGui::NextColumn();

  ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

  const float lineHeight =
      ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2.0f;
  const ImVec2 btn = {lineHeight + 3.0f, lineHeight};

  auto axis = [&](const char *axisText, float &v, const ImVec4 &col,
                  const char *dragID) {
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImVec4{col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, col.w});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    if (ImGui::Button(axisText, btn)) {
      v = resetValue;
      changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    changed |= ImGui::DragFloat(dragID, &v, 0.1f);
    ImGui::PopItemWidth();
    ImGui::SameLine();
  };

  axis("X", values.x, ImVec4{0.8f, 0.1f, 0.15f, 1.0f}, "##X");
  axis("Y", values.y, ImVec4{0.2f, 0.7f, 0.2f, 1.0f}, "##Y");
  axis("Z", values.z, ImVec4{0.1f, 0.25f, 0.8f, 1.0f}, "##Z");

  ImGui::PopStyleVar();
  ImGui::Columns(1);

  ImGui::PopID();
  ImGui::PopID();
  return changed;
}


template <typename T, typename UIFunc>
static void DrawComponent(const char *name, Entity entity, UIFunc uiFn,
                          bool removable = true) {
  if (!entity.HasComponent<T>())
    return;

  auto &comp = entity.GetComponent<T>();

  ImGui::PushID(name);
  bool open = false;
  bool removeComponent = false;
  if (ImGui::BeginTable("ComponentHeader", removable ? 2 : 1,
                        ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    if (removable)
      ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,
          ImGui::CalcTextSize("...").x + ImGui::GetStyle().FramePadding.x * 2);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    open = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_NoTreePushOnOpen);
    if (removable) {
      ImGui::TableNextColumn();
      if (ImGui::SmallButton("...")) ImGui::OpenPopup("ComponentSettings");
      if (ImGui::BeginPopup("ComponentSettings")) {
        removeComponent = ImGui::MenuItem("Remove Component");
        ImGui::EndPopup();
      }
    }
    ImGui::EndTable();
  }
  if (open && !removeComponent) uiFn(entity, comp);

  if (removeComponent) {
    if constexpr (std::is_same_v<T, ModelComponent>) {
      if (entity.HasComponent<AnimatorComponent>()) entity.RemoveComponent<AnimatorComponent>();
    }
    entity.RemoveComponent<T>();
  }
  ImGui::PopID();
}

// Editable paths also accept files dragged from the content browser.
static void AssetField(const char *label, const std::string &current,
                       const std::function<bool(const std::string &)> &load) {
  ImGui::PushID(label);
  static std::unordered_map<ImGuiID, std::string> drafts;
  static std::unordered_map<ImGuiID, bool> failures;
  const ImGuiID id = ImGui::GetID("Path");
  auto [it, inserted] = drafts.try_emplace(id, current);
  char path[2048]{};
  std::strncpy(path, it->second.c_str(), sizeof(path) - 1);
  if (ImGui::InputText(label, path, sizeof(path))) it->second = path;
  bool apply = false;
  if (ImGui::BeginDragDropTarget()) {
    if (const auto *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      std::filesystem::path dropped(static_cast<const char *>(payload->Data));
      it->second = (dropped.is_absolute() ? dropped : std::filesystem::path("Resources") / dropped).string();
      apply = true;
    }
    ImGui::EndDragDropTarget();
  }
  if (ImGui::Button("Load")) apply = true;
  if (apply) failures[id] = it->second.empty() || !std::filesystem::is_regular_file(it->second) || !load(it->second);
  if (failures[id]) ImGui::TextWrapped("Could not load this asset. Check the file and its format.");
  ImGui::PopID();
}

static void TextureField(Ref<Texture2D> &texture) {
  AssetField("Texture", texture ? texture->GetPath() : "", [&](const std::string &path) {
    auto candidate = Texture2D::Create(path);
    if (!candidate->IsLoaded()) return false;
    texture = candidate;
    return true;
  });
  if (texture) {
    ImGui::Image((ImTextureID)texture->GetRendererID(), ImVec2(64,64), ImVec2(0,1), ImVec2(1,0));
    if (ImGui::Button("Clear texture")) texture.reset();
  }
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
  m_SelectionContext = entity;
}
SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene> &context) {
  SetContext(context);
}

void SceneHierarchyPanel::SetContext(const Ref<Scene> &context) {
  m_Context = context;
  m_SelectionContext = {};
  m_HierarchyActions.clear();
  m_ExpandEntity = {};
  m_HierarchyError.clear();
}

namespace {
struct HierarchyPayload { Scene *SceneContext; entt::entity EntityID; };
constexpr const char *HierarchyPayloadType = "LF_SCENE_ENTITY";
}
void SceneHierarchyPanel::AcceptEntityDrop(Entity parent) {
  if (!ImGui::BeginDragDropTarget()) return;
  if (const auto *payload=ImGui::AcceptDragDropPayload(HierarchyPayloadType)) {
    if (payload->DataSize==sizeof(HierarchyPayload)) {
      const auto data=*static_cast<const HierarchyPayload *>(payload->Data);
      if (data.SceneContext==m_Context.get()) {
        Entity child(data.EntityID,m_Context.get());
        m_HierarchyActions.push_back([this,child,parent]() {
          if (!child) return;
          if (!m_Context->SetParent(child,parent,true))
            m_HierarchyError=m_Context->GetHierarchyError();
          else {
            m_HierarchyError.clear();
            m_SelectionContext=child;
            m_ExpandEntity=parent;
          }
        });
      }
    }
  }
  ImGui::EndDragDropTarget();
}
void SceneHierarchyPanel::OnImGuiRender() {
  if (!m_Context) return;
  ImGui::Begin("Scene Hierarchy");
  ImGui::Selectable("Scene Root  (drop here to unparent)",false);
  AcceptEntityDrop({});
  ImGui::Separator();
  for (auto entity : m_Context->GetChildren({})) DrawEntityNode(entity);
  if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
    m_SelectionContext={};
  if (ImGui::BeginPopupContextWindow(0,ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
    if (ImGui::MenuItem("Create Empty"))
      m_HierarchyActions.push_back([this]() { m_SelectionContext=m_Context->CreateEntity("Empty"); });
    ImGui::EndPopup();
  }
  if (!m_HierarchyError.empty()) {
    ImGui::TextWrapped("%s",m_HierarchyError.c_str());
    if (ImGui::SmallButton("Dismiss")) m_HierarchyError.clear();
  }
  ImGui::End();
  // Apply changes after traversing the tree; no invalidated references or duplicate rows.
  auto actions=std::move(m_HierarchyActions);
  m_HierarchyActions.clear();
  for (auto &action : actions) action();
  ImGui::Begin("Properties");
  if (m_SelectionContext) DrawComponents(m_SelectionContext);
  ImGui::End();
}
void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
  if (!entity) return;
  const auto children=m_Context->GetChildren(entity);
  auto flags=ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
  if (m_SelectionContext==entity) flags|=ImGuiTreeNodeFlags_Selected;
  if (children.empty()) flags|=ImGuiTreeNodeFlags_Leaf;
  if (m_ExpandEntity==entity) { ImGui::SetNextItemOpen(true); m_ExpandEntity={}; }
  const bool opened=ImGui::TreeNodeEx((void *)(uintptr_t)(uint32_t)entity,flags,"%s",entity.GetName().c_str());
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) m_SelectionContext=entity;
  if (ImGui::BeginDragDropSource()) {
    HierarchyPayload payload{m_Context.get(),static_cast<entt::entity>(entity)};
    ImGui::SetDragDropPayload(HierarchyPayloadType,&payload,sizeof(payload));
    ImGui::Text("Parent %s under another entity",entity.GetName().c_str());
    ImGui::EndDragDropSource();
  }
  AcceptEntityDrop(entity);
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Create Child"))
      m_HierarchyActions.push_back([this,entity]() {
        auto child=m_Context->CreateEntity("Empty");
        m_Context->SetParent(child,entity,false);
        m_SelectionContext=child; m_ExpandEntity=entity;
      });
    if (ImGui::MenuItem("Move to Scene Root",nullptr,false,bool(m_Context->GetParent(entity))))
      m_HierarchyActions.push_back([this,entity]() {
        if (!m_Context->Unparent(entity)) m_HierarchyError=m_Context->GetHierarchyError();
        else m_HierarchyError.clear();
      });
    if (ImGui::MenuItem("Duplicate"))
      m_HierarchyActions.push_back([this,entity]() { m_SelectionContext=m_Context->DuplicateEntity(entity); });
    if (ImGui::MenuItem(children.empty()?"Delete":"Delete Entity and Children"))
      m_HierarchyActions.push_back([this,entity]() {
        if (m_Context->IsDescendant(m_SelectionContext,entity)) m_SelectionContext={};
        m_Context->DestroyEntity(entity);
      });
    ImGui::EndPopup();
  }
  if (opened) {
    for (auto child : children) DrawEntityNode(child);
    ImGui::TreePop();
  }
}

static void DrawAddComponentPopup(Entity entity) {
  if (ImGui::BeginPopup("AddComponentPopup")) {
    if (!entity.HasComponent<CameraComponent>()) {
      if (ImGui::MenuItem("Camera")) {
        entity.AddComponent<CameraComponent>();
        ImGui::CloseCurrentPopup();
      }
    }

    if (!entity.HasComponent<CubeComponent>() && ImGui::MenuItem("Cube"))
      entity.AddComponent<CubeComponent>();
    if (!entity.HasComponent<RigidbodyComponent>() && ImGui::MenuItem("Rigid Body"))
      entity.AddComponent<RigidbodyComponent>();
    if (!entity.HasComponent<BoxColliderComponent>() && !entity.HasComponent<SphereColliderComponent>() && ImGui::MenuItem("Box Collider"))
      entity.AddComponent<BoxColliderComponent>();
    if (!entity.HasComponent<BoxColliderComponent>() && !entity.HasComponent<SphereColliderComponent>() && ImGui::MenuItem("Sphere Collider"))
      entity.AddComponent<SphereColliderComponent>();

    if (!entity.HasComponent<ModelComponent>() && ImGui::MenuItem("Model")) entity.AddComponent<ModelComponent>();
    if (!entity.HasComponent<AnimatorComponent>() && ImGui::MenuItem("Animator")) {
      if (!entity.HasComponent<ModelComponent>()) entity.AddComponent<ModelComponent>();
      entity.AddComponent<AnimatorComponent>().InitFromModel(entity.GetComponent<ModelComponent>().ModelData);
    }
    if (!entity.HasComponent<SpriteRendererComponent>() && ImGui::MenuItem("Sprite Renderer")) entity.AddComponent<SpriteRendererComponent>();
    if (!entity.HasComponent<LightComponent>() && ImGui::MenuItem("Light")) entity.AddComponent<LightComponent>();
    if (!entity.HasComponent<SkyboxComponent>() && ImGui::MenuItem("Skybox")) entity.AddComponent<SkyboxComponent>();
    if (!entity.HasComponent<CircleComponent>() && ImGui::MenuItem("Circle")) entity.AddComponent<CircleComponent>();
    if (!entity.HasComponent<RectangleComponent>() && ImGui::MenuItem("Rectangle")) entity.AddComponent<RectangleComponent>();
    if (!entity.HasComponent<LineComponent>() && ImGui::MenuItem("Line")) entity.AddComponent<LineComponent>();
    if (!entity.HasComponent<UIElement>() && ImGui::MenuItem("UI Image")) entity.AddComponent<UIElement>();
    if (!entity.HasComponent<ButtonComponent>() && ImGui::MenuItem("Button")) entity.AddComponent<ButtonComponent>();
    if (!entity.HasComponent<TextUIComponent>() && ImGui::MenuItem("Text")) entity.AddComponent<TextUIComponent>();
    if (!entity.HasComponent<LuaScriptComponent>() && ImGui::MenuItem("Lua Script")) entity.AddComponent<LuaScriptComponent>();
    if (!entity.HasComponent<NativeScriptComponent>() && ImGui::MenuItem("Native Script")) entity.AddComponent<NativeScriptComponent>();

    ImGui::EndPopup();
  }
}

void SceneHierarchyPanel::DrawComponents(Entity entity) {
  ImGui::PushID(std::to_string((uint64_t)entity.GetUUID()).c_str());
  // Tag
  if (entity.HasComponent<TagComponent>()) {
    auto &tag = entity.GetComponent<TagComponent>().Tag;
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
    if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
      tag = std::string(buffer);
  }

  // Add Component button
  ImGui::SameLine();
  if (ImGui::Button("Add Component"))
    ImGui::OpenPopup("AddComponentPopup");
  DrawAddComponentPopup(entity);

  if (auto parent=m_Context->GetParent(entity))
    ImGui::TextWrapped("Parent: %s (transform values are local)",parent.GetName().c_str());

  // Transform
  DrawComponent<TransformComponent>(
      "Transform", entity,
      [](Entity, TransformComponent &tc) {
        DrawVec3Control("Translation", tc.Translation);
        glm::vec3 degrees = glm::degrees(tc.Rotation);
        if (DrawVec3Control("Rotation", degrees))
          tc.Rotation = glm::radians(degrees);
        DrawVec3Control("Scale", tc.Scale, 1.0f);
      },
      /*removable*/ false);

  // Camera
  DrawComponent<CameraComponent>(
      "Camera", entity, [this](Entity entity, CameraComponent &cc) {
        if (ImGui::Checkbox("Primary", &cc.Primary) && cc.Primary) {
          auto cameras = m_Context->GetRegistry().view<CameraComponent>();
          for (auto other : cameras)
            if (other != (entt::entity)entity) cameras.get<CameraComponent>(other).Primary = false;
        }
        ImGui::Checkbox("Show Camera Frustum", &cc.ShowFrustum);
        ImGui::Checkbox("Fixed Aspect", &cc.FixedAspectRatio);
        auto &camera = cc.Camera;
        int projection = static_cast<int>(camera.GetProjectionType());
        if (ImGui::Combo("Projection", &projection, "Perspective\0Orthographic\0"))
          camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(projection));
        if (projection == 0) {
          float fov = glm::degrees(camera.GetPerspectiveVerticalFOV());
          float nearClip = camera.GetPerspectiveNearClip(), farClip = camera.GetPerspectiveFarClip();
          if (ImGui::SliderFloat("Field of View", &fov, 1, 179)) camera.SetPerspectiveVerticalFOV(glm::radians(fov));
          if (ImGui::DragFloat("Near", &nearClip, 0.01f, 0.001f, farClip - 0.001f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) camera.SetPerspectiveNearClip(nearClip);
          if (ImGui::DragFloat("Far", &farClip, 1, nearClip + 0.001f, 100000, "%.3f", ImGuiSliderFlags_AlwaysClamp)) camera.SetPerspectiveFarClip(farClip);
        } else {
          float size = camera.GetOrthographicSize(), nearClip = camera.GetOrthographicNearClip(), farClip = camera.GetOrthographicFarClip();
          if (ImGui::DragFloat("Size", &size, 0.1f, 0.001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp)) camera.SetOrthographicSize(size);
          if (ImGui::DragFloat("Near", &nearClip, 0.1f, -100000, farClip - 0.001f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) camera.SetOrthographicNearClip(nearClip);
          if (ImGui::DragFloat("Far", &farClip, 0.1f, nearClip + 0.001f, 100000, "%.3f", ImGuiSliderFlags_AlwaysClamp)) camera.SetOrthographicFarClip(farClip);
        }
      });

  DrawComponent<CubeComponent>("Cube", entity, [](Entity, CubeComponent &cube) {
    ImGui::ColorEdit3("Color", glm::value_ptr(cube.Color));
  });
  DrawComponent<RigidbodyComponent>("Rigid Body", entity, [](Entity, RigidbodyComponent &rb) {
    ImGui::BeginDisabled(rb.RuntimeCreated);
    int type = static_cast<int>(rb.Type);
    if (ImGui::Combo("Body Type", &type, "Static\0Dynamic\0Kinematic\0"))
      rb.Type = static_cast<RigidbodyComponent::BodyType>(type);
    ImGui::DragFloat("Mass", &rb.Mass, 0.1f, 0.001f, 100000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Linear Damping", &rb.LinearDamping, 0, 1);
    ImGui::SliderFloat("Angular Damping", &rb.AngularDamping, 0, 1);
    ImGui::Checkbox("Use Gravity", &rb.UseGravity);
    ImGui::Checkbox("Lock Rotation", &rb.LockRotation);
    ImGui::DragFloat3("Initial Velocity", glm::value_ptr(rb.LinearVelocity), 0.1f);
    ImGui::EndDisabled();
  });
  DrawComponent<BoxColliderComponent>("Box Collider", entity, [](Entity e, BoxColliderComponent &box) {
    ImGui::Checkbox("Show Collider Wireframe", &box.ShowWireframe);
    const bool running = e.HasComponent<RigidbodyComponent>() && e.GetComponent<RigidbodyComponent>().RuntimeCreated;
    ImGui::BeginDisabled(running);
    ImGui::DragFloat3("Half Size", glm::value_ptr(box.HalfSize), 0.05f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Friction", &box.Friction, 0, 1);
    ImGui::SliderFloat("Restitution", &box.Restitution, 0, 1);
    ImGui::Checkbox("Trigger", &box.IsTrigger);
    ImGui::EndDisabled();
  });

  DrawComponent<SphereColliderComponent>("Sphere Collider", entity, [](Entity e, SphereColliderComponent &sphere) {
    ImGui::Checkbox("Show Collider Wireframe", &sphere.ShowWireframe);
    const bool running = e.HasComponent<RigidbodyComponent>() && e.GetComponent<RigidbodyComponent>().RuntimeCreated;
    ImGui::BeginDisabled(running);
    ImGui::DragFloat("Radius", &sphere.Radius, 0.05f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("Friction", &sphere.Friction, 0, 1);
    ImGui::SliderFloat("Restitution", &sphere.Restitution, 0, 1);
    ImGui::Checkbox("Trigger", &sphere.IsTrigger);
    ImGui::EndDisabled();
  });

  DrawComponent<ModelComponent>("Model", entity, [](Entity entity, ModelComponent &model) {
    AssetField("Model file", model.ModelData ? model.ModelData->m_Path : "", [&](const std::string &path) {
      auto candidate = CreateRef<Model>(path);
      if (!candidate->IsLoaded()) return false;
      model.ModelData = candidate;
      if (entity.HasComponent<AnimatorComponent>()) entity.GetComponent<AnimatorComponent>().InitFromModel(candidate);
      return true;
    });
    if (model.ModelData) ImGui::Text("Meshes: %d  Joints: %d", model.ModelData->GetMeshCount(), model.ModelData->GetSkeleton()->num_joints());
  });
  DrawComponent<AnimatorComponent>("Animator", entity, [](Entity entity, AnimatorComponent &anim) {
    auto model = entity.HasComponent<ModelComponent>() ? entity.GetComponent<ModelComponent>().ModelData : nullptr;
    if (model != anim.ModelRef) anim.InitFromModel(model);
    if (!model) { ImGui::TextWrapped("Load a model in the Model component first."); return; }
    int clipIndex = anim.CurrentAnimation.GetClipIndex();
    ImGui::PushID("ClipIndex");
    static std::unordered_map<ImGuiID, int> indices;
    auto it = indices.try_emplace(ImGui::GetID("index"), clipIndex).first;
    ImGui::InputInt("Clip index", &it->second);
    it->second = std::max(0, it->second);
    AssetField("Animation file", anim.CurrentAnimation.GetPath(), [&](const std::string &path) {
      return anim.Play(Animation(path, model->GetSkeleton(), it->second));
    });
    ImGui::PopID();
    ImGui::Checkbox("Show Skeleton", &anim.ShowSkeleton);
    ImGui::Checkbox("Loop", &anim.Loop);
    ImGui::SliderFloat("Speed", &anim.Speed, 0.0f, 4.0f, "%.2fx");
    if (auto *clip = anim.CurrentAnimation.Get()) {
      if (ImGui::Button(anim.Playing ? "Pause" : "Play")) {
        if (!anim.Playing && anim.Time >= clip->duration()) anim.Time = 0;
        anim.Playing = !anim.Playing;
      }
      ImGui::SameLine();
      if (ImGui::Button("Restart")) { anim.Time = 0; anim.Playing = true; }
      if (ImGui::SliderFloat("Time", &anim.Time, 0, clip->duration(), "%.3f s")) anim.Playing = false;
    }
  });
  DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](Entity, SpriteRendererComponent &sprite) {
    ImGui::ColorEdit4("Tint", glm::value_ptr(sprite.Color));
    ImGui::DragFloat("Tiling", &sprite.TilingFactor, 0.1f, 0.001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    TextureField(sprite.Texture);
  });
  DrawComponent<LightComponent>("Light", entity, [](Entity, LightComponent &light) { ImGui::ColorEdit4("Color", glm::value_ptr(light.Color)); });
  DrawComponent<SkyboxComponent>("Skybox", entity, [](Entity, SkyboxComponent &sky) {
    const char *labels[] = {"Right (+X)", "Left (-X)", "Top (+Y)", "Bottom (-Y)", "Front (+Z)", "Back (-Z)"};
    for (int i = 0; i < 6; ++i) {
      char path[2048]{};
      std::strncpy(path, sky.Faces[i].c_str(), sizeof(path) - 1);
      if (ImGui::InputText(labels[i], path, sizeof(path))) sky.Faces[i] = path;
    }
    if (ImGui::Button("Load skybox")) {
      int size = 0;
      bool valid = true;
      for (const auto &path : sky.Faces) {
        int width = 0, height = 0, channels = 0;
        if (!stbi_info(path.c_str(), &width, &height, &channels) || width != height || (size && size != width)) valid = false;
        size = width;
      }
      if (valid) sky.skybox = Skybox::Create(std::vector<std::string>(sky.Faces.begin(), sky.Faces.end()));
      else ImGui::OpenPopup("Skybox error");
    }
    if (ImGui::BeginPopup("Skybox error")) { ImGui::TextWrapped("Use six square images of the same size."); ImGui::EndPopup(); }
  });
  DrawComponent<CircleComponent>("Circle", entity, [](Entity, CircleComponent &circle) {
    ImGui::ColorEdit4("Color", glm::value_ptr(circle.Color));
    ImGui::DragFloat("Radius", &circle.Radius, 0.01f, 0.001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
  });
  DrawComponent<RectangleComponent>("Rectangle", entity, [](Entity, RectangleComponent &rectangle) { ImGui::ColorEdit4("Color", glm::value_ptr(rectangle.Color)); });
  DrawComponent<LineComponent>("Line", entity, [](Entity, LineComponent &line) {
    ImGui::ColorEdit4("Color", glm::value_ptr(line.Color));
    ImGui::DragFloat2("Start", glm::value_ptr(line.p0), 0.05f);
    ImGui::DragFloat2("End", glm::value_ptr(line.p1), 0.05f);
    ImGui::DragFloat("Thickness", &line.Thickness, 0.01f, 0.001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
  });
  DrawComponent<UIElement>("UI Image", entity, [](Entity, UIElement &ui) {
    ImGui::ColorEdit4("Tint", glm::value_ptr(ui.Color));
    TextureField(ui.Texture);
  });
  DrawComponent<ButtonComponent>("Button", entity, [](Entity, ButtonComponent &button) {
    if (ImGui::ColorEdit4("Color", glm::value_ptr(button.BaseColor))) button.Color = button.CurrentColor = button.BaseColor;
    TextureField(button.Texture);
    ImGui::TextUnformatted(button.OnClick ? "Click action is bound" : "Bind a click action from a C++ script.");
  });
  DrawComponent<TextUIComponent>("Text", entity, [](Entity, TextUIComponent &text) {
    char buffer[4096]{};
    std::strncpy(buffer, text.Text.c_str(), sizeof(buffer) - 1);
    if (ImGui::InputTextMultiline("Text", buffer, sizeof(buffer))) text.Text = buffer;
    ImGui::ColorEdit4("Color", glm::value_ptr(text.Color));
    AssetField("Font", text.m_Font ? text.m_Font->GetPath() : "", [&](const std::string &path) {
      auto candidate = CreateRef<Font>(path);
      if (!candidate->GetTexture()) return false;
      text.m_Font = candidate;
      return true;
    });
  });

  DrawComponent<NativeScriptComponent>("Native Script", entity,
      [](Entity e, NativeScriptComponent &script) {
        if (ImGui::Checkbox("Enabled", &script.Enabled)) {
          e.GetScene()->DestroyScriptInstance(e);
          script.Error.clear();
        }
        if (ImGui::BeginCombo("Class", script.ClassName.empty() ? "Select script..." : script.ClassName.c_str())) {
          if (ImGui::Selectable("None", script.ClassName.empty())) {
            e.GetScene()->DestroyScriptInstance(e);
            NativeScriptRegistry::Bind(script, "");
          }
          for (const auto &[name, factory] : NativeScriptRegistry::Types()) {
            if (ImGui::Selectable(name.c_str(), script.ClassName == name)) {
              e.GetScene()->DestroyScriptInstance(e);
              NativeScriptRegistry::Bind(script, name);
            }
          }
          ImGui::EndCombo();
        }
        ImGui::TextUnformatted(script.Instance ? "Running" : "Runs in Play mode");
        if (!script.Error.empty()) ImGui::TextWrapped("%s", script.Error.c_str());
        if (ImGui::Button("Restart Script")) {
          e.GetScene()->DestroyScriptInstance(e);
          script.Error.clear();
        }
        if (script.Instance) {
          try { script.Instance->OnImGuiRender(); }
          catch (const std::exception &error) { script.Error = error.what(); }
          catch (...) { script.Error = "Native inspector callback failed"; }
        }
      });
  DrawComponent<LuaScriptComponent>("Lua Script", entity,
      [](Entity e, LuaScriptComponent &script) {
        if (ImGui::Checkbox("Enabled", &script.Enabled)) {
          e.GetScene()->DestroyLuaScriptInstance(e);
          script.Error.clear();
        }
        AssetField("Script file", script.Path, [&](const std::string &path) {
          if (std::filesystem::path(path).extension() != ".lua" || !std::filesystem::is_regular_file(path)) return false;
          e.GetScene()->DestroyLuaScriptInstance(e);
          script.Path = path;
          script.Error.clear();
          return true;
        });
        if (ImGui::Button("Reload Script")) {
          e.GetScene()->DestroyLuaScriptInstance(e);
          script.Error.clear();
        }
        ImGui::TextUnformatted(script.Instance ? "Running" : "Runs in Play mode");
        if (!script.Error.empty()) ImGui::TextWrapped("%s", script.Error.c_str());
      });
  ImGui::PopID();
}

} // namespace LevyeForge
