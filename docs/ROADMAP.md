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

Detailed technical findings, source comparisons, sample measurements, and implementation notes belong in [RESEARCH_NOTES.md](RESEARCH_NOTES.md), not in this high-level roadmap.

### Achieved
- [x] Established the native, cross-platform C++ application foundation.
- [x] Implemented core NSF/NSD loading, inspection, editing, and paired saving.
- [x] Added Crash 2 zone viewing with textured scenery, transparency, and depth-aware rendering.
- [x] Built GOOL entry inspection, including instruction decoding, data-reference resolution, state information, and frame-group parsing.
- [x] Added an initial MIPS decoder and native-code mode handling for GOOL instruction streams.
- [x] Established automated Windows, Linux, and macOS build workflows.

### In progress
- [ ] Validate file edits and renderer behavior against more real Crash 2 levels and the game itself.
- [ ] Complete the native MIPS decoder audit and investigate COP2/GTE instruction support.
- [ ] Continue reverse-engineering SLST and other formats needed for custom scenery.

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
- [x] Add Crash 2 opcode names and decode operand bitfields from the original CrashEdit instruction format definitions.
- [x] Add native MIPS-mode tracking and an initial R3000A instruction decoder for GOOL streams.
- [ ] Complete the MIPS decoder audit against original CrashEdit behavior, including its exact mode-return sentinel and delay-slot handling.
- [ ] Audit and add PlayStation COP2/GTE disassembly beyond the original CrashEdit decoder, preserving unknown encodings.
- [ ] Build a MIPS assembler before exposing safe native-instruction editing.
- [x] Resolve data-pool references when the required local or parent pool is available; explicitly label unresolved external pools and special references rather than guessing.
- [ ] Resolve object-field references to symbolic field names and connect external GOOL entries.
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

## Technical documentation

Detailed format specifications, reverse-engineering evidence, original-source comparisons, sample analyses, and implementation history are maintained in [RESEARCH_NOTES.md](RESEARCH_NOTES.md) and the focused audit documents under `docs/`. Keep this roadmap focused on project-level achievements and upcoming goals; put low-level implementation details in the research documentation.

## Definition of success

CrashEdit-VC will be succeeding when a newcomer can complete common editing tasks without needing a veteran to explain hidden steps; an experienced modder can still inspect and control the underlying data; custom scenery can be built and exported with a dependable workflow; and generated files are validated in the target game.

The goal is not merely feature parity with the original CrashEdit. The goal is a better overall experience.
