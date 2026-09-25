#include "lfpch.h"
#include "Renderer/Framebuffer.h"
#include "Log.h"
#include "Renderer/Renderer.h"
#include "Core/LF_Assert.h"
#include "Platform/OpenGL/OpenGLFramebuffer.h"

namespace LevyeForge {
	
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLFramebuffer>(spec);
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
