# Model Editor roadmap

## Goal

Build a Crash 2 Model Editor that safely decodes and edits the Model NSF entry, with the existing 3D Animation Editor acting as the live preview for model geometry and materials. The Model entry owns topology and rendering metadata; Animation frames own pose-specific vertex positions. Keep these responsibilities separate in the data model.

Research reference: original cbhacks/CrashEdit source, branch deprecate, especially the Model format classes and AnimationRenderer.cs. Treat the original implementation as a format reference, not an infallible specification. Crash 2 is the first validation target.

## Confirmed format outline

A Model entry contains five items, or six when the optional compressed-position metadata item exists:

| Item | Size / role | Known fields |
|---|---|---|
| 0 | 80-byte model info | XYZ scale; TPAG EIDs; counts for textures, polygon commands, colors, vertices, animated textures; special-vertex count used by compressed models |
| 1 | 32-bit polygon command stream | Interleaved triangle/index commands and color-pair commands; terminated by 0xFFFFFFFF |
| 2 | 4-byte color records | RGBA-like color bytes; per-corner color indices refer into this table |
| 3 | 12-byte texture descriptors | Four UV pairs, CLUT coordinates, color mode, blend mode, segment and page offset |
| 4 | 4-byte extended texture words | Flipbook animation metadata or LOD selectors, distinguished by sign in the original implementation |
| 5 (optional) | 4-byte compressed-position records | Per-axis base values and bit widths used to decode frame temporal vertex data |

Header offsets and counts observed in original source:
- Scale X/Y/Z: 0x00, 0x04, 0x08.
- TPAG EID array begins at 0x0C.
- Texture descriptor count: 0x34.
- Vertex count: 0x38.
- Color count: 0x3C.
- TPAG count: 0x40.
- Polygon command count: 0x44.
- Extended texture count: 0x48.
- Special vertex count for compressed models: 0x4C.

These offsets are source-confirmed from the original loader/model class. Validate the 80-byte header and every derived count/size before allocation or indexing.

## Key decoding findings

### Polygon command stream

The original ModelStruct decoder classifies a word as a triangle command when its upper 16 bits are nonzero; otherwise it is a color-pair command. 0xFFFFFFFF terminates the stream. A triangle command carries a texture index, animated flag, color index, position key, two unknown bits, original/duplicate index mode, a flag, and packed triangle type/subtype fields.

ModelEntry.ConvertIndices() performs a pre-pass to map original position keys to sequential vertex indices and resolve duplicate references. A second pass interprets triangle type 0/1/2 differently and carries color-pair state across adjacent commands to recover three vertex indices and three per-corner color indices. This is stream-stateful logic, not a simple one-word-to-one-triangle conversion.

Implementation must preserve raw command words, unknown fields, and stream ordering. Build a bounded decoder with explicit diagnostics for missing duplicate keys, invalid texture/color indices, malformed terminators, unsupported topology modes, and out-of-range vertex indices. Do not silently discard unsupported commands.

### Vertex colors

Item 2 is a table of four-byte color records. Each decoded triangle stores three color indices, one per corner; the renderer resolves those indices through the model color table. This means color data belongs to the Model resource and can be edited independently of Animation frame positions.

### Texture mapping and TPAG linkage

Each item-3 descriptor is 12 bytes and contains four UV coordinate pairs plus packed CLUT, color-depth, blend, segment, and page-offset fields. Triangle commands choose a descriptor by texture index. The descriptor's page selector is used with the model's TPAG EID list, which must resolve to NSF texture-page chunks. The original renderer also applies face/culling and winding decisions based on triangle type/subtype.

Animated texture words in item 4 can describe flipbook behavior or LOD metadata. Preserve raw words and initially expose their decoded interpretations as read-only until their timing/LOD consumers are independently verified against samples.

### Model-driven animation vertex decompression

The original Frame.MakeVertices(model) uses the model's optional item-5 position records for compressed animation frames. Each record provides X/Y/Z base values and per-axis bit widths. Temporal bits are consumed in X-Z-Y order, sign-extended per axis, accumulated with the base values, and wrapped to 8 bits. The model's special-vertex count determines the boundary between directly stored frame vertices and the compressed remainder. For models without position records, the renderer uses an uncompressed temporal path. Crash 3 applies an additional scale factor in the original code; do not apply that to Crash 2.

This is the strongest evidence that model metadata is essential to understanding and eventually re-encoding animation vertex streams. The native implementation must keep the game-version-specific rules explicit.

## Implementation phases

