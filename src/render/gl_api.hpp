#pragma once

#include <GLFW/glfw3.h>
#include <cstddef>
#include <string>

namespace CrashEdit::GL {

using SizePtr = std::ptrdiff_t;

#define CRASHEDIT_GL_PROC(returnType, name, arguments) \
    using name##Proc = returnType (APIENTRY*) arguments; \
    inline name##Proc name = nullptr

CRASHEDIT_GL_PROC(void, GenVertexArrays, (int, unsigned int*));
CRASHEDIT_GL_PROC(void, BindVertexArray, (unsigned int));
CRASHEDIT_GL_PROC(void, DeleteVertexArrays, (int, const unsigned int*));
CRASHEDIT_GL_PROC(void, GenBuffers, (int, unsigned int*));
CRASHEDIT_GL_PROC(void, BindBuffer, (unsigned int, unsigned int));
CRASHEDIT_GL_PROC(void, BufferData, (unsigned int, SizePtr, const void*, unsigned int));
CRASHEDIT_GL_PROC(void, DeleteBuffers, (int, const unsigned int*));

CRASHEDIT_GL_PROC(unsigned int, CreateShader, (unsigned int));
CRASHEDIT_GL_PROC(void, ShaderSource, (unsigned int, int, const char* const*, const int*));
CRASHEDIT_GL_PROC(void, CompileShader, (unsigned int));
CRASHEDIT_GL_PROC(void, GetShaderiv, (unsigned int, unsigned int, int*));
CRASHEDIT_GL_PROC(void, GetShaderInfoLog, (unsigned int, int, int*, char*));
CRASHEDIT_GL_PROC(void, DeleteShader, (unsigned int));
CRASHEDIT_GL_PROC(unsigned int, CreateProgram, ());
CRASHEDIT_GL_PROC(void, AttachShader, (unsigned int, unsigned int));
CRASHEDIT_GL_PROC(void, LinkProgram, (unsigned int));
CRASHEDIT_GL_PROC(void, GetProgramiv, (unsigned int, unsigned int, int*));
CRASHEDIT_GL_PROC(void, GetProgramInfoLog, (unsigned int, int, int*, char*));
CRASHEDIT_GL_PROC(void, DeleteProgram, (unsigned int));
CRASHEDIT_GL_PROC(void, UseProgram, (unsigned int));
CRASHEDIT_GL_PROC(int, GetUniformLocation, (unsigned int, const char*));
CRASHEDIT_GL_PROC(void, UniformMatrix4fv, (int, int, unsigned char, const float*));
CRASHEDIT_GL_PROC(void, Uniform1i, (int, int));

CRASHEDIT_GL_PROC(void, EnableVertexAttribArray, (unsigned int));
CRASHEDIT_GL_PROC(void, VertexAttribPointer, (unsigned int, int, unsigned int, unsigned char, int, const void*));
CRASHEDIT_GL_PROC(void, DrawArrays, (unsigned int, int, int));
CRASHEDIT_GL_PROC(void, DrawElements, (unsigned int, int, unsigned int, const void*));

CRASHEDIT_GL_PROC(void, GenTextures, (int, unsigned int*));
CRASHEDIT_GL_PROC(void, BindTexture, (unsigned int, unsigned int));
CRASHEDIT_GL_PROC(void, DeleteTextures, (int, const unsigned int*));
CRASHEDIT_GL_PROC(void, ActiveTexture, (unsigned int));
CRASHEDIT_GL_PROC(void, TexParameteri, (unsigned int, unsigned int, int));
CRASHEDIT_GL_PROC(void, TexImage2D, (unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*));
CRASHEDIT_GL_PROC(void, PixelStorei, (unsigned int, int));
CRASHEDIT_GL_PROC(void, GenRenderbuffers, (int, unsigned int*));
CRASHEDIT_GL_PROC(void, BindRenderbuffer, (unsigned int, unsigned int));
CRASHEDIT_GL_PROC(void, DeleteRenderbuffers, (int, const unsigned int*));
CRASHEDIT_GL_PROC(void, RenderbufferStorage, (unsigned int, unsigned int, int, int));
CRASHEDIT_GL_PROC(void, FramebufferRenderbuffer, (unsigned int, unsigned int, unsigned int, unsigned int));
CRASHEDIT_GL_PROC(void, GenFramebuffers, (int, unsigned int*));
CRASHEDIT_GL_PROC(void, BindFramebuffer, (unsigned int, unsigned int));
CRASHEDIT_GL_PROC(void, DeleteFramebuffers, (int, const unsigned int*));
CRASHEDIT_GL_PROC(void, FramebufferTexture2D, (unsigned int, unsigned int, unsigned int, unsigned int, int));
CRASHEDIT_GL_PROC(unsigned int, CheckFramebufferStatus, (unsigned int));
CRASHEDIT_GL_PROC(void, GetIntegerv, (unsigned int, int*));
CRASHEDIT_GL_PROC(void, Viewport, (int, int, int, int));
CRASHEDIT_GL_PROC(void, ClearColor, (float, float, float, float));
CRASHEDIT_GL_PROC(void, Clear, (unsigned int));
CRASHEDIT_GL_PROC(void, Enable, (unsigned int));
CRASHEDIT_GL_PROC(void, Disable, (unsigned int));
CRASHEDIT_GL_PROC(void, DepthFunc, (unsigned int));
CRASHEDIT_GL_PROC(void, BlendFunc, (unsigned int, unsigned int));

#undef CRASHEDIT_GL_PROC

inline bool load(std::string& error) {
    error.clear();
#define CRASHEDIT_LOAD_GL(name) \
    name = reinterpret_cast<name##Proc>(glfwGetProcAddress("gl" #name)); \
    if (!name) { if (!error.empty()) error += ", "; error += "gl" #name; }
    CRASHEDIT_LOAD_GL(GenVertexArrays)
    CRASHEDIT_LOAD_GL(BindVertexArray)
    CRASHEDIT_LOAD_GL(DeleteVertexArrays)
    CRASHEDIT_LOAD_GL(GenBuffers)
    CRASHEDIT_LOAD_GL(BindBuffer)
    CRASHEDIT_LOAD_GL(BufferData)
    CRASHEDIT_LOAD_GL(DeleteBuffers)
    CRASHEDIT_LOAD_GL(CreateShader)
    CRASHEDIT_LOAD_GL(ShaderSource)
    CRASHEDIT_LOAD_GL(CompileShader)
    CRASHEDIT_LOAD_GL(GetShaderiv)
    CRASHEDIT_LOAD_GL(GetShaderInfoLog)
    CRASHEDIT_LOAD_GL(DeleteShader)
    CRASHEDIT_LOAD_GL(CreateProgram)
    CRASHEDIT_LOAD_GL(AttachShader)
    CRASHEDIT_LOAD_GL(LinkProgram)
    CRASHEDIT_LOAD_GL(GetProgramiv)
    CRASHEDIT_LOAD_GL(GetProgramInfoLog)
    CRASHEDIT_LOAD_GL(DeleteProgram)
    CRASHEDIT_LOAD_GL(UseProgram)
    CRASHEDIT_LOAD_GL(GetUniformLocation)
    CRASHEDIT_LOAD_GL(UniformMatrix4fv)
    CRASHEDIT_LOAD_GL(Uniform1i)
    CRASHEDIT_LOAD_GL(EnableVertexAttribArray)
    CRASHEDIT_LOAD_GL(VertexAttribPointer)
    CRASHEDIT_LOAD_GL(DrawArrays)
    CRASHEDIT_LOAD_GL(DrawElements)
    CRASHEDIT_LOAD_GL(GenTextures)
    CRASHEDIT_LOAD_GL(BindTexture)
    CRASHEDIT_LOAD_GL(DeleteTextures)
    CRASHEDIT_LOAD_GL(ActiveTexture)
    CRASHEDIT_LOAD_GL(TexParameteri)
    CRASHEDIT_LOAD_GL(TexImage2D)
    CRASHEDIT_LOAD_GL(PixelStorei)
    CRASHEDIT_LOAD_GL(GenRenderbuffers)
    CRASHEDIT_LOAD_GL(BindRenderbuffer)
    CRASHEDIT_LOAD_GL(DeleteRenderbuffers)
    CRASHEDIT_LOAD_GL(RenderbufferStorage)
    CRASHEDIT_LOAD_GL(FramebufferRenderbuffer)
    CRASHEDIT_LOAD_GL(GenFramebuffers)
    CRASHEDIT_LOAD_GL(BindFramebuffer)
    CRASHEDIT_LOAD_GL(DeleteFramebuffers)
    CRASHEDIT_LOAD_GL(FramebufferTexture2D)
    CRASHEDIT_LOAD_GL(CheckFramebufferStatus)
    CRASHEDIT_LOAD_GL(GetIntegerv)
    CRASHEDIT_LOAD_GL(Viewport)
    CRASHEDIT_LOAD_GL(ClearColor)
    CRASHEDIT_LOAD_GL(Clear)
    CRASHEDIT_LOAD_GL(Enable)
    CRASHEDIT_LOAD_GL(Disable)
    CRASHEDIT_LOAD_GL(DepthFunc)
    CRASHEDIT_LOAD_GL(BlendFunc)
#undef CRASHEDIT_LOAD_GL
    return error.empty();
}

} // namespace CrashEdit::GL
