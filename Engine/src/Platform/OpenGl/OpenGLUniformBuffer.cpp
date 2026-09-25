#include "lfpch.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"

#include <glad/glad.h>

// tmp
#include <GLFW/glfw3.h>

namespace LevyeForge {

	OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size, uint32_t binding)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW); // TODO: investigate usage hint
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
	}

	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
          if (m_RendererID) {
	    if(glfwGetCurrentContext())
	      glDeleteBuffers(1, &m_RendererID);
          }

	  m_RendererID = 0;      
	}


	void OpenGLUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		GLint previousBuffer = 0;
		glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previousBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
		glBindBuffer(GL_UNIFORM_BUFFER, previousBuffer);
	}

}
