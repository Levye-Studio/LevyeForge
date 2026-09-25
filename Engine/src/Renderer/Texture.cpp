#include "lfpch.h"
#include "Renderer/Texture.h"
#include "Core/LF_Assert.h"
#include "Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"
namespace LevyeForge {
	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(width, height);
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(path);
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	Ref<Texture2D> Texture2D::Create(unsigned int id)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(id);
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, GLenum internalFormat, GLenum dataFormat)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(width, height, internalFormat, dataFormat);
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(Ref<Framebuffer>& buffer)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    LF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(buffer);
		}

		LF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
