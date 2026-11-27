# NSF resource support research

Research reference: [CrashEdit-RE](https://github.com/airumu/CrashEdit/tree/t/net9), repository airumu/CrashEdit, branch t/net9. The implementation notes below are based on its format loaders, resource models, controllers, and animation renderer. This is a format-analysis checkpoint, not a claim that these formats are implemented in CrashEdit-VC. Do not use cbhacks/CrashEdit or its deprecated branches as the comparison baseline.

## Entry types versus chunk types

NSF entry types live inside type-0 entry chunks. Texture pages are different: the original editor represents them as **chunk type 1**, not as a normal entry. CrashEdit-VC already recognizes type-1 chunks and opens its Texture Viewer / CLUT tools for them. The next work should extend their linkage from model texture-page references rather than invent a separate Texture entry type.

## Type 1 — Animation

Original source:
- `Crash/Formats/Crash Formats/Animation/AnimationEntryLoader.cs`
- `Crash/Formats/Crash Formats/Animation/Frame.cs`
- `Crash/Formats/Crash Formats/Animation/FrameCollision.cs`
- `CrashEdit/Controls/3D/AnimationEntryViewer.cs`
- `CrashEdit/Utils/OpenTK Integration/AnimationRenderer.cs`

An Animation entry contains a sequence of frame items. Crash 2 frames use a 24-byte header; Crash 3 frames use a 28-byte header. The frame records three signed offsets, a small unknown field, vertex count, collision count, a model/unknown reference field, and a header-size value. Each collision record is 0x28 bytes. The header-size boundary distinguishes special, directly stored vertices from the remaining temporal bitstream.

For Crash 2, the frame header directly supplies the Model EID. The renderer resolves that Model entry for each frame. For Crash 3, the original viewer can derive a model list from GOOL vertex frame groups, and supports model cycling when multiple models correspond to an animation.

The original renderer:
1. Resolves the model for the current frame.
2. Decodes frame vertices. Compressed frames use the model's packed position records as per-axis bit widths and bases; uncompressed frames read signed/delta-style bitstream data.
3. Applies frame offsets and model scale, including the original axis permutation.
4. Walks the model's decoded polygon commands, choosing vertex colors and texture coordinates.
5. Resolves texture-page EIDs and animated-texture metadata from the model.
6. Optionally interpolates adjacent frames when compatible and enabled.
7. Can display the frame's collision boxes.

**Viewer requirement:** add a separate dockable **3D Animation Viewer** alongside **3D Zone Editor**. It must have its own camera/playback state. The current Animation and its resolved Model should be read from the live NSF data so model edits are reflected immediately without reopening the animation. The initial viewer should support frame stepping/playback, linked-model display, interpolation toggle, texture toggle, and collision visibility. Unknown frame fields must remain exposed as raw values until understood.

## Type 2 — Model

Original source:
- `Crash/Formats/Crash Formats/Model/ModelEntryLoader.cs`
- `Crash/Formats/Crash Formats/Model/ModelEntry.cs`
- `Crash/Formats/Crash Formats/Model/ModelStruct.cs`
- `Crash/Formats/Crash Formats/Model/ModelTexture.cs`
- `Crash/Formats/Crash Formats/Model/ModelPosition.cs`
- `Crash/Formats/Crash Formats/Model/ModelExtendedTexture.cs`

A Model entry has five items, or six when compressed-position records are present:
1. Model information/header (scale, texture-page EIDs, and element counts).
2. Packed polygon command words.
3. Vertex color records.
4. Per-polygon texture/UV/CLUT/page descriptors (12 bytes each).
5. Animated-texture/LOD words.
6. Optional packed base-position and bit-width records (4 bytes each).

The polygon stream is not a plain index buffer. It interleaves triangle/index commands and color-pair commands, has a terminating word, supports original and duplicate vertex keys, and encodes several triangle topology/subtype modes. It must be decoded in stream order to reconstruct triangle vertices and per-corner colors correctly. Texture descriptors contain UVs, CLUT coordinates, color depth, blend mode, segment, and texture-page offset. Extended texture words can represent flipbook animation or LOD metadata.

**Editing requirement:** the Model need not automatically open in the 3D viewer. Its editable decoded data must remain the authoritative source used by the Animation Viewer, so changes are visible there immediately.

**Implementation milestone (2026-11-12):** CrashEdit-VC now has an in-viewport Face Texture Selector in the Animation Editor. On the user-tested TNT Box face, atlas selection can reposition the existing UV mapping while preserving its relative shape. The placement bounds are based on the triangle's UV1-UV3; auxiliary UV4 no longer blocks otherwise valid atlas positions. The existing fixed-size texture-descriptor setter remains the NSF write path. See [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md) and [RESEARCH_NOTES.md](RESEARCH_NOTES.md) for the implementation trail and remaining Cr10T / multi-color-mode validation.

## Type 4 — Sort List (SLST)

Original source:
- `Crash/Formats/Crash Formats/SLST/SLSTEntry.cs`
- `Crash/Formats/Crash Formats/SLST/SLSTEntryLoader.cs`
- `Crash/Formats/Crash Formats/SLST/SLSTSource.cs`
- `Crash/Formats/Crash Formats/SLST/SLSTDelta.cs`

A Sort List is a sequence of items: a start source, zero or more delta records, and an end source. A source record has a 4-byte header (signed 16-bit value count and type, currently expected to be 0), followed by 16-bit polygon IDs. A delta record has a header containing node count, type (expected 1), add-node index and swap-node index, followed by 16-bit node values split into remove, add, and swap ranges. This is not simply a flat list of polygon IDs; the delta sequence describes changes between sort-list states.

CrashEdit's SLST controller family exposes source and delta structures separately. CrashEdit-VC should preserve the original item sequence and unknown/invalid values, validate all count/index boundaries, and provide an editor for the start/end sources and each delta. Runtime ordering/world-indexing behavior should be investigated separately from binary parsing.

### Relationship to the future Scenery / Geometry authoring category

As of 2026-11-16, SLST entry research is explicitly grouped with the future **Scenery / Geometry Editing and level-object authoring** category in [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md). This category will cover the eventual scenery/geometry editing workflow and level-object creation, import, conversion, or related authoring. SLST research is part of that same body of work, rather than a detached resource-editor task.

Research should connect the binary source/delta sequence to actual scenery polygon ordering, camera-dependent display behavior, zone/level-object relationships, and the game's runtime constraints. Confirm whether and where SLST is required for newly created or converted geometry; do not assume all scenery or level objects share one path. Parsing and lossless inspection can establish the format, but an SLST generator and integrated authoring UI must wait for sample-based semantic analysis and original-game validation.
## Type 12 — Sound

Original source:
- `Crash/Formats/Crash Formats/Sound/SoundEntry.cs`
- `Crash/Formats/Crash Formats/Sound/SoundEntryLoader.cs`

A Sound entry has one item decoded as a `SampleSet`. The original documentation notes that the entry itself does not contain all playback metadata such as sample rate; object code determines playback parameters. Support should begin with lossless sample-set decoding and metadata inspection. Playback must not assume a universal sample rate.

## Type 13 — Music

Original source:
- `Crash/Formats/Crash Formats/Music/MusicEntry.cs`
- `Crash/Formats/Crash Formats/Music/MusicEntryLoader.cs`

A Music entry has three items:
1. A 36-byte header: sequence count, VH EID, and seven VB resource EIDs.
2. An optional VH wavebank header.
3. SEP data containing the SEQ tracks.

The original editor models each sequence as a track and retains the VH and VB resource references. The zone header selects a music EID for playback. Implement parsing and resource-reference inspection before attempting playback; the original Crash music uses a custom instrument bank and is not General MIDI.

## Type 19 — Demo

Original source:
- `Crash/Formats/Crash Formats/Demo/DemoEntry.cs`
- `Crash/Formats/Crash Formats/Demo/DemoEntryLoader.cs`

The original loader expects one item and wraps it as an opaque `MysteryUniItemEntry`. It does not document or decode an input-event stream. The proposed prerecorded-input interpretation remains a hypothesis. First support lossless raw inspection, item metadata, and export; only decode records after comparing multiple real Demo entries and tracing their consumers.

## Texture pages

Original source:
- `Crash/Formats/Crash Formats/Texture/TextureChunk.cs`
- `Crash/Formats/Crash Formats/Texture/TextureChunkLoader.cs`

Texture pages are chunk type 1. The EID is stored in the page data header, and the original editor treats each page as directly uploaded VRAM data. The current CrashEdit-VC Texture Viewer/CLUT path is already attached to selected type-1 chunks. The next integration is resolving the model's TPAG EIDs to these chunks and honoring the model's texture descriptors.

## Recommended implementation order

1. **Animation + Model foundation:** bounded decoders for frame headers/collision/special vertices/temporal data and model headers/polygon streams/colors/textures/compressed positions. Keep raw item bytes authoritative and preserve unknown fields.
2. **3D Animation Viewer:** resolve Animation-to-Model links, render decoded topology and textures, support playback/interpolation/collision display, and keep the viewer live against model edits.
3. **SLST:** bounded source/delta parsing and lossless format research. Keep the integrated SLST editor/generator and its relationship to scenery geometry, level objects, and polygon sorting grouped with the future Scenery / Geometry authoring category in [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md); do not treat it as a detached near-term editor task.
4. **Sound:** decode the single SampleSet item and inspect sample metadata without guessing playback parameters.
5. **Music:** parse the 36-byte header, VH, SEP tracks and referenced VB resources; then consider a custom-bank-aware preview.
6. **Demo:** raw lossless support first, research consumer code and real files before defining its structure.
7. **Cross-resource references and regression fixtures:** verify EID resolution, missing/duplicate references, malformed inputs, save/reload and Crash 2 retail fixtures. Crash 3 format differences remain a separate compatibility pass.

## Safety and compatibility rules

- Never discard or normalize unknown fields when a resource is re-saved.
- Validate every item count, offset, byte size, vertex index, polygon index and referenced EID before decoding or rendering.
- Keep parsing separate from UI and rendering.
- Treat the original editor as a format reference, not as proof that every heuristic is universally correct.
- Crash 2 is the first validation target; Crash 3-specific layouts and GOOL-derived model association must be gated by game version.


## Model/Animation editor relationship

The shared Model/Animation architecture and current priorities are maintained in [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md). The immediate authoring priority is to make Bone creation and per-frame posing usable; after that, the roadmap shifts to Zones / Level Editing. This file remains the format-reference notebook for NSF resource types.

The key architectural relationship is source-confirmed in the original CrashEdit animation renderer: each Crash 2 frame resolves its Model EID; the frame's vertex stream is decoded using that model's optional compressed-position records; the model's decoded triangles then select vertex indices, per-corner colors, texture descriptors, animated texture state, and TPAG pages. Consequently, model geometry and material edits should update the existing Animation Viewer live, while pose positions remain owned by the Animation frame.

Vertex inspection/selection, frame editing, and the tested Complex-to-Normal conversion workflow have since been implemented. This does not enable topology changes: a variable-size Model serializer and dependent Animation updates are still required before adding/removing polygons or changing vertex counts.


## Source-baseline correction

All future format and UI comparisons in this roadmap use CrashEdit-RE's t/net branch. The previous research note that cited cbhacks/CrashEdit was an incorrect source selection and has been corrected in [RESEARCH_NOTES.md](RESEARCH_NOTES.md). The Model Editor roadmap now cites the exact CrashEdit-RE source files and branch used for its findings.


**Real-game texture-mod validation (2026-11-12):** the user confirmed a complete TNT Box workflow: edit the face texture in CrashEdit-VC, save the NSF, reopen it successfully in CrashEdit-RE, and load it in the original Crash Bandicoot 2 game. The in-game screenshot confirms the modified box textures render correctly. This is a targeted real-archive/game compatibility result for the tested face-texture edit; it does not yet establish compatibility for every model or color mode. See the end-to-end milestone in [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md) and [RESEARCH_NOTES.md](RESEARCH_NOTES.md).

---

## NSF chunk capacity and whole-entry relocation — 2026-11-12

### Confirmed container layout

CrashEdit-VC decodes each NSF chunk to a fixed **64 KiB** byte buffer. A normal entry chunk has type `0` in the 16-bit field at byte offset 2. Its header fields used by the current parser are:

| Offset | Size | Meaning |
|---:|---:|---|
| `0x00` | 2 | Chunk magic (`0x1234` for a raw chunk; `0x1235` for compressed input) |
| `0x02` | 2 | Chunk type |
| `0x04` | 4 | Chunk ID field |
| `0x08` | 4 | Entry count for type-0 chunks |
| `0x0C` | 4 | Retained header/reserved bytes |
| `0x10` | `4 * (count + 1)` | Entry-offset table, including the terminal used-data offset |

The type-0 offset table therefore ends at `20 + 4 * entryCount`. Each entry begins with the `0x0100FFFF` entry marker, followed by its EID, entry type, item count, and an item-offset table. Entry/item offsets are checked against their containing boundaries by the native parser.

A type-1 texture page is not an entry inside this table; it is a separate chunk type. Do not move a type-1 texture chunk using the type-0 entry relocation path.

### Why frame insertion can fail

Animation entries are variable-size entry blocks. Their item table grows when a frame is appended or inserted, and the selected frame's complete raw item bytes are duplicated. Rebuilding the item table can enlarge the entry; the entry's new size shifts subsequent entries and advances the chunk's terminal used-data offset. The operation must fit all of the following inside the same 64 KiB chunk:

- the enlarged type-0 entry-offset table;
- the entry's enlarged frame-item-offset table;
- any retained prefix padding before the first item;
- every frame payload, including the duplicated frame;
- retained trailing padding; and
- all following entry blocks and the chunk's terminal used-data boundary.

A frame edit must not silently truncate an entry, overwrite a following resource, or simply increase the chunk buffer beyond 64 KiB.

### Implemented relocation behavior

The 2026-11-12 implementation adds a capacity preflight for Animation Append/Insert and a generic `moveEntryToNewChunk` operation for valid entries in normal type-0 chunks.

When the preflight proves the requested edit cannot fit:

1. The UI asks whether to create a new chunk and move the complete entry.
2. Cancel leaves the archive unchanged.
3. Confirmation first works on a candidate archive copy, so validation failure does not partially mutate the live archive.
4. The selected raw entry block is copied in full to a new normal type-0 chunk. Its EID, entry type, item data, and entry-internal offsets are retained.
5. The original chunk is rebuilt with its remaining entries, updated entry count and offset table, while preserving its original header bytes, prefix padding, complete remaining entry blocks, and unused tail bytes.
6. Both rebuilt chunks are decoded and their entry tables validated before the candidate replaces the live archive.
7. The selected resource location is changed to the new chunk, then the original Append/Insert operation is retried against the relocated entry.
8. GOOL, zone, and Animation/Model decoded structures are refreshed where their decoders had been active.

The new chunk is currently emitted as a raw `0x1234`, type-0 chunk with one entry, a 24-byte initial entry offset, and a terminal offset immediately after the copied entry. Its Chunk ID field is currently written as zero; this is an implementation choice, not a proven universal Chunk ID allocation rule. The meaning/uniqueness requirements of nonzero Chunk ID values in normal type-0 chunks remain a separate research question. Do not invent a `max ChunkId + 1` rule without evidence.

### Verification and remaining coverage

- Synthetic regression coverage fills the source chunk close to its 64 KiB boundary, forces relocation, verifies the untouched following entry remains valid, checks retained trailing bytes, appends the duplicated frame, saves, reloads, and checks the resulting EID/type/frame count.
- GitHub Actions workflow [workflow run](https://github.com/leorelic/CrashEdit-VC/actions/runs/43152396834) passed its build and test jobs on Windows, Ubuntu 22.04, and macOS.
- User validation: the user confirmed the new expansion workflow works as intended in CrashEdit-VC on 2026-11-12.
- Further research/testing: inspect Chunk ID values across a broader set of retail NSF files; validate relocation of additional entry types and cross-resource references; test save/reopen and retail-game loading for archives that actually gained a chunk. Current user validation is not a claim that every entry type or every expanded archive has been tested in the original game.

## Crash 2 Model/Animation sample-pair regression — 2026-11-19

Private companion fixtures: `entry_8_2_El1eG.bin`, `entry_8_3_El1eV.bin`, `entry_7_3_Cr10G.bin`, and `entry_14_3_Cr10V.bin` in `CrashEdit-VC-Game-Samples`.

- `El1eG` (`50A82755`) parses as a six-item compressed Model with 38 vertices, 108 polygon words, 64 triangles, 25 texture descriptors, 15 colors, no extended-texture records, and three TPAG EIDs. `El1eV` (`50A82773`) contains seven frames of 38 vertices; every frame references `50A82755`.
- `Cr10G` (`4CD82055`) parses as a six-item compressed Model with 309 vertices, 760 polygon words, 574 triangles, 917 texture descriptors, 90 colors, 14 extended-texture records, and one TPAG EID (`4CD8206F`). `Cr10V` (`4CD82073`) contains eleven frames of 309 vertices; every frame references `4CD82055`.
- Model polygon reconstruction was compared against CrashEdit-RE `ModelEntry.ConvertIndices()` for both samples. Every triangle's vertex indices, per-corner color indices, texture index, animated flag, triangle type, and subtype matched (64/64 and 574/574).
- Animation frame headers and the signed X-Z-Y temporal bitstream ordering follow CrashEdit-RE `Frame.MakeVertices()`; sample frame counts, model links, and decoded vertex counts agree.
- The native C++ regression suite covers synthetic compressed/uncompressed vertex writes, compressed multi-vertex batches, atomic rejection, offset rebasing, collision-byte preservation, and NSF save/reload. A private sample workflow now runs the native parser/serializer against both extracted real-sample pairs, compares golden triangle/frame hashes, and verifies exact Model/Animation entry-byte preservation after save/reload. The workflow passed: [run 44416679912](https://github.com/leorelic/CrashEdit-VC-Game-Samples/actions/runs/44416679912). Full-archive and original-game validation remain pending.