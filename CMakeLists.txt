cmake_minimum_required(VERSION 3.24)
project(CrashEdit VERSION 0.1.0 LANGUAGES CXX)

option(CRASHEDIT_BUILD_APP "Build the GLFW / Dear ImGui application" ON)
option(CRASHEDIT_BUILD_TESTS "Build CrashEdit format tests" ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CRASHEDIT_BUILD_APP)
  include(FetchContent)
  find_package(OpenGL REQUIRED)
  FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG d682dc41f58b98c9343ffbaa7d94f07e9024b3ca GIT_SHALLOW FALSE)
  FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(imgui glfw)

  add_library(crashedit_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(crashedit_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_link_libraries(crashedit_imgui PUBLIC glfw OpenGL::GL)

  add_executable(CrashEdit WIN32
    src/main.cpp src/app/application.cpp src/core/workspace.cpp
    src/crash/formats/nsf/nsf_archive.cpp src/ui/editor_ui.cpp)
  target_include_directories(CrashEdit PRIVATE src)
  target_link_libraries(CrashEdit PRIVATE crashedit_imgui glfw OpenGL::GL)
  if(MSVC)
    target_compile_options(CrashEdit PRIVATE /W4 /permissive-)
  else()
    target_compile_options(CrashEdit PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  install(TARGETS CrashEdit RUNTIME DESTINATION . BUNDLE DESTINATION .)
endif()

if(CRASHEDIT_BUILD_TESTS)
  enable_testing()
  add_executable(crashedit_nsf_tests tests/nsf_archive_tests.cpp src/crash/formats/nsf/nsf_archive.cpp)
  target_include_directories(crashedit_nsf_tests PRIVATE src)
  add_test(NAME nsf_archive_tests COMMAND crashedit_nsf_tests)
endif()
