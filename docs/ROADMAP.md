# CrashEdit-VC — Project Progress & Roadmap

> Living project document. Update this file as implementation and format research are completed.
> Current development branch: `cpp-port-foundation`.
> Current game focus: **Crash Bandicoot 2**.

## Project vision

CrashEdit-VC aims to become a practical, cross-platform editor for Crash Bandicoot game data that is substantially easier to learn and use than the original CrashEdit.

This is not just a language or platform port. The long-term goal is to improve the entire modding workflow: make common tasks discoverable, make complex formats understandable, provide strong visual tools, catch mistakes before export, and document knowledge that has historically been difficult for newcomers to obtain.

A mature release should be easy to recommend because people can open it, understand what to do, make changes confidently, and produce valid game data without needing to reverse-engineer the editor itself.

## Product principles

1. **Usability is a core feature.** Prefer clear workflows, sensible defaults, contextual help, searchable actions, useful error messages, and safe operations over exposing raw format details everywhere.
2. **Preserve control for advanced users.** Friendly workflows should not remove access to low-level data, inspection, or expert controls.
3. **Visual feedback matters.** Show the game world and edited assets as faithfully as possible, with correct depth, materials, textures, and selection behavior.
4. **Protect user data.** Prefer predictable saves, validation, backups or recoverable operations, and explicit warnings when an export may be incomplete or lossy.
5. **Build on evidence.** Use original source code, format samples, controlled experiments, and in-game validation. Mark uncertain format behavior as unknown until verified.
6. **Document as we learn.** Record format discoveries, constraints, workflows, and test cases so knowledge is reusable by the whole modding community.
7. **Ship in contained steps.** Keep changes focused, testable, and compatible with the existing project rather than attempting a huge editor rewrite all at once.

## Current progress

### Foundation and application
- [x] Separate native C++ rewrite established; original C# application remains separate.
- [x] C++20 / CMake project foundation.
- [x] Dear ImGui docking workspace and portable OpenGL viewport foundation.
- [x] Dockable workspace panels, including resource hierarchy, properties, console, and asset browser.
- [x] Cross-platform Windows, Linux, and macOS CI build configuration.
- [x] NSF file loading foundation and initial physical chunk reader/tests.
- [x] NSF drag-and-drop into the application, including UTF-8 path handling.
- [x] Global NSF entry search window with type filters, text matching, and hierarchy navigation.
- [x] Default dock layout places entry search with Console / Diagnostics and GOOL Inspector with the 3D Zone Editor; existing saved layouts are migrated once.
- [x] Main editor host no longer scrolls; dockspace reserves room for the bottom status line so it remains visible within the application window.
- [x] Windows application runs without a separate console window; startup, GLFW, NSF loading, NSD availability, and save messages feed a bounded in-app Console / Diagnostics log with severity levels.
- [x] NSF and NSD save actions wired through the editor.
- [x] Initial NSD serialization support, including preservation of opaque/original data where required.

### Renderer and zone viewing
- [x] Initial zone geometry/viewer foundation.
- [x] Texture-page (TPAG) matching and textured-polygon identification in the current test zone.
- [ ] Finish reliable texture display in the viewport; matching texture records does not yet guarantee correct rendered output.
- [ ] Correct depth/occlusion behavior so scenery properly hides camera guides and scene objects where appropriate.
- [ ] Continue matching the original editor's zone-viewing behavior while improving clarity and controls.

### Format research
- [x] Began tracing the original CrashEdit source alongside the native implementation.
- [x] Identified SLST as a camera-dependent polygon sort/display list used by the original viewer path.
- [ ] Document the exact Crash 2 SLST binary structure and record semantics from source and real files.
- [ ] Confirm how Crash 2 camera positions map to SLST records.
- [ ] Establish what the game expects from SLST data at runtime; do not assume viewer behavior alone proves game behavior.

## Roadmap

The order is intentionally flexible: work should follow dependencies and the most useful contained next step.

### 1. Stabilize the Crash 2 viewing and file workflow
- Finish the current renderer depth/occlusion work.
- Resolve textured polygon rendering from TPAG data.
- Verify zone geometry, texture assignment, camera guides, and object visibility against known-good files and the original editor.
- Continue strengthening NSF/NSD round-trip safety and save error reporting.
- Add regression fixtures for every format or rendering bug fixed.

### 2. Complete the Crash 2 data foundation
- Expand NSF/NSD parsing and writing only as format behavior is understood.
- Implement and validate the Crash 2 zone, camera, entity, scenery, and related structures needed by the editor.
- Provide useful structured inspection rather than forcing users to rely on raw hex for routine tasks.
- Keep unknown or preserved fields safe during round trips.

### 3. SLST inspection, decoding, and generation
- Reverse-engineer and document Crash 2 SLST records using original source, representative files, and controlled experiments.
- Implement a parser/decoder and expose SLST contents in an inspection view.
- Integrate decoded sort lists into the renderer where appropriate.
- Build diagnostics for invalid references, missing polygons, duplicates, and other verifiable consistency problems.
- Design a generator for edited or newly created scenery and camera paths.
- Validate generated SLSTs in the actual game. Generation is not complete until the game accepts and displays the result correctly.

### 4. GOOL inspection, decompilation, and editing
- Expand instruction decoding using verified Crash 2 opcode definitions from the original source and independent samples.
- Resolve GOOL references against local/external data pools and object fields.
- Build control-flow analysis and a readable decompiler for selected GOOL entries, not an indiscriminate whole-game decompilation pass.
- Preserve unknown instructions and opaque data so inspection never damages unsupported code.
- Add safe instruction editing and reassembly, then validate modified objects in-game.
- Link level entities and other resources to their GOOL entries for direct object-to-code navigation.

### 5. Scenery and geometry editing
- Select and inspect scenery objects and polygons.
- Edit vertices, edges, and faces with move, rotate, and scale tools.
- Support mesh creation and editing, polygon/material assignment, and texture/UV workflows.
- Provide snapping, useful coordinate controls, selection filters, and clear transform feedback.
- Support zone boundaries, camera paths, and relevant scene placement data.
- Make it possible to create custom scenery rather than only rearrange existing assets.

### 6. A cohesive, beginner-friendly editing workflow
- Undo/redo and a reliable command-based editing model.
- Scene hierarchy, searchable resources, contextual properties, and multi-selection where appropriate.
- Viewport navigation and gizmos with predictable controls.
- Helpful validation and actionable explanations of export problems.
- Guided workflows for common tasks, while retaining advanced inspection tools.
- Consistent shortcuts, menus, terminology, and panel behavior.

### 7. Broader Crash game and asset support
- Expand beyond the current Crash 2 focus when the shared foundations are stable.
- Add resource-specific editors for textures/palettes, models, animation, audio/music, GOOL, and other supported formats.
- Improve import/export and compatibility through round-trip tests.
- Document game/version-specific differences instead of silently assuming formats are interchangeable.

### 8. Release quality and community documentation
- Clear installation and build instructions for supported platforms.
- User guide and task-oriented tutorials.
- Format notes and technical documentation for modders and researchers.
- Sample projects and known-good test assets where licensing permits.
- Regression test suite and documented compatibility status.
- A clear distinction between implemented, experimental, and unsupported features.

## GOOL research notes

The original CrashEdit source identifies GOOL resources as entry type 11. Its loader reads a 24-byte header, an instruction item containing 32-bit words, and a data-pool item containing 32-bit values. Format 1 entries additionally contain a 16-bit state map, 16-byte state descriptors, and optionally frame-group data. The header contains ID, class, format, heap base, event count, and entry count.

The native implementation currently parses these structures for the selected game version and exposes instruction words as opcode (high byte) plus raw 24-bit operand payload. This is an inspection foundation, **not yet semantic disassembly or decompilation**. Mnemonic tables, version-specific operand interpretation, control-flow recovery, editing, and reassembly remain future work. Unknown code and frame-group data must be preserved.

Reference source: [GOOL entry loader](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/GOOL/GOOLEntryLoader.cs), [GOOL entry model](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/GOOL/GOOLEntry.cs), and [GOOL instruction decoder](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/GOOL/GOOLInstruction.cs).

## SLST research notes

Current working understanding: SLST is a sort/display list associated with camera-dependent polygon visibility and rendering order. In the original CrashEdit source, the older viewer decodes a list for a camera position and uses its polygon references to decide which scenery polygons to render. This is more than simply clipping individual vertices outside the screen.

Important caveat: the exact Crash 2 record semantics, interpolation rules, and runtime constraints still need to be confirmed. Do not treat the current understanding as a complete specification, and do not implement a generator based only on assumptions from the viewer.

Useful upstream source locations (reference only; the native implementation belongs in this repository):
- Original source branch: [airumu/CrashEdit — t/net9](https://github.com/airumu/CrashEdit/tree/t/net9)
- [SLST format directory](https://github.com/airumu/CrashEdit/tree/t/net9/Crash/Formats/Crash%20Formats/SLST)
- [Old zone viewer](https://github.com/airumu/CrashEdit/blob/t/net9/CrashEdit/Controls/3D/OldZoneEntryViewer.cs)
- [Old scenery viewer](https://github.com/airumu/CrashEdit/blob/t/net9/CrashEdit/Controls/3D/OldSceneryEntryViewer.cs)
- [Newer scenery viewer](https://github.com/airumu/CrashEdit/blob/t/net9/CrashEdit/Controls/3D/SceneryEntryViewer.cs)

## Definition of success

CrashEdit-VC will be succeeding when a newcomer can complete common editing tasks without needing a veteran to explain hidden steps; an experienced modder can still inspect and control the underlying data; custom scenery can be built and exported with a dependable workflow; and generated files are validated in the target game.

The goal is not merely feature parity with the original CrashEdit. The goal is a better overall experience.
