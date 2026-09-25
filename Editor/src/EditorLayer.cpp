#include "EditorLayer.h"
#include "Animation/AnimationSystem.h"
#include "Application.h"
#include "Runtime/Components.h"
#include "ScriptTest.h"
#include "Runtime/Playground.h"
#include "Audio/Audio.h"
#include "Runtime/NativeScriptRegistry.h"
#include "Skybox.h"
#include "lfpch.h"
#include <ImGuiFileDialog.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

EditorLayer::EditorLayer(const glm::vec2 &size) : m_Size(size) {}

void EditorLayer::OnAttach() {
  NativeScriptRegistry::Register<PlayerController>("Player Controller");
  // LF_PROFILE_FUNCTION("Editor::OnAttach");
  console.AddLog("Starting");
  m_IconPlay = Texture2D::Create("Resources/Icons/PlayButton.png");
  m_IconStop = Texture2D::Create("Resources/Icons/StopButton.png");

  m_EditorScene = CreateRef<EditorScene>(m_Size.x, m_Size.y);
  //   m_RuntimeScene = CreateRef<RuntimeScene>();
  m_SceneHierarchyPanel = SceneHierarchyPanel(m_EditorScene);

  Playground::RegisterScripts();
  auto args = Application::Get().GetCommandLineArgs();
  if (args.Count > 1) {
    SceneSerializer serializer(m_EditorScene);
    if (!serializer.Deserialize(args[1])) console.AddLog("Could not load the requested scene");
  } else {
    Playground::Populate(*m_EditorScene);
    m_EditorScene->GetCamera().SetDistance(22);
  }
  console.AddLog("Power the Beacon ready. Press Play to begin.");

}

void EditorLayer::OnUpdate(Timestep ts) {
  if (m_RuntimeScene)
    if (auto *controller = Playground::Controller(*m_RuntimeScene))
      controller->InputEnabled = m_ViewportFocused && !ImGui::GetIO().WantTextInput;
  m_EditorScene->CameraInputEnabled = m_ViewportHovered && !ImGuizmo::IsUsing();
  // LF_PROFILE_FUNCTION("Editor::OnUpdate");
  // Resize
  if (FramebufferSpecification spec =
          m_EditorScene->m_Framebuffer->GetSpecification();
      m_ViewportSize.x > 0.0f &&
      m_ViewportSize.y > 0.0f && // zero sized framebuffer is invalid
      (spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y)) {
    m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x,
                                    (uint32_t)m_ViewportSize.y);

    Application::Get().GetSceneManager().Resize((uint32_t)m_ViewportSize.x,
                                                (uint32_t)m_ViewportSize.y);
  }

  auto [mx, my] = ImGui::GetMousePos();
  mx -= m_ViewportBounds[0].x;
  my -= m_ViewportBounds[0].y;
  glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
  my = viewportSize.y - my;
  int mouseX = (int)mx;
  int mouseY = (int)my;
  {
    LF_PROFILE_SCOPE("Scene::Update");
    switch (m_SceneState) {
    case SceneState::Edit: {

      m_EditorScene->ReadPixelEntity(mouseX, mouseY, viewportSize);

      if (m_ViewportFocused && m_ViewportHovered)
        m_EditorScene->OnMouseInput(Input::GetMouseX(), Input::GetMouseY(),
                                    Input::IsMouseButtonPressed(0), ts);

      m_EditorScene->OnUpdate(ts);
      break;
    }
    case SceneState::Play: {
      Application::Get().GetSceneManager().GetActiveScene()->ReadPixelEntity(
          mouseX, mouseY, viewportSize);

      if (m_ViewportFocused && m_ViewportHovered)
        Application::Get().GetSceneManager().GetActiveScene()->OnMouseInput(
            Input::GetMouseX(), Input::GetMouseY(),
            Input::IsMouseButtonPressed(0), ts);


      Application::Get().GetSceneManager().Update(ts);
      break;
    }
    }
  }
}

bool EditorLayer::OnKeyPressed(KeyPressedEvent &e) {
  // Shortcuts
  if (e.GetRepeatCount() > 0 || ImGui::GetIO().WantTextInput)
    return false;

  bool control = Input::IsKeyPressed(Key::LeftControl) ||
                 Input::IsKeyPressed(Key::RightControl);
  bool shift = Input::IsKeyPressed(Key::LeftShift) ||
               Input::IsKeyPressed(Key::RightShift);

  const auto key = e.GetKeyCode();
  if ((key == Key::Q || key == Key::W || key == Key::E || key == Key::R) &&
      (control || !m_ViewportFocused || m_SceneState != SceneState::Edit))
    return false;

  switch (e.GetKeyCode()) {
  case Key::N: {
    if (control)
      NewScene();

    return true;
  }
  case Key::O: {
    if (control)
      OpenScene();

    return true;
  }
  case Key::S: {
    if (control) {
      if (shift)
        SaveSceneAs();
      else
        SaveScene();
    }

    return true;
  }

  // Scene Commands
  case Key::D: {
    if (control)
      OnDuplicateEntity();

    return true;
  }

  case Key::F5: {
    if (m_SceneState == SceneState::Edit)
      OnScenePlay();
    else if (m_SceneState == SceneState::Play)
      OnSceneStop();
    return true;
  }

  // Gizmos
  case Key::Q: {
    if (!ImGuizmo::IsUsing())
      m_GizmoType = -1;
    return true;
  }
  case Key::W: {
    if (!ImGuizmo::IsUsing())
      m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
    return true;
  }
  case Key::E: {
    if (!ImGuizmo::IsUsing())
      m_GizmoType = ImGuizmo::OPERATION::ROTATE;
    return true;
  }
  case Key::R: {
    if (!ImGuizmo::IsUsing())
      m_GizmoType = ImGuizmo::OPERATION::SCALE;
    return true;
  }
  default:
    return false;
  }
}

bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent &e) {
  if (e.GetMouseButton() == Mouse::ButtonLeft) {
    if (m_ViewportHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() &&
        !Input::IsKeyPressed(Key::LeftAlt)) {
      if (m_SceneState == SceneState::Edit) {
        m_SceneHierarchyPanel.SetSelectedEntity(
            m_EditorScene->GetHoveredEntity());
      } else if (m_SceneState == SceneState::Play) {

        m_SceneHierarchyPanel.SetSelectedEntity(Application::Get()
                                                    .GetSceneManager()
                                                    .GetActiveScene()
                                                    ->GetHoveredEntity());
      }
    }
  }
  return false;
}
void EditorLayer::OnEvent(Event &e) {
  if (m_SceneState == SceneState::Edit && m_ViewportHovered &&
      !ImGuizmo::IsUsing())
    m_EditorScene->GetCamera().OnEvent(e);
  EventDispatcher dispatcher(e);
  dispatcher.Dispatch<KeyPressedEvent>(
      BIND_EVENT_FN(EditorLayer::OnKeyPressed));
  dispatcher.Dispatch<MouseButtonPressedEvent>(
      BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
}

void EditorLayer::OnImGuiRender() {
  // LF_PROFILE_FUNCTION("Editor::OnImGuiRender");
  // Note: Switch this to true to enable dockspace
  static bool dockspaceOpen = true;
  static bool opt_fullscreen_persistant = true;
  bool opt_fullscreen = opt_fullscreen_persistant;
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

  // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window
  // not dockable into, because it would be confusing to have two docking
  // targets within each others.
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  }

  // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render
  // our background and handle the pass-thru hole, so we ask Begin() to not
  // render a background.
  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  // Important: note that we proceed even if Begin() returns false (aka window
  // is collapsed). This is because we want to keep our DockSpace() active. If a
  // DockSpace() is inactive, all active windows docked into it will lose their
  // parent and become undocked. We cannot preserve the docking relationship
  // between an active window and an inactive docking, otherwise any change of
  // dockspace/settings would lead to windows being stuck in limbo and never
  // being visible.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
  ImGui::PopStyleVar();

  if (opt_fullscreen)
    ImGui::PopStyleVar(2);

  // DockSpace
  ImGuiIO &io = ImGui::GetIO();
  ImGuiStyle &style = ImGui::GetStyle();
  // float minWinSizeX = style.WindowMinSize.x;
  // style.WindowMinSize.x = 370.0f;
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  }

  // style.WindowMinSize.x = minWinSizeX;

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      // Disabling fullscreen would allow the window to be moved to the front of
      // other windows, which we can't undo at the moment without finer window
      // depth/z control.
      // ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen_persistant);1
      if (ImGui::MenuItem("New", "Ctrl+N")) {
        NewScene();
        console.AddLog("Open New Scene");
      }

      if (ImGui::MenuItem("Open...", "Ctrl+O")) {
        OpenScene();
        console.AddLog("Open Scene");
      }

      if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
        SaveScene();
        console.AddLog("Save Scene");
      }

      if (ImGui::MenuItem("New Playground")) {
        if (m_SceneState == SceneState::Play) OnSceneStop();
        NewScene();
        Playground::Populate(*m_EditorScene);
        m_EditorScene->GetCamera().SetDistance(22);
      }

      if (ImGui::MenuItem("Exit")) {
        console.AddLog("Good bye");
        Application::Get().Close();
      }

      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  m_SceneHierarchyPanel.OnImGuiRender();
  m_ContentBrowserPanel.OnImGuiRender();
  bool p_open = true;
  console.Draw("Console", &p_open);

  // {
  //     LF_PROFILE_SCOPE("ImGui::Viewport");
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
  ImGui::Begin("Viewport");
  m_ViewportFocused = ImGui::IsWindowFocused();
  m_ViewportHovered = ImGui::IsWindowHovered();
  Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused &&
                                                  !m_ViewportHovered);

  ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
  m_ViewportSize = {viewportPanelSize.x, viewportPanelSize.y};

  Ref<Scene> activeScene =
      (m_SceneState == SceneState::Edit)
          ? std::static_pointer_cast<Scene>(m_EditorScene)
          : std::static_pointer_cast<Scene>(
                Application::Get().GetSceneManager().GetActiveScene());

  ImTextureID textureID =
      activeScene->m_Framebuffer->GetColorAttachmentRendererID();

  // ImTextureID textureID =
  //     m_EditorScene->m_Framebuffer->GetColorAttachmentRendererID();
  ImGui::Image(textureID, ImVec2{m_ViewportSize.x, m_ViewportSize.y},
               ImVec2{0, 1}, ImVec2{1, 0});

  const auto imageMin = ImGui::GetItemRectMin();
  const auto imageMax = ImGui::GetItemRectMax();
  m_ViewportBounds[0] = {imageMin.x, imageMin.y};
  m_ViewportBounds[1] = {imageMax.x, imageMax.y};
  m_ViewportHovered = ImGui::IsItemHovered();
  if (m_SceneState == SceneState::Play)
    Playground::DrawHUD(*activeScene, m_ViewportBounds[0], m_ViewportSize);

  // dragging

  // Gizmos
  Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
  if (m_SceneState == SceneState::Edit && selectedEntity &&
      selectedEntity.HasComponent<TransformComponent>() && m_GizmoType != -1 &&
      m_ViewportSize.x > 0 && m_ViewportSize.y > 0) {
    ImGuizmo::Enable(!Input::IsKeyPressed(Key::LeftAlt));
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();

    ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
                      m_ViewportBounds[1].x - m_ViewportBounds[0].x,
                      m_ViewportBounds[1].y - m_ViewportBounds[0].y);

    // Camera

    // Runtime camera from entity
    // const glm::mat4 &cameraProjection =
    //     m_EditorScene->GetCamera().GetProjection();
    // glm::mat4 cameraView = glm::inverse(
    //     cameraEntity.GetComponent<TransformComponent>().GetTransform());

    // Editor camera
    const glm::mat4 &cameraProjection =
        m_EditorScene->GetCamera().GetProjectionMatrix();
    glm::mat4 cameraView = m_EditorScene->GetCamera().GetViewMatrix();

    // Entity transform
    // TransformComponent ts;
    auto &tc = selectedEntity.GetComponent<TransformComponent>();
    glm::mat4 transform = selectedEntity.GetWorldTransform();

    // Snapping
    bool snap = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
    float snapValue = 0.5f; // Snap to 0.5m for translation/scale
    // Snap to 45 degrees for rotation
    if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
      snapValue = 45.0f;

    float snapValues[3] = {snapValue, snapValue, snapValue};

    if (m_SceneState == SceneState::Edit) {

      ImGuizmo::Manipulate(
          glm::value_ptr(cameraView), glm::value_ptr(cameraProjection),
          (ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL,
          glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

      if (ImGuizmo::IsUsing()) {
        // Preserve untouched channels, including mirrored scale signs.
        if (activeScene->GetParent(selectedEntity)) {
          activeScene->SetWorldTransform(selectedEntity, transform);
        } else if (m_GizmoType == ImGuizmo::TRANSLATE) {
          tc.Translation = glm::vec3(transform[3]);
        } else if (m_GizmoType == ImGuizmo::SCALE) {
          const glm::mat3 basis = glm::mat3_cast(glm::quat(tc.Rotation));
          for (int axis = 0; axis < 3; ++axis)
            tc.Scale[axis] = glm::dot(glm::vec3(transform[axis]), basis[axis]);
        } else if (m_GizmoType == ImGuizmo::ROTATE &&
                   glm::all(glm::greaterThan(glm::abs(tc.Scale), glm::vec3(1e-6f)))) {
          glm::mat4 rotationMatrix = transform;
          for (int axis = 0; axis < 3; ++axis)
            rotationMatrix[axis] /= tc.Scale[axis];
          glm::vec3 translation, rotation, scale;
          if (Math::DecomposeTransform(rotationMatrix, translation, rotation, scale))
            tc.Rotation = rotation;
        }
      }
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();
  // }

  if (ImGuiFileDialog::Instance()->Display("OpenScene")) {
    if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK
      filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      // action
      if (!filePathName.empty())
        OpenScene(filePathName);
    }

    // close
    ImGuiFileDialog::Instance()->Close();
  }

  if (ImGuiFileDialog::Instance()->Display("SaveSceneAs")) {
    if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK
      std::string SavefilePathName =
          ImGuiFileDialog::Instance()->GetFilePathName();
      std::string SavefilePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      // action
      if (!SavefilePathName.empty()) {
        SerializeScene(m_EditorScene, SavefilePathName);
        m_EditorScenePath = SavefilePathName;
      }
    }

    // close
    ImGuiFileDialog::Instance()->Close();
  }

  UI_Toolbar();

  ImGui::End();
}

