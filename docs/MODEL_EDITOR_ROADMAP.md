# Model Editor roadmap

## Independent development and licensing boundary

**Hard project constraint — preserve CrashEdit-VC's MIT license.** CrashEdit-VC must remain an independently implemented MIT-licensed project. The following repositories are deliberately excluded as sources of code, implementation, dependencies, design templates, feature checklists, or implementation inspiration:

- **Airumu / Blender_CrashXform** — GPL-3.0. Do not inspect or restudy this project, use its video or repository as a design reference, port or adapt its code, reproduce its implementation, or incorporate its dependencies, assets, or generated output.
- **ManDude / GOOC** — its licensing has not been established as compatible with CrashEdit-VC's MIT distribution. Treat it as unavailable for reuse. Do not inspect it for GOOL decompilation guidance, copy or adapt its source, compiler/decompiler logic, tools, dependencies, assets, generated output, or implementation patterns.

These are deliberate exclusions, not tasks to revisit. Do not add features to CrashEdit-VC merely to match either project, and do not use either repository as a roadmap or comparison checklist. Build features from CrashEdit-VC's own code and accomplishments, independent analysis of Crash 2 data and runtime behavior, primary technical evidence that can be used consistently with the MIT goal, and our own regression fixtures and tests.

When planning or implementing a feature, check this boundary first. If a proposed approach depends on either excluded project, stop and design an independent approach instead. Do not assume that public availability grants permission to reuse code or other copyrighted implementation material.

## Reference baseline and scope

