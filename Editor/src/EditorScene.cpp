#include "EditorScene.h"
#include "Animation/AnimationSystem.h"
#include "Components.h"
#include "Log.h"
#include "Renderer2D.h"
#include "Renderer3D.h"

EditorScene::EditorScene(uint32_t width, uint32_t height) {
  m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 2000.0f);
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
EditorScene::~EditorScene() {}

void EditorScene::OnUpdate(Timestep ts) {
  m_EditorCamera.OnUpdate(ts, CameraInputEnabled);
  m_EditorCamera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
  LF_PROFILE_FUNCTION();
  m_Framebuffer->Bind();
  // Clear our entity ID attachment to -1
  RenderCommand::SetClearColor({0.1f, 0.1f, 0.1f, 1});
  RenderCommand::Clear();
  m_Framebuffer->ClearAttachment(1, -1);

  AnimationSystem::Update(this, ts);

  Renderer3D::BeginCamera(m_EditorCamera);
  GroupEntity<SkyboxComponent>(
      [this](auto entity, auto &comp, auto &transform, auto id) {
        if (comp.skybox) Renderer3D::DrawSkybox(comp.skybox, m_EditorCamera);
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

          if (animator.ShowSkeleton && animator.HasPose && animator.ModelRef)
          Renderer3D::DrawSkeleton(*animator.ModelRef->GetSkeleton(),
                                   animator.ModelMatrices,
                                   GetWorldTransform(entity));
        }

        Renderer3D::DrawModel(comp.ModelData, GetWorldTransform(entity), bones,
                              (int)id);
      });

  GroupEntity<CubeComponent>([this](auto entity, auto &comp, auto &transform,
                                    auto id) {
    Renderer3D::DrawCube(GetWorldTransform(entity), comp.Color, 1.0f, (int)id);
  });

  GroupEntity<CameraComponent>([this](auto entity, auto &comp, auto &transform, auto id) {
    if (comp.ShowFrustum) {
      const auto pose = glm::translate(glm::mat4(1), GetWorldTransformComponents(entity).Translation) *
                        glm::mat4_cast(glm::quat(GetWorldTransformComponents(entity).Rotation));
      Renderer3D::DrawCameraFrustum(comp.Camera, pose);
    }
  });

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

  Renderer2D::BeginCamera(m_EditorCamera);
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

  Renderer2D::EndCamera();

  // ReadPixelEntity(mouseX, mouseY, viewportSize);

  m_Framebuffer->Unbind();

  RenderCommand::SetClearColor({0.1f, 0.1f, 0.1f, 1});
  RenderCommand::Clear();

  FlushEntityDestruction();
}