#include "lfpch.h"
#include "Renderer/GraphicsContext.h"
#include "Renderer/Renderer.h"
#include "LF_Assert.h"
#include "Platform/OpenGL/OpenGLContext.h"

namespace LevyeForge {

    Scope<GraphicsContext> GraphicsContext::Create(void* window)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope<OpenGLContext>(static_cast<GLFWwindow*>(window));
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
