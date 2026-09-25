#include "lfpch.h"
#include "Renderer/RenderCommand.h"

namespace LevyeForge {

    Scope<RendererAPI> RenderCommand::s_RendererAPI = RendererAPI::Create();
}