# Crash 2 animation format research and recovery roadmap

Status: active investigation; the current compressed/special-vertex rendering issue is **not resolved**.
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
6. Writes decoded compressed positions to the Model-linked portion of the frame, after the direct special-vertex prefix.

The renderer uses the compressed Model's reconstructed triangle indices with an added `specialVertexCount` vertex-index offset. Keep the distinction between a frame's full vertex array (special prefix plus compressed Model vertices) and Model-local vertex indices explicit at every call site.

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

Recent native patches changed compressed bit ordering and changed a synthetic special-vertex test's expected value to agree with that patch. The RE source shows the compressed decoder consumes the temporal array directly; it does not use the uncompressed byte-lane reorder. Treat those patches/tests as investigation history, not as an authoritative specification. Re-check the current native branch and test expectations against this document before relying on them.

The user has reported that some models/animations render correctly while others (including Cr10V / Cr10G-linked models) do not. This remains an open visual compatibility defect. Do not call it fixed based solely on CI.

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

- User-confirmed uncompressed save milestone: see `docs/MILESTONES.md`, 2026-11-23 checkpoint `4b6592852d761d742358313b0250add7112e411e`.
- Compressed/special-vertex visual correctness: **open**.
- Recent compressed bit-order changes and tests require reconciliation with the RE behavior above before being treated as stable.
- Next action: inspect current native decode/renderer against this specification and compare the actual Snow Go frame data; do not start another speculative byte-order patch.

## Verified save-path pitfall: special prefix aliases temporal bytes

The special vertex prefix and temporal words overlap at the same frame-relative offset. When writing updated temporal words, those writes can overwrite the raw special-vertex bytes even though the compressed delta cursor starts after the special prefix. Preserve the edited special prefix explicitly after temporal-word serialization, then re-decode the frame.

Regression: `nsf_archive_tests` edits a special vertex and a compressed model vertex, saves/reloads, and checks both coordinates. CI exposed this ordering bug at the special vertex reload assertion. Fix commit: `d6beb80a9a975a3e8faa2082e6e1b06732275898`. This is code-level round-trip evidence; it does not establish the Cr10V/Cr10G visual issue as resolved.


## Relationship to the existing project research log

This focused document supplements—not replaces—[`RESEARCH_NOTES.md`](RESEARCH_NOTES.md). Read both before modifying animation or Model decoding/rendering.

The broader research log already records important source/sample evidence:
- CrashEdit-RE baseline: `airumu/CrashEdit`, branch `t/net9` (not the legacy cbhacks repository).
- `Frame.MakeVertices()` compressed temporal decoding in signed X-Z-Y order, special-prefix handling, and ModelPosition packed base/width fields.
- Real `El1eG/El1eV` and `Cr10G/Cr10V` structure counts and triangle-record comparisons.
- The private sample workflow's golden checks for decoded frame vertex streams and triangle records, plus raw-entry preservation on save/reload.

Keep that evidence intact. The current visual defect does not invalidate those specific data-parity results, but neither do those results prove the complete native renderer is visually correct. Treat the issue as a separate unresolved end-to-end checkpoint: compare the same affected frame in RE and VC through decoded vertices, Model-local indices, final index offsets, coordinate transforms, materials/textures, and rasterized output. Do not re-run or replace already documented research unless a new reproducible discrepancy requires it.

The existing `MODEL_EDITOR_ROADMAP.md` also contains the Model-entry research and planned validation. Update that roadmap when a finding affects Model topology, texture/material resolution, or Model serialization; update this document for frame/temporal decoding; and update `RESEARCH_NOTES.md` for cross-subsystem findings or milestone evidence. Cross-link rather than copy long sections.


## Verified compressed-expansion write-order regression (2026-11-25)

CI caught a save/reload regression at tests/nsf_archive_tests.cpp:346: the edited special vertex did not survive expansion of a compressed Model position table. In expandCompressedModelPositions(), the code copied special XYZ bytes into the payload, encoded the compressed temporal bit array, then serialized temporal words over the same payload bytes. Because the special prefix intentionally aliases the first temporal bytes, this final serialization clobbered the special vertex.

Correct order: serialize all temporal words first, then restore each special vertex's raw XYZ bytes at fakeHeader + vi*3. This is not a compressed bit-order conversion; it preserves the format's overlapping raw prefix. Regression coverage edits both a special vertex and a compressed Model vertex, saves, reloads, and checks the special XYZ values. Patch: a4d6228 (2026-11-25). CI must pass on all platforms before treating the save-path fix as verified.
