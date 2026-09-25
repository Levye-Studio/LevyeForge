#pragma once

#include "Core/Config.h"
#include "Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace LevyeForge {

	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(GLFWwindow* windowHandle);

		virtual void Init() override;
		virtual void SwapBuffers() override;
	private:
		GLFWwindow* m_WindowHandle;
	};

}