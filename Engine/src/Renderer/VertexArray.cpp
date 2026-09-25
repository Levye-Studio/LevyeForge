#include "lfpch.h"
#include "Renderer/VertexArray.h"
#include "Core/LF_Assert.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"
#include "Renderer/Renderer.h"

namespace LevyeForge {

	Ref<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLVertexArray>();
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
