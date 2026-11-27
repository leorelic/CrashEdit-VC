# CrashEdit-VC milestones

> Animation status: the tested Complex-to-Normal conversion and edited Crash ear geometry have survived VC save, CE-RE round-trip, and original-game playback. Direct editing in the original Complex representation and broader resource compatibility remain open. Read [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md) and [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md) before changing frame decoding, conversion, or chunk handling.

This file is the concise project-history index of verified milestones. It records implemented checkpoints and user-confirmed outcomes separately from pending work. A milestone is not a claim that every file variant or workflow is complete.

## Independent development and licensing boundary

**Hard project constraint — preserve CrashEdit-VC's MIT license.** CrashEdit-VC must remain an independently implemented MIT-licensed project. The following repositories are deliberately excluded as sources of code, implementation, dependencies, design templates, feature checklists, or implementation inspiration:

- **Airumu / Blender_CrashXform** — GPL-3.0. This exclusion applies to Model/Animation Editor work. Do not inspect or restudy this project, use its video or repository as a design reference, port or adapt its code, reproduce its implementation, or incorporate its dependencies, assets, or generated output.
- **ManDude / GOOC** — its licensing has not been established as compatible with CrashEdit-VC's MIT distribution. This exclusion applies to GOOL research. Do not inspect it for GOOL decompilation guidance, copy or adapt its source, compiler/decompiler logic, tools, dependencies, assets, generated output, or implementation patterns.

These are deliberate exclusions, not tasks to revisit. Do not add features to CrashEdit-VC merely to match either project, and do not use either repository as a roadmap or comparison checklist. Build features from CrashEdit-VC's own code and accomplishments, independent analysis of Crash 2 data and runtime behavior, primary technical evidence that can be used consistently with the MIT goal, and our own regression fixtures and tests.

When planning or implementing a feature, check this boundary first. If a proposed approach depends on either excluded project, stop and design an independent approach instead. Do not assume that public availability grants permission to reuse code or other copyrighted implementation material.


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

## Complex-frame editing limitation and validated conversion route

Direct editing of the original Complex (compressed ModelPosition) representation remains a separate unresolved capability. Do not confuse this with NSF chunk capacity or the known-good preview mapping.

