#include "Runtime/SceneSerializer.h"
#include "lfpch.h"

#include "Auxiliaries/YamlConvert.h"
#include "Core/LF_Assert.h"
#include "Runtime/Components.h"
#include "Runtime/Entity.h"
#include <fstream>

namespace LevyeForge {

YAML::Emitter &operator<<(YAML::Emitter &out, const glm::vec2 &v) {
  out << YAML::Flow;
  out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
  return out;
}

YAML::Emitter &operator<<(YAML::Emitter &out, const glm::vec3 &v) {
  out << YAML::Flow;
  out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
  return out;
}

YAML::Emitter &operator<<(YAML::Emitter &out, const glm::vec4 &v) {
  out << YAML::Flow;
  out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
  return out;
}

SceneSerializer::SceneSerializer(const Ref<Scene> &scene) : m_Scene(scene) {}

static void SerializeEntity(YAML::Emitter &out, Entity entity) {
  LF_CORE_ASSERT(entity.HasComponent<IDComponent>());

  out << YAML::BeginMap; // Entity
  out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

  if (auto parent=entity.GetScene()->GetParent(entity))
    out << YAML::Key << "Parent" << YAML::Value << uint64_t(parent.GetUUID());

  if (entity.HasComponent<TagComponent>()) {
    out << YAML::Key << "TagComponent";
    out << YAML::BeginMap; // TagComponent

    auto &tag = entity.GetComponent<TagComponent>().Tag;
    out << YAML::Key << "Tag" << YAML::Value << tag;

    out << YAML::EndMap; // TagComponent
  }

  if (entity.HasComponent<TransformComponent>()) {
    out << YAML::Key << "TransformComponent";
    out << YAML::BeginMap; // TransformComponent

    auto &tc = entity.GetComponent<TransformComponent>();
    out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
    out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
    out << YAML::Key << "Scale" << YAML::Value << tc.Scale;

    out << YAML::EndMap; // TransformComponent
  }

  if (entity.HasComponent<CubeComponent>()) {
    out << YAML::Key << "CubeComponent";
    out << YAML::BeginMap; // TransformComponent

    auto &tc = entity.GetComponent<CubeComponent>();
    out << YAML::Key << "Color" << YAML::Value << tc.Color;

    out << YAML::EndMap; // TransformComponent
  }

  if (entity.HasComponent<CameraComponent>()) {
    out << YAML::Key << "CameraComponent";
    out << YAML::BeginMap; // CameraComponent

    auto &cameraComponent = entity.GetComponent<CameraComponent>();
    auto &camera = cameraComponent.Camera;

    out << YAML::Key << "Camera" << YAML::Value;
    out << YAML::BeginMap; // Camera
    out << YAML::Key << "ProjectionType" << YAML::Value
        << (int)camera.GetProjectionType();
    out << YAML::Key << "PerspectiveFOV" << YAML::Value
        << camera.GetPerspectiveVerticalFOV();
    out << YAML::Key << "PerspectiveNear" << YAML::Value
        << camera.GetPerspectiveNearClip();
    out << YAML::Key << "PerspectiveFar" << YAML::Value
        << camera.GetPerspectiveFarClip();
    out << YAML::Key << "OrthographicSize" << YAML::Value
        << camera.GetOrthographicSize();
    out << YAML::Key << "OrthographicNear" << YAML::Value
        << camera.GetOrthographicNearClip();
    out << YAML::Key << "OrthographicFar" << YAML::Value
        << camera.GetOrthographicFarClip();
    out << YAML::EndMap; // Camera

    out << YAML::Key << "ShowFrustum" << YAML::Value << cameraComponent.ShowFrustum;
    out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
    out << YAML::Key << "FixedAspectRatio" << YAML::Value
        << cameraComponent.FixedAspectRatio;

    out << YAML::EndMap; // CameraComponent
  }

  if (entity.HasComponent<ModelComponent>()) {
    out << YAML::Key << "ModelComponent";
    out << YAML::BeginMap; // ModelComponent

    auto &modelComponent = entity.GetComponent<ModelComponent>();
    auto &model = modelComponent.ModelData;

    out << YAML::Key << "Model" << YAML::Value;
    out << YAML::BeginMap; // Model
    out << YAML::Key << "Path" << YAML::Value << (model ? model->m_Path : std::string());
    out << YAML::EndMap; // Model

    out << YAML::EndMap; // ModelComponent
  }

  if (entity.HasComponent<SpriteRendererComponent>()) {
    out << YAML::Key << "SpriteRendererComponent";
    out << YAML::BeginMap; // SpriteRendererComponent

    auto &spriteRendererComponent =
        entity.GetComponent<SpriteRendererComponent>();
    out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.Color;
    out << YAML::Key << "Texture" << YAML::Value << (spriteRendererComponent.Texture ? spriteRendererComponent.Texture->GetPath() : "");
    out << YAML::Key << "TilingFactor" << YAML::Value << spriteRendererComponent.TilingFactor;

    out << YAML::EndMap; // SpriteRendererComponent
  }

  if (entity.HasComponent<RigidbodyComponent>()) {
    const auto &rb = entity.GetComponent<RigidbodyComponent>();
    out << YAML::Key << "RigidbodyComponent" << YAML::BeginMap;
    out << YAML::Key << "Type" << YAML::Value << static_cast<int>(rb.Type);
    out << YAML::Key << "Mass" << YAML::Value << rb.Mass;
    out << YAML::Key << "LinearVelocity" << YAML::Value << rb.LinearVelocity;
    out << YAML::Key << "LinearDamping" << YAML::Value << rb.LinearDamping;
    out << YAML::Key << "AngularDamping" << YAML::Value << rb.AngularDamping;
    out << YAML::Key << "UseGravity" << YAML::Value << rb.UseGravity;
    out << YAML::Key << "LockRotation" << YAML::Value << rb.LockRotation;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<BoxColliderComponent>()) {
    const auto &box = entity.GetComponent<BoxColliderComponent>();
    out << YAML::Key << "BoxColliderComponent" << YAML::BeginMap;
    out << YAML::Key << "HalfSize" << YAML::Value << box.HalfSize;
    out << YAML::Key << "Friction" << YAML::Value << box.Friction;
    out << YAML::Key << "Restitution" << YAML::Value << box.Restitution;
    out << YAML::Key << "ShowWireframe" << YAML::Value << box.ShowWireframe;
    out << YAML::Key << "IsTrigger" << YAML::Value << box.IsTrigger;
    out << YAML::EndMap;
  }

  if (entity.HasComponent<SphereColliderComponent>()) {
    const auto &sphere = entity.GetComponent<SphereColliderComponent>();
    out << YAML::Key << "SphereColliderComponent" << YAML::BeginMap;
    out << YAML::Key << "Radius" << YAML::Value << sphere.Radius;
    out << YAML::Key << "Friction" << YAML::Value << sphere.Friction;
    out << YAML::Key << "Restitution" << YAML::Value << sphere.Restitution;
    out << YAML::Key << "ShowWireframe" << YAML::Value << sphere.ShowWireframe;
    out << YAML::Key << "IsTrigger" << YAML::Value << sphere.IsTrigger;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<LightComponent>()) {
    const auto &component = entity.GetComponent<LightComponent>();
    out << YAML::Key << "LightComponent" << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << component.Color;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<CircleComponent>()) {
    const auto &component = entity.GetComponent<CircleComponent>();
    out << YAML::Key << "CircleComponent" << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << component.Color;
    out << YAML::Key << "Radius" << YAML::Value << component.Radius;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<RectangleComponent>()) {
    const auto &component = entity.GetComponent<RectangleComponent>();
    out << YAML::Key << "RectangleComponent" << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << component.Color;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<LineComponent>()) {
    const auto &component = entity.GetComponent<LineComponent>();
    out << YAML::Key << "LineComponent" << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << component.Color;
    out << YAML::Key << "Thickness" << YAML::Value << component.Thickness;
    out << YAML::Key << "Start" << YAML::Value << component.p0;
    out << YAML::Key << "End" << YAML::Value << component.p1;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<AnimatorComponent>()) {
    const auto &anim = entity.GetComponent<AnimatorComponent>();
    out << YAML::Key << "AnimatorComponent" << YAML::BeginMap;
    out << YAML::Key << "Path" << YAML::Value << anim.CurrentAnimation.GetPath();
    out << YAML::Key << "ClipIndex" << YAML::Value << anim.CurrentAnimation.GetClipIndex();
    out << YAML::Key << "InPlaceJoint" << YAML::Value << anim.InPlaceJoint;
    out << YAML::Key << "Loop" << YAML::Value << anim.Loop;
    out << YAML::Key << "Speed" << YAML::Value << anim.Speed;
    out << YAML::Key << "Playing" << YAML::Value << anim.Playing;
    out << YAML::Key << "Time" << YAML::Value << anim.Time;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<SkyboxComponent>()) {
    const auto &sky = entity.GetComponent<SkyboxComponent>();
    out << YAML::Key << "SkyboxComponent" << YAML::BeginMap;
    out << YAML::Key << "Faces" << YAML::Value << YAML::BeginSeq;
    for (const auto &face : sky.Faces) out << face;
    out << YAML::EndSeq << YAML::EndMap;
  }
  if (entity.HasComponent<UIElement>()) {
    const auto &ui = entity.GetComponent<UIElement>();
    out << YAML::Key << "UIElement" << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << ui.Color;
    out << YAML::Key << "Texture" << YAML::Value << (ui.Texture ? ui.Texture->GetPath() : "");
    out << YAML::EndMap;
  }
  if (entity.HasComponent<ButtonComponent>()) {
    const auto &ui = entity.GetComponent<ButtonComponent>();
    out << YAML::Key << "ButtonComponent" << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << ui.Color;
    out << YAML::Key << "Texture" << YAML::Value << (ui.Texture ? ui.Texture->GetPath() : "");
    out << YAML::Key << "BaseColor" << YAML::Value << ui.BaseColor;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<TextUIComponent>()) {
    const auto &ui = entity.GetComponent<TextUIComponent>();
    out << YAML::Key << "TextUIComponent" << YAML::BeginMap;
    out << YAML::Key << "Color" << YAML::Value << ui.Color;
    out << YAML::Key << "Texture" << YAML::Value << (ui.Texture ? ui.Texture->GetPath() : "");
    out << YAML::Key << "Text" << YAML::Value << ui.Text;
    out << YAML::Key << "Font" << YAML::Value << (ui.m_Font ? ui.m_Font->GetPath() : "");
    out << YAML::EndMap;
  }
  if (entity.HasComponent<NativeScriptComponent>()) {
    const auto &script = entity.GetComponent<NativeScriptComponent>();
    out << YAML::Key << "NativeScriptComponent" << YAML::BeginMap;
    out << YAML::Key << "Class" << YAML::Value << script.ClassName;
    out << YAML::Key << "Enabled" << YAML::Value << script.Enabled;
    out << YAML::EndMap;
  }
  if (entity.HasComponent<LuaScriptComponent>()) {
    const auto &script = entity.GetComponent<LuaScriptComponent>();
    out << YAML::Key << "LuaScriptComponent" << YAML::BeginMap;
    out << YAML::Key << "Path" << YAML::Value << script.Path;
    out << YAML::Key << "Enabled" << YAML::Value << script.Enabled;
    out << YAML::EndMap;
  }
  out << YAML::EndMap; // Entity
}

void SceneSerializer::Serialize(const std::string &filepath) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << "Scene" << YAML::Value << "Untitled";
  out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
  m_Scene->m_Registry.each([&](auto entityID) {
    Entity entity = {entityID, m_Scene.get()};
    if (!entity)
      return;

    SerializeEntity(out, entity);
  });
  out << YAML::EndSeq;
  out << YAML::EndMap;

