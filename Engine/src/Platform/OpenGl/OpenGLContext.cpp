#include "lfpch.h"
#include "Platform/OpenGL/OpenGLContext.h"
#include "Core/LF_Assert.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace LevyeForge {

	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		: m_WindowHandle(windowHandle)
	{
		LF_CORE_ASSERT(windowHandle, "Window handle is null!")
	}

	void OpenGLContext::Init()
	{
		LF_PROFILE_FUNCTION();

		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		LF_CORE_ASSERT(status, "Failed to initialize Glad!");

		LF_CORE_INFO("OpenGL Info:");
		LF_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		LF_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		LF_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));

		LF_CORE_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 1), "Levye Forge requires at least OpenGL version 4.1!");
	}

	void OpenGLContext::SwapBuffers()
	{
		LF_PROFILE_FUNCTION();

		glfwSwapBuffers(m_WindowHandle);
	}

}
