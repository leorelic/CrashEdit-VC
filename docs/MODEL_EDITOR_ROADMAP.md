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

## Implementation phases

### Phase 0 — native inventory and source comparison
- [x] Audit the existing native Model parser: `NsfModel` stores the Info header, scales, TPAG EIDs, raw polygon words, decoded colors/textures, extended-texture words, compressed-position records and reconstructed triangles. The original NSF chunk bytes remain available through the archive for loss-preserving reads.
- [x] Audit the existing native Animation parser: `NsfAnimationFrame` retains frame header values, collisions, special vertices, temporal words, raw item offsets/sizes and decoded vertices.
- [x] Trace Model resolution: Crash 2 frames use their direct Model EID; the current Crash 3 path derives Model EIDs from GOOL vertex frame groups. Selecting a Model in the UI searches for an Animation that uses it and opens that Animation in the 3D Animation Editor.
- [x] Compare temporal vertex decoding with CrashEdit-RE `Frame.MakeVertices(model)`: the native code follows the X-Z-Y compressed component order, sign extension, accumulation/wrapping, special-vertex temporal offset, uncompressed byte ordering and Crash 3 scale behavior.
- [x] Resolve the 0x2C versus 0x44 count fields: 0x2C is the serialized polygon-command word count; 0x44 is the reconstructed polygon/triangle count shown by ModelBox.
- [ ] Validate paired Model + Animation examples from real Crash 2 NSF files, including compressed and uncompressed models. Current automated coverage is synthetic and does not yet prove broad sample parity.

**Native audit gaps:** there are no dedicated Model field setters or verified Model edit serializer yet. The current Model tests cover basic item decoding and a synthetic fan-triangle reconstruction; Animation tests cover frame parsing and an uncompressed temporal sample, but do not yet provide a comprehensive paired compressed Model/Animation round-trip. The current UI handoff from a selected Model to an Animation is present, but visual face selection and face-level material editing are not.

### Phase 1 — complete and harden the shared Model representation
- [x] Establish the native `NsfModel` representation and bounded decoding of the Info block, scales, TPAG EIDs, item counts, colors, texture descriptors, extended-texture words and optional compressed-position records.
- [x] Retain raw polygon command words and the original archive bytes; keep unknown packed fields available for loss-preserving work.
- [ ] Audit count/size validation and malformed-entry behavior against real Crash 2 samples.
- [ ] Add explicit Model setters and a verified item serializer only after each editable field has a known binary mapping.
- [ ] Add parse/serialize/parse tests for untouched Model entries before exposing edits.

### Phase 2 — validate topology reconstruction
- [x] Implement the two-pass polygon command decoder with original/duplicate key mapping.
- [x] Preserve raw command words and expose reconstructed triangle vertex indices, per-corner colors, texture index, triangle type/subtype and animated flag.
- [ ] Audit all malformed-stream and index-boundary cases, including duplicate-key failures and command/footer handling.
- [ ] Compare native reconstructed triangles against CrashEdit-RE on several real Crash 2 models; the current synthetic fan-triangle test is only a starting point.

### Phase 3 — structural Model inspection and Animation Editor integration
- [ ] Build temporary General, Polygons, Colors, Textures, Extended Textures and conditional Positions pages as research/inspection tabs; do not treat the Colors and Textures tables as the final user-facing material-editing workflow.
- [ ] In Polygons, provide both raw command inspection and decoded triangle inspection.
- [x] Keep the existing Model-to-Animation workspace handoff, selecting an Animation that references the chosen Model when possible.
- [ ] Resolve texture descriptors through TPAG EIDs into existing type-1 texture chunks and CLUT data.
- [ ] Add visual face selection in the Animation Editor, with a clear mapping from a selected rendered face to its Model triangle and per-corner color/texture references.
- [ ] Ensure Model data changes refresh the Animation Editor without reopening the Animation entry or resetting camera/playback state.

### Phase 4 — visual material editing through the Animation Editor
- [ ] Implement face-level color editing in the Animation Editor; edits must update the corresponding Model color records and per-corner color references safely.
- [ ] Implement face-level texture assignment and descriptor editing in the Animation Editor, resolving TPAG page, texture descriptor and CLUT references.
- [ ] Keep the Model Editor's Colors and Textures pages available as raw/structured inspection tools during this transition; do not remove them until the visual workflow covers their useful capabilities.
- [ ] Add scale, TPAG and extended-texture editing only after renderer/resource resolution and binary round-trips are validated.
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
- [x] Native Model and Animation decoding plus Model EID handoff into the 3D Animation Editor — existing implementation audited on 2026-11-09; real-sample parity remains pending.
- [ ] Temporary structural Model research tabs, including Colors and Textures.
- [ ] Visual face selection and material editing in the Animation Editor, writing changes back to the shared Model data.
- [ ] Verified Model edit serialization and Model-driven animation temporal vertex encoder — pending source parity and round-trip tests.
