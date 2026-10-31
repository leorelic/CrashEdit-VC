cmake_minimum_required(VERSION 3.24)
project(CrashEdit VERSION 0.1.0 LANGUAGES CXX)

option(CRASHEDIT_BUILD_APP "Build the GLFW / Dear ImGui application" ON)
option(CRASHEDIT_BUILD_TESTS "Build CrashEdit format tests" ON)
option(CRASHEDIT_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CRASHEDIT_BUILD_APP)
    include(FetchContent)
    find_package(OpenGL REQUIRED)

    # Dear ImGui's docking branch provides the dockspace API used by this editor.
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(stb)
    if(NOT stb_POPULATED)
        FetchContent_Populate(stb)
    endif()

    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_Populate(imgui)
    endif()

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    add_library(crashedit_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(crashedit_imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(crashedit_imgui PUBLIC glfw OpenGL::GL)

    add_executable(CrashEdit
        src/main.cpp
        src/app/application.cpp
        src/core/workspace.cpp
        src/crash/formats/nsf/nsf_archive.cpp
        src/crash/formats/nsd/nsd_file.cpp
        src/ui/editor_ui.cpp
    )
    target_include_directories(CrashEdit PRIVATE src ${stb_SOURCE_DIR})
    target_compile_definitions(CrashEdit PRIVATE CRASHEDIT_SOURCE_ASSET_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/Embeds/Images\")
    add_custom_command(TARGET CrashEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CrashEdit>/Assets/Images"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Embeds/Images" "$<TARGET_FILE_DIR:CrashEdit>/Assets/Images"
    )
    target_link_libraries(CrashEdit PRIVATE crashedit_imgui glfw OpenGL::GL)
    if(WIN32)
        target_link_libraries(CrashEdit PRIVATE comdlg32)
    endif()

    if(MSVC)
        target_compile_options(CrashEdit PRIVATE /W4 /permissive-)
        if(CRASHEDIT_WARNINGS_AS_ERRORS)
            target_compile_options(CrashEdit PRIVATE /WX)
        endif()
    else()
        target_compile_options(CrashEdit PRIVATE -Wall -Wextra -Wpedantic)
        if(CRASHEDIT_WARNINGS_AS_ERRORS)
            target_compile_options(CrashEdit PRIVATE -Werror)
        endif()
    endif()

    install(TARGETS CrashEdit RUNTIME DESTINATION . BUNDLE DESTINATION .)
    install(DIRECTORY Embeds/Images/ DESTINATION Assets/Images)
endif()

if(CRASHEDIT_BUILD_TESTS)
    enable_testing()
    add_executable(crashedit_nsf_tests
        tests/nsf_archive_tests.cpp
        src/crash/formats/nsf/nsf_archive.cpp
    )
    target_include_directories(crashedit_nsf_tests PRIVATE src)
    if(MSVC)
        target_compile_options(crashedit_nsf_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(crashedit_nsf_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME nsf_archive_tests COMMAND crashedit_nsf_tests)

    add_executable(crashedit_nsd_tests
        tests/nsd_file_tests.cpp
        src/crash/formats/nsd/nsd_file.cpp
    )
    target_include_directories(crashedit_nsd_tests PRIVATE src)
    if(MSVC)
        target_compile_options(crashedit_nsd_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(crashedit_nsd_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME nsd_file_tests COMMAND crashedit_nsd_tests)
endif()