  std::ofstream fout(filepath);
  fout << out.c_str();
}

void SceneSerializer::SerializeRuntime(const std::string &filepath) {
  // Not implemented
  LF_CORE_ASSERT(false);
}

bool SceneSerializer::Deserialize(const std::string &filepath) {
  try {
  YAML::Node data = YAML::LoadFile(filepath);

  if (!data["Scene"])
    return false;

  std::string sceneName = data["Scene"].as<std::string>();
  LF_CORE_TRACE("Deserializing scene '{0}'", sceneName);

  std::unordered_map<uint64_t, Entity> loaded;
  std::vector<std::pair<uint64_t,uint64_t>> parents;
  auto entities = data["Entities"];
  if (entities) {
    for (auto entity : entities) {
      uint64_t uuid = entity["Entity"].as<uint64_t>();

      std::string name;
      auto tagComponent = entity["TagComponent"];
      if (tagComponent)
        name = tagComponent["Tag"].as<std::string>();

      LF_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid,
                    name);

      if (loaded.count(uuid)) { LF_CORE_ERROR("Duplicate entity UUID in scene"); return false; }
      Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);
      loaded.emplace(uuid, deserializedEntity);
      if (entity["Parent"]) parents.emplace_back(uuid,entity["Parent"].as<uint64_t>());

      auto transformComponent = entity["TransformComponent"];
      if (transformComponent) {
        // Entities always have transforms
        auto &tc = deserializedEntity.GetComponent<TransformComponent>();
        tc.Translation = transformComponent["Translation"].as<glm::vec3>();
        tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
        tc.Scale = transformComponent["Scale"].as<glm::vec3>();
      }

      auto cubeComponent = entity["CubeComponent"];
      if (cubeComponent) {
        // Entities always have transforms
        auto &src = deserializedEntity.AddComponent<CubeComponent>();
        src.Color = cubeComponent["Color"].as<glm::vec3>();
      }

      auto cameraComponent = entity["CameraComponent"];
      if (cameraComponent) {
        auto &cc = deserializedEntity.AddComponent<CameraComponent>();

        const auto &cameraProps = cameraComponent["Camera"];
        cc.Camera.SetProjectionType(
            (SceneCamera::ProjectionType)cameraProps["ProjectionType"]
                .as<int>());

        cc.Camera.SetPerspectiveVerticalFOV(
            cameraProps["PerspectiveFOV"].as<float>());
        cc.Camera.SetPerspectiveNearClip(
            cameraProps["PerspectiveNear"].as<float>());
        cc.Camera.SetPerspectiveFarClip(
            cameraProps["PerspectiveFar"].as<float>());

        cc.Camera.SetOrthographicSize(
            cameraProps["OrthographicSize"].as<float>());
        cc.Camera.SetOrthographicNearClip(
            cameraProps["OrthographicNear"].as<float>());
        cc.Camera.SetOrthographicFarClip(
            cameraProps["OrthographicFar"].as<float>());

        cc.ShowFrustum = cameraComponent["ShowFrustum"].as<bool>(false);
        cc.Primary = cameraComponent["Primary"].as<bool>();
        cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
      }

      if (auto node = entity["RigidbodyComponent"]) {
        auto &rb = deserializedEntity.AddComponent<RigidbodyComponent>();
        const auto typeName = node["Type"].as<std::string>("Static");
        int type = typeName == "Static" ? 0 : typeName == "Dynamic" ? 1 :
                   typeName == "Kinematic" ? 2 : node["Type"].as<int>();
        if (type < 0 || type > 2)
          throw YAML::RepresentationException(node.Mark(), "Invalid rigidbody type");
        rb.Type = static_cast<RigidbodyComponent::BodyType>(type);
        rb.Mass = node["Mass"].as<float>(1.0f);
        rb.LinearVelocity = node["LinearVelocity"] ? node["LinearVelocity"].as<glm::vec3>() :
                            node["Velocity"].as<glm::vec3>(glm::vec3(0));
        rb.LinearDamping = node["LinearDamping"].as<float>(0.0f);
        rb.AngularDamping = node["AngularDamping"].as<float>(0.05f);
        rb.UseGravity = node["UseGravity"].as<bool>(true);
        rb.LockRotation = node["LockRotation"].as<bool>(false);
      }
      auto boxNode = entity["BoxColliderComponent"] ? entity["BoxColliderComponent"] : entity["BoxShapeComponent"];
      if (auto node = boxNode) {
        auto &box = deserializedEntity.AddComponent<BoxColliderComponent>();
        box.HalfSize = node["HalfSize"].as<glm::vec3>(glm::vec3(0.5f));
        box.Friction = node["Friction"].as<float>(0.5f);
        box.Restitution = node["Restitution"].as<float>(0.0f);
        box.IsTrigger = node["IsTrigger"].as<bool>(false);
        box.ShowWireframe = node["ShowWireframe"].as<bool>(false);
      }

      auto sphereNode = entity["SphereColliderComponent"] ? entity["SphereColliderComponent"] : entity["SphereShapeComponent"];
      if (auto node = sphereNode) {
        auto &sphere = deserializedEntity.AddComponent<SphereColliderComponent>();
        sphere.Radius = node["Radius"].as<float>(0.5f);
        sphere.Friction = node["Friction"].as<float>(0.5f);
        sphere.Restitution = node["Restitution"].as<float>(0.0f);
        sphere.IsTrigger = node["IsTrigger"].as<bool>(false);
        sphere.ShowWireframe = node["ShowWireframe"].as<bool>(false);
      }
      auto modelComponent = entity["ModelComponent"];
      if (modelComponent) {
        auto &mc = deserializedEntity.AddComponent<ModelComponent>();
        const auto &modelProps = modelComponent["Model"];
        const auto &modelAnimProps = modelComponent["ModelAnimation"];
        const auto path = modelProps["Path"].as<std::string>("");
        if (!path.empty()) {
          auto model = CreateRef<Model>(path);
          if (model->IsLoaded()) mc.ModelData = model;
        }
      }

      auto spriteRendererComponent = entity["SpriteRendererComponent"];
      if (spriteRendererComponent) {
        auto &src = deserializedEntity.AddComponent<SpriteRendererComponent>();
        src.Color = spriteRendererComponent["Color"].as<glm::vec4>();
        src.TilingFactor = spriteRendererComponent["TilingFactor"].as<float>(1);
        const auto texturePath = spriteRendererComponent["Texture"].as<std::string>("");
        if (!texturePath.empty()) {
          auto texture = Texture2D::Create(texturePath);
          if (texture->IsLoaded()) src.Texture = texture;
        }
      }
      if (auto node = entity["LightComponent"]) {
        auto &component = deserializedEntity.AddComponent<LightComponent>();
        component.Color = node["Color"].as<glm::vec4>(glm::vec4(1));
      }
      if (auto node = entity["CircleComponent"]) {
        auto &component = deserializedEntity.AddComponent<CircleComponent>();
        component.Color = node["Color"].as<glm::vec4>(glm::vec4(1));
        component.Radius = node["Radius"].as<float>(0.5f);
      }
      if (auto node = entity["RectangleComponent"]) {
        auto &component = deserializedEntity.AddComponent<RectangleComponent>();
        component.Color = node["Color"].as<glm::vec4>(glm::vec4(1));
      }
      if (auto node = entity["LineComponent"]) {
        auto &component = deserializedEntity.AddComponent<LineComponent>();
        component.Color = node["Color"].as<glm::vec4>(glm::vec4(1));
        component.Thickness = node["Thickness"].as<float>(0.05f);
        component.p0 = node["Start"].as<glm::vec2>(glm::vec2(0));
        component.p1 = node["End"].as<glm::vec2>(glm::vec2(1,0));
      }
      if (auto node = entity["AnimatorComponent"]) {
        auto &anim = deserializedEntity.AddComponent<AnimatorComponent>();
        if (deserializedEntity.HasComponent<ModelComponent>()) {
          const auto &model = deserializedEntity.GetComponent<ModelComponent>().ModelData;
          anim.InitFromModel(model);
          const auto path = node["Path"].as<std::string>("");
          if (model && !path.empty()) anim.Play(Animation(path, model->GetSkeleton(), node["ClipIndex"].as<int>(0)));
        }
        anim.InPlaceJoint = node["InPlaceJoint"].as<std::string>("");
        anim.Loop = node["Loop"].as<bool>(true);
        anim.Speed = std::clamp(node["Speed"].as<float>(1), 0.0f, 4.0f);
        anim.Playing = node["Playing"].as<bool>(true);
        if (anim.CurrentAnimation.Get()) anim.Time = std::clamp(node["Time"].as<float>(0), 0.0f, anim.CurrentAnimation.Get()->duration());
      }
      if (auto node = entity["SkyboxComponent"]) {
        auto &sky = deserializedEntity.AddComponent<SkyboxComponent>();
        auto faces = node["Faces"].as<std::vector<std::string>>(std::vector<std::string>{});
        if (faces.size() == 6) {
          std::copy(faces.begin(), faces.end(), sky.Faces.begin());
          if (std::all_of(faces.begin(), faces.end(), [](const auto &path) { return std::filesystem::is_regular_file(path); }))
            sky.skybox = Skybox::Create(faces);
        }
      }
      if (auto node = entity["UIElement"]) {
        auto &ui = deserializedEntity.AddComponent<UIElement>();
        ui.Color = node["Color"].as<glm::vec4>(glm::vec4(1));
        const auto path = node["Texture"].as<std::string>("");
        if (!path.empty()) {
          auto texture = Texture2D::Create(path);
          if (texture->IsLoaded()) ui.Texture = texture;
        }
      }
      if (auto node = entity["ButtonComponent"]) {
        auto &ui = deserializedEntity.AddComponent<ButtonComponent>();
        ui.Color = node["Color"].as<glm::vec4>(glm::vec4(1));
        const auto path = node["Texture"].as<std::string>("");
        if (!path.empty()) {
          auto texture = Texture2D::Create(path);
          if (texture->IsLoaded()) ui.Texture = texture;
        }
        ui.BaseColor = ui.CurrentColor = node["BaseColor"].as<glm::vec4>(ui.Color);
      }
      if (auto node = entity["TextUIComponent"]) {
        auto &ui = deserializedEntity.AddComponent<TextUIComponent>();
        ui.Color = node["Color"].as<glm::vec4>(glm::vec4(1));
        const auto path = node["Texture"].as<std::string>("");
        if (!path.empty()) {
          auto texture = Texture2D::Create(path);
          if (texture->IsLoaded()) ui.Texture = texture;
        }
        ui.Text = node["Text"].as<std::string>("");
        const auto fontPath = node["Font"].as<std::string>("");
        if (!fontPath.empty()) ui.m_Font = CreateRef<Font>(fontPath);
      }
      if (auto node = entity["NativeScriptComponent"]) {
        auto &script = deserializedEntity.AddComponent<NativeScriptComponent>();
        script.ClassName = node["Class"].as<std::string>("");
        script.Enabled = node["Enabled"].as<bool>(true);
      }
      if (auto node = entity["LuaScriptComponent"]) {
        auto &script = deserializedEntity.AddComponent<LuaScriptComponent>();
        script.Path = node["Path"].as<std::string>("");
        script.Enabled = node["Enabled"].as<bool>(true);
      }
    }
  }

  for (const auto &[childID,parentID] : parents) {
    auto parent=loaded.find(parentID);
    if (parent==loaded.end() || !m_Scene->SetParent(loaded.at(childID),parent->second,false)) {
      LF_CORE_ERROR("Invalid or cyclic parent reference in scene");
      return false;
    }
  }
  return true;
  } catch (const YAML::Exception &e) {
    LF_CORE_ERROR("Cannot load scene {}: {}", filepath, e.what());
    return false;
  }
}

bool SceneSerializer::DeserializeRuntime(const std::string &filepath) {
  // Not implemented
  LF_CORE_ASSERT(false);
  return false;
}

} // namespace LevyeForge