**Validated workaround — user-confirmed 2026-11-27:** convert the Complex Model and linked Animations to Normal, edit the larger Crash ear position, save through CrashEdit-VC, save/reopen through CrashEdit-RE (CE-RE), and test the edited result in Crash 2. The user confirmed the edited geometry works in-game. See [90f4a99](https://github.com/leorelic/CrashEdit-VC/commit/90f4a995e4f399cdd71a7d951ed27171ef6b9357).

This validates the tested resource set and edit only. It does not establish unrestricted coordinate ranges, universal Model/Animation compatibility, direct Complex-frame editing, or a complete multi-frame custom-animation workflow. Preserve the [ff5359a preview mapping](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac). Detailed source findings, conversion constraints, and regression requirements remain in [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md) and [RESEARCH_NOTES.md](RESEARCH_NOTES.md).

## Animation preview recovery — user-confirmed 2026-11-27

- **Known-good preview contract:** `decodeAnimationVertices()` matches `ff5359a`; compressed ModelPosition `i` is assigned to `frame.vertices[i]`. Renderer Model triangle indices continue to use `triangleIndex + specialVertexCount` in all relevant render/picking/overlay paths.
- User tested the recovery build and confirmed Cr10V renders correctly again.
- The shifted decoder mapping and removed renderer offset caused the repeated corrupted-preview builds. Do not reintroduce either change.
- This is a renderer/preview recovery only. Direct editing of the original Complex representation remains unresolved; the tested Complex-to-Normal conversion and edited-geometry workflow are separately validated in the later milestone below. Preserve this preview mapping.
- Reference baseline: [`ff5359a`](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac). Current research is in [RESEARCH_NOTES.md](RESEARCH_NOTES.md) and [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md).


## Animation format identification — Normal / Complex — 2026-11-27

**Checkpoint:** [e4aa8e5](https://github.com/leorelic/CrashEdit-VC/commit/e4aa8e57107e88467b3b05b9a36aa7dce975dfbc) — *Document animation format labels*.

**User-confirmed:** the 3D Animation Editor now displays the format classification beside the selected Model information, and the user verified the label in the running build.

- **Normal** identifies an Animation frame whose linked Model is uncompressed (`NsfModel::compressed == false`). User-tested reference: `BT10V` linked to `BT10G`.
- **Complex** identifies an Animation frame whose linked Model uses compressed ModelPosition records (`NsfModel::compressed == true`). User-tested reference: `Cr10V` linked to `Cr10G`.
- **Unknown** is displayed when the linked Model cannot be resolved; the UI does not guess.
- Classification follows the currently displayed frame's Model reference, using the existing Crash 2 `modelEid` / Crash 3 `linkedModelEid` resolution path.
- `specialVertexCount` and special vertices are independent frame-layout concepts; they do not determine Normal versus Complex.
- The label is informational only. It does not alter decoding, rendering, editing, or serialization.

The user supplied screenshots confirming `BT10V` displays **Normal** and `Cr10V` displays **Complex**. Preserve these as representative UI/regression references when changing animation identification. This milestone establishes clear editor terminology and identification; it does **not** claim that Complex animation vertex editing or unrestricted coordinate serialization is solved. The Cr10V preview recovery contract and the outstanding Complex edit/save investigation remain as documented above.


## Complex-to-Normal Model/Animation conversion with packed NSF relocation — 2026-11-27

**User-confirmed:** a Complex Model and its linked Animations convert successfully to Normal resources in the editor. The initial working checkpoint was [b846498](https://github.com/leorelic/CrashEdit-VC/commit/b846498a65bdf47ba4c95675a163f43655c3f5ac); follow-up work corrected destination chunk packing before accepting this as the capacity-aware milestone.

- Batch conversion now packs overflowed Animations into conversion-created type-0 chunks, calculating capacity from the complete converted entry size, growing offset table, prefix padding, and existing destination entries. It starts another chunk only when the next converted entry cannot fit.
- Relocation returns the actual destination entry index; conversion no longer assumes each destination chunk contains only one Animation.
- The selected type-0 chunk Properties panel displays **Remaining space** from the terminal used-data offset.
- Regression uses a one-frame Animation and a 20-frame Animation: both overflow their source chunk, both are packed into one new chunk, retain distinct EIDs, and survive archive Save/reopen. The original chunk retains its unrelated filler entry.
- Implementation checkpoint: [9673820](https://github.com/leorelic/CrashEdit-VC/commit/9673820bfa2104dd44596a1511eea187d459efa9).
- **CI verified:** the two-animation capacity/packing/save-reopen regression passed on Ubuntu and macOS in [workflow 46052914125](https://github.com/leorelic/CrashEdit-VC/actions/runs/46052914125). Windows was still building when this milestone note was written; do not claim Windows validation until its job completes.

**Major milestone — user-confirmed 2026-11-27:** the user saved the converted archive through CrashEdit-RE (CE-RE), then tested that saved file in Crash 2 and reported that it worked correctly in-game. This confirms the tested converted Model and linked Animations survive conversion, packed chunk relocation, archive save/round-trip, and actual game loading/playback. The milestone is [90f4a99](https://github.com/leorelic/CrashEdit-VC/commit/90f4a995e4f399cdd71a7d951ed27171ef6b9357).

Scope is the tested resource set; continue validating other models/animation layouts and Zone load-list edge cases separately. Detailed evidence and limits are recorded in [RESEARCH_NOTES.md](RESEARCH_NOTES.md).


## Edited larger Crash ear position confirmed in-game — 2026-11-27

**Major milestone — user-confirmed end-to-end success:** the larger Crash ear position made possible by Complex-to-Normal conversion has now been tested in the original game and works.

The validated pipeline is:
1. Convert the Complex Model and linked Animations to Normal.
2. Move Crash's ear to a substantially larger position previously unavailable in the Complex representation.
3. Save the edited model in CrashEdit-VC.
4. Save/reopen through CrashEdit-RE (CE-RE).
5. Load the edited result in Crash 2 and confirm the larger ear position works in-game.

This upgrades the preceding conversion milestone from “converted resources play in-game” to “actual edited geometry survives the full save/round-trip/game pipeline.” It is a major step toward fully custom animations because it validates a practical route for editing geometry beyond the former Complex coordinate/edit limitation.

**Not yet claimed:** a complete custom-animation workflow. Multi-frame pose authoring, reliable serialization of all frame edits, reference/chunk/Zone constraints, and full custom-animation playback still need their own validation. Exact coordinate limits and broad resource coverage also remain unmeasured. See [RESEARCH_NOTES.md](RESEARCH_NOTES.md) for the detailed evidence and scope.
