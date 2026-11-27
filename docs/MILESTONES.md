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

**Correction to earlier implementation notes:** the shifted unified-frame mapping claim below was wrong for preview rendering and was reverted. The decoder now matches the known-good `ff5359a` implementation, and renderer triangle consumers retain their special-vertex offset. The user visually confirmed Cr10V preview recovery on 2026-11-27. Do not change this decoder/renderer pairing while investigating the independent edit/save coordinate-limit problem.

The separate save-side work remains documented in the research notes: shared Model expansion/clone isolation, pending-pose relocation identity, and coordinate-input IDs are distinct concerns. They must not be treated as proof that unrestricted vertex editing works.

Implementation/test commits: [9c7b26b](https://github.com/leorelic/CrashEdit-VC/commit/9c7b26b1eae4c799cb20c19854ee0a8765f18724), [7309155](https://github.com/leorelic/CrashEdit-VC/commit/7309155e1cc58379163a4acf996fecf4fecad321), [aba9be9](https://github.com/leorelic/CrashEdit-VC/commit/aba9be96b897149ab656a158da6c2040824c714e), [c084b97](https://github.com/leorelic/CrashEdit-VC/commit/c084b97a953d8146219b7445feb4961fda851994), and [9feefdd](https://github.com/leorelic/CrashEdit-VC/commit/9feefdda2e0eb88aac222b73dab2bd9cc854127e).

- After a passing build, test the Cr10V right-ear edit in the real 4-special-vertex frame; verify the pose survives save/reopen.
- If a capacity prompt appears, confirm only the edited Cr10V Animation entry is moved; unrelated animations must remain in their original chunks.
- Confirm the conflicting-ImGui-ID warning no longer appears when hovering the decoded-vertex editor.
- Acceptance: requested coordinates persist, the pose is exact after reload, no unrelated Animation entry is relocated, and the UI remains free of the ID warning.

Preserve `ff5359a` as the known-good preview/save baseline while investigating.


## Animation preview recovery — user-confirmed 2026-11-27

- **Known-good preview contract:** `decodeAnimationVertices()` matches `ff5359a`; compressed ModelPosition `i` is assigned to `frame.vertices[i]`. Renderer Model triangle indices continue to use `triangleIndex + specialVertexCount` in all relevant render/picking/overlay paths.
- User tested the recovery build and confirmed Cr10V renders correctly again.
- The shifted decoder mapping and removed renderer offset caused the repeated corrupted-preview builds. Do not reintroduce either change.
- This is a renderer/preview recovery only. The original per-vertex coordinate/encoding limit and chunk-relocation/save issue remains unresolved. Solve it in the edit/encode path without disturbing the verified preview mapping.
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


## Complex-to-Normal conversion unlocks larger Crash ear edits — 2026-11-27

**User-confirmed major practical milestone:** after converting Crash's Complex Model/Animation resources to Normal, the user can move Crash's ear to much larger positions that were previously not possible. CrashEdit-VC saves the edited model successfully.

This is a follow-up to the [90f4a99 conversion/game-compatibility milestone](https://github.com/leorelic/CrashEdit-VC/commit/90f4a995e4f399cdd71a7d951ed27171ef6b9357):
- The converted Model and linked Animations were saved through CrashEdit-RE and confirmed to work in Crash 2.
- The new user test confirms larger ear-position edits can be made in the converted Normal representation and saved by CrashEdit-VC.
- The earlier Complex coordinate/edit limitation is therefore practically bypassed for the tested resource by conversion, without claiming unrestricted editing of every Model or coordinate.

**Scope / follow-up:** exact coordinate limits and broad model coverage remain unmeasured. Reopen the larger edited save in CE-RE and test it in-game to confirm the edited pose—not only the unchanged converted animation—survives the full pipeline. Zone load-list and chunk-capacity edge cases remain separate validation items.