void EditorLayer::UI_Toolbar() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  auto &colors = ImGui::GetStyle().Colors;
  const auto &buttonHovered = colors[ImGuiCol_ButtonHovered];
  ImGui::PushStyleColor(
      ImGuiCol_ButtonHovered,
      ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
  const auto &buttonActive = colors[ImGuiCol_ButtonActive];
  ImGui::PushStyleColor(
      ImGuiCol_ButtonActive,
      ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

  ImGui::Begin("##toolbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  float size = ImGui::GetWindowHeight() - 4.0f;
  Ref<Texture2D> icon =
      m_SceneState == SceneState::Edit ? m_IconPlay : m_IconStop;
  ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) -
                       (size * 0.5f));
  if (ImGui::ImageButton("##play", (ImTextureID)icon->GetRendererID(),
                         ImVec2(size, size), ImVec2(0, 1), ImVec2(1, 0))) {
    if (m_SceneState == SceneState::Edit)
      OnScenePlay();
    else if (m_SceneState == SceneState::Play)
      OnSceneStop();
  }
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);
  ImGui::End();
}

void EditorLayer::NewScene() {

  OnSceneStop();
  m_EditorScene = CreateRef<EditorScene>(std::max(1u, (uint32_t)m_ViewportSize.x),
                                       std::max(1u, (uint32_t)m_ViewportSize.y));
  m_SceneHierarchyPanel.SetContext(m_EditorScene);
  m_EditorScenePath.clear();
}

