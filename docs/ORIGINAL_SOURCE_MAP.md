# Original source map
The original C# source remains on `t/net9`. This table maps major reference areas to the native C++ architecture; it is not a porting checklist, and the native implementation remains independent.
| Original area | Responsibility | Native C++ destination / status |
|---|---|---|
| `Core/Workspace.cs`, `Core/IResource.cs` | Workspace/resource contracts | `src/core/` |
| `Core/Controller/`, `Core/Verb/`, `Core/Command/` | Editing commands/controllers | `src/core/`, `src/editing/` |
| `Core/Import/`, `Core/Export/` | Import/export | `src/io/` |
| `Crash/LevelWorkspace.cs` | NSF/NSD workspace and EID index | `src/crash/workspace/` |
| `Crash/Formats/Crash Formats/NSF.cs` | NSF container | `src/crash/formats/nsf/` |
| `Crash/Formats/Crash Formats/NSD/` | NSD variants | `src/crash/formats/nsd/` |
| `Crash/Formats/Crash Formats/Zone/` | Zones, entities, cameras | `src/crash/formats/zone/` |
| Animation / Model viewers | Animation frames, linked Models, editor-side vertex-group Bone Rig and viewport gizmos | `src/ui/editor_ui.cpp`; Bone Rig is sidecar/editor metadata, not a game skeleton |
| `Crash/Formats/Crash Formats/GOOL/` | GOOL instruction analysis, control flow, decompilation, and script representation | `src/crash/formats/gool/`; NSF entry/container parsing remains in `src/crash/formats/nsf/` |
| `CrashEdit/Render/` | OpenTK renderer | `src/render/` (portable OpenGL) |
| `CrashEdit/`, `Crash.UI/` | Windows Forms UI | Dear ImGui panels |
