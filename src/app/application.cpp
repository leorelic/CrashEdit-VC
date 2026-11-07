#include "app/application.hpp"
#include "core/workspace.hpp"
#include "core/diagnostics.hpp"
#include "ui/editor_ui.hpp"
#include "render/gl_api.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <filesystem>
#include <cstdint>
#include <string>
#include <array>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
namespace CrashEdit {
static void glfwError(int c,const char*d){Diagnostics::log(DiagnosticLevel::Error,"GLFW "+std::to_string(c)+": "+(d?d:"unknown"));}
static EditorUI* editorUiForDrop=nullptr;
static void glfwDropFiles(GLFWwindow*,int count,const char** paths){if(editorUiForDrop)editorUiForDrop->handleDroppedFiles(count,paths);}

static std::filesystem::path executableDirectory();
static ImFont* loadUiFont() {
 const auto bundledFont=executableDirectory()/"fonts"/"JetBrainsMono-Regular.ttf";
 std::error_code ec;
 if(std::filesystem::is_regular_file(bundledFont,ec)){
  if(ImFont* font=ImGui::GetIO().Fonts->AddFontFromFileTTF(bundledFont.string().c_str(),28.0f)){
   Diagnostics::log(DiagnosticLevel::Info,"JetBrains Mono loaded as the modern UI font.");
   return font;
  }
 }
 Diagnostics::log(DiagnosticLevel::Warning,
  "Bundled JetBrains Mono font unavailable; modern UI font will use the ImGui default fallback.");
 return ImGui::GetIO().Fonts->AddFontDefault();
}

static std::filesystem::path executableDirectory() {
#ifdef _WIN32
 wchar_t buffer[32768]{};
 DWORD n=GetModuleFileNameW(nullptr,buffer,static_cast<DWORD>(sizeof(buffer)/sizeof(buffer[0])));
 if(n>0 && n<(sizeof(buffer)/sizeof(buffer[0]))) return std::filesystem::path(buffer).parent_path();
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
 glfwSetErrorCallback(glfwError);if(!glfwInit()){Diagnostics::log(DiagnosticLevel::Error,"GLFW initialization failed.");return 1;}
#if defined(__APPLE__)
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GLFW_TRUE);
#else
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#endif
 GLFWwindow*w=glfwCreateWindow(1440,900,"CrashEdit — native editor",nullptr,nullptr);if(!w){Diagnostics::log(DiagnosticLevel::Error,"OpenGL window creation failed.");glfwTerminate();return 1;}
 glfwMakeContextCurrent(w);
 std::string glError;
 if(!GL::load(glError)){Diagnostics::log(DiagnosticLevel::Error,"Required OpenGL functions are unavailable: "+glError);glfwDestroyWindow(w);glfwTerminate();return 1;}
 glfwSwapInterval(1);IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();static const std::string imguiIniPath=(executableDirectory()/"cevc.ini").string();io.IniFilename=imguiIniPath.c_str();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_ViewportsEnable;ImGui::StyleColorsDark();io.Fonts->AddFontDefault();loadUiFont();auto&style=ImGui::GetStyle();style.FramePadding=ImVec2(7.0f,5.0f);style.ItemSpacing=ImVec2(8.0f,6.0f);style.WindowPadding=ImVec2(10.0f,10.0f);
 ImGui_ImplGlfw_InitForOpenGL(w,true);
#if defined(__APPLE__)
 ImGui_ImplOpenGL3_Init("#version 150");
#else
 ImGui_ImplOpenGL3_Init("#version 330");
#endif
 Workspace workspace;EditorUI ui(workspace);editorUiForDrop=&ui;glfwSetDropCallback(w,glfwDropFiles);
 while(!glfwWindowShouldClose(w)){glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ui.draw();ImGui::Render();int x,y;glfwGetFramebufferSize(w,&x,&y);glViewport(0,0,x,y);glClearColor(.075f,.082f,.095f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());if(io.ConfigFlags&ImGuiConfigFlags_ViewportsEnable){auto*ctx=glfwGetCurrentContext();ImGui::UpdatePlatformWindows();ImGui::RenderPlatformWindowsDefault();glfwMakeContextCurrent(ctx);}glfwSwapBuffers(w);}
 ImGui::SaveIniSettingsToDisk(io.IniFilename);ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();editorUiForDrop=nullptr;glfwDestroyWindow(w);glfwTerminate();return 0;
}
}
