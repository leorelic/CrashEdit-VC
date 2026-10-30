# Original source map
The original C# source remains on `t/net9`. This native rewrite is intentionally separate.
| Original area | Responsibility | Planned C++ destination |
|---|---|---|
| `Core/Workspace.cs`, `Core/IResource.cs` | Workspace/resource contracts | `src/core/` |
| `Core/Controller/`, `Core/Verb/`, `Core/Command/` | Editing commands/controllers | `src/core/`, `src/editing/` |
| `Core/Import/`, `Core/Export/` | Import/export | `src/io/` |
| `Crash/LevelWorkspace.cs` | NSF/NSD workspace and EID index | `src/crash/workspace/` |
| `Crash/Formats/Crash Formats/NSF.cs` | NSF container | `src/crash/formats/nsf/` |
| `Crash/Formats/Crash Formats/NSD/` | NSD variants | `src/crash/formats/nsd/` |
| `Crash/Formats/Crash Formats/Zone/` | Zones, entities, cameras | `src/crash/formats/zone/` |
| `CrashEdit/Render/` | OpenTK renderer | `src/render/` (portable OpenGL) |
| `CrashEdit/`, `Crash.UI/` | Windows Forms UI | Dear ImGui panels |
