# CrashEdit-VC milestones

> Active investigation: animation vertex transform limits remain unresolved. Read [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md), [RESEARCH_NOTES.md](RESEARCH_NOTES.md), and [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md) before changing frame boundaries, coordinate limits, or chunk-expansion behavior.

This file is the concise project-history index of verified milestones. It records implemented checkpoints and user-confirmed outcomes separately from pending work. A milestone is not a claim that every file variant or workflow is complete.

## Native C++ application foundation

- Established the native C++20/CMake application with Dear ImGui docking, GLFW and OpenGL, plus Windows/Linux/macOS CI.
- Built the workspace around resource hierarchy navigation, a persistent 3D zone viewport, bottom-docked tools, search, console/diagnostics, NSF open/drag-and-drop, and GOOL inspection.
- Added application preferences in `cevc.ini` rather than beside opened NSF files in `imgui.ini`.
- Added paired NSF/NSD save staging and recovery, with optional independent backups.
- Added native NSF parsing, entry navigation, and preservation-oriented raw resource handling.

These are foundational implementation milestones; broad retail archive and game validation remains a separate acceptance requirement.

## NSD editing and zone workflow

- Implemented native inspection and editing for ProtoNSD, OldNSD, standard Crash 2 NSD, and Crash 3 NSD layouts, retaining opaque/prelude/compression data where supported.
- Added spawn-point editing: add/delete/reorder/copy/paste, EID and coordinate changes, camera values, GOOL map inspection/editing, and Get Spawn from selected object/checkpoint data.
- Added staged paired NSF/NSD save behavior and recovery if replacing one output fails.
- Recorded the examined Crash 2 `00_eZ` spawn/checkpoint comparison as a sample-specific regression, not a universal coordinate rule.

**Still pending:** broad real-level round trips and original-game validation across NSD variants.

## Scenery renderer and viewport

- Added GPU scenery rendering with textured Crash 2 scenery, transparency, and a software fallback.
- Traced scenery texture entities to connect decoded texture pages to actual polygon rendering; texture-page matching alone was insufficient.
- Added depth-buffer controls and corrected overlay/object occlusion behavior when depth testing is enabled.
- Corrected skybox scale against the original viewer for the examined zone and added neighboring-zone rendering.
- Added viewport camera/renderer/diagnostic controls.

Renderer parity in a viewer is not proof of game-runtime behavior; retain representative scenery regression files.

## GOOL and native instruction research

- Implemented native GOOL entry/header, instruction, data-pool, state-map/state-descriptor, and Crash 2 frame-group parsing with bounded diagnostics and raw-data retention.
- Audited Crash 2 opcode operand formats against the original CrashEdit source, including writable destination references, special source-reference encodings, unresolved references, and version-scoped opcode assignments.
- Added native MIPS decoding for common R3000A instructions and documented the parser's exact GOOL/MIPS mode-return raw-word convention.
- Identified a valid PS1 COP2/GTE instruction in a real GOOL native-code stream; this is documented research, not implemented GTE decoding.
- Inspected real GOOL samples `DispC` and `WillC`, confirming their item layouts and frame-group records consume the sample data exactly.

The longer opcode audit and corrections live in [gool-crash2-instruction-audit.md](gool-crash2-instruction-audit.md) and [RESEARCH_NOTES.md](RESEARCH_NOTES.md). Read the source-correction note before changing the MIPS mode-return sentinel.

## Animation vertex inspection milestone — 2026-11-09

**User-confirmed:** interactive vertex inspection works in the 3D Animation Editor.

- Toggle vertex markers from viewport settings or V.
- Hover/select individual vertices; selected vertex coordinates appear in the Animation Frame Editor.
- Marker rendering respects depth/object position and works in wireframe independently of Collision Viewer.
- Corrected marker rendering state so animation blend state does not make markers black.

This milestone is inspection/selection only; it did not yet establish safe temporal vertex serialization.

## Model format research and structural Model Editor

- Re-audited the authoritative reference against CrashEdit-RE, `airumu/CrashEdit` branch `t/net9` (not the deprecated cbhacks source).
- Confirmed Model topology/material data is separate from pose-specific Animation frame data; the Model contains polygon commands, colors, texture descriptors, TPAG references, extended textures, and optional compressed-position records.
- Implemented the structural Model Editor pages: General, Polygons, Colors, Textures, Extended Textures, and conditional Positions.
- Added raw polygon command inspection alongside reconstructed triangles; retained the stateful command-stream interpretation rather than treating each word as a conventional index.
- Added native Model decoding and regression comparisons for real `El1eG` and `Cr10G` samples.