void EditorLayer::OpenScene() {
  // std::string filepath = FileDialogs::OpenFile("Levye Forge Scene
  // (*.UE)\0*.UE\0"); if (!filepath.empty())
  //     OpenScene(filepath);
  IGFD::FileDialogConfig config;
  config.path = ".";
  ImGuiFileDialog::Instance()->OpenDialog("OpenScene", "Choose File", ".UE",
                                          config);
}

void EditorLayer::OpenScene(const std::filesystem::path &path) {
  if (m_SceneState != SceneState::Edit)
    OnSceneStop();

  if (path.extension().string() != ".UE") {
    LF_WARN("Could not load {0} - not a scene file", path.filename().string());
    return;
  }

  auto newScene = CreateRef<EditorScene>(std::max(1u, (uint32_t)m_ViewportSize.x),
                                         std::max(1u, (uint32_t)m_ViewportSize.y));
  SceneSerializer serializer(newScene);
  if (serializer.Deserialize(path.string())) {
    m_EditorScene = newScene;
    m_SceneHierarchyPanel.SetContext(m_EditorScene);
    m_EditorScenePath = path;
  }
}

void EditorLayer::SaveScene() {
  if (!m_EditorScenePath.empty())
    SerializeScene(m_EditorScene, m_EditorScenePath);
  else
    SaveSceneAs();
}

void EditorLayer::SaveSceneAs() {
  // std::string filepath = FileDialogs::SaveFile("Levye Forge Scene
  // (*.UE)\0*.UE\0"); if (!filepath.empty())
  // {
  //     SerializeScene(m_EditorScene, filepath);
  //     m_EditorScenePath = filepath;
  // }

  IGFD::FileDialogConfig config;
  config.path = ".";
  ImGuiFileDialog::Instance()->OpenDialog("SaveSceneAs", "Choose File", ".UE",
                                          config);
}

void EditorLayer::SerializeScene(Ref<Scene> scene,
                                 const std::filesystem::path &path) {
  SceneSerializer serializer(scene);
  serializer.Serialize(path.string());
}

void EditorLayer::OnScenePlay() {
  Audio::Initialize();
  m_SceneState = SceneState::Play;

  // m_EditorScene = m_EditorScene;
  if (!m_RuntimeScene)
    m_RuntimeScene = CreateRef<RuntimeScene>();

  m_RuntimeScene = Scene::Copy(m_EditorScene);

  Application::Get().GetSceneManager().LoadScene(m_RuntimeScene);

  m_SceneHierarchyPanel.SetContext(m_RuntimeScene);
}

void EditorLayer::OnSceneStop() {
  if (m_SceneState != SceneState::Play)
    return;
  m_SceneState = SceneState::Edit;

  Application::Get().GetSceneManager().StopActiveScene();
  m_RuntimeScene.reset();

  m_SceneHierarchyPanel.SetContext(m_EditorScene);
}

void EditorLayer::OnDuplicateEntity() {
  if (m_SceneState != SceneState::Edit)
    return;

  Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
  if (selectedEntity)
    m_EditorScene->DuplicateEntity(selectedEntity);
}
