#include "app/application.hpp"
#include "core/workspace.hpp"
#include "ui/editor_ui.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
namespace CrashEdit {
static void glfwError(int c,const char*d){std::cerr<<"GLFW "<<c<<": "<<(d?d:"unknown")<<'\n';}

static std::filesystem::path executableDirectory() {
#ifdef _WIN32
 wchar_t buffer[32768]{};
 DWORD n=GetModuleFileNameW(nullptr,buffer,static_cast<DWORD>(std::size(buffer)));
 if(n>0 && n<std::size(buffer)) return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
 std::uint32_t size=0; _NSGetExecutablePath(nullptr,&size);
 std::string buffer(size,' ');
 if(_NSGetExecutablePath(buffer.data(),&size)==0) return std::filesystem::weakly_canonical(std::filesystem::path(buffer)).parent_path();
#else
 char buffer[4096]{};
 auto n=readlink("/proc/self/exe",buffer,sizeof(buffer)-1);
 if(n>0) { buffer[n]='\0'; return std::filesystem::path(buffer).parent_path(); }
#endif
 std::error_code ec; return std::filesystem::current_path(ec);
}

int Application::run(){
 glfwSetErrorCallback(glfwError);if(!glfwInit()){std::cerr<<"GLFW initialization failed.\n";return 1;}
#if defined(__APPLE__)
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE);
#else
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#endif
 GLFWwindow*w=glfwCreateWindow(1440,900,"CrashEdit — native editor",nullptr,nullptr);if(!w){glfwTerminate();std::cerr<<"OpenGL window creation failed.\n";return 1;}
 glfwMakeContextCurrent(w);glfwSwapInterval(1);IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();static const std::string imguiIniPath=(executableDirectory()/"imgui.ini").string();io.IniFilename=imguiIniPath.c_str();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_ViewportsEnable;ImGui::StyleColorsDark();
 ImGui_ImplGlfw_InitForOpenGL(w,true);
#if defined(__APPLE__)
 ImGui_ImplOpenGL3_Init("#version 150");
#else
 ImGui_ImplOpenGL3_Init("#version 330");
#endif
 Workspace workspace;EditorUI ui(workspace);
 while(!glfwWindowShouldClose(w)){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ui.draw();ImGui::Render();int x,y;glfwGetFramebufferSize(w,&x,&y);glViewport(0,0,x,y);glClearColor(.075f,.082f,.095f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());if(io.ConfigFlags&ImGuiConfigFlags_ViewportsEnable){auto*ctx=glfwGetCurrentContext();ImGui::UpdatePlatformWindows();ImGui::RenderPlatformWindowsDefault();glfwMakeContextCurrent(ctx);}glfwSwapBuffers(w);}
 ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return 0;
}
}
