# CrashEdit — native C++ foundation (0.1.0)

This is a separate native rewrite of CrashEdit. It does not replace or compile the original C# application.

**Project direction and current progress:** see [docs/ROADMAP.md](docs/ROADMAP.md).\n\n**Reverse-engineering discoveries and implementation notes:** see [docs/RESEARCH_NOTES.md](docs/RESEARCH_NOTES.md), including the distinction between original CrashEdit behavior, native improvements, and findings still under investigation.

## Stack
- C++20, CMake 3.24+
- Dear ImGui docking branch (pinned commit), GLFW 3.4
- OpenGL 3.3 core (OpenGL 3.2 core on macOS)

CMake fetches the two UI dependencies from source. A C++ compiler, CMake, OpenGL development files and internet access for the first configure are required.

## Build
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Foundation features
- Dockable editor workspace with a permanent central 3D Zone Editor placeholder.
- Right-side Resource Hierarchy and bottom Properties, Console and Asset Browser panels.
- Workspace folder browsing and basic file selection/metadata.
- Initial NSF physical chunk reader and tests for raw/compressed chunks.
- Windows, Linux and macOS GitHub Actions builds.

**Not yet implemented:** the original CrashEdit 3D renderer, full NSF/NSD resource semantics, zone/entity formats, resource-specific editors, import/export and round-trip compatibility. The viewport is currently a grid/orientation placeholder. Do not treat this as feature-equivalent to the original editor.

## Porting sequence
1. Binary readers/writers and stream abstractions.
2. NSF/NSD archives, entry IDs, legacy variants and workspace synchronization.
3. Zone headers, entities, cameras, properties, geometry and scenery.
4. Portable OpenGL renderer preserving original materials/shaders.
5. Hex, texture/palette, model, animation, audio/music and GOOL editors.
6. Import/export plus round-trip tests against original files.

Original C# source remains on branch `t/net9`; this rewrite lives on `cpp-port-foundation`.
