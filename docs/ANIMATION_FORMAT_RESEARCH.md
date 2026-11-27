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
6. The known-good native preview decoder writes compressed ModelPosition `i` to `frame.vertices[i]`; renderer triangle lookup separately applies `+ specialVertexCount`. This established preview mapping must not be changed based only on the frame vector's unified storage. The editor's selected-vertex and save-encoder mapping is a separate conversion and remains under investigation.

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

The earlier shifted-decoder conclusion was wrong for the known-good preview path. Baseline code decodes compressed ModelPosition `pi` into `frame.vertices[pi]`, while the renderer adds `specialVertexCount` to Model-local triangle indices. The decoder's destination, renderer's triangle lookup, and editor's encoder source must be studied as separate steps; do not force them into one index formula without sample-backed parity.

The separate temporal-prefix serialization finding remains: temporal words overlap the special-prefix bytes, and restoring stale raw XYZ after temporal serialization overwrites compressed delta bits. Keep serialized temporal bytes authoritative and verify the re-decoded pose.

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
- The temporal words are read from `fakeHeader` to the end of the frame item, **not from HeaderSize**. The special-vertex prefix occupies the first `specialVertexCount * 24` bits of that temporal stream; compressed deltas begin immediately after that prefix. Do not treat `HeaderSize` as a separate temporal-data start. The expansion routine may preserve exactly those prefix bytes, but must not write beyond the prefix into delta bits.
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


### Regression coverage audit — 2026-11-26

Current `tests/nsf_archive_tests.cpp` contains synthetic checks for a narrow compressed delta width being expanded to width 7, a coordinate edit to 255, save-time rebasing, special-vertex serialization, and preservation of an unencodable working pose. `tests/real_sample_compatibility_tests.cpp` compares decoded hashes/Model topology for real `El1eG/El1eV` and `Cr10G/Cr10V`, and exercises an uncompressed `BT10V` edit; it does not currently perform a real compressed Cr10/El1e edit that forces Model-width expansion and then save/reopen.

Therefore, the current tests support the general encoder/decoder mechanism but do not establish that the fallback succeeds for a real shared Model with multiple linked Animations or that the exact user-reported chunk prompt path is correct. Next regression should use the private retail sample, capture the actual ModelPosition records and first rejected delta, and test the complete edited pose through width expansion, all linked Animation payload rebuilds, save/reopen, and (separately) in-game.


### Width-expansion special-prefix regression correction — 2026-11-26

The earlier claim that `expandCompressedModelPositions` had a destination-index defect was **wrong**. The reference source `Frame.MakeVertices()` writes compressed positions to `verts[i]`, and the renderer separately adds `SpecialVertexCount` when indexing triangles. We changed expansion to `specialVertexCount + pi`; CI then failed on the regression's expected expanded pose. Inspection of the actual RE source confirmed the original `pi` mapping was correct, so the code was restored to `frame.vertices[pi]`.

The synthetic test had a second issue: its packed position fixture used `0x1f9`, which encoded X width 7, not width 1. That was corrected to `0x7f`. The test now exercises the width-1-to-width-7 expansion while asserting the reference decoder's unshifted destination behavior and save/reload pose.

