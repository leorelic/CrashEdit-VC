# Crash 2 animation format research and recovery roadmap

Status: active investigation; the compressed decoder destination and temporal-prefix save behavior have been re-aligned with CrashEdit-RE source. CI, real-sample, and in-game validation are still required; the visual compatibility defect remains open.
Reference baseline: [CrashEdit-RE, airumu/CrashEdit, branch t/net9](https://github.com/airumu/CrashEdit/tree/t/net9).
Primary source files:
- `Crash/Formats/Crash Formats/Animation/Frame.cs`
- `Crash/Formats/Crash Formats/Model/ModelPosition.cs`
- `CrashEdit/Utils/OpenTK Integration/AnimationRenderer.cs`
- `CrashEdit/Controls/3D/AnimationEntryViewer.cs`

This document is the durable record for animation parsing, decoding, model linkage, coordinate transforms, and acceptance tests. Read it before changing animation/model decoding. Do not restart the same source investigation unless a cited behavior is contradicted by a reproducible sample or newer authoritative source.

## Evidence and rules

- **RE source-confirmed** means the behavior is directly visible in the named t/net9 source.
- **Sample-confirmed** must identify the exact game sample/file and resource EIDs.
- **VC tested** means a native regression test passed.
- **Game tested** means the user confirmed the produced build and saved file in-game.
- Never promote a hypothesis or a synthetic test's expected value to a format fact without independent support.
- A passing CI build or synthetic round-trip is not proof that Cr10V/Cr10G renders correctly.
- Preserve CrashEdit-VC's independent MIT implementation. Use the reference to understand format behavior; do not copy its code.

## Crash 2 frame structure (RE source-confirmed)

The Crash 2 frame fixed header is 24 bytes:
- signed 16-bit X/Y/Z offsets at 0, 2, 4
- unknown signed 16-bit field at 6
- vertex count at 8
- collision count at 12
- linked Model EID at 16
- header-size value at 20

Each collision record is 0x28 bytes and is explicitly described by RE as never compressed. The byte offset after the fixed header and collisions is:

`fakeHeaderSize = 24 + collisionCount * 0x28`

The special vertex prefix length is:

`specialVertexCount = (headerSize - fakeHeaderSize) / 3`

Those vertices are stored as direct XYZ bytes at `fakeHeaderSize + i*3`. The temporal bit array begins at `fakeHeaderSize` too: the direct special-vertex bytes overlap the first temporal bytes. This overlap is intentional in RE's loader; do not skip those bytes in the temporal array except through the decoder's bit offset.

RE reads each 32-bit temporal word as a little-endian integer, then extracts bits from bit 31 down to bit 0 into its boolean array. RE's `Frame.SaveC2()` writes that boolean array back with the same MSB-first word packing. This establishes the temporal bit-array order; do not add an extra byte-lane reorder to the compressed path.

## Compressed model position records (RE source-confirmed)

A compressed Model has an optional sixth item: one 4-byte position record per compressed model vertex. `ModelPosition` extracts:
- X base: bits 25..31
- Z base: bits 17..24
- Y base: bits 9..16
- X width: bits 6..8
- Z width: bits 3..5
- Y width: bits 0..2

RE's compressed frame decoder:
1. Starts the temporal bit cursor at `specialVertexCount * 24`.
2. Iterates compressed Model position records in their stored order.
3. Resets an axis accumulator to zero when that axis width is 7.
4. Reads signed delta fields in **X, Z, Y** order. Each field is one sign bit followed by the configured number of magnitude bits, sign-extended.
5. Adds the Model base (X base is shifted left by one; Z and Y are not) and delta to the corresponding accumulator, then wraps each accumulator to 8 bits.
6. Writes decoded compressed positions to `verts[i]`, starting at frame vertex index zero, as `Frame.MakeVertices()` does in CrashEdit-RE. `SpecialVertexCount` controls the temporal cursor and the renderer's triangle-index offset; it does not shift this decoder destination.

The renderer uses the compressed Model's reconstructed triangle indices with an added `specialVertexCount` vertex-index offset. This is distinct from `Frame.MakeVertices()`, which writes compressed Model positions beginning at output index zero. Preserve this source behavior rather than assuming the decoder destination is `specialVertexCount + modelPositionIndex`.

## Uncompressed frame bit order (RE source-confirmed)

RE's uncompressed path performs a byte-lane reorder on the temporal bit array, per 32-bit word, before reading 24 bits per vertex. The reorder maps each byte lane to the opposite lane while preserving bit order inside the byte. This conversion is specifically in the uncompressed decoder. It must not be generalized to compressed deltas.

## Renderer coordinate pipeline (RE source-confirmed)

For a resolved frame/Model, RE:
- resolves the Model for that frame;
- calls `Frame.MakeVertices(model)` (or uses already cached frame positions);
- converts frame offsets to translation by dividing each offset by 4;
- builds scale from Model ScaleX/Y/Z divided by `GameScales.ModelC1 * GameScales.AnimC1`;
- permutes each decoded point from (X,Y,Z) to (X,Z,Y);
- adds the frame translation, then applies the Model scale;
- for triangle rendering, indexes `verts[tri.Vertex[v_n] + frame.SpecialVertexCount]`;
- resolves per-corner Model colors, texture descriptor, TPAG, UV order/flip, and triangle subtype.

When diagnosing a model that appears flat, skewed, tiny, or partly missing, inspect the entire pipeline—not just compressed temporal bits. In particular, verify the linked Model EID per frame, decoded frame vertex count, special prefix count, Model-local triangle indices, index offset, axis permutation, model scales, frame offsets, polygon reconstruction and material/texture resolution.

## Known investigation mistake to avoid

The compressed Model position destination and special-prefix save behavior were previously misdiagnosed. CrashEdit-RE `Frame.MakeVertices()` writes compressed positions to `verts[i]` starting at zero; `SpecialVertexCount` controls the temporal cursor and renderer triangle-index offset. `Frame.SaveC2()` serializes temporal words over the overlapping prefix and does not restore old raw XYZ bytes afterward.

Do **not** restore stale special-vertex bytes after temporal serialization: that overwrites compressed delta bits. The native save path now verifies the re-decoded pose and refreshes its special-vertex cache from the bytes actually serialized. This behavior is source-aligned and regression-tested. See the 2026-11-26 milestone and the correction in `RESEARCH_NOTES.md`.

## Roadmap and acceptance gates

### 1. Establish an independent visual oracle
- Use the same unmodified Snow Go NSF/NSD and linked Cr10V/Cr10G resources from the private Game Samples repository.
- Record animation EID, frame index, frame's linked Model EID, vertex count, special count, compressed Model position count, and triangle count.
- Compare the same frame/resource in CrashEdit-RE and CrashEdit-VC. Capture at least one frame that currently looks wrong and one that looks correct.
- Confirm that the sample is byte-identical to the known unmodified source before interpreting results.

### 2. Trace decoded data before changing rendering
For the chosen bad frame, compare RE and VC at these checkpoints:
- frame header fields and collision boundaries;
- special vertex bytes and temporal bits;
- each Model position record and axis width/base;
- decoded full frame vertices before transforms;
- Model-local triangle indices and color indices;
- final renderer vertex indices after special-prefix offset;
- post-axis-permutation, offset, and scale coordinates.

Use first-difference diagnostics with exact vertex/axis/bit positions. Avoid speculative global byte-order changes.

### 3. Correct the smallest verified mismatch
- Make one focused code change based on a reproducible difference.
- Add a regression based on the source/sample oracle; assert all relevant vertices and the rendered/model linkage, not one convenient coordinate.
- Keep uncompressed and compressed fixtures separate.
- Test frames with zero and nonzero special-vertex prefixes, multiple compressed axis widths (including width 7 reset), multiple frames/models, and a model shared by animations.
- Verify save/reload and in-game behavior separately from decode correctness.

### 4. Milestone acceptance
Do not declare an animation-decoding milestone until:
- CI passes on supported platforms;
- unmodified representative compressed and uncompressed samples render correctly;
- Snow Go Cr10V/Cr10G affected frames match the RE visual/data oracle;
- edit/save/reopen preserves the intended pose and the game displays it correctly;
- no unrelated animation/model is substituted when frame payloads relocate or chunks split;
- a milestone entry records the tested sample, resource EIDs, build/commit, test evidence, and remaining limitations.

## Current state

- Recovery checkpoint [`ff5359af45779fcd29b3827a563215b433cef1ac`](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac): user reports the animation preview looks correct again and has confirmed saving works. Treat this as a stable baseline, not resolution of the original transform-limit defect.
- Original defect still open: some vertices cannot be moved past a particular X/Y/Z gizmo location; the editor wrongly prompts for a new NSF chunk, and the edit is still not applied. Investigate the actual Animation frame coordinate/encoding boundary and the gizmo/save validation path. Chunk expansion should not be offered as the remedy unless a separate, demonstrated chunk-capacity problem exists.
- Determine the precise boundary from source and controlled sample edits: record the frame/vertex, requested and current coordinates, axis, encoded base/width/reset state, frame offset/boundary fields, and exact rejection path. Compare the limit used by the gizmo with the limit accepted by the encoder and serialized frame.
- Preserve exact-pose verification. Do not silently clamp, wrap, drop, or misroute an edit. Confirm the previously blocked movement is applied and survives save/reopen; validate original-game behavior separately.
- Native tests and private real-sample checks passed for representative compressed `Cr10G/Cr10V` and `El1eG/El1eV` resources. This is code-level evidence, not user confirmation that the coordinate-limit issue is resolved.

## Relationship to the existing project research log

This focused document supplements—not replaces—[`RESEARCH_NOTES.md`](RESEARCH_NOTES.md). Read both before modifying animation or Model decoding/rendering.

The broader research log already records important source/sample evidence:
- CrashEdit-RE baseline: `airumu/CrashEdit`, branch `t/net9` (not the legacy cbhacks repository).
- `Frame.MakeVertices()` compressed temporal decoding in signed X-Z-Y order, special-prefix handling, and ModelPosition packed base/width fields.
- Real `El1eG/El1eV` and `Cr10G/Cr10V` structure counts and triangle-record comparisons.
- The private sample workflow's golden checks for decoded frame vertex streams and triangle records, plus raw-entry preservation on save/reload.

Keep that evidence intact. The current visual defect does not invalidate those specific data-parity results, but neither do those results prove the complete native renderer is visually correct. Treat the issue as a separate unresolved end-to-end checkpoint: compare the same affected frame in RE and VC through decoded vertices, Model-local indices, final index offsets, coordinate transforms, materials/textures, and rasterized output. Do not re-run or replace already documented research unless a new reproducible discrepancy requires it.

The existing `MODEL_EDITOR_ROADMAP.md` also contains the Model-entry research and planned validation. Update that roadmap when a finding affects Model topology, texture/material resolution, or Model serialization; update this document for frame/temporal decoding; and update `RESEARCH_NOTES.md` for cross-subsystem findings or milestone evidence. Cross-link rather than copy long sections.


## Correction to earlier save-path notes (2026-11-26)

Earlier entries in this document and `RESEARCH_NOTES.md` incorrectly recommended restoring raw special XYZ bytes after temporal-word serialization. That recommendation was wrong for the verified CrashEdit-RE behavior. The temporal stream intentionally aliases the prefix; serializing temporal words last clobbers stale raw XYZ, and restoring those bytes afterward clobbers compressed deltas. The current implementation keeps the temporal serialization authoritative, checks the re-decoded pose, and refreshes cache data from the serialized bytes. Do not revive the old restore-after-serialization patch.


## Source trace: frame prefix, compressed-coordinate limits, and chunk prompt (2026-11-26)

**Source-confirmed:** compared CrashEdit-RE `airumu/CrashEdit` branch `t/net9` (`Frame.cs`, `ModelPosition.cs`) with the current native implementation (`src/crash/formats/nsf/nsf_archive.cpp`, `src/ui/editor_ui.cpp`). This is a code-path explanation; the exact user's failing frame/vertex/axis still needs a controlled sample trace.

### Frame layout and boundary

- For Crash 2, the fixed frame header is 24 bytes; each collision record is 0x28 bytes. `fakeHeader = 24 + collisionCount * 0x28`.
- `HeaderSize` is validated against `fakeHeader` and the frame item size. The special-vertex prefix is `(HeaderSize - fakeHeader) / 3` vertices; its byte count must be divisible by 3.
- The temporal words are read from `fakeHeader` to the end of the frame item, **not from HeaderSize**. Therefore the special-vertex raw bytes and temporal-word stream alias the same payload prefix. Do not treat `HeaderSize` as a separate temporal-data start or restore stale raw prefix bytes after temporal serialization.
- The frame item’s own byte length and 32-bit temporal alignment are separate from the NSF chunk’s 64 KiB capacity.

### Coordinate representation

- Crash 2 decoded local XYZ components are unsigned bytes, 0–255. Frame offsets are signed 16-bit values; the renderer adds the frame offset (divided by 4) and then applies linked Model scale.
- Compressed ModelPosition records provide per-axis bases and delta widths. Width 7 is the full signed-byte delta width and also resets that axis accumulator. The decoder accumulates each axis modulo 256.
- Native encoder `encodeAnimationFrameVertex` rejects local values outside 0–255. For compressed positions it computes a shortest modulo-256 signed delta and verifies that delta against each record’s width. A narrow width can reject a target even when the target itself is a valid byte coordinate.
- Save-time Crash 2 rebasing can shift a whole frame into the 0–255 local byte range while adjusting signed-16-bit frame offsets, provided each axis span is at most 255 and the adjusted offsets remain representable. A pose spanning more than 255 local units on an axis cannot be represented by one byte-coordinate frame.

### Why the chunk prompt can appear

- When a compressed edit cannot be encoded with the Model’s current widths, `commitAnimationEdits` calls `expandCompressedModelPositions`.
- That fallback changes every compressed position’s X/Y/Z widths to 7 and rebuilds all Animation entries/frames that share the Model, because the Model position table is shared. The enlarged temporal payloads may be substantially larger.
- If rebuilding one affected Animation entry does not fit its existing NSF chunk, the expansion returns an error containing that chunk/entry. `EditorUI::saveFiles` converts this specific payload-capacity error into the “move Animation entry to a new chunk” prompt.
- Thus the prompt is a downstream **archive-capacity failure during compressed-Model expansion**, not a coordinate-space limit. Relocation can provide room, but it cannot by itself resolve a frame whose axis span exceeds 255, an offset overflow, a malformed special-prefix layout, or an encoding mismatch.
- The expansion is performed on a candidate archive during save, so a failed commit preserves the working pose and does not commit the partial Model-width changes.

### Next controlled trace (no speculative patch)

For the exact blocked edit, capture: game/version; animation EID, chunk/entry and frame index; linked Model EID and its chunk/entry; selected vertex and axis; current/requested local coordinate; frame offsets; `HeaderSize`, collision count, special count; ModelPosition base and X/Z/Y widths/reset bits for every compressed record up to the edited vertex; frame byte size and current chunk used-end. Then distinguish these outcomes:

1. **Encoder-width rejection:** requested byte coordinate is valid, but the current delta width rejects it; verify width-7 fallback reconstructs the entire edited pose.
2. **Frame-space rejection:** local frame span exceeds 255 or required rebased offsets exceed signed 16-bit; no chunk move can fix this.
3. **Payload capacity:** width expansion is valid but one or more shared Animation entries cannot fit their current chunk; identify the exact entry named by the capacity error and relocate only that proven entry, then retry.
4. **Boundary/index mismatch:** special-prefix count, compressed-position count, vertex index, or temporal cursor differs from the reference; compare exact bit/byte offsets before changing code.

Keep the `ff5359a` preview/save checkpoint intact. Do not change the encoder until the failing sample produces one of these exact, logged outcomes and the relevant source/sample parity check.
