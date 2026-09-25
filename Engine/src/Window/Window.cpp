#include "lfpch.h"
#include "Window.h"
#include "Renderer/Renderer.h"
#include "Core/Input.h"
#include "Core/LF_Assert.h"
#include "Events/ApplicationEvent.h"
#include "Events/MouseEvent.h"
#include "Events/KeyEvent.h"
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stdexcept>

namespace LevyeForge {

    glm::vec2 Window::s_LastMousePosition = glm::vec2(0.0f);
    glm::vec2 Window::s_MouseDelta = glm::vec2(0.0f);

    static uint8_t s_GLFWwindowCount = 0;

    static void GLFWErrorCallback(int error, const char* desc){
        LF_CORE_ERROR("GLFW Error ({0}): {1}", error, desc);
    }

    Window::Window(const WindowProps& props){
        LF_PROFILE_FUNCTION();
        Init(props);
    }

    Window::~Window(){
        LF_PROFILE_FUNCTION();
	LF_CORE_INFO("[WINDOW] Shutting down window...");
        Shutdown();
    }

    void Window::Init(const WindowProps& props){
        LF_PROFILE_FUNCTION();
        m_Data.Title = props.m_Title;
        m_Data.Width = props.m_Width;
        m_Data.Height = props.m_Height;

        LF_CORE_INFO("[WINDOW] Creating window {0} ({1}, {2})", props.m_Title, props.m_Width, props.m_Height);

        if( s_GLFWwindowCount == 0){
            int success = glfwInit();
            LF_CORE_ASSERT(success, "[WINDOW] Could not initialize GLFW!");
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        if (Renderer::GetAPI() == RendererAPI::API::OpenGL) {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef LF_PLATFORM_MACOS
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        }

        glfwWindowHint(GLFW_RESIZABLE, props.m_Resize);

        #if defined(LF_DEBUG)
            if (Renderer::GetAPI() == RendererAPI::API::OpenGL)
                glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
        #endif

        m_Window = glfwCreateWindow((int)props.m_Width, (int)props.m_Height, m_Data.Title.c_str(), nullptr, nullptr);
        if (!m_Window)
            throw std::runtime_error("Failed to create an OpenGL 4.1 window");
        ++s_GLFWwindowCount;
        
        m_Context = GraphicsContext::Create(m_Window);
        m_Context->Init();

        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(true);

#ifndef LF_PLATFORM_MACOS
        stbi_set_flip_vertically_on_load_thread(0);
        GLFWimage images[1]; images[0].pixels = stbi_load("Data/images/logo.png", &images[0].width, &images[0].height, 0, 4); //rgba channels 
        glfwSetWindowIcon(m_Window, 1, images); 
        stbi_image_free(images[0].pixels);
#endif

        // Set GLFW callbacks
        Input::s_SetCursorVisible = [this](bool visible) {
            glfwSetInputMode(m_Window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        };

        Input::s_SetCursorPos = [this](float x, float y) {
            glfwSetCursorPos(m_Window, x, y);
        };
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height){
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;

            WindowResizeEvent event(width, height);
            data.EventCallback(event);
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            if (key >= 0 && key < 512) {
            if (action == GLFW_PRESS)
                Input::s_Keys[key] = true;
            else if (action == GLFW_RELEASE)
                Input::s_Keys[key] = false;
        }


            switch (action)
            {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
            }
        });

        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            KeyTypedEvent event(keycode);
            data.EventCallback(event);
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            if (button >= 0 && button < 32) {
                if (action == GLFW_PRESS)
                    Input::s_MouseButtons[button] = true;
                else if (action == GLFW_RELEASE)
                    Input::s_MouseButtons[button] = false;
            }

            switch (action)
            {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
            }
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            MouseScrolledEvent event((float)xOffset, (float)yOffset);
            data.EventCallback(event);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            Input::s_MousePos = { (float)xPos, (float)yPos };

            MouseMovedEvent event((float)xPos, (float)yPos);
            data.EventCallback(event);
        });
        
    }

    void Window::Shutdown(){
        LF_PROFILE_FUNCTION();
        glfwDestroyWindow(m_Window);
        --s_GLFWwindowCount;

        if(s_GLFWwindowCount == 0){
            glfwTerminate();
        }
    }

    void Window::OnUpdate(){
        LF_PROFILE_FUNCTION();

        glfwPollEvents();
        m_Context->SwapBuffers();

        glm::vec2 currentPos = Input::GetMousePosition();
        s_MouseDelta = currentPos - s_LastMousePosition;
        s_LastMousePosition = currentPos;
    }

    void Window::SetVSync(bool enabled){

        LF_PROFILE_FUNCTION();

        if(enabled)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        m_Data.VSync = enabled;
    }

    bool Window::IsVSync() const{
        return m_Data.VSync;
    }

}
