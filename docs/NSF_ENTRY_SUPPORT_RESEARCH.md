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

## Type 4 — Sort List (SLST)

Original source:
- `Crash/Formats/Crash Formats/SLST/SLSTEntry.cs`
- `Crash/Formats/Crash Formats/SLST/SLSTEntryLoader.cs`
- `Crash/Formats/Crash Formats/SLST/SLSTSource.cs`
- `Crash/Formats/Crash Formats/SLST/SLSTDelta.cs`

A Sort List is a sequence of items: a start source, zero or more delta records, and an end source. A source record has a 4-byte header (signed 16-bit value count and type, currently expected to be 0), followed by 16-bit polygon IDs. A delta record has a header containing node count, type (expected 1), add-node index and swap-node index, followed by 16-bit node values split into remove, add, and swap ranges. This is not simply a flat list of polygon IDs; the delta sequence describes changes between sort-list states.

CrashEdit's SLST controller family exposes source and delta structures separately. CrashEdit-VC should preserve the original item sequence and unknown/invalid values, validate all count/index boundaries, and provide an editor for the start/end sources and each delta. Runtime ordering/world-indexing behavior should be investigated separately from binary parsing.

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
3. **SLST:** bounded source/delta parser, editable start/end sources and delta records, round-trip tests.
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


## Model Editor phase — current priority

The Model entry is now the next active editor target. See [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md) for the implementation sequence and source map.

The key architectural relationship is source-confirmed in the original CrashEdit animation renderer: each Crash 2 frame resolves its Model EID; the frame's vertex stream is decoded using that model's optional compressed-position records; the model's decoded triangles then select vertex indices, per-corner colors, texture descriptors, animated texture state, and TPAG pages. Consequently, model geometry and material edits should update the existing Animation Viewer live, while pose positions remain owned by the Animation frame.

The currently completed vertex-marker milestone is recorded in [RESEARCH_NOTES.md](RESEARCH_NOTES.md). It provides the inspection and selection foundation, but it does not yet implement vertex-stream re-encoding or topology editing.


## Source-baseline correction

All future format and UI comparisons in this roadmap use CrashEdit-RE's t/net branch. The previous research note that cited cbhacks/CrashEdit was an incorrect source selection and has been corrected in [RESEARCH_NOTES.md](RESEARCH_NOTES.md). The Model Editor roadmap now cites the exact CrashEdit-RE source files and branch used for its findings.