Commits:
- Invalid fixture correction: [ed21d4c](https://github.com/leorelic/CrashEdit-VC/commit/ed21d4c97e606bb9acd6237fb3f1bf243a5aa872)
- Incorrect shifted-index attempt (reverted): [c9027db](https://github.com/leorelic/CrashEdit-VC/commit/c9027db85b567a43ec25a3c18da697c04c8d195e)
- Source-aligned correction: [a4e0154](https://github.com/leorelic/CrashEdit-VC/commit/a4e015407111393ab2b81891b27e7988775b99ce)
- Regression aligned with RE: [d3aa546](https://github.com/leorelic/CrashEdit-VC/commit/d3aa54664ae168fb7f87d5bac71067535da5a23b)

The exact user-reported coordinate-limit/chunk-prompt defect remains unresolved. CI and real retail sample/game validation are still required.


### Relocation identity regression — 2026-11-26

The first pending-pose preservation implementation matched entries by EID and frame index. That key is not unique: an NSF may contain multiple Animation entries with the same EID. In that case, restoration could reapply a dirty pose to the wrong entry and overwrite another pending pose, even though the raw relocation operation copies only the selected entry.

The restoration now keys each pending pose by its original chunk index, entry index, and frame index. When the selected entry is removed from its source chunk, entries after it are mapped to their shifted index; the selected entry maps to entry 0 of the newly appended destination chunk. A synthetic regression uses duplicate EIDs for the moved and retained entries and checks both distinct poses through save/reload.

The latest UI change also gives each special-vertex coordinate input a row-and-axis ID with a hidden label, avoiding reliance on repeated visible X/Y/Z labels. CI and the user's actual Cr10V workflow still need validation.


### Native unified-frame indexing and shared Model isolation — 2026-11-26

Cr10V reports 309 model/frame vertices and 4 special vertices, with 305 compressed ModelPosition records. Counts alone do not prove which editor frame slot corresponds to each ModelPosition or which triangle indices are Model-local. The attempted shifted decoder mapping corrupted the preview and has been reverted. Determine the per-vertex relationship from the retail sample and CrashEdit-RE's decode/render/edit/save path before asserting that any tail vertices are absent or changing the renderer.

A second save issue is shared Model mutation: widening a ModelPosition table changes the decode contract for every Animation referencing that Model. Rebuilding all linked entries can overflow unrelated chunks and cause repeated relocation prompts. The current patch uses a non-mutating encode probe to determine whether an edited Animation's current widths reject its pose; only then does it clone the compressed Model into a new type-0 chunk with an unused EID and redirect that Animation's frames to the clone before width expansion. This keeps other Animations on the original Model. Synthetic shared-model save/reload regression and CI are pending; actual Cr10V testing remains the acceptance gate.


### 2026-11-27 correction — decoder/render mapping and Cr10V preview recovery

A previous research conclusion proposed moving decoded compressed ModelPosition `i` to `frame.vertices[i + specialVertexCount]`. That conclusion was not verified against the baseline renderer and caused a preview regression. In the known-good `ff5359a` code, compressed positions decode to `frame.vertices[i]`; renderer Model triangle lookup still applies `+ specialVertexCount`. The editor/save path's ModelPosition-to-frame mapping is a separate conversion and must not be inferred from decoder storage.

The regression fix restores the original `decodeAnimationVertices()` implementation and retains the renderer's original special-vertex offset. Keep this preview behavior stable. Investigate edit/save independently by tracing the exact CrashEdit-RE decode, selection, encode, and triangle-index conventions against the private Cr10V sample. Specifically compare Model-local vertex index, decoded frame slot, selected frame slot, encoded ModelPosition slot, and save/reload slot. Do not change preview decoding or renderer indexing until this end-to-end mapping is demonstrated by a real sample and regression.


### User-confirmed Cr10V preview recovery — 2026-11-27

The user tested the recovery build and confirmed the Cr10V preview looks correct again. The source was compared against the known-good `ff5359a` checkpoint:

- `decodeAnimationVertices()` is identical to the `ff5359a` implementation. In the compressed path it reads the temporal stream from `specialVertexCount * 24` bits and stores decoded ModelPosition record `i` at `frame.vertices[i]`.
- The renderer continues to resolve Model triangle indices with `+ frame.specialVertexCount`. This offset is present in main triangle rendering, face picking/selection, face-edge overlay, and vertex/surface picking.
- **This exact decoder/renderer pairing is the known-good, user-visually-confirmed Cr10V preview contract.** Do not shift decoder destinations to `i + specialVertexCount`, and do not remove the renderer offset. Both attempted changes caused corrupted previews in prior builds.
- Decoder storage and edit/save indexing must be treated as separate mappings until proven against the retail Cr10V sample and CrashEdit-RE. Do not change the preview path to solve the coordinate-edit limitation.

The current source decoder was compared directly to `ff5359a` and matched. User visual confirmation was received on 2026-11-27. This is a preview recovery checkpoint only; unrestricted vertex editing remains unresolved.


## CrashEdit-RE source comparison: compressed-frame editing is read-only in the reference UI

Inspected the correct reference: [airumu/CrashEdit, `t/net9`](https://github.com/airumu/CrashEdit/tree/t/net9), specifically `Frame.cs`, `FrameVertex.cs`, `ModelPosition.cs`, `ModelEntry.cs`, `FrameBox.cs`, and `AnimationRenderer.cs`.

- RE's `Frame.MakeVertices(model)` starts the temporal cursor at `SpecialVertexCount * 24`, decodes ModelPosition records in X-Z-Y order, and stores record `i` at `verts[i]`.
- RE's renderer separately looks up a polygon vertex at `verts[tri.Vertex + frame.SpecialVertexCount]`. Model polygon index conversion applies a negative initial index bias for compressed Models. Do not collapse these distinct conversions into one guessed indexing rule.
- RE's legacy `FrameBox` labels compressed Model vertices read-only, and `UpdateTemporals()` immediately returns for compressed Models. `Frame.SaveC2()` writes the temporal bit array as-is; it does not encode changed cached positions back into compressed ModelPosition deltas.
- RE does expose `ModelEntry.ConvertCompressedToUncompressed()` and `DecompressFrame()`, which create an uncompressed Model/frame representation. This is a candidate safer editing architecture, subject to Cr10V sample tests and correct EID/reference handling.

### Native save-failure candidate

Current native `setAnimationFrameVertex()` sets `verticesEdited=true` even when the new component equals the existing value. A change followed by restoring the original coordinate therefore remains dirty and enters save processing.

There is also a concrete source asymmetry to investigate: native `decodeAnimationVertices()` was deliberately restored to the RE-compatible baseline that writes compressed ModelPosition record `i` to `frame.vertices[i]`, while native `encodeAnimationFrameVertex()` currently sources record `i` from `frame.vertices[i + specialVertexCount]`. RE's `Frame.MakeVertices()` confirms the former decode destination. Native `commitAnimationEdits()` verifies the post-encode decoded pose against the desired pose, so this mismatch is a plausible cause of no-op save rejection. This is not yet declared the proven runtime cause; add focused Cr10V no-op and edit-then-restore tests first.

**Next:** establish no-op and restore-to-original save regressions; trace first/last compressed positions and special-prefix boundary indices; compare each temporal bit against RE; then evaluate converting the linked Model + Animation to an uncompressed pair before editing. Keep the confirmed renderer/preview mapping untouched.


## Editor terminology: Normal and Complex

The 3D Animation Editor displays an **Animation Type** label based on the linked Model for the currently displayed frame:

- **Normal** = linked Model is uncompressed (`NsfModel::compressed == false`).
- **Complex** = linked Model is compressed (`NsfModel::compressed == true`).
- **Unknown** = linked Model cannot be resolved; do not infer a type.

The displayed frame's Model reference is selected using the existing Crash 2 `modelEid` / Crash 3 `linkedModelEid` logic. The label therefore follows frame playback. This terminology deliberately does not use `specialVertexCount`: special vertices are a separate frame-layout concept and must not be conflated with compressed Model positions.


## User-tested editor terminology: Normal / Complex — 2026-11-27

The 3D Animation Editor displays **Animation Type** from the linked Model's compressed-position representation:

- **Normal**: linked Model is uncompressed (`NsfModel::compressed == false`). User-confirmed sample: `BT10V` / `BT10G`.
- **Complex**: linked Model is compressed (`NsfModel::compressed == true`). User-confirmed sample: `Cr10V` / `Cr10G`.
- **Unknown**: linked Model cannot be resolved; do not guess.

The user verified both labels in the running editor. The label follows the Model reference for the currently displayed frame, is informational only, and does not change the renderer or serializer. Do not classify by `specialVertexCount`; special vertices are a distinct frame-layout concept.

Checkpoint: [e4aa8e5](https://github.com/leorelic/CrashEdit-VC/commit/e4aa8e57107e88467b3b05b9a36aa7dce975dfbc). This identification milestone does not mean Complex animation vertex editing is solved. Keep the known-good Cr10V preview behavior intact while investigating Complex edit/save support separately.

## Separate editing strategies for Normal and Complex — research memo (2026-11-27)

Normal and Complex are linked-Model encoding classifications, not values inferred from specialVertexCount.

### Reference behavior

Direct inspection of CrashEdit-RE (airumu/CrashEdit:t/net9) confirms:

- Frame.MakeVertices(model) uses the ModelPosition delta decoder for Complex and the byte-coordinate temporal decoder for Normal.
- Frame.SaveC2() writes the temporal bit array as-is; it does not encode edited cached positions into Complex ModelPosition deltas.
- RE's FrameBox marks Complex vertices read-only, and its temporal updater returns immediately for compressed frames. Thus RE is a decode/view reference, not a working Complex editing implementation.
- RE does provide ConvertCompressedToUncompressed(), which builds an uncompressed Model and uncompressed frame temporal streams. This is a candidate architecture, not a proven drop-in solution. Its conversion helper reuses the original Model/Animation EIDs, and its special-prefix handling needs independent verification. Shared Model references and special-vertex ordering must be handled safely in CrashEdit-VC.

### CrashEdit-VC guardrails and research plan

Preserve the known-good Cr10V decoder/renderer pairing from ff5359a. Do not shift decoded compressed positions or remove the renderer's special-vertex offset while studying editing. Treat decode storage, renderer polygon lookup, edit selection, and save encoding as distinct index mappings until traced against the real Cr10V sample.

Before enabling Complex editing: inventory BT10V/BT10G and Cr10V/Cr10G; trace every index and temporal bit around the special prefix; add no-op and edit-then-restore save tests; then compare a true Complex delta encoder against isolated Complex-to-Normal conversion. Conversion must preserve every frame pose, special vertices, collision/header data, polygon rendering, shared Model users, and archive/game behavior after save/reopen.

Do not claim Complex editing is solved. Keep Normal's existing working editing path stable and do not route Complex edits into it without an explicit, validated conversion step.
## Complex-to-Normal conversion and in-game compatibility (2026-11-27)

CrashEdit-RE has `ModelEntry.ConvertCompressedToUncompressed()`, proving a conversion approach was explored in that project. It does not establish that the output is game-compatible, and its current helper has two warning signs that must be resolved before porting:

- `DecompressModel()` drops the ModelPosition table but reuses Info and PolyData. `ConvertIndices()` changes its initial vertex-index bias based on whether Positions is null (`-Info[0x4C]` for compressed, zero for uncompressed). Keeping the same polygon stream while changing this condition can alter triangle vertex mapping.
- `MakeTemporals()` iterates `vertexCount - specialVertexCount` but reads `vertices[i]` from index zero, despite the uncompressed frame decoder reading ordinary coordinates from index `specialVertexCount`. Special-prefix preservation therefore needs a precise round-trip proof.

Treat the conversion as structurally plausible, not proven in-game. The target is the game's existing Normal/uncompressed representation. Validate all frames, world-space poses, special vertices, collision/header data, polygon-to-vertex mapping, ModelEID references/shared users, NSF reparse, and original-game playback. Then edit one vertex and repeat save/reopen/game playback.

Keep two eventual strategies: genuine Complex delta encoding and explicit Complex-to-Normal conversion. Conversion must be user-invoked, previewable, atomic, and rollback-safe; never silently convert during ordinary save. Preserve the verified Cr10V renderer/decoder.
## UI operation requirement: Convert to Normal — 2026-11-27

User-requested workflow: right-click a Complex Animation entry in Resource Hierarchy and choose **Convert to Normal**. Conversion overwrites the existing resources in place, retaining their EIDs; no new Model/Animation EIDs or duplicate resources are introduced in this first version.

Implementation constraints discovered in the native archive/UI:

- The Resource Hierarchy's `drawResourceContext` lambda handles right-click menus for chunk, entry, item, and frame nodes. The Animation entry row is the `drawResourceContext(1, chunk, entry, -1, ...)` call inside `EditorUI::drawHierarchy()`; add the operation specifically for valid decoded type-1 Animation entries whose linked Model is Complex.
- Native archive data is represented by `NsfEntry.items` and `NsfChunk.bytes`; `NsfArchive::chunks()` is const-only. A conversion API belongs on `NsfArchive`, not as direct UI byte mutation.
- `NsfArchive::decodeAnimationModels()` resolves each frame's linked Model EID and decodes the frame. For Crash 2, frame `modelEid` is the linked Model EID.
- A Model's compressed representation is a sixth item (ModelPosition records). Its info item stores the special-vertex count at offset `0x4C`; the normal Model parser uses the same field as its polygon vertex base metadata. Do not blindly zero it or rewrite polygon words: the renderer/model polygon index convention must be preserved.
- Animation frame items have variable sizes. Existing `replaceAnimationFramePayloads()` can rebuild an Animation entry's item payloads and repair following entry offsets, but Model entries need a corresponding safe item-list rebuild path.
- Existing `encodeAnimationFrameVertex()` already has a Normal/uncompressed temporal writer branch, including the Normal byte-lane permutation. A conversion implementation may reuse that logic only after the candidate Model has been rebuilt/reparsed as uncompressed and after frame payload capacity has been prepared; do not change the established decoder/renderer mapping.

### Shared-resource and atomicity rules

Because the user explicitly wants to retain current EIDs, the operation must overwrite the existing Model EID and Animation EID. Before mutation, enumerate every valid Animation frame in the archive whose `linkedModelEid` equals the target Model EID. All such frames must be converted in the same candidate archive; otherwise the shared Model would become Normal while other referencing Animations still contain Complex temporal data. If any referencing animation is undecoded, has an incompatible frame layout, cannot be represented in Normal's byte-coordinate range after safe frame-offset rebasing, or cannot be rebuilt within NSF chunk capacity, abort the entire operation without mutating the live archive.

### UI and validation acceptance

- Menu item appears only on decoded Complex Animation entry rows; it is absent for Normal entries, non-animation entries, and unsupported game versions/layouts.
- Menu action calls one archive-level conversion operation on a candidate copy. On success replace the live archive, refresh decoded animation/model state, preserve selection and viewer target, and add one undo-history action. On failure show a specific reason and leave original archive bytes and decoded state unchanged.
- Keep original EIDs, chunk/entry identity, texture/material data, collisions, frame offsets/world-space poses, special-vertex prefix, and triangle-to-vertex rendering semantics.
- Test multiple Animations sharing one Model, a single Animation, specialVertexCount=0 and nonzero, all frames, chunk-capacity failure, unsupported frame rejection, save/reopen, and byte-for-byte no-change on failed conversion.
- In-game acceptance is mandatory: the unchanged converted sample must play in the original Crash 2 game/emulator. Then edit one vertex in the converted Normal animation, save/reopen, and verify playback again.

**Status:** UI insertion point and archive integration constraints identified. The menu/operation is not implemented yet; do not expose a nonfunctional menu item or claim conversion support until the archive-level atomic conversion and regression tests exist.

## Experimental batch conversion workflow — 2026-11-27

User-approved direction: provide **Experimental → Convert Model/Animation**. Activating it opens a dedicated window in which the user selects a Complex Model; the window then lists every Animation in the archive that references that Model. The intended operation is a coordinated batch conversion of the selected Model and all of its linked Animations, rather than a context-menu action on one Animation entry.

### Workflow and resource scope

- Add a **Convert Model/Animation** button under the existing Experimental tab.
- Open a dedicated conversion window. Populate its Model selector with eligible Complex Models in the currently loaded archive.
- Once a Model is selected, enumerate every Animation frame/resource that references its EID. Show enough identifying information to distinguish entries (name when available, EID, frame count, and conversion eligibility/reason).
- Default to converting every linked Animation together; do not silently omit a linked Animation. If partial selection is exposed, clearly explain that every remaining reference must still be compatible with the converted Model. The safe initial behavior is to require all linked Animations to pass validation and convert as one batch.
- Convert the Model and its linked Animation frame payloads together, preserving existing Model and Animation EIDs. Do not create duplicate resources or change references in this first implementation.
- Report the complete conversion result and any validation failure before applying changes.

### Saving is part of the feature, not a later enhancement

The operation must modify the archive's real resource representation so the normal NSF/NSD save path writes the converted Model and Animation entries. It is not sufficient to convert only decoded in-memory viewer data. After conversion:

1. The archive must be marked dirty and the ordinary Save / Save As path must serialize the converted Model and every converted Animation entry, including updated item sizes, entry offsets, and chunk sizes as required.
2. Reopening the saved archive must parse the Model as uncompressed/Normal and every linked Animation frame as Normal, with all EIDs resolving correctly.
3. All frame poses, special vertices, collision/header bytes, texture/material data, and triangle-to-vertex rendering semantics must remain correct.
4. Saving must not depend on a special export path or leave the original Complex payloads in the file while only the preview changes.
5. The operation must be atomic: construct and validate a candidate archive first; only replace live archive state and mark it dirty after every resource has converted and archive-level validation succeeds. Any failure leaves the original archive byte-for-byte unchanged.

### Shared Model and linked Animation handling

This UI explicitly addresses shared Model references by making the Model the selection root and displaying all referencing Animations. Before conversion, build the complete reference set from the archive, not merely the currently visible hierarchy rows. A Model must never become Normal while any Animation still referencing it retains Complex temporal data. Initially, convert all compatible references as one transaction or reject the batch with a precise per-Animation reason. Do not silently convert a subset.

### Required tests and acceptance

- Model selector lists only supported Complex Models; Normal and unresolved/unsupported resources are not offered as convertible targets.
- Reference enumeration finds every Animation linked to the selected Model, including multiple Animation entries and multiple frames.
- Batch conversion preserves existing EIDs and references and does not create duplicates.
- Reject unsupported/malformed linked Animations, unrepresentable coordinates, item/chunk capacity failures, and incomplete reference sets atomically.
- Cover specialVertexCount zero and nonzero, multiple linked Animations, every frame, and archive chunk-boundary/size changes.
- Verify decoded poses and rendered triangle references before and after conversion.
- Save the converted archive through the normal save path, close/reopen it, and verify all resources reparse as Normal and remain editable through the existing Normal path.
- Edit a vertex after conversion, save/reopen again, and verify the edit persists.
- Finally load the saved NSF/NSD in the original Crash 2 game/emulator and verify animation playback. Editor preview and archive reparse alone do not prove game compatibility.

**Status:** workflow approved; implementation not started. Preserve the known-good Cr10V preview/renderer baseline. The conversion itself remains unproven for original-game compatibility until the game test passes.


### Implementation update — Experimental conversion discovery window (2026-11-27)

The first UI slice is now present on `cpp-port-foundation`:

- Experimental menu now contains **Convert Model/Animation...** (enabled for an open Crash 2 archive).
- The window lists decoded Complex Models and identifies each by EID plus chunk/entry location.
- Selecting a Model shows its vertex count, compressed-position count, special-vertex count, and decoded Animation entries/frames that reference its Model EID.
- The conversion action is deliberately disabled. This is an inspection window only; it does not mutate the archive and does not claim to convert or save anything.

This is not feature completion. The next implementation stage is the archive-level atomic Model + Animation payload converter, including frame stream generation, Model item-table rebuilding, chunk-capacity handling, reparse/pose validation, undo integration, and ordinary NSF save/reopen coverage. Do not enable the UI action until those pieces and regression tests are in place. Original Crash 2 game playback remains the final compatibility gate.

Source commits: `73245a3` (window state/declaration), `f922b51` (Experimental menu and discovery window).
