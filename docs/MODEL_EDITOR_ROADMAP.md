# Model Editor roadmap

## Model/Animation research boundary

**Airumu / Blender_CrashXform is excluded from Model/Animation Editor research.** It is GPL-3.0. Do not inspect or restudy the project, use its video or repository as a design reference, port or adapt its code, reproduce its implementation, or incorporate its dependencies, assets, or generated output. Keep Model/Animation Editor work independently implemented under CrashEdit-VC's MIT license. The project's overall repository-exclusion policy is recorded in [MILESTONES.md](MILESTONES.md).

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

This is one connected Model + Animation Editor feature, not a sequence of user-facing mini-releases. Implement the full workflow against the checkpoint at `3d4cbbf565ec063a27c5b61ab53732a3aa17b2d5`. Internal work may still be organized and tested in dependency order, but do not stop after each small phase for approval. If the integrated implementation proves unstable, the checkpoint is the rollback target.

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
- [x] Preserve the complete desired pose while writing uncompressed frames: component writes re-decode the frame, so the save path restores the remaining edited coordinates between writes and verifies the final decoded pose exactly.
- [x] Support edits to selected decoded animation vertices while preserving unrelated temporal bits, collision data, special-vertex bytes and frame metadata; reject values that cannot be represented by the frame's bit widths.
- [x] User-confirmed in-game save milestone: edits to both Crash's `Cr10V` special/ear vertices and the uncompressed `BT10V` animation now persist after saving.
- [x] Preserve the Model's compressed-position table and all other Animation frames that reference the Model when editing a single frame's temporal stream.
- [x] Keep topology/vertex-count changes disabled as cross-resource operations until every dependent frame can be updated and round-tripped.
- [x] Re-decode the edited Animation frame after temporal writes without resetting the viewer camera; fixed-size Model edits refresh the shared Model data directly.

### Regression and acceptance criteria

- [x] Compare reconstructed Model triangle indices, corner colors, texture references/flags and types with CrashEdit-RE `airumu/CrashEdit` branch `t/net9` on `El1eG` and `Cr10G`.
- [x] Inspect real Crash 2 Model/Animation pairs: `El1eG` / `El1eV` and `Cr10G` / `Cr10V`; confirm all Animation frames link to the expected Model and have the expected decoded vertex counts.
- [x] Run the native C++ parser/serializer on the extracted real-sample Model/Animation pairs; decoded triangle/frame golden hashes match and the raw Model and Animation entry bytes remain identical after save/reload.
- [x] Test targeted edits to Model colors, descriptors, scale, TPAG EID, face texture flag, per-corner color reference and extended-texture word, plus compressed/uncompressed and multi-vertex Animation writes, atomic rejection, rebasing and save/reload in synthetic NSF fixtures.
- [x] Add a dedicated uncompressed multi-vertex save regression that edits later vertices and multiple axes, saves/reloads the NSF, and verifies every decoded coordinate.
- [x] Verify NSF save/reload and checksum handling for synthetic fixed-size Model edits and compressed/uncompressed Animation vertex edits. Real archive-wide preservation testing remains pending.
- [ ] Exercise malformed counts, truncated items, invalid texture indices, missing TPAG pages, invalid animation links and extended-texture boundary cases.
- [ ] Keep Crash 2 as the acceptance target. Validate Crash 3-specific model/frame behavior separately and do not generalize it from Crash 2 samples.
- [ ] Build and run the complete test suite; report exactly what was and was not verified.

## Research fixtures and confirmed sample facts

The private companion repository `leorelic/CrashEdit-VC-Game-Samples` contains:
- `entry_8_2_El1eG.bin` and `entry_8_3_El1eV.bin` — Snow Go Model/Animation baseline.
- `entry_7_3_Cr10G.bin` and `entry_14_3_Cr10V.bin` — Crash model/Animation pair with animated eye textures.