### Phase 0 — inventory and integration audit
- [ ] Audit the current native NSF parser, hierarchy, entry editing patterns, and Animation Viewer model lookup path.
- [ ] Identify whether existing Model entry bytes are retained losslessly and whether item count/length validation exists.
- [ ] Map the native NsfAnimationFrame decoded vertex representation to the original Frame.MakeVertices(model) behavior; record confirmed differences and missing fields.
- [ ] Gather multiple real Crash 2 Model entries and their referenced Animation frames, including compressed and uncompressed examples.

### Phase 1 — loss-preserving Model decoder
- [ ] Add a typed Model entry structure while retaining all original item bytes.
- [ ] Validate the 80-byte header, item count (5 or 6), all count/size products, TPAG references, and optional position-record count.
- [ ] Decode scales, TPAG EIDs, and all item counts.
- [ ] Decode item-2 color records and item-3 texture descriptors while preserving their raw bytes and unknown bits.
- [ ] Decode item-4 extended texture words without assuming every word is a flipbook record.
- [ ] Decode item-5 compressed-position records, retaining raw 32-bit words and unpacked base/bit-width fields.
- [ ] Add malformed-input and round-trip tests before exposing editing controls.

### Phase 2 — polygon topology decoder
- [ ] Implement a two-pass command-stream decoder matching original/duplicate position-key semantics.
- [ ] Reconstruct triangle indices, per-corner color indices, texture descriptor indices, winding, culling and supported subtype flags.
- [ ] Represent color-pair commands and state transitions explicitly in the decoded model rather than losing them after triangulation.
- [ ] Validate every index and reference; keep unsupported/unknown command fields visible.
- [ ] Compare reconstructed topology against the original Animation Viewer on multiple Crash 2 models.

### Phase 3 — live Model preview and inspection UI
- [ ] Add Model-specific hierarchy naming and a Model Editor inspector.
- [ ] Expose model header, scales, TPAG EIDs, vertex/color/texture/polygon counts, and compression metadata.
- [ ] Add selectable polygon/vertex inspection and per-corner color/UV/texture diagnostics.
- [ ] Use the existing 3D Animation Viewer as the live preview for a Model's linked animation when one is known; do not create a second renderer unnecessarily.
- [ ] Ensure model edits refresh the animation preview immediately without reopening the Animation entry or resetting camera/playback state.

### Phase 4 — safe material and geometry editing
- [ ] Enable edits to color records and supported texture descriptors first, with validated indices and immediate preview.
- [ ] Add edits to model scale and TPAG references only after texture-page resolution and renderer behavior are validated.
- [ ] Add topology edits only after the stream encoder is proven: triangle insertion/removal, vertex-key remapping, duplicate-key handling, and color-pair command regeneration.
- [ ] Keep model topology edits distinct from animation pose edits; warn when a topology change invalidates one or more animation frame vertex streams.
- [ ] Preserve unsupported commands and unknown fields byte-for-byte unless the user explicitly performs an operation that requires regenerating the affected stream.

### Phase 5 — animation editing unlocked by Model knowledge
- [ ] Use model position records to fully document compressed temporal vertex decoding and its bit ordering.
- [ ] Implement a matching temporal vertex encoder and round-trip tests for both compressed and uncompressed Crash 2 frames.
- [ ] Enable selected vertex coordinate edits only when the encoder can preserve unaffected bits and data.
- [ ] Investigate add/remove vertex support across every animation frame that references the model; topology and vertex counts must remain consistent.
- [ ] Revisit the frame Unknown (0x06) field only with sample comparisons or runtime evidence; do not conflate it with unrelated editor options.

## Validation and safety requirements

- Keep raw item bytes authoritative until a decoded structure has a verified serializer.
- Check all arithmetic for overflow before allocating arrays or computing item lengths.
- Reject or diagnose invalid triangle/color/texture/TPAG/vertex references without out-of-bounds reads.
- Test parse → serialize → parse equality for untouched entries and fields.
- Test edits on copies of representative Crash 2 NSF files, then reopen in CrashEdit-VC and compare against the original CrashEdit viewer.
- Do not claim game compatibility from a correct preview alone; in-game validation remains a separate step.
- Gate Crash 3-specific frame/model behavior behind explicit version handling.

## Current status

- [x] Animation vertex marker display, V shortcut, depth-aware hardware overlay, hover/selection coloring, and selected-vertex inspection — user-confirmed working on 2026-11-09 (major milestone).
- [ ] Model format decoder and Model Editor UI — not started in this phase.
- [ ] Model-driven animation temporal encoder — pending Model decoder and verified round-trip behavior.
