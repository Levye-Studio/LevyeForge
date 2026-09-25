#include "lfpch.h"
#include "Renderer/Skybox.h"
#include "Renderer/Renderer.h"
#include "Core/LF_Assert.h"
#include "Platform/OpenGL/OpenGLSkybox.h"

namespace LevyeForge {

    Ref<Skybox> Skybox::Create(const std::vector<std::string>& faces){
        switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLSkybox>(faces);
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
    }

}
