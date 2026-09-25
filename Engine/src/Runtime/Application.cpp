#include "Application.h"
#include "Audio/Audio.h"
#include "Core/Input.h"
#include "Platform/OpenGL/OpenGLImGuiLayerContext.h"
#include "lfpch.h"
#include <GLFW/glfw3.h>
#include <Renderer/Renderer.h>

namespace LevyeForge {

Application *Application::s_Instance = nullptr;

Application::Application(const std::string &name, const glm::vec2 &size, bool resize,
                         ApplicationCommandLineArgs args)
    : m_CommandLineArgs(args) {

  LF_PROFILE_FUNCTION();
  LF_CORE_ASSERT(!s_Instance, "Application already exists!");

  s_Instance = this;
  m_Window = CreateScope<Window>(WindowProps(name, size.x, size.y, resize));
  m_Window->SetEventCallback(BIND_EVENT_FN(Application::OnEvent));

  LevyeForge::Renderer::Init();

  m_SceneManager = CreateRef<SceneManager>(size.x, size.y);

  m_ImGuiLayer = new ImGuiLayer(new OpenGLImGuiLayerContext);
  PushOverlay(m_ImGuiLayer);
}

Application::~Application() {
  LF_PROFILE_FUNCTION();
  LF_CORE_INFO("Shutting down Application..");
  m_SceneManager->StopActiveScene();
  Audio::Shutdown();
  LevyeForge::Renderer::Shutdown();
}

void Application::PushLayer(Layer *layer) {
  LF_PROFILE_FUNCTION();
  m_LayerStack.PushLayer(layer);
  layer->OnAttach();
}

void Application::PushOverlay(Layer *layer) {
  LF_PROFILE_FUNCTION();
  m_LayerStack.PushOverlay(layer);
  layer->OnAttach();
}

void Application::PopLayer(Layer *layer) {
  LF_PROFILE_FUNCTION();
  m_LayerStack.PopLayer(layer);
}

void Application::Close() { m_Running = false; }

void Application::OnEvent(Event &e) {
  LF_PROFILE_FUNCTION();
  EventDispatcher dispatcher(e);
  dispatcher.Dispatch<WindowCloseEvent>(
      BIND_EVENT_FN(Application::OnWindowClose));
  dispatcher.Dispatch<WindowResizeEvent>(
      BIND_EVENT_FN(Application::OnWindowResize));

  for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it) {
    if (e.Handled)
      break;
    (*it)->OnEvent(e);
  }
}

void Application::Run() {

  LF_PROFILE_FUNCTION();
  while (m_Running) {
    LF_PROFILE_SCOPE("RunLopp");
    float time = (float)glfwGetTime();
    Timestep timestep = time - m_LastFrameTime;
    m_LastFrameTime = time;

    // m_SceneManager->Update(timestep);

    if (!m_Minimized) {
      {
        LF_PROFILE_SCOPE("LayerStack OnUpdate");
        for (Layer *layer : m_LayerStack)
          layer->OnUpdate(timestep);
      }

      m_ImGuiLayer->Begin();
      {
        LF_PROFILE_SCOPE("LayerStack OnImGuiRender");
        for (Layer *layer : m_LayerStack)
          layer->OnImGuiRender();
      }

      m_ImGuiLayer->End();
    }

    Input::Update();
    m_Window->OnUpdate();

    while (!m_LayerActionQueue.empty()) {
      LayerAction action = m_LayerActionQueue.front();
      m_LayerActionQueue.pop();

      if (action.Type == LayerActionType::Push) {
        PushLayer(action.LayerPtr);
      } else if (action.Type == LayerActionType::Pop) {
        PopLayer(action.LayerPtr);
        delete action.LayerPtr;
      }
    }
  }
}

bool Application::OnWindowClose(WindowCloseEvent &e) {

  m_Running = false;
  return true;
}

bool Application::OnWindowResize(WindowResizeEvent &e) {

  LF_PROFILE_FUNCTION();

  if (e.GetWidth() == 0 || e.GetHeight() == 0) {
    m_Minimized = true;
    return false;
  }

  m_Minimized = false;

  LevyeForge::Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());

  return false;
}
} // namespace LevyeForge
