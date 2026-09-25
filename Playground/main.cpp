#include <LevyeForge.h>
#include "Runtime/Playground.h"
#include "Audio/Audio.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

using namespace LevyeForge;
class PlaygroundLayer : public Layer {
public:
  void OnAttach() override {
    auto &app=Application::Get();
    const auto args=app.GetCommandLineArgs();
    for (int i=1;i<args.Count;++i) {
      const std::string arg=args[i];
      if (arg=="--mute") Audio::SetMuted(true);
      if (arg.rfind("--export-scene=",0)==0) m_Export=arg.substr(15);
    }
    Audio::Initialize();
    app.GetImGuiLayer()->BlockEvents(false);
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
    int width,height;
    glfwGetFramebufferSize(static_cast<GLFWwindow *>(app.GetWindow().GetNativeWindow()),&width,&height);
    m_Scene=CreateRef<RuntimeScene>(width,height);
    m_Scene->ClearColor({0.065f,0.10f,0.15f,1});
    Playground::Populate(*m_Scene);
    if (!m_Export.empty()) SceneSerializer(m_Scene).Serialize(m_Export);
    app.GetSceneManager().LoadScene(m_Scene);
  }
  void OnUpdate(Timestep ts) override {
    auto &app=Application::Get();
    int width,height;
    glfwGetFramebufferSize(static_cast<GLFWwindow *>(app.GetWindow().GetNativeWindow()),&width,&height);
    if (width<=0 || height<=0) return;
    const auto spec=m_Scene->m_Framebuffer->GetSpecification();
    if (spec.Width!=width || spec.Height!=height) m_Scene->OnViewportResize(width,height);
    m_Scene->OnUpdate(std::min((float)ts,0.1f));
    m_Scene->m_Framebuffer->DrawBuffer(width,height);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if (Input::IsKeyJustPressed(Key::Escape)) app.Close();
  }
  void OnImGuiRender() override {
    auto *viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::Begin("Playground HUD",nullptr,ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    Playground::DrawHUD(*m_Scene,{viewport->Pos.x,viewport->Pos.y},{viewport->Size.x,viewport->Size.y});
    ImGui::End();
  }
private:
  Ref<RuntimeScene> m_Scene;
  std::string m_Export;
};
class PlaygroundApp : public Application {
public:
  PlaygroundApp(ApplicationCommandLineArgs args) : Application("Levye Forge - Power the Beacon",{1280,720},true,args) {
    PushLayer(new PlaygroundLayer());
  }
};
Application *LevyeForge::CreateApplication(ApplicationCommandLineArgs args) { return new PlaygroundApp(args); }
#include <Core/EntryPoint.h>
