# CrashEdit-VC — native C++ editor

CrashEdit-VC is an independently implemented native editor for the Crash Bandicoot game data formats. The goal is to make inspecting, understanding, editing, testing, and creating game content accessible through a visual workflow—not to require users to work directly with raw binary structures or assembly for everyday tasks.

The native application is a separate C++ rewrite; it does not replace or compile the original C# CrashEdit application.

## Project goals and current status

This is the high-level project checklist. The detailed implementation plans and format research live in the linked roadmap documents. Status describes the current project state, not a promise that every game/version/file variant is already supported.

| Goal | Status | Scope |
|---|---|---|
| Native Windows, Linux and macOS support | **Implemented** | Cross-platform C++ application and CI build matrix. |
| GOOL decompilation and recompilation | **In progress** | Readable analysis/decompilation and Blueprint foundations exist. Full semantic coverage, direct authoring, compiler and safe round-trip recompilation remain future work. |
| Create and edit custom geometry, models and animations | **In progress** | Existing Model/Animation data can be inspected and edited; topology creation, variable-size serialization and full custom-content workflows remain future work. |
| Usable workflow, keyboard shortcuts and rearrangeable windows | **Implemented / ongoing polish** | Dear ImGui docking workspace, editor panels, shortcuts and viewport controls. |
| Undo and redo | **Implemented** | Continue validating coverage as new editing operations are added. |
| Model face textures, texture descriptors and CLUT details | **Implemented foundation** | Visual face-texture workflow and CLUT/texture inspection are available; broader format and game-version validation continues. |
| Easy-to-use Zone Editor GUI | **In progress** | Zone/resource inspection and editor foundations are being expanded. |
| Zone viewer with visible entities | **In progress** | Entity-aware viewing and interaction need further development. |
| Human-readable entity properties using GOOL context | **Planned** | Replace unexplained argument numbers with names, types and context derived from verified GOOL semantics. |
| Zone effects in the viewer | **Planned** | Visualize relevant environmental and gameplay effects. |
| Camera creator and editor | **Planned** | Create, inspect and edit level cameras through a visual workflow. |
| Build PS1 BIN/CUE images from a selected Crash game folder | **Planned** | Package the required game files into a testable disc image, subject to user-provided game data. |
| Direct Play / launch workflow | **Planned** | Build or select a BIN/CUE and launch it in a configured emulator. |
| Music and sound-effect player/replacer | **Planned** | Inspect, preview, replace and validate audio resources without assuming undocumented playback metadata. |
| Collision creator and editor | **Partially implemented** | Existing collision boxes can be inspected and edited; creation and broader authoring workflows remain. |
| NSF/NSD diagnostics and repair assistance | **In progress / planned** | Improve malformed-data detection, actionable diagnostics, safe recovery and explicit repair operations. |
| Reliable NSF/NSD saving | **Implemented foundation; validation ongoing** | Supported edits have regression and real-sample save/reload coverage. Full archive and original-game compatibility must continue to be tested per feature and format. |

### Additional project-wide goals

- **Visual-first authoring:** expose meaningful game concepts and properties in editor tools. Keep raw bytes, hex and assembly available for inspection and specialist work, but not as the default workflow.
- **Format safety:** preserve unknown fields and untouched data; validate offsets, counts, indices, references and encoding constraints before writing.
- **Honest warnings, not arbitrary UI restrictions:** where the game format has a representational limit, show users the valid range and warn when content falls outside it. Avoid silently clamping, wrapping or discarding user edits. For example, the Animation Editor is gaining an optional green/red 0–254 frame-bounds guide; it is intended as a visual warning, not a transform lock.
- **Real-game validation:** distinguish parser tests, synthetic tests, real-sample save/reload, original-editor compatibility and successful in-game behavior. A pass at one level does not imply all the others.
- **Crash 2 first:** use Crash 2 as the primary compatibility target. Treat Crash 3-specific layouts and behavior as a separate validation track.
- **Extensible workspace:** keep editors dockable and workflows consistent, with configurable shortcuts and useful contextual inspectors.
- **Documentation and regression fixtures:** record confirmed format behavior, uncertainty and unsupported cases; add tests whenever a format interpretation becomes reliable.
- **Independent implementation and licensing:** keep CrashEdit-VC's MIT-licensed implementation independent. Do not copy code, assets, dependencies or implementation designs from projects whose licenses or reuse permissions are incompatible.

## Detailed roadmaps

- [GOOL Development Roadmap](docs/GOOL_ROADMAP.md) — inspection, decompilation, native MIPS analysis, future authoring and recompilation.
- [Model Editor Roadmap](docs/MODEL_EDITOR_ROADMAP.md) — Model/Animation editing, visual transforms, Bone Rig foundation, collision editing, Scenery/Geometry work and PS1 texture import/export.
- [Project Milestones](docs/MILESTONES.md) — verified checkpoints, user-tested progress and remaining acceptance work.
- [NSF Entry Support Research](docs/NSF_ENTRY_SUPPORT_RESEARCH.md) — resource layouts, decoding research and compatibility notes.
- [Research Notes](docs/RESEARCH_NOTES.md) — confirmed findings, open questions and implementation evidence.

## Build

### Requirements
- C++20 compiler
- CMake 3.24+
- OpenGL development files
- Internet access for the first CMake configure (UI dependencies are fetched from source)

### Build and test
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Development principles

1. Inspect and understand the format before writing it.
2. Keep parsing, editing logic, rendering and UI responsibilities separate.
3. Preserve original data wherever possible and retain unknown fields.
4. Make edits transactional and reject corrupt writes rather than partially modifying files.
5. Test on synthetic fixtures and real Crash 2 samples; use original-game testing for important authoring milestones.
6. Do not mark a feature universally complete based only on one model, one file or one successful build.
