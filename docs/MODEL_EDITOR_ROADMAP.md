# Model Editor roadmap

## Reference baseline and scope

**Authoritative comparison target:** [CrashEdit-RE — airumu/CrashEdit, branch t/net9](https://github.com/airumu/CrashEdit/tree/t/net9). The README identifies this as CrashEdit: Re and documents its Model / Compressed Model editors. Future research must use CrashEdit-RE and the appropriate t/net branch. Do not use cbhacks/CrashEdit or its deprecated branches as the project's reference implementation.

This roadmap targets Crash 2 first. The Model entry owns polygon topology, per-corner color selection, texture mapping, texture-page references, and (for compressed models) metadata used to decode animation-frame vertex positions. Animation frames own the changing pose data. The Model Editor should therefore become the source of topology/material data consumed by the existing 3D Animation Viewer.

## CrashEdit-RE Model editor structure

The CrashEdit-RE ModelBox exposes these pages:
- **General:** model scale values and model information.
- **Polygons:** raw polygon command stream and reconstructed triangle list.
- **Colors:** editable model color table.
- **Textures:** texture descriptors and texture-page/CLUT mapping.
- **Extended Textures:** flipbook and LOD metadata.
- **Positions:** compressed-model position records only.

The separate Vertices page in ModelBox is removed for Model entries; it is used by Scenery. Do not copy that Scenery-specific page into the Model Editor without a distinct reason. The Model's vertex positions are resolved through Animation frames.

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
- **Open question:** ModelEntry.PolyCount separately reads 0x44. Trace all consumers and compare real files before assigning this field a semantic name. Do not replace the loader's 0x2C count with 0x44.

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

## Implementation phases

### Phase 0 — native inventory and source comparison
- [ ] Audit current native Model entry parsing, hierarchy labels, item preservation and save behavior.
- [ ] Audit current Animation frame decoder against CrashEdit-RE Frame.Load, Frame.LoadNew and Frame.MakeVertices(model).
- [ ] Trace native Model EID resolution from Animation frames into the 3D Animation Viewer.
- [ ] Resolve the 0x2C versus 0x44 polygon-count field discrepancy from source consumers and representative Crash 2 samples.
- [ ] Collect paired Model + Animation examples, including compressed and uncompressed models.

### Phase 1 — loss-preserving Model data model
- [ ] Introduce a typed Model structure while retaining original raw item bytes.
- [ ] Decode the Info block, scales, TPAG EIDs and all counts with bounded reads.
- [ ] Decode item-2 color records and item-3 texture descriptors.
- [ ] Decode item-4 extended texture words while preserving raw values.
- [ ] Decode optional item-5 compressed-position records into X/Z/Y bases and bit widths.
- [ ] Add parse/serialize/parse tests for untouched Model entries before exposing edits.

### Phase 2 — topology reconstruction
- [ ] Implement the two-pass polygon command decoder with original/duplicate key mapping.
- [ ] Preserve command type, key, unknown bits, flags, triangle type/subtype, animated bit and color-pair commands.
- [ ] Reconstruct triangle vertex indices, per-corner colors and texture references.
- [ ] Add bounds diagnostics for footer, command count, duplicate keys, vertex/color/texture indices and malformed sequences.
- [ ] Compare native reconstructed triangles against CrashEdit-RE on several Crash 2 models.

### Phase 3 — Model Editor UI and live preview
- [ ] Build General, Polygons, Colors, Textures, Extended Textures and conditional Positions inspector pages, following the useful organization of CrashEdit-RE while retaining CrashEdit-VC's native UI conventions.
- [ ] In Polygons, provide both raw command inspection and decoded triangle inspection.
- [ ] Make model vertices and polygons inspectable in the existing 3D Animation Viewer, using the selected animation pose when a linked frame is available.
- [ ] Resolve texture descriptors through TPAG EIDs into existing type-1 texture chunks and CLUT data.
- [ ] Ensure model edits refresh the Animation Viewer without reopening the Animation entry or resetting camera/playback state.

### Phase 4 — safe material and model editing
- [ ] Start with color-table editing and texture-descriptor editing with validated references and live preview.
- [ ] Add scale and TPAG edits only after renderer/resource resolution is validated.
- [ ] Add extended-texture edits with separate flipbook/LOD field validation.
- [ ] Keep unsupported raw command bits and unknown fields intact.

### Phase 5 — topology and animation vertex editing
- [ ] Implement a verified polygon-command serializer before adding/removing triangles or changing vertex-key relationships.
- [ ] Test duplicate-key behavior and per-corner color-pair regeneration.
- [ ] Implement a matching compressed and uncompressed temporal vertex encoder based on CrashEdit-RE Frame behavior.
- [ ] Enable selected animation vertex coordinate edits only after round-trip tests prove unaffected bits and records are preserved.
- [ ] Treat topology changes as cross-resource changes: every Animation frame using the Model must remain consistent with its vertex count and indexing.

## Validation requirements

- Raw item bytes remain authoritative until the decoded representation has a verified serializer.
- Check item counts, byte-size arithmetic, TPAG EIDs, duplicate keys and every polygon/vertex/color/texture index before access.
- Preserve unsupported commands and unknown fields byte-for-byte where possible.
- Test parse → serialize → parse equality for untouched entries and targeted edits.
- Compare representative Crash 2 Model/Animation pairs against CrashEdit-RE t/net9.
- Validate Crash 3-specific frame/model behavior separately; do not generalize it to Crash 2 without evidence.

## Current status

- [x] Animation vertex marker display, V shortcut, depth-aware hardware overlay, hover/selection coloring and selected-vertex inspection — user-confirmed working on 2026-11-09 (major milestone).
- [ ] Model decoder and Model Editor UI — next active phase.
- [ ] Model-driven animation temporal vertex encoder — pending Model/Animation source parity and round-trip tests.
