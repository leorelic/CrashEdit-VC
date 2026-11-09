# Model Editor roadmap

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
- [x] Add face-level texture assignment and Model texture-descriptor editing, with validation of texture indices, page slots, CLUT coordinates, color mode, blend mode, UVs and segment. A dedicated in-viewport texture-page/CLUT picker remains a follow-up.
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

## Current implementation status (2026-11-10)

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
- [ ] The full build and test suite have not yet been run in this environment.

The implementation is being developed directly on the checkpoint branch rather than stopping for approval after each internal work unit. If a build or real-sample test exposes a regression, roll back to `7cd6d28d366f14dabd0f07168d04ccace401981b`.
