#include "app/application.hpp"
#include "core/workspace.hpp"
#include "ui/editor_ui.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
namespace CrashEdit {
static void glfwError(int c,const char*d){std::cerr<<"GLFW "<<c<<": "<<(d?d:"unknown")<<'\n';}
int Application::run(){
 glfwSetErrorCallback(glfwError);if(!glfwInit()){std::cerr<<"GLFW initialization failed.\n";return 1;}
#if defined(__APPLE__)
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE);
#else
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#endif
 GLFWwindow*w=glfwCreateWindow(1440,900,"CrashEdit — native editor",nullptr,nullptr);if(!w){glfwTerminate();std::cerr<<"OpenGL window creation failed.\n";return 1;}
 glfwMakeContextCurrent(w);glfwSwapInterval(1);IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_ViewportsEnable;ImGui::StyleColorsDark();
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
