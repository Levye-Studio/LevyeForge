#include "lfpch.h"
#include "Renderer/RendererAPI.h"
#include "Log.h"
#include "Core/LF_Assert.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace LevyeForge {

	RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;

	Scope<RendererAPI> RendererAPI::Create()
	{
		switch (s_API)
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope<OpenGLRendererAPI>();
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