The Cr10G sample has 309 vertices, 574 reconstructed triangles, 760 polygon words, 917 texture descriptors, 90 color records, 14 extended-texture records and one TPAG EID (`4CD8206F`). All 14 extended records are flipbook-style (not LOD); animated triangles resolve through texture indices 0–13. The Cr10V sample has 11 frames, each with 309 vertices, and every frame links to Cr10G. The El1eG baseline has 38 vertices, 64 reconstructed triangles, 108 polygon words, 25 texture descriptors, 15 color records, no extended-texture records and three TPAG EIDs; El1eV has seven frames of 38 vertices, all linked to El1eG. A comparison of the reconstructed triangle records against CrashEdit-RE `ModelEntry.ConvertIndices()` matched all indices, corner-color references, texture indices/flags and triangle types/subtypes for both models. The native frame decoder follows the `Frame.MakeVertices()` signed X-Z-Y temporal order and the real samples confirm expected frame/model links and vertex counts. The private native C++ workflow now checks golden hashes for all reconstructed triangles and decoded frame vertex streams, then confirms that extracted Model and Animation entry bytes are unchanged after save/reload. Full-archive and original-game validation remain open; preserve the counts as regression expectations and continue investigating exact descriptor-index boundary semantics rather than assuming the final offset/mask range is invalid.

## Current implementation status (2026-11-28)