**Authoritative comparison target:** [CrashEdit-RE — airumu/CrashEdit, branch t/net9](https://github.com/airumu/CrashEdit/tree/t/net9). The README identifies this as CrashEdit: Re and documents its Model / Compressed Model editors. Future research must use CrashEdit-RE and the appropriate t/net branch. Do not use cbhacks/CrashEdit or its deprecated branches as the project's reference implementation.

This roadmap targets Crash 2 first. The Model entry owns polygon topology, per-corner color selection, texture mapping, texture-page references, and (for compressed models) metadata used to decode animation-frame vertex positions. Animation frames own the changing pose data. The native archive already decodes both resources and links Animation frames to Models; the shared Model representation should remain the source of topology/material data consumed by the 3D Animation Editor.

**Agreed editor responsibility:** the Model Editor remains the structural/data inspector. Its Colors and Textures pages will stay available temporarily as research and raw-data inspection tools. The long-term visual workflow belongs in the Animation Editor: select reconstructed faces in 3D, apply colors or texture descriptors/pages to those faces, and write the changes back to the shared Model data. This avoids asking users to edit face materials through disconnected numeric tables when the corresponding geometry is visible. Do not remove the interim Model pages until the Animation Editor can replace their useful inspection and editing capabilities.

## CrashEdit-RE Model editor structure and interim research pages

The CrashEdit-RE ModelBox exposes these pages:
- **General:** model scale values and model information.
- **Polygons:** raw polygon command stream and reconstructed triangle list.
- **Colors:** editable model color table.
- **Textures:** texture descriptors and texture-page/CLUT mapping.
- **Extended Textures:** flipbook and LOD metadata.
- **Positions:** compressed-model position records only.

The separate Vertices page in ModelBox is removed for Model entries; it is used by Scenery. Do not copy that Scenery-specific page into the Model Editor without a distinct reason. The Model's visible pose is reconstructed with Animation frame data. CrashEdit-VC may retain General, Polygons, Colors, Textures, Extended Textures and conditional Positions pages as interim research/inspection tabs, but face-level material editing is intended to move to the Animation Editor.

Relevant source files on the reference branch:
- Crash/Formats/Crash Formats/Model/ModelEntryLoader.cs
- Crash/Formats/Crash Formats/Model/ModelEntry.cs
- Crash/Formats/Crash Formats/Model/ModelStruct.cs
- Crash/Formats/Crash Formats/Model/ModelTexture.cs
- Crash/Formats/Crash Formats/Model/ModelPosition.cs
- Crash/Formats/Crash Formats/Model/ModelExtendedTexture.cs
- Crash/Formats/Crash Formats/Model/ModelTransformedTriangle.cs
- Crash/Formats/Crash Formats/Animation/Frame.cs
- CrashEdit/Controls/ModelBox.cs

## Model entry layout confirmed by t/net9

The loader expects five items, or six when the optional compressed-position table is present:

| Item | Contents | Record size |
|---|---|---:|
| 0 | Model Info block | 0x50 bytes implied by fields through 0x4C; validate against samples |
| 1 | Polygon command words | 4 bytes per command |
| 2 | Color records (R, G, B, Extra) | 4 bytes each |
| 3 | Texture descriptors | 12 bytes each |
| 4 | Extended texture words | 4 bytes each |
| 5 (optional) | Compressed position records | 4 bytes each |

Important Info offsets read by the reference loader/model class:
- Scale X/Y/Z: 0x00, 0x04, 0x08.
- TPAG EID slots begin at 0x0C; TPAGCount is at 0x40.
- Texture descriptor count: 0x34.
- Vertex count: 0x38.
- Color count: 0x3C.
- Extended texture count: 0x48.
- Special vertex count: 0x4C.
- **Polygon command count used by ModelEntryLoader for item 1: 0x2C.**
- **Decoded polygon count: 0x44.** ModelEntry.PolyCount reads this field, and CrashEdit-RE ModelBox displays it as the model's Polygon count. It is distinct from the 0x2C polygon-command-word count used to size item 1. The native decoder uses 0x44 to validate the number of reconstructed triangles. These fields therefore describe different stages/counts; do not substitute one for the other.

The reference loader checks the item lengths against the associated counts, but the native implementation should add stricter bounds, overflow checks, and diagnostics rather than copying any unsafe assumptions.

## Polygon command stream and topology

The polygon item is a stream of 32-bit words, not a conventional triangle index buffer. ModelEntry.ConvertPolyItem() classifies:
- 0xFFFFFFFF as the footer.
- Words with nonzero upper 16 bits as ModelTriangle.
- Other words as ModelColor.

ModelTriangle.Load() decodes texture index, animated flag, color index, position key, unknown bits, original/duplicate index mode, flag, triangle type, and triangle subtype. ModelColor.Load() carries two color indices. Keep raw words and all unknown fields in the native representation.

ModelEntry.ConvertIndices() performs two passes:
1. Map original position keys to sequential vertex indices and resolve duplicate references.
2. Walk the decoded command stream, interpreting triangle types 0, 1 and 2 differently. It carries prior command/color state to reconstruct three vertex indices and three per-corner color indices for each triangle.

This is a stateful reconstruction algorithm. Do not simplify it into one command = one triangle. The reference code contains assumptions and at least one explicitly described dirty hack; reproduce its observed behavior carefully, but build native diagnostics for missing duplicate keys, invalid references, unsupported modes, truncated streams and invalid color/texture indices.

The decoded triangle model in CrashEdit-RE retains:
- Three vertex indices.
- Three per-corner color indices.
- Texture descriptor index.
- Triangle type and subtype.
- Animated flag.

The Model Editor should show both raw commands and reconstructed triangles. This makes it possible to inspect how command-level edits affect actual topology.

## Colors and texture mapping

### Colors

Item 2 contains four-byte color records: Red, Green, Blue and Extra. Reconstructed triangles reference three color-table indices, one per corner. The native renderer should resolve these indices from the Model resource rather than treating vertex color as animation-frame data.

### Texture descriptors

Item 3 contains 12-byte descriptors. CrashEdit-RE decodes four UV coordinate pairs (U1/V1 through U4/V4), CLUT X/Y fields, color mode, blend mode, segment, and texture-page offset. The descriptor also derives a texture rectangle and adjusted coordinates for display.

Triangle commands select a texture descriptor by index. The Model's TPAG EID array resolves texture-page references to NSF type-1 texture chunks. The native Model Editor must expose the relationship between descriptor → TPAG slot/page EID → texture chunk, and report unresolved or invalid references.

### Extended textures

Item 4 contains 32-bit extended-texture words. The reference implementation exposes an offset and an IsLOD flag. Depending on that flag, the remaining bits are interpreted as either LOD selectors or flipbook fields (mask, delay, latency and leap). Preserve the raw word and do not assume all entries have the same interpretation.

## Compressed positions and Animation frame decoding

Item 5 is present for compressed models. The loader computes the position-record count as:

Model.VertexCount - Model.SpecialVertexCount

Each 4-byte ModelPosition record contains packed X, Z and Y base values plus X, Z and Y bit widths. The exact bit extraction is implemented by ModelPosition.Load() and Save().

The reference Frame.MakeVertices(model) shows how these records affect animation frames:
- The directly stored special-vertex prefix is skipped in the temporal bitstream.
- For each compressed position record, temporal components are read in **X-Z-Y order**.
- Each component is sign-extended using its corresponding bit width.
- The decoded delta is added to its per-axis base value and accumulated.
- Coordinates are wrapped to 8 bits.
- If the model has no position records, the frame uses the uncompressed temporal-bit path.
- The reference applies an additional scale factor of 8 for its newer/Crash 3 frame mode; keep game-version behavior explicit and verify against Crash 2 samples.

This is the key Model-to-Animation dependency. Model position records are not the animated vertex coordinates themselves; they are the compression parameters needed to reconstruct those coordinates from each frame.

## Integrated implementation target

This is one connected Model + Animation Editor feature, not a sequence of user-facing mini-releases. Implement the full workflow against the checkpoint at `7cd6d28d366f14dabd0f07168d04ccace401981b`. Internal work may still be organized and tested in dependency order, but do not stop after each small phase for approval. If the integrated implementation proves unstable, the checkpoint is the rollback target.

### Shared editable Model representation

- [ ] Make the decoded Model representation the authoritative editable in-memory source for geometry and materials while retaining the original item bytes and all unknown fields.
- [x] Add explicit, validated fixed-size setters for Model scales, TPAG EIDs, color records, texture descriptors, extended-texture words, per-face texture flags and per-corner color references.
- [ ] Preserve untouched raw data and reject edits that cannot be serialized without losing unsupported information.
- [ ] Add a Model serializer that writes the existing five/six item layout back into the owning NSF entry, updates all affected item offsets/sizes and entry length/table boundaries safely, and preserves unrelated entries/chunks.
- [ ] Keep the original item count and optional compressed-position item semantics intact. Do not silently convert compressed Models into uncompressed ones.
- [ ] Re-decode edited Model data after writes so UI state always reflects serialized bytes, not stale cached structs.

### Model Editor: structural inspection

- [x] Add the structural Model Editor pages: General, Polygons, Colors, Textures, Extended Textures and conditional Positions.
- [x] Show raw polygon commands alongside reconstructed triangles, with useful diagnostics for unsupported commands and invalid indices.
- [ ] Expose TPAG EID references and their resolved type-1 texture chunks.
- [x] Keep Colors and Textures as useful raw/structured inspection and fallback editing surfaces even after the Animation Editor gains visual material editing.
- [x] Do not add a Model Vertices page; visible pose vertices belong to Animation frames.

### Animation Editor: visual geometry and face editing

- [x] Build a stable mapping between reconstructed Model triangle indices and rendered triangles, accounting for source triangle order, strip/fan reconstruction, winding, special-vertex offsets and frame changes.
- [x] Add depth-aware face hover and click selection in both hardware and wireframe rendering. Selection follows the nearest visible reconstructed face.
- [x] Show selected face identity and its three vertex/color references, texture descriptor, TPAG slot/page and animated-texture status.
- [x] Add face-level color editing, including per-corner color selection and editing the referenced color record. Changes update the shared Model and immediately refresh the viewport.
- [x] Add face-level texture assignment and Model texture-descriptor editing, with validation of texture indices, page slots, CLUT coordinates, color mode, blend mode, UVs and segment.
- [x] Add the in-viewport Face Texture Selector for repositioning a selected face's UV mapping directly on its texture atlas. Atlas clicks translate the existing mapping while preserving the triangle's relative UV shape and keeping the descriptor serializable.
- [x] Correct triangle atlas-selection bounds: UV1-UV3 define the visible triangle and its legal placement; the auxiliary UV4 corner must not prevent selecting otherwise valid atlas regions. UV4 is translated when representable and otherwise retains its original byte values.
- [x] Fit the complete atlas within the selector's available viewport so its displayed pixels remain within the corresponding clickable region.
- [ ] Preserve camera, playback position and current face/vertex selection when changing material values or refreshing the Model.
- [ ] Keep vertex inspection and collision overlays working independently of face selection and material editing.

### Texture-page and CLUT resolution

- [ ] Centralize Model texture resolution: triangle texture reference → extended-texture rule when animated → Model texture descriptor → TPAG slot/EID → NSF type-1 page.
- [ ] Share this resolver between the Animation Editor, Model Editor and preview controls rather than maintaining renderer-only lookup logic.
- [ ] Report missing TPAG entries/pages and out-of-range descriptor references without crashing or silently selecting another page.
- [ ] Use the existing texture-page decoder and CLUT interpretation; do not introduce a second incompatible image conversion path.

### Animated Model textures

- [x] Decode extended-texture words into typed flipbook/LOD views while preserving the exact raw word.
- [ ] Implement the CrashEdit-RE-compatible flipbook offset, mask, delay, latency and leap behavior, including chained/leap records and LOD selectors.
- [ ] Keep texture animation timing independent from animation pose interpolation and the selected PAL/NTSC pose playback rate.
- [x] Add a 30 Hz descriptor-sequence preview for extended-texture records and show how many animated faces reference each record.
- [x] Allow users to edit supported flipbook/LOD sequence metadata through bounded bitfield controls while preserving unrelated raw bits.
- [ ] Validate the real Crash sample pair `Cr10G` / `Cr10V`, where Crash's eyes use animated textures, as the primary animated-texture regression. Also retain `El1eG` / `El1eV` as the basic non-animated baseline.

### Animation pose editing and serialization

- [x] Implement compressed signed-delta and uncompressed byte-stream temporal vertex encoders based on the established CrashEdit-RE `Frame.MakeVertices(model)` decode ordering.
- [x] Support edits to selected decoded animation vertices while preserving unrelated temporal bits, collision data, special-vertex bytes and frame metadata; reject values that cannot be represented by the frame's bit widths.
- [x] Preserve the Model's compressed-position table and all other Animation frames that reference the Model when editing a single frame's temporal stream.
- [x] Keep topology/vertex-count changes disabled as cross-resource operations until every dependent frame can be updated and round-tripped.
- [x] Re-decode the edited Animation frame after temporal writes without resetting the viewer camera; fixed-size Model edits refresh the shared Model data directly.

### Regression and acceptance criteria

- [ ] Compare reconstructed Model triangles, texture references and frame poses with CrashEdit-RE `airumu/CrashEdit` branch `t/net9`.
- [ ] Test real Crash 2 Model/Animation pairs: `El1eG` / `El1eV` (unanimated textures) and `Cr10G` / `Cr10V` (animated eye textures).
- [ ] Test parse → serialize → parse equality for untouched Models and Animations.
- [x] Test targeted edits to Model colors, descriptors, scale, TPAG EID, face texture flag, per-corner color reference and extended-texture word, plus compressed and uncompressed Animation vertex writes in synthetic NSF fixtures.
- [x] Verify NSF save/reload and checksum handling for synthetic fixed-size Model edits and compressed/uncompressed Animation vertex edits. Real archive-wide preservation testing remains pending.
- [ ] Exercise malformed counts, truncated items, invalid texture indices, missing TPAG pages, invalid animation links and extended-texture boundary cases.
- [ ] Keep Crash 2 as the acceptance target. Validate Crash 3-specific model/frame behavior separately and do not generalize it from Crash 2 samples.
- [ ] Build and run the complete test suite; report exactly what was and was not verified.

## Research fixtures and confirmed sample facts

The private companion repository `leorelic/CrashEdit-VC-Game-Samples` contains:
- `entry_8_2_El1eG.bin` and `entry_8_3_El1eV.bin` — Snow Go Model/Animation baseline.
- `entry_7_3_Cr10G.bin` and `entry_14_3_Cr10V.bin` — Crash model/Animation pair with animated eye textures.

The Cr10G sample has 309 vertices, 574 reconstructed triangles, 917 texture descriptors, 90 color records, 14 extended-texture records and one TPAG EID. All 14 extended records are flipbook-style (not LOD); animated triangles resolve through texture indices 0–13. The Cr10V sample has 11 frames, each with 309 vertices, and every frame links to Cr10G. The decoded model triangle count matches the Model header. Preserve these sample facts as regression expectations, but continue investigating the exact descriptor-index boundary semantics rather than assuming the final offset/mask range is invalid.

## Current implementation status (2026-11-13)

- [x] Animation vertex point display, V shortcut, depth-aware overlay, hover/selection and selected-vertex inspection — user-confirmed working before this implementation.
- [x] Shared decoded Model data supports validated fixed-size edits for scale values, TPAG EIDs, color records, texture descriptors, extended-texture words, face texture/animated flags and per-corner color references.
- [x] Structural Model Editor pages are available: General, Polygons, Colors, Textures, Extended Textures and conditional Positions.
- [x] Animation Editor face selection is depth-aware and maps rendered faces back to reconstructed Model triangles in hardware and wireframe modes.
- [x] Selected-face inspector supports texture assignment, animated-texture flags, per-corner color-index assignment and editing the referenced shared color records.
- [x] Extended-texture controls expose flipbook metadata and all eight LOD selectors, with a 30 Hz descriptor-sequence preview. Static/animated descriptor indexing is shared between the renderer and face inspector.
- [x] Compressed signed-delta and uncompressed byte-stream Animation vertex writes are implemented. The selected decoded frame is re-decoded after a write, and the viewer camera is left intact.
- [x] Synthetic NSF regression coverage exercises Model edits, face material edits, LOD resolution, compressed and uncompressed Animation vertex edits, and save/reload.
- [ ] A variable-size Model item serializer is still required before adding/removing polygons, descriptors, colors, extended-texture records or changing vertex counts. Current setters patch existing fixed-size fields in place and preserve item offsets/sizes.
- [ ] TPAG/CLUT resolution still needs a fully shared resource-preview path; the Animation Editor displays resolved descriptor and TPAG references while the renderer retains texture-page decoding.
- [ ] Full CrashEdit-RE parity for chained flipbook/leap records and LOD edge cases remains to be validated against the t/net9 runtime implementation.
- [ ] Real-sample parity and round-trip validation against `El1eG` / `El1eV` and `Cr10G` / `Cr10V` remains pending.
- [ ] Topology editing and cross-resource vertex-count changes remain disabled until the Model serializer and every dependent Animation frame encoder are verified.
- [x] GitHub CI build and test jobs passed on Windows, Ubuntu 22.04 and macOS for implementation commit `8ab76bf9a650d05558b9b17b765f9c3282cde961`.

The implementation is being developed directly on the checkpoint branch rather than stopping for approval after each internal work unit. If a build or real-sample test exposes a regression, roll back to `7cd6d28d366f14dabd0f07168d04ccace401981b`.

### Face Texture Selector milestone — 2026-11-12

- [x] User-confirmed working on the TNT Box face: the in-viewport atlas selector can now reposition the selected face's texture mapping into atlas regions that were previously inaccessible.
- [x] Preserve the existing face texture assignment and NSF write path; the selector edits the existing fixed-size 12-byte Model texture descriptor through the established setter.
- [x] CI passed on Windows, Ubuntu 22.04 and macOS for the selector-boundary fix (workflow run [#1017](https://github.com/leorelic/CrashEdit-VC/actions/runs/43113110594)).
- [ ] Repeat the same full-atlas selection checks on Cr10T and other representative models / color modes. TNT Box is the current user-confirmed regression fixture; do not mark all models validated yet.

Implementation trail:
- `2d4b731` — preserve the existing four-corner UV shape while translating the selection.
- `7043352` — fit the full atlas inside the selector viewport and clickable area.
- `1e6a172` — stop auxiliary UV4 from restricting the triangle's selectable atlas range.


## Major milestone — first in-game NSF texture mod validated

**Implemented and end-to-end tested — user-confirmed on 2026-11-12 (TNT Box, Crash Bandicoot 2):** the user edited the TNT Box face texture in CrashEdit-VC, saved the modified NSF, opened that saved NSF successfully in the original CrashEdit-RE, and then loaded it in the original game. The in-game screenshot confirms the changed box textures render correctly. This is the first confirmed end-to-end proof that CrashEdit-VC's face texture editing and NSF save path produce a working Crash 2 game mod.

### Verified chain

1. Face Texture Selector edits the selected TNT Box face's UV mapping in CrashEdit-VC.
2. CrashEdit-VC saves the modified NSF successfully.
3. The saved NSF is accepted and displayed correctly by CrashEdit-RE.
4. The modified content is loaded in the original Crash Bandicoot 2 game, where the changed box textures render correctly.

### Scope and remaining validation

- This is a real-game compatibility pass for the tested TNT Box texture edit, not proof that every Model, texture descriptor/color mode, or resource-editing workflow is game-safe.
- Keep Cr10T, other models and representative 4-bit/8-bit/16-bit descriptor validation pending.
- Keep broader archive-preservation, variable-size Model serialization, topology editing, and unrelated resource-editing validation pending.
- The existing fixed-size texture-descriptor setter and NSF save path were used; this milestone does not imply variable-size Model serialization is complete.


---

## Animation and Model authoring roadmap — updated 2026-11-13

This is the agreed implementation order for the next stage of Crash 2 animation/model authoring. The existing 3D Animation Editor, frame editor, circle-dot Animation Timeline, face selection, and face texture workflow are foundations to extend—not replace. Preserve working controls and rendering behavior while adding each capability as a small, independently testable step.

### 1. NSF chunk expansion for growing entries

- [x] Detect when appending or inserting an Animation frame would exceed the current 64 KiB NSF chunk capacity.
- [x] Ask before changing archive structure: explain that the edit exceeds the current chunk and offer to create a new chunk and move the entire entry.
- [x] On confirmation, relocate the whole entry, retain its EID and entry type, rebuild the affected offset tables, and automatically retry the requested frame operation.
- [x] Cancel without modifying the archive.
- [x] Preserve complete raw entry data, frame payloads, and retained padding; validate the rebuilt source and destination chunks before committing the change in memory.
- [x] Update the selected resource location and animation editor after relocation.
- [x] Add a synthetic regression fixture covering a nearly full chunk, following-entry preservation, trailing padding, append-after-relocation, and save/reload.
- [x] User-confirmed working in CrashEdit-VC. GitHub Actions build and test jobs passed on Windows, Ubuntu 22.04, and macOS (workflow [workflow run](https://github.com/leorelic/CrashEdit-VC/actions/runs/43152396834)).
- [ ] Exercise relocation with additional entry types and representative real archives; validate game loading for expanded archives before treating every resource type as proven safe.

The archive relocation API is generic for valid entries in normal type-0 chunks, while the current capacity preflight and confirmation workflow is connected to Animation Append/Insert. See the NSF chunk research section in [NSF_ENTRY_SUPPORT_RESEARCH.md](NSF_ENTRY_SUPPORT_RESEARCH.md).

### 2. Multi-vertex selection and editing

- [x] Support Shift-click selection and deselection of multiple visible vertex markers in the 3D Animation Editor. Preserve existing depth-aware picking, hover feedback, selected-marker appearance, and the `V` visibility shortcut.
- [x] Add visible vertex-edit toolbar modes beside the existing viewport controls; Free Move, Axis Move and Scale have been built and user-validated:
  - **Free movement:** drag selected vertices in the camera view plane.
  - **Axis movement:** display X/Y/Z gizmo axes and constrain movement to the selected axis.
  - **Scale:** drag the scale handle to scale selected vertices symmetrically around their shared center.
- [x] Keep the existing numeric vertex controls as a precise fallback; editing a selected coordinate applies the same relative offset to every selected vertex, with a copied-archive preflight before live writes.
- [x] With two or more selected vertices, provide Blender-like symmetric scaling so the selection expands/contracts around its shared center rather than translating as a block; user-confirmed working.
- [x] Confirm the Crash 2 frame coordinate representation: observed animation vertex coordinates stay within 0–254, regardless of the object's apparent in-game size. Enforce 0–254 for Crash 2 writes; keep Crash 3's separate byte-space behavior unchanged pending its own research. Compressed-model temporal deltas impose additional per-vertex bit-width limits. Do not widen, clamp or wrap coordinates.
- [x] Preserve the existing per-frame vertex encoding and re-decode the edited frame after each committed transform. Drag transforms are calculated from the immutable positions captured at drag start and submitted through the atomic batch setter; out-of-range coordinates retain the last valid value until mouse release. User-confirmed working.

### 3. Collision editing — completed major milestone (2026-11-13)

<badge color="success">User-confirmed complete</badge>

The user has built and tested the collision editor and explicitly confirmed that the final interaction fixes work perfectly. The implementation uses six selectable rectangular faces rather than independently selectable corner points.

- [x] Extend the existing depth-aware collision viewer into a selectable, editable collision-face workflow activated alongside the `C` collision toggle.
- [x] Correct collision face picking to use the same X/Y/Z coordinate mapping as the renderer.
- [x] Select any of the six rectangle faces; highlight hovered and selected faces in textured and wireframe modes.
- [x] Shift-click add multiple faces to the selection without toggling earlier selections off. This supports selecting opposite sides of a box.
- [x] Axis Move updates the selected face boundary; the shared stored boundary moves all four corners together while the opposite side remains fixed.
- [x] Scale requires the corresponding opposite faces to be selected and scales both boundaries symmetrically around their shared center.
- [x] Give gizmo handles and scale controls mouse-input priority over camera orbit, with forgiving hit regions.
- [x] Remove Free Move from Collision Editing; it remains available for vertex editing.
- [x] Preserve the existing collision rendering, raw 0x28-byte collision record representation, and transactional frame/archive write path.
- [x] User-confirmed working after local build and interaction testing (2026-11-13).

Implementation trail:
- `10d07b5` — six-face selection, corrected collision coordinate mapping, shared-boundary transforms, and face overlays.
- `f058b2e` — reliable gizmo input priority, additive Shift selection, removal of collision Free Move, and symmetric two-sided scaling.

### 4. Optional editor-only Bone Rig Creator

- [ ] Open the Bone Rig Creator with `B` and provide a visible bone toolbar icon.
- [ ] Place its UI in a dock beside Properties.
- [ ] Allow creating bones and assigning selected vertices and faces to each bone through clear lists/selection workflows.
- [x] Improve vertex selection with visible-vertex click selection, Shift-add, Ctrl-remove, and drag-box selection.
- [x] Use camera-space triangle depth checks so occluded vertices are not picked or included in box selections; leave X-ray selection for a later milestone.
- [x] Add a bone-only three-axis rotation gizmo with a fixed pivot and Local/World axis mode. Rotation is applied to assigned vertices on the selected animation frame through the existing frame encoder.
- [ ] Treat bones strictly as an optional, temporary authoring helper. They are not part of the Crash 2 animation format, are not written to NSF, and are never exported as a game skeleton.
- [ ] Retain the original vertex positions and derive each frame's transformed positions from those originals plus the rig transform. Avoid cumulative deformation from repeatedly applying transforms to already transformed coordinates.
- [ ] When saving, flatten the resulting pose into ordinary per-frame vertex positions using the existing Crash 2 Animation frame encoder. Animations that do not need bones remain ordinary vertex animations.

**Initial implementation status (2026-11-13 — pending build and user validation):**

- The first manual vertex-group workflow is implemented in the editor: the Bone List opens with `B` or its visible viewport toolbar icon and docks into the Properties area.
- Users can create, rename and delete bones; assign the currently selected vertices; remove selected vertices; and select all vertices assigned to a bone.
- A vertex belongs to one bone at a time. Selecting a bone's vertices reuses the existing Free Move, Axis Move and Scale tools, so each transform is encoded into the currently selected animation frame through the existing frame-vertex writer.
- Bone definitions are currently in-memory editor state, scoped to the linked Model EID. They are not serialized into the NSF or persisted between application sessions.
- The viewport now supports visible-vertex click selection, Shift-add, Ctrl-remove, and drag-box selection. Triangle-depth testing excludes vertices hidden behind the model; no X-ray mode is included.
- The Bone List provides a Rotate Bone mode and Local/World axis toggle. Three projected rotation rings surround a stable pivot; rotation edits only the assigned vertices in the current animation frame using the existing frame encoder. Per-frame rotation orientation metadata remains editor-only and is keyed by animation EID plus frame index.
- These additions are implementation-complete but still require a fresh CI result and user interaction testing before the roadmap acceptance checks can be marked complete.
- Automatic bone-shape detection, hierarchical joints, face assignment, and cross-frame rig-pose authoring remain deferred.


### 5. PS1 texture import/export

- [ ] Add PlayStation 1-specific texture import/export after the editing workflows above are stable; this is not a generic modern texture pipeline.
- [ ] Support the relevant PS1 representations: 4bpp indexed, 8bpp indexed, and 16-bit direct color, including PS1 transparency/semi-transparency conventions.
- [ ] Export texture data to a usable image format and import edited images back into the existing texture-page/descriptor workflow.
- [ ] Preserve the existing CLUT/palette by default. For indexed images, map edited colors to the existing palette wherever possible.
- [ ] If an edited image contains colors not representable by the current CLUT, report the limitation and offer an explicit palette/CLUT-editing workflow rather than silently changing the palette or degrading colors.
- [ ] Update the texture-page data and all affected mapping/selectors/references consistently, then verify save/reopen and real-game behavior on representative 4bpp, 8bpp, and 16-bit samples.

### Deferred: topology and geometry creation

Adding/removing vertices and faces (Blender-style topology creation/deletion) is deliberately **not** part of the current five-step roadmap. Defer it to the later Custom Scenery / Geometry work, after variable-size Model serialization and dependent Animation vertex-count updates can be handled safely. Multi-vertex transforms edit existing vertices only; they must not implicitly add/remove geometry.

### Roadmap acceptance rules

- Keep Crash 2 as the first target; Crash 3-specific authoring remains deferred unless explicitly promoted.
- Preserve raw bytes and unknown fields unless a specific field is understood and safely writable.
- Keep the existing circle-dot timeline and working animation/model rendering intact.
- Keep editor-only rig data separate from the game file format.
- Mark work complete only after regression coverage and user validation appropriate to the feature; distinguish editor preview/save validation from original-game validation.
- Continue independent implementation under the project's MIT licensing boundary. Do not use excluded repositories as implementation sources, design templates, or feature checklists.