Do not add a Model Vertices page: visible pose vertices are reconstructed from Animation frames. See [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md) for the integrated Model/Animation design and remaining serializer work.

## Face selection, material editing, and Face Texture Selector

- Added stable reconstructed-triangle/rendered-face mapping, depth-aware hover and click selection, and selected-face identity/reference display.
- Added face-level color editing, per-corner color references, texture assignment, and texture-descriptor editing with bounded validation.
- Added in-viewport Face Texture Selector for moving a face's existing UV mapping across the atlas while preserving its relative UV shape.
- Fixed selector image/hit-test fitting and the UV4 constraint that incorrectly restricted placement of the visible UV1–UV3 triangle.
- Commits: [2d4b731](https://github.com/leorelic/CrashEdit-VC/commit/2d4b731), [7043352](https://github.com/leorelic/CrashEdit-VC/commit/7043352), [1e6a172](https://github.com/leorelic/CrashEdit-VC/commit/1e6a172).
- **User-confirmed 2026-11-12:** TNT Box atlas selection/placement works. This validation is specific to that tested model/descriptor path.

## First in-game NSF texture mod — 2026-11-12

**User-confirmed end-to-end:** edited TNT Box face texture in CrashEdit-VC, saved the NSF, reopened it successfully in CrashEdit-RE, then tested it in Crash 2; the supplied screenshot showed the edited textures rendering in-game.

Verified chain: VC edit → NSF save → RE reopen → original game.

This proves the tested fixed-size texture-descriptor edit/save path, not universal model/color-mode compatibility or variable-size Model serialization.

## NSF chunk expansion for Animation Append/Insert — 2026-11-12

- Confirmed type-0 NSF chunks have a 64 KiB buffer and entry-offset table with a terminal used-data offset.
- Added capacity preflight, user confirmation, complete raw-entry relocation to a new type-0 chunk, source chunk table rebuild, validation before commit, selection/resource refresh, and retry of the requested append/insert.
- Added synthetic overflow, preservation, append, and save/reload regression coverage.
- Implementation checkpoint: [c200773](https://github.com/leorelic/CrashEdit-VC/commit/c200773d4354096dcd84805b8868d1b0f8f8bb15).
- **User-confirmed:** frame-growth relocation workflow worked as intended; CI passed Windows, Ubuntu, and macOS in [workflow 43152396834](https://github.com/leorelic/CrashEdit-VC/actions/runs/43152396834).

This is a genuine archive-capacity feature for Append/Insert. It is not a remedy for animation coordinate/encoding limits; do not conflate the two issues.

## TNT Box animation / GOOL relationship research — paused

- Preserved original, unmodified `BT10V` as the correct one-frame Animation relationship sample for `BT10G` and BoxSC.
- Examined BoxSC state descriptors and instruction references, but the `ANIM`/`ANIF` runtime semantics are not yet sufficient to infer a complete state machine.
- Decision: pause the temporary state-machine viewer; resume when GOOL decompilation/readable script work can explain the execution semantics. Keep existing GOOL inspector and State Descriptions unchanged.

## Animation/Model editor sprint — 2026-11-19

### Vertex selection and camera input
- Added click replace, Shift-add, Ctrl-remove, and drag-box selection; improved hover picking/depth tie-break and selected/hovered markers.
- Separated camera input from selection: middle-drag orbit, right-drag pan, wheel zoom; gated camera controls while selection/transform/collision interactions own the pointer.
- Dense-model usability and X-ray selection remain pending user validation.

### Bone Rig sidecar and per-frame rest poses
- Added editor-only Bone Rig sidecar data with vertex membership, pivots, per-frame rotation, rest positions, and frame offsets.
- Sidecar v2 persists per-frame rest data while v1 remains readable; load validation rejects invalid frame keys, non-finite values, duplicate assignments, invalid parents, and hierarchy cycles.
- Rotation recalculates from cached rest poses rather than repeatedly rotating quantized positions. Move/Scale and membership/pivot edits invalidate affected rest caches.
- Parent links remain metadata only; child transforms do not yet propagate through a hierarchy.

### Transform and archive regression
- Added numerical pivot editing/recentering and optional bone-pivot Move/Scale.
- Transform operations reject out-of-range or unrepresentable results rather than silently clamping; retain the last valid pose.
- Added synthetic tests for atomic rejection, rebasing, compressed multi-vertex edits, collision-byte preservation, and save/reload.

### Real Crash 2 Model/Animation parity
- Compared all 64 `El1eG` and 574 `Cr10G` reconstructed triangles against CrashEdit-RE, including vertex indices, corner colors, texture refs/flags, and triangle type/subtype.
- Confirmed sample frame counts and Model links: `El1eV` 7 frames × 38 vertices; `Cr10V` 11 frames × 309 vertices.
- Native sample harness checks golden hashes and entry save/reload preservation for these private samples. This is entry-level compatibility evidence, not full archive/game validation.
- Reference/sample workflow: [private sample workflow 44416679912](https://github.com/leorelic/CrashEdit-VC-Game-Samples/actions/runs/44416679912).

## Animation vertex save reliability — 2026-11-23

**Checkpoint:** [4b6592852d761d742358313b0250add7112e411e](https://github.com/leorelic/CrashEdit-VC/commit/4b6592852d761d742358313b0250add7112e411e).

- Fixed uncompressed Animation frame saving by preserving the complete desired pose between component writes and verifying final decoded coordinates.
- Added a synthetic multi-vertex/multi-axis save and reload regression.
- **User-confirmed in-game:** Crash's `Cr10V` ear-vertex edits and `BT10V` edits persist when representable by the frame encoding.
- Known limitation: `Cr10V` edits can still be rejected outside the frame encoder's representable range even if they appear within the viewer boundary.
- Earlier reference [bf646ab](https://github.com/leorelic/CrashEdit-VC/commit/bf646ab4c18cc7a4a9111cd556774816b62c4bbd) is superseded for ongoing work by the November 23 checkpoint.

## Animation preview/save recovery checkpoint — 2026-11-26

**Checkpoint:** [ff5359af45779fcd29b3827a563215b433cef1ac](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac).

- User reports the animation preview looks correct again and saving works.
- This is a stable recovery/reference checkpoint, **not a fix for the original vertex-transform limitation**.
- Native regression and private real-sample checks passed for representative `Cr10G/Cr10V` and `El1eG/El1eV`; automated checks are not user in-game validation.

## Current unresolved issue — Animation vertex coordinate/encoding limits

**User-reported:** certain vertices cannot move beyond a particular location on the X/Y/Z gizmo. The editor prompts to move the Animation entry into a new NSF chunk, but the requested edit does not apply.

**Corrections after the unchanged real-sample test: the native unified frame array was incorrectly writing compressed Model positions at unshifted indices, overwriting the special prefix and leaving the final specialVertexCount vertices unencoded. Decoder, encoder, and expansion now map ModelPosition i to frame.vertices[specialVertexCount+i], matching the renderer's triangle index offset. The native special-vertex setter also refreshes temporal-word cache from the raw bytes before decoding; otherwise a special edit could be overwritten by stale compressed bits in a non-word-aligned prefix. Shared Model expansion probes existing widths and clones the compressed Model only when the edited pose needs width expansion, redirecting only that Animation so unrelated animations retain the original Model. Pending-pose relocation uses exact entry location (duplicate-EID regression), and both selected/special vertex coordinate widgets have explicit unique IDs.

Implementation/test commits: [9c7b26b](https://github.com/leorelic/CrashEdit-VC/commit/9c7b26b1eae4c799cb20c19854ee0a8765f18724), [7309155](https://github.com/leorelic/CrashEdit-VC/commit/7309155e1cc58379163a4acf996fecf4fecad321), [aba9be9](https://github.com/leorelic/CrashEdit-VC/commit/aba9be96b897149ab656a158da6c2040824c714e), [c084b97](https://github.com/leorelic/CrashEdit-VC/commit/c084b97a953d8146219b7445feb4961fda851994), and [9feefdd](https://github.com/leorelic/CrashEdit-VC/commit/9feefdda2e0eb88aac222b73dab2bd9cc854127e). The latest CI run is [45889342090](https://github.com/leorelic/CrashEdit-VC/actions/runs/45889342090); CI is pending and no real Cr10V save/reopen/game test has passed yet.

- After a passing build, test the Cr10V right-ear edit in the real 4-special-vertex frame; verify the pose survives save/reopen.
- If a capacity prompt appears, confirm only the edited Cr10V Animation entry is moved; unrelated animations must remain in their original chunks.
- Confirm the conflicting-ImGui-ID warning no longer appears when hovering the decoded-vertex editor.
- Acceptance: requested coordinates persist, the pose is exact after reload, no unrelated Animation entry is relocated, and the UI remains free of the ID warning.

Preserve `ff5359a` as the known-good preview/save baseline while investigating.