- [x] **Animation vertex save milestone — checkpoint [`4b65928`](https://github.com/leorelic/CrashEdit-VC/commit/4b6592852d761d742358313b0250add7112e411e).** Fixed the uncompressed Animation save path: each component write re-decodes the frame, so the save routine now preserves the complete intended pose between writes and verifies the decoded result before committing.
- [x] User-tested in the original game: edited `Cr10V` Crash ear vertices save and work when their values fit the frame's encoding; edited `BT10V` vertices now also persist instead of reverting to defaults.
- [x] Added synthetic multi-vertex uncompressed-frame regression coverage, including later vertices and multiple coordinate axes, with save/reload verification.
- [x] **Validated Complex-to-Normal workaround:** convert the Complex Model and linked Animations, edit the larger Crash ear position, save through VC, round-trip through CE-RE, and confirm the edited result in Crash 2. User-confirmed for the tested resource set at [90f4a99](https://github.com/leorelic/CrashEdit-VC/commit/90f4a995e4f399cdd71a7d951ed27171ef6b9357).
- [ ] **Remaining limitation:** direct editing of the original Complex representation is not established as reliable. Do not generalize the conversion result to every resource layout or arbitrary coordinate range.


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
- [x] Real-sample Model triangle parity, Animation frame linkage/vertex-count checks, and native C++ extracted-entry save/reload validation completed for `El1eG` / `El1eV` and `Cr10G` / `Cr10V`; broad full-archive and cross-resource original-game validation remain pending; the specific converted/edited Crash ear path is user-confirmed in-game.
- [ ] Topology editing and cross-resource vertex-count changes remain disabled until the Model serializer and every dependent Animation frame encoder are verified.
- [x] GitHub CI build and test jobs passed on Windows, Ubuntu 22.04 and macOS for code commit `fd90a3ce489459f3816ca005a06021f3ae7e6af1` (run [44419838173](https://github.com/leorelic/CrashEdit-VC/actions/runs/44419838173)); the private real-sample workflow also passed (run [44416679912](https://github.com/leorelic/CrashEdit-VC-Game-Samples/actions/runs/44416679912)).

The implementation is developed on `cpp-port-foundation`. The dedicated rollback reference is branch `checkpoint/pre-four-roadmap-items-3d4cbbf`, pinned to `3d4cbbf565ec063a27c5b61ab53732a3aa17b2d5`. If a later change destabilizes the work, restore that exact checkpoint.

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

## Animation and Model authoring roadmap — updated 2026-11-19

The current Animation/Model editor is the foundation for custom frame authoring. The immediate priority is to make the editor-only Bone Rig and per-frame bone posing workflow genuinely usable: select groups of vertices, create and save bones for those groups, then pose those bones naturally on individual Animation frames. The existing implementation is a rough foundation, not a finished tool; its current interaction and posing workflow needs substantial improvement. Once this bone-editing workflow is dependable and validated, put further Animation Editor expansion on hold and shift the main development focus to the broader Zones / Level Editing category. Preserve the existing 3D Animation Editor, frame editor, circle-dot Animation Timeline, face selection, face-texture workflow, collision editor, and known-good rendering behavior.

### Completed foundation milestones

#### NSF chunk expansion for growing entries

- [x] Detect when appending or inserting an Animation frame would exceed the current 64 KiB NSF chunk capacity.
- [x] Ask before changing archive structure and offer to create a new chunk and move the entire entry.
- [x] On confirmation, relocate the whole entry, retain its EID and entry type, rebuild affected offset tables, and retry the requested frame operation.
- [x] Cancel without modifying the archive.
- [x] Preserve complete raw entry data, frame payloads, and retained padding; validate rebuilt chunks before committing.
- [x] Update the selected resource location and animation editor after relocation.
- [x] Add synthetic regression coverage for a nearly full chunk, following-entry preservation, trailing padding, append-after-relocation, and save/reload.
- [x] User-confirmed working in CrashEdit-VC. GitHub Actions passed on Windows, Ubuntu 22.04, and macOS (workflow [run](https://github.com/leorelic/CrashEdit-VC/actions/runs/43152396834)).
- [ ] Exercise relocation with additional entry types and representative real archives; validate game loading for expanded archives before treating every resource type as proven safe.

The archive relocation API is generic for valid entries in normal type-0 chunks, while the current capacity preflight and confirmation workflow is connected to Animation Append/Insert. See the NSF chunk research in [NSF_ENTRY_SUPPORT_RESEARCH.md](NSF_ENTRY_SUPPORT_RESEARCH.md).

#### Collision editing

- [x] Extend the depth-aware collision viewer into a selectable, editable collision-face workflow activated alongside the `C` collision toggle.
- [x] Correct collision face picking to use the renderer's X/Y/Z coordinate mapping.
- [x] Select any of the six rectangular faces; highlight hovered and selected faces in textured and wireframe modes.
- [x] Shift-click add multiple faces without clearing earlier selections.
- [x] Axis Move updates the selected face boundary while leaving the opposite side fixed.
- [x] Scale requires the corresponding opposite faces and scales both boundaries symmetrically.
- [x] Give gizmo handles mouse-input priority over camera orbit.
- [x] Remove Free Move from Collision Editing; it remains available for vertex editing.
- [x] Preserve the raw 0x28-byte collision record and transactional frame/archive write path.
- [x] User-confirmed working after local build and interaction testing (2026-11-13).

Implementation trail: `10d07b5` added six-face selection and transforms; `f058b2e` fixed gizmo input priority, additive selection, and symmetric scaling.

### 1. Vertex selection and 3D viewport usability

- [x] Support visible-vertex click selection, Shift-add, Ctrl-remove, and drag-box selection in normal vertex mode and Bone List mode.
- [x] Keep box selection depth-aware while avoiding triangle-occlusion checks for vertices outside the selection rectangle; hover picking tests occlusion only for nearby candidates.
- [x] Tighten the pick radius and use camera depth to break equal-distance ties, improving precision around dense/overlapping vertices.
- [x] Distinguish selected and hovered markers with a persistent selection ring and a separate hover ring/color.
- [x] Separate camera input from selection: middle-drag orbits, right-drag pans, the wheel zooms, and left-drag selects/box-selects. Shift-left-drag remains an orbit gesture while Bone List is open.
- [x] Gate camera movement/zoom while selection boxes, transform handles, or collision tools own the pointer.
- [x] Provide Free Move, Axis Move, and Scale toolbar modes, with numeric coordinate editing as a precise fallback.
- [x] Scale multi-vertex selections around their shared center by default; an enabled Bone List pivot option can use the selected bone's custom pivot for Move/Scale.
- [ ] Validate picking and editing usability on large and dense models with hands-on user testing; X-ray selection remains deferred.

### 2. Optional editor-only Bone Rig Creator

- [x] Open the Bone List with `B` or its visible viewport toolbar icon and dock it beside Properties.
- [x] Create, rename, and delete bones; assign selected vertices; remove selected vertices; and select all vertices assigned to a bone.
- [x] Enforce one-bone-per-vertex membership when assigning vertices and validate sidecar assignments on load.
- [x] Store parent-bone metadata and prevent self-parenting/cycles when editing or loading a rig. Parent links are organizational metadata; child-transform propagation is not implemented.
- [x] Provide a bone-only three-axis rotation gizmo with Local/World axis mode and editable numeric pivot coordinates.
- [x] Allow the selected bone pivot to drive Move/Scale gizmos, with a toggle to use the ordinary selection center instead.
- [x] Capture a separate rest pose for each Animation EID/frame and calculate each bone rotation from that immutable base pose, rather than repeatedly rotating already-quantized vertex positions.
- [x] Store the original frame offsets alongside each rest pose so Crash 2 coordinate rebasing does not move the base pose on subsequent rotations.
- [x] Invalidate affected rest-pose data when assigned vertices are moved/scaled, reassigned, removed, or the pivot changes.
- [x] Save/load editor-only rig data through a version-2 sidecar; retain version-1 sidecar compatibility, validate frame keys/coordinates/hierarchy, and replace saved files through a temporary file.
- [x] Define the frame workflow: each Animation frame has its own rest pose and rotation state; edits are applied to that frame and written back as ordinary encoded Animation vertex positions. The rig itself remains editor-only.
- [ ] Complete user interaction testing of multi-frame rig editing, save/reload, and undo/redo on representative Crash 2 animations.
- [ ] Decide separately whether child-bone transform propagation, automatic bone-shape detection, face assignment, and X-ray selection are worth adding; they are not prerequisites for the current helper.

**Current implementation notes:** bone definitions, parent metadata, rest poses, pivots, and per-frame rotation metadata are stored in the editor sidecar, not in the Crash 2 Animation format or NSF. The rig is an editor-only vertex-group/transform helper, not a game skeleton. Version-1 sidecars load without rest-pose caches; the first rotation establishes the current visible pose as that frame's new base.

**Status: foundational implementation exists, but the user considers the current bone workflow rough and not acceptable as a finished authoring tool.** Creating bones, assigning selected vertices, saving/loading the rig, and basic per-frame rotation are already implemented. Do not mistake those checkmarks for completion of the intended Blender-like posing experience.

#### Next priority — usable bone creation and per-frame posing

- [x] Add a direct **Create From Selection** workflow that creates a bone from the current viewport selection, transfers those vertices out of any previous groups, and initializes the pivot from the group's current-frame center; selecting a bone in the list now also selects/highlights its assigned vertices in the viewport.
- [x] Add viewport hit-testing for the visible bone segments so clicking a bone selects it and its assigned vertices directly; existing axis/rotation tools can then operate on that group without returning to the Bone List.
- [ ] Add direct bone-body dragging and connected parent/child transform propagation; the current parent links remain organizational until this is implemented and validated.
- [x] Include per-frame rest-pose vertex contents and rest offsets in undo-history change detection so editor-only pose-cache changes are not silently omitted from history.
- [ ] Make the workflow for selecting a group of vertices and creating a bone for that group fast, clear, and predictable; retain the existing multi-selection and one-bone-per-vertex rules unless research demonstrates a better safe model.
- [ ] Make bone creation, naming, selection, vertex assignment/removal, and saved-rig management feel like one coherent rigging workflow rather than disconnected utility controls.
- [x] Improve the first-pass bone silhouette with a tapered shaft, rounded endpoint joints, and dark outlines; this is a visual foundation, not yet a final anatomically styled bone mesh.
- [x] Add **Connect Pivot to Parent** for a selected child bone: after choosing a parent, snap the child pivot to the parent's pivot and invalidate its cached rest/rotation data.
- [ ] Improve the full visual bone representation and viewport interaction so users can understand each bone's pivot, orientation, assigned vertices, and relationship to other bones.
- [ ] Implement parent/child transform propagation so connected descendants follow parent movement/rotation while retaining their relative joint offsets; current hierarchy connections do not yet propagate transforms.
- [ ] Make posing operate on the selected bone and its assigned vertices as a natural bone transform, with intuitive rotation and pivot behavior rather than requiring awkward manual workarounds.
- [ ] Make per-frame posing dependable: each Animation frame must retain its own pose/rest state, and changing a bone on one frame must not unexpectedly alter other frames.
- [ ] Support practical pose iteration across frames, including predictable frame switching, pose adjustment, and preservation of previously authored poses.
- [ ] Make save/reload preserve the rig, bone assignments, hierarchy, pivots, rest poses, and every edited frame pose; test both the editor sidecar and the actual Animation data.
- [ ] Add a usable undo/redo workflow for bone creation, assignment, hierarchy changes, and per-frame posing.
- [ ] Validate the complete workflow on representative multi-frame Crash 2 animations, including a custom pose sequence authored with bones and confirmed after save/reopen and in-game testing.

**Exit condition for the Animation/Model authoring focus:** a user can select vertices, create and save bones for those groups, pose the bones independently on multiple frames, revisit and refine those poses, and reliably save/reopen the result without losing or unintentionally changing the rig or animation. Once this is achieved and validated, pause further Animation Editor feature expansion and move the primary roadmap focus to Zones / Level Editing. Keep any remaining animation compatibility defects documented, but do not let optional animation polish indefinitely block the next category.

### 3. Transform and pivot tools

- [x] Provide Free Move, Axis Move, Scale, and bone rotation controls, with Shift snapping on axis transforms.
- [x] Make multi-vertex scaling use the shared selection center and provide an optional selected-bone pivot for Move/Scale.
- [x] Allow direct numeric editing of a bone pivot and recenter it from assigned vertices.
- [x] Reject Move/Scale results outside the observed Crash 2 coordinate range instead of silently clamping or wrapping; keep the last valid pose during a drag.
- [x] Calculate bone rotations from per-frame rest positions and rest offsets, then submit edits through the existing atomic Animation frame encoder/rebase path.
- [x] Keep the established scale-down → rotate → scale-back workaround available; do not add or reintroduce a Flip button.
- [ ] Complete hands-on validation of transform behavior across different selections and multiple frames.
- [ ] Test undo/redo and save/reopen for Move, Scale, and bone rotation in the application; synthetic archive tests cover the underlying vertex encoders and rebasing.

### 4. Compatibility and regression validation

- [x] Compare reconstructed Model triangles against CrashEdit-RE `airumu/CrashEdit` branch `t/net9` for the real `El1eG` and `Cr10G` samples; triangle indices, corner colors, texture indices, animated flags, and triangle type/subtype matched exactly (64/64 and 574/574 triangles).
- [x] Check real Animation pairs: `El1eV` has 7 frames of 38 vertices, all linked to `El1eG`; `Cr10V` has 11 frames of 309 vertices, all linked to `Cr10G`.
- [x] Confirm the Model item/header counts and compressed-position counts for both real sample pairs against the original `ModelEntryLoader`/`ModelEntry` source.
- [x] Add synthetic regression coverage for atomic batch rejection, duplicate vertex indices, Crash 2 frame-offset rebasing, compressed multi-vertex edits, and save/reload while preserving collision bytes.
- [x] Match the Animation frame layout and signed X-Z-Y temporal decoding order to CrashEdit-RE `Frame.MakeVertices()`; sample frame counts, model links, and decoded vertex counts agree.
- [x] Run the native C++ parser/serializer against both private real-sample pairs in the dedicated sample workflow; triangle/frame golden hashes match and the extracted Model/Animation entry bytes are preserved exactly after save/reload.
- [x] Validate extracted real-sample entry save/reload and decoded-data stability for both pairs. [ ] Still validate full NSF/NSD archive behavior, original-game loading, texture-page/CLUT resolution, chained animated-texture edge cases, missing references, and malformed resource boundaries.
- [x] Keep Crash 2 as the acceptance target; Crash 3-specific model/frame behavior remains a separate compatibility pass.

### 5. Zones / Level Editing — next major roadmap category

This is the next broad development focus after the Bone Rig and per-frame posing workflow reaches its exit condition. **Zones / Level Editing is substantially broader than Scenery / Geometry Editing.** Scenery and Geometry are important sub-areas within the Zone workflow, not the definition of the entire category. The exact feature breakdown will grow as we research the formats and runtime behavior; do not prematurely constrain this category to the currently known tasks.

#### Zone structure and level-resource relationships

- [ ] Research the overall Crash 2 Zone structure and identify the resources, metadata, and references that make up a playable level.
- [ ] Map how Zone resources reference, load, order, share, and depend on NSF/NSD entries and other level data.
- [ ] Understand Zone boundaries, neighboring/connected Zones, transitions, and any runtime rules that affect loading or visibility.
- [ ] Document the relationship between Zone data, spawn/checkpoint data, cameras, scenery, geometry, level objects, and other relevant resources.
- [ ] Establish representative real Crash 2 Zone fixtures and a safe round-trip / original-game validation strategy before broad editing.

#### Scenery / Geometry editing and level-object authoring

This is one major sub-area of the Zones category. It covers Scenery/Geometry resources and the workflows needed to inspect, edit, create, import, convert, and place level geometry and objects.

- [ ] Research Scenery/Geometry resource layouts and how level objects reference or combine their geometry, materials, and related resources.
- [ ] Research the Type 4 **SLST (Sort List)** entry as part of Scenery/Geometry and Zone rendering, not as an unrelated standalone editor task.
- [ ] Establish how SLST source and delta records relate to scenery polygon ordering, camera-dependent sorting, Zones, and level-object rendering.
- [ ] Confirm runtime semantics, interpolation/transition behavior, and generation constraints from source, multiple real Crash 2 samples, and in-game tests before implementing an SLST generator.
- [ ] Define safe geometry/topology serialization, including variable-size data, index/count updates, dependent-resource references, and round-trip preservation.
- [ ] Design the user workflow for scenery/geometry editing and level-object creation/import/conversion, with SLST inspection/editing and any required regeneration handled as part of the same sub-area.

#### Additional Zone editing areas to discover

- [ ] Identify and document the other Zone-level editing workflows needed for meaningful level authoring; expand this list as research uncovers the relevant formats and runtime behavior.
- [ ] Determine how Zone-level changes should be previewed, validated, saved, and tested in the original game without damaging unrelated level resources.

The existing Animation/Model vertex tools continue to edit positions on existing topology only. Adding/removing vertices or faces, constructing scenery, and creating/converting level objects belong under this broader Zones / Level Editing category. See the SLST section in [NSF_ENTRY_SUPPORT_RESEARCH.md](NSF_ENTRY_SUPPORT_RESEARCH.md) and the focused research note in [RESEARCH_NOTES.md](RESEARCH_NOTES.md).

### 6. PS1 Texture Import/Export — final roadmap category

This remains intentionally last, after the Zones / Level Editing category (including Scenery/Geometry and the other Zone-level workflows) has been researched and established. It is a PlayStation 1-specific workflow, not a generic modern texture pipeline.

- [ ] Support relevant PS1 representations: 4bpp indexed, 8bpp indexed, and 16-bit direct color, including PS1 transparency/semi-transparency conventions.
- [ ] Export texture data to a usable image format and import edited images back into the existing texture-page/descriptor workflow.
- [ ] Preserve the existing CLUT/palette by default. For indexed images, map edited colors to the existing palette wherever possible.
- [ ] If edited colors cannot be represented by the current CLUT, report the limitation and offer an explicit palette/CLUT-editing workflow rather than silently changing the palette or degrading colors.
- [ ] Update texture-page data and all affected mapping/selectors/references consistently, then verify save/reopen and real-game behavior on representative 4bpp, 8bpp, and 16-bit samples.

### Roadmap acceptance rules

- Keep Crash 2 as the first target; Crash 3-specific authoring remains deferred unless explicitly promoted.
- Preserve raw bytes and unknown fields unless a specific field is understood and safely writable.
- Keep the existing circle-dot timeline and working animation/model rendering intact.
- Keep editor-only rig data separate from the game file format.
- Mark work complete only after regression coverage and user validation appropriate to the feature; distinguish editor preview/save validation from original-game validation.
- Continue independent implementation under the project's MIT licensing boundary. Do not use excluded repositories as implementation sources, design templates, or feature checklists.


---

## Viewport camera controls — polish pass (2026-11-16)

- [x] Move camera movement instructions and view presets into a right-side Camera Guide, opened by a top-right viewer button and animated in from right to left.
- [x] Provide clickable Front, Back, Right, Left, Top and Bottom presets in the guide.
- [x] Ease Numpad view transitions rather than snapping; mouse orbit remains direct and responsive.
- [x] Remove the incomplete Perspective / Orthographic switch and Numpad 5 binding. The textured hardware renderer did not honor the old orthographic flag, so the editor now consistently presents Perspective rather than exposing a misleading control.
- [ ] Verify slide animation, guide hit-testing, presets, and smooth Numpad transitions on Windows and Linux; macOS remains in the CI matrix.
- [ ] Revisit Orthographic only as a complete renderer-wide feature, including hardware rendering, overlays, picking, collision geometry and gizmos.

Existing navigation and editing shortcuts remain unchanged: W/A/S/D, Shift-modified movement, Q/E, R, mouse orbit/pan, wheel zoom, playback, selection, transforms, face editing, collision tools and bone tools.


## Animation recovery checkpoint and unresolved coordinate limit — 2026-11-26

Checkpoint [`ff5359af45779fcd29b3827a563215b433cef1ac`](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac): user-confirmed preview looks correct again and saving works. This is a known-good recovery baseline, not resolution of the original Animation vertex transform issue.

The remaining defect: some vertices cannot move beyond a particular X/Y/Z gizmo location. The editor incorrectly offers moving the Animation entry to a new NSF chunk, but the edit still does not apply. Investigate frame boundaries and actual coordinate/encoding limits, comparing gizmo bounds with frame metadata, encoder representability, and save validation. Do not use chunk relocation as a workaround without evidence of chunk-capacity exhaustion.

Acceptance: reproduce and record the exact frame, vertex, axis, current/requested coordinate, and rejection reason; move past the former limit; confirm the edit applies and survives save/reopen; test original-game behavior separately. Keep exact-pose checks and never silently clamp, wrap, or discard edits. See [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md) and [RESEARCH_NOTES.md](RESEARCH_NOTES.md).
### Normal / Complex Animation editing separation

- [x] Identify linked Model compression in the 3D Animation Editor as Normal (uncompressed) or Complex (compressed); user-confirmed on BT10V/BT10G and Cr10V/Cr10G.
- [ ] Preserve the existing Normal edit/save path as a regression baseline.
- [ ] Keep Complex editing research-only until its index mapping and serialization are proven; do not reuse the Normal encoder implicitly.
- [ ] Trace Complex frame slots, special-prefix slots, Model-local polygon indices, ModelPosition records, and temporal bit ranges against Cr10V and CrashEdit-RE.
- [ ] Add Complex no-op and edit-then-restore save regressions.
- [ ] Evaluate true Complex delta encoding versus explicit, isolated Complex-to-Normal conversion. Conversion must preserve all frame poses, special vertices, collisions/header fields, Model references, polygon rendering, and unrelated Animations.
- [ ] Enable Complex editing only after save/reopen and original-game validation. Preserve the user-confirmed Cr10V preview baseline (ff5359a).

Research details: ANIMATION_FORMAT_RESEARCH.md and RESEARCH_NOTES.md.