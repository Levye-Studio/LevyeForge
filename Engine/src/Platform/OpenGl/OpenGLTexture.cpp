#include "lfpch.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Core/LF_Assert.h"
#include <stb_image.h>
#include <glad/glad.h>

// tmp
#include <GLFW/glfw3.h>

namespace LevyeForge {
	namespace {
		// Bound texture operations must not change the caller's texture state.
		struct TextureEditScope {
			GLint binding = 0, unpackAlignment = 0;
			TextureEditScope(GLuint texture) {
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
				glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
				glBindTexture(GL_TEXTURE_2D, texture);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			}
			~TextureEditScope() {
				glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
				glBindTexture(GL_TEXTURE_2D, binding);
			}
		};
	}

	OpenGLTexture2D::OpenGLTexture2D(unsigned int id){
		m_RendererID = id;
		m_OwnsTexture = false;
		m_IsLoaded = id != 0;
	}

	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		LF_PROFILE_FUNCTION();

		m_InternalFormat = GL_RGBA8;
		m_DataFormat = GL_RGBA;

		glGenTextures(1, &m_RendererID);
		TextureEditScope edit(m_RendererID);
		glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		m_IsLoaded = true;
	}

	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, GLenum internalFormat, GLenum dataFormat)
		: m_Width(width), m_Height(height),  m_InternalFormat(internalFormat), m_DataFormat(dataFormat)
	{
		LF_PROFILE_FUNCTION();

		glGenTextures(1, &m_RendererID);
		TextureEditScope edit(m_RendererID);
		glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		m_IsLoaded = true;
	}

	OpenGLTexture2D::OpenGLTexture2D(Ref<Framebuffer>& buffer){
		LF_PROFILE_FUNCTION();
		LF_CORE_INFO("Creating Texture2D from Framebuffer");
		m_RendererID = buffer->GetColorAttachmentRendererID();
		m_Width = buffer->GetSpecification().Width;
		m_Height = buffer->GetSpecification().Height;
		m_IsLoaded = true;
		m_OwnsTexture = false;
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
		: m_Path(path)
	{
		LF_PROFILE_FUNCTION();

		int width, height, channels;
		// Sprites use bottom-left UVs; ImGui previews invert their UVs.
		stbi_set_flip_vertically_on_load_thread(1);
		stbi_uc* data = nullptr;
		{			
			LF_PROFILE_SCOPE("stbi_load - OpenGLTexture2D::OpenGLTexture2D(const std::string&)");
			data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		}
			
		if (data)
		{
			m_IsLoaded = true;

			m_Width = width;
			m_Height = height;

			GLenum internalFormat = GL_RGBA8, dataFormat = GL_RGBA;

			m_InternalFormat = internalFormat;
			m_DataFormat = dataFormat;

			LF_CORE_ASSERT(internalFormat & dataFormat, "Format not supported!");

			glGenTextures(1, &m_RendererID);
			TextureEditScope edit(m_RendererID);
			glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, dataFormat, GL_UNSIGNED_BYTE, nullptr);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);			
		}
		else{
			LF_CORE_WARN("Could not load {0}", path);
		}
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
	  LF_PROFILE_FUNCTION();
	  
          if (m_OwnsTexture && m_RendererID) {
	    if(glfwGetCurrentContext())
	      glDeleteTextures(1, &m_RendererID);
          }

	  m_RendererID = 0;       
	}

	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		LF_PROFILE_FUNCTION();

		uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
		LF_CORE_ASSERT(size == m_Width * m_Height * bpp, "Data must be entire texture!");
		TextureEditScope edit(m_RendererID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{		
		LF_PROFILE_FUNCTION();
		LF_CORE_ASSERT(m_RendererID != 0, "Texture RendererID is 0!");
		GLint previousUnit = 0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &previousUnit);
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		glActiveTexture(previousUnit);
	}
}
