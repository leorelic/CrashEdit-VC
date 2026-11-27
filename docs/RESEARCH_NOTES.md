# CrashEdit-VC research and implementation notes

> Living engineering log for discoveries made while building the native rewrite. This document records both inherited CrashEdit behavior and findings that go beyond the original implementation. It is intentionally more detailed than the user-facing README; stable, verified highlights can be promoted to the README as the project matures.
>
> **Evidence labels**
> - **Source-confirmed:** directly supported by the original CrashEdit source.
> - **Sample-confirmed:** observed in one or more real Crash 2 files.
> - **Implemented/tested:** present in the native rewrite with regression coverage or successful application testing.
> - **Inferred / pending validation:** a reasoned interpretation that still needs more samples, source comparison, or in-game testing.

## Project direction

CrashEdit-VC is a native C++ rewrite, not a continuation of the C# application's runtime. The current focus is Crash Bandicoot 2 NSF/NSD data. The aim is not merely to reproduce every old control: the long-term GOOL goal is readable, source-like decompilation and eventually recompilation. Structured editing remains valuable when it represents data more safely and clearly than editing decompiled text.

The project deliberately separates:
- **Compatibility work:** reproduce established file formats and behaviors from CrashEdit and real files.
- **Improvements:** fix misleading output, improve usability, add decoding the original editor lacked, and build stronger validation.
- **Research:** record uncertain behavior explicitly rather than treating a plausible interpretation as fact.

## Native application and renderer milestones

The following are project milestones from the iterative native rewrite. They describe implemented work, not a claim that every subsystem is feature-complete.

- Established a C++20 / CMake application using Dear ImGui docking, GLFW, and OpenGL, with Windows, Linux, and macOS CI targets.
- Built the editor workspace around a right-side resource hierarchy, a persistent central 3D zone editor, and bottom-docked tools. The design avoids the original editor's more fragmented workflow.
- Added NSF file opening and drag-and-drop, resource hierarchy navigation, entry search, GOOL inspection, and a console/diagnostics panel.
- Added NSD inspection and editing for spawn points, EIDs, coordinates, camera values, and the GOOL map. The Get Spawn workflow uses the selected object's data (including the `obj_willy` / checkpoint object in the examined sample) to derive a spawn position.
- Added paired NSF/NSD saving with staged output replacement and recovery if the second replacement fails. Optional independent backup modes preserve `.NSF.bak` / `.NSD.bak` copies and dated archive pairs.
- Added a GPU scenery rendering path with textured Crash 2 scenery and transparency, alongside a software fallback. Texture-page matching alone was not sufficient: the renderer work included tracing scenery texture entities and connecting the decoded texture data to actual polygon rendering.
- Added a depth-buffer option and corrected scene overlay behavior so camera guides and objects are occluded by scenery when depth testing is enabled. The software fallback does not provide the same depth-buffer behavior.
- Corrected skybox scaling to match the original CrashEdit viewer in the examined zone, and added neighboring-zone rendering after selecting the relevant zone.
- Added viewport navigation/settings controls, including camera speed, renderer mode, depth-buffer and diagnostic options.
- UI preferences include a dedicated `cevc.ini` configuration file rather than writing `imgui.ini` beside the opened NSF. The original ImGui font remains the default; the modern font and UI scaling are optional experimental settings.

These milestones should continue to be backed by representative test files and regression checks. Renderer appearance should be compared with the original viewer, but viewer similarity alone does not prove game-runtime behavior.

## NSD format research and implementation

**Source-confirmed:** the original CrashEdit source recognizes ProtoNSD, OldNSD, standard NSD, and Crash 3 NSD layouts. The native serializer preserves original prelude/compression metadata and opaque regions while rebuilding variable-length index and spawn sections.

The native implementation supports:
- ProtoNSD's 256-entry hash-key map and variable-length index.
- OldNSD's legacy magic/ID/start-zone/camera fields, 64-entry GOOL map, and trailing extra data.
- Standard Crash 2 NSD's spawn count, level ID, entity count, 64-entry GOOL map, fixed 0xCC-byte extra-data block, 24-byte spawn records, and trailing image data.
- Crash 3 NSD's 128-entry GOOL map and later spawn/extra-data offsets.

Compatibility checks mirror observed CrashEdit behavior: warn for zero spawn records, normalize invalid OldNSD magic to 1, and normalize nonzero reserved blank values to 0 while retaining load warnings. Negative count values are rejected before serialization.

**Implemented/tested:** spawn-point append, delete (while retaining at least one row), reorder, copy/paste, EID and coordinate editing, and camera-aware Get Spawn. NSF and NSD are saved as a pair, with staged replacement and recovery behavior.

**Sample observation:** in the examined Crash 2 level `00_eZ` (level ID `0E`), the spawn record and `obj_willy` checkpoint coordinates were compared. The object's position was X=902, Y=548, Z=760 in decimal. The Get Spawn result was `10FDF00, 120400, 2C17500`. This is a useful regression/example case, not a universal coordinate rule.

**Still pending:** broad real-level round-trip coverage and actual in-game loading validation. A successful parse/serialize cycle does not by itself prove game compatibility.

## GOOL research boundary

GOOC is excluded from this project's GOOL research. The scoped restriction is maintained in [GOOL_ROADMAP.md](GOOL_ROADMAP.md); the complete project-wide policy is in [MILESTONES.md](MILESTONES.md).

## GOOL container and data research

**Source-confirmed:** GOOL resources are NSF entry type 11. The original loader reads a 24-byte header, instruction words, and 32-bit data-pool words. Format 1 entries can also contain a 16-bit state map, 16-byte state descriptors, and frame-group data. Header fields include ID, class, format, heap base, event count, and entry count.

The native parser validates and decodes the header, instruction stream, data pool, state map, and state descriptors. It also parses Crash 2 frame-group records:
- Type 1: 12-byte vertex-group header.
- Type 2: 8-byte header plus 16 bytes per sprite frame.
- Type 3: 8-byte header plus 20 bytes per font frame.
- Type 4: NUL-terminated text strings aligned to four bytes.
- Type 5: 12-byte header plus 24 bytes per frame-part.

Unknown, truncated, or out-of-range frame-group records produce a specific diagnostic while retaining the raw entry data for inspection. The original loader also has a Crash 3-to-Crash 2 compatibility path for type 1 groups that consumes 20 bytes under a user setting/heuristic; do not assume that variant is the standard Crash 2 layout.

### Real sample: `entry_7_0_DispC.bin`

A user-supplied 20,492-byte GOOL entry was inspected. Its six items were:
- Header: offset 0x2C, size 24.
- Instructions: offset 0x44, size 13,100 (3,275 words).
- Data pool: offset 0x3370, size 524 (131 words).
- State map: offset 0x357C, size 216 (108 signed 16-bit values).
- State descriptors: offset 0x3654, size 592 (37 descriptors of 16 bytes).
- Frame groups: offset 0x38A4, size 5,992.

The header decoded to ID 4, class 0x200, format 1, heap base 81, event count 80, and entry count 2. All 37 descriptor GOOLIndex values were within the 131-word data pool.

The frame-group stream consumed exactly 5,992 bytes: two type-2 sprite groups (14 and 1 frames), seven type-1 vertex groups (one frame each), two type-3 font groups (65 frames each), and one type-4 text group (151 NUL-terminated strings, padded to a four-byte boundary). All 12 records fit exactly.

### Real sample: `entry_13_1_WillC.bin`

A second user-supplied entry (28,184 bytes) used the same six-item format-1 layout:
- Header: offset 0x2C, size 24.
- Instructions: offset 0x44, size 22,124 (5,531 words).
- Data pool: offset 0x56B0, size 1,664 (416 words).
- State map: offset 0x5D30, size 172 (86 signed 16-bit values).
- State descriptors: offset 0x5DDC, size 2,128 (133 descriptors of 16 bytes).
- Frame groups: offset 0x662C, size 2,028.

Header values were ID 0, class 0x100, format 1, heap base 108, event count 80, and entry count 86. The 133 descriptor GOOLIndex values observed ranged from 0 to 11; this is recorded as an observation, not an interpretation that they are data-pool indices.

The frame-group stream consisted of exactly 169 type-1 Crash 2 vertex groups. Each 12-byte record consumed the full 2,028-byte item, confirming the standard type-1 record size on a second real entry and exercising interpolated and non-interpolated groups.

## GOOL instruction decoding findings

**Source-confirmed:** the original CrashEdit instruction registry defines Crash 2 opcode names and operand formats. Each instruction is a 32-bit word: the upper 8 bits are the opcode and the lower 24 bits are operand data. Format strings are consumed from the least-significant operand bit upward. The original syntax distinguishes source references, writable destination references, process/object fields, signed immediates, and plain immediate values.

The native implementation has:
- A Crash 2-version-scoped opcode table; it does not apply Crash 2 assignments to other game versions.
- Operand-field extraction retaining field names, widths, raw values, and reference classes.
- Data-pool resolution when the required local/parent pool is available.
- Explicit unresolved/external/special reference labels rather than guessed addresses.
- Correct handling of writable destination references separately from ordinary source references.
- Regression coverage for special source-reference encodings, signed stack/argument indices, null and stack-pointer markers, self/parent links, and object fields.

Important audited details:
- Crash 2 opcode `0x11` (`SETF`) uses the writable destination + source format, not two ordinary source references. The same destination form is specified for opcodes 18, 20, 23, 39, 43, 44, and unidentified opcode 46.
- The original source-reference resolver has multiple special encodings under bit 0x800, including signed immediate forms, exact `0xBE0` and `0xBF0` markers, linked-object fields, and the `[sp]` marker.
- Opcode 73 (`0x49`) is the native MIPS marker. It is not a normal GOOL operand layout.
- The original registry explicitly leaves opcodes 46, 72, and 74–77 unidentified. Their known operand layouts can still be retained without inventing mnemonic meanings.

The existing detailed opcode/bitfield inventory and test notes are in [gool-crash2-instruction-audit.md](gool-crash2-instruction-audit.md).

## Native MIPS mode: source comparison and applied correction

The original CrashEdit implementation switches from GOOL decoding into native MIPS decoding after opcode 73 (`0x49`). Its GOOL entry parser recognizes two exact 32-bit words as MIPS-mode return patterns:
- `0x03E00008` (`jr $ra`)
- `0x03E0A809` (a special raw-word sentinel in this parser)

The parser includes the instruction after the return pattern as the delay-slot instruction, then resumes GOOL decoding. This is an exact raw-word convention in the original parser; it must not be replaced by a guess based only on conventional MIPS instruction semantics.

**Important distinction:** `0x03E0A809` conventionally disassembles as `jalr $s5,$ra`, not `jalr $ra,$ra`. The canonical encoding for `jalr $ra,$ra` is `0x03E0F809`. Therefore, the disassembler should display the instruction according to its actual fields, while the GOOL stream parser must independently honor the original CrashEdit sentinel convention.

**Correction applied:** the native parser now uses the original raw-word sentinel `0x03E0A809`, with regression coverage that separately verifies:
- `0x03E0A809` displays as `jalr $s5,$ra`;
- `0x03E0F809` displays as `jalr $ra,$ra`;
- the GOOL/MIPS parser exits MIPS mode on the original raw sentinel `0x03E0A809` (and `jr $ra`) after consuming the delay slot.

The native MIPS decoder now covers the supported R3000A subset plus the modeled COP0/COP2/GTE forms described in [GOOL_ROADMAP.md](GOOL_ROADMAP.md). Register operand ordering for `jalr` and source-register selection for `mthi`/`mtlo` were corrected during source/encoding review. Keep raw words visible for verification.

## COP2 / PlayStation GTE finding — research only, not implemented

A real instruction word observed in the GOOL native-code stream was `0x49BE0BE0`, which the original CrashEdit decoder displays as `???`.

This can be identified without an entry in CrashEdit's source by decoding the MIPS fields:
- Primary opcode is `0x12` (decimal 18), the COP2 instruction family.
- On PlayStation 1, COP2 is the Geometry Transformation Engine (GTE) interface.
- The low six-bit command function is `0x20`, which identifies the GTE `NCT` (Normal Color Triple) command.

This is an encoding-level identification, not a feature copied from CrashEdit. It shows that the original decoder's `???` can conceal a valid PS1-specific instruction.

**Status:** research finding only. COP2/GTE support has not been implemented. Before implementation, audit the full COP2 encoding space: GTE command words, control/data register transfers, and load/store forms. Decode command flags and register indices rather than merely printing a mnemonic. Preserve raw words for unknown/reserved forms. This is a disassembler task, not a GTE emulator.

## MIPS decoder comparison with original CrashEdit

The original `MIPSInstruction.cs` handles a useful subset of common R3000A instructions but does not provide COP2/GTE decoding. This explains why the valid GTE word above is shown as unknown by the original application.

The source comparison also exposed formatting limitations in the original decoder:
- Its `mthi`/`mtlo` operand formatting appears to use the `rd` field instead of the source register `rs`; the native decoder uses the source register.
- Branch operands are displayed as immediate offsets rather than resolved destinations.
- Jump operands show the raw encoded target rather than a full address.
- Unknown instructions do not receive a useful architecture-specific fallback decode.

The native decoder should retain accurate architectural operand formatting even when the GOOL stream parser applies CrashEdit-specific raw-word conventions.

## SLST / scenery sorting research

Current working understanding from original viewer source: SLST is a camera-dependent polygon sort/display list used by the original zone/scenery viewer to determine which scenery polygons to render for a camera position. It is more than simply clipping individual vertices outside the screen.

The exact Crash 2 record semantics, interpolation rules, and runtime constraints still need confirmation. Do not implement a generator based only on viewer behavior. The long-term goal is to inspect, decode, validate, and eventually generate SLST data for custom scenery, with actual in-game validation.

**Roadmap placement (updated 2026-11-16):** SLST research is grouped with the future **Scenery / Geometry Editing and level-object authoring** category. It is not a separate near-term editor task and is not folded into the current Animation/Model vertex-editing roadmap. That future category covers the eventual Scenery/Geometry workflow and whatever level-object creation, import, conversion, or related authoring tools prove appropriate.

The combined research should establish:
- How Type 4 SLST source and delta records represent polygon ordering and transitions between sort-list states.
- How SLST data is associated with scenery geometry, zones, camera positions, and level-object rendering.
- The exact runtime meaning of record types, node indices, interpolation/transition behavior, and any ordering or count constraints.
- Whether custom or converted geometry requires new SLST data, how it should be generated, and how the game consumes it.
- Safe geometry and SLST serialization, including unknown-field preservation, bounds validation, and real-game regression.

Treat the relationship between SLST and scenery authoring as a research question to prove, not an assumption that every geometry object uses the same sort-list path. Use multiple Crash 2 samples and in-game validation before defining a generator or final UI workflow.

## Documentation and evidence policy

Focused subsystem research documents are companions to this living log, not replacements for it. In particular, read [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md) alongside the animation/Model evidence recorded below and in this log. Preserve the existing `airumu/CrashEdit` `t/net9` source baseline and the real-sample parity results; do not repeat prior source investigations unless a reproducible mismatch requires it. When a subsystem document adds detail, cross-link it here or from the relevant roadmap rather than maintaining conflicting duplicate specifications.

For every significant discovery, record:
1. The exact file/sample, instruction word, record, or behavior examined.
2. Whether the claim is source-confirmed, sample-confirmed, implemented/tested, or inferred.
3. The relevant original source file or external technical reference.
4. The native implementation location and regression test, when applicable.
5. Remaining uncertainties and the next validation step.

Do not silently convert an inference into a specification. When later evidence changes an interpretation, keep a short correction note so the research history remains understandable. Promote concise, stable conclusions into the top-level README only after they are well-supported; keep detailed binary layouts and audit trails in focused documents under `docs/`.


## Major milestone — Animation vertex inspection and selection

**Implemented/tested — user-confirmed on 2026-11-09:** the 3D Animation Editor now has an interactive vertex inspection workflow. The user confirmed the latest build works as intended.

- Vertex point display can be toggled from the viewport settings and with the V keyboard shortcut.
- Vertex markers are object/position-aware in hardware rendering and respect depth testing rather than being drawn indiscriminately over the scene.
- Individual vertex markers can be hovered and selected; hover is red and selection is green.
- The selected vertex is exposed in the Animation Frame Editor with its decoded coordinates.
- The overlay works independently of Collision Viewer; the marker pass explicitly establishes its own blend state so animation rendering state does not turn the markers black.
- Markers are also available in wireframe mode to make triangle/vertex relationships easier to inspect.

This completes the current Animation Editor vertex-inspection milestone. It does **not** mean packed temporal vertex editing, topology editing, or vertex-stream re-encoding is complete. Those remain dependent on understanding the Model entry and the relationship between its topology/compression metadata and Animation frame data.

## Model Editor research — source-confirmed findings

The CrashEdit-RE source in airumu/CrashEdit, branch t/net9, separates animated positions from model topology. Crash 2 Animation frames reference a Model EID; the Model entry supplies the polygon stream, color records, texture descriptors, texture-page references, and optional compressed-position metadata. The Animation frame supplies the pose-specific vertex data. This means the Model Editor is the next foundational phase and a likely source of the missing context for safe animation geometry editing.

The original Model loader expects five items, or six when compressed-position metadata is present. The detailed phased plan, field notes, and validation requirements are in [MODEL_EDITOR_ROADMAP.md](MODEL_EDITOR_ROADMAP.md).

**Important implementation constraint:** do not treat the polygon command stream as a conventional index buffer. Its words encode both triangle commands and color-pair commands; original/duplicate vertex keys and multiple topology modes must be interpreted in stream order. Preserve raw words and unknown bitfields while building a validated decoded representation.


## Research-source correction — CrashEdit-RE baseline

**Correction (2026-11-09):** the previous Model research pass incorrectly cited cbhacks/CrashEdit and its deprecate branch. Those are not the project's comparison baseline. Disregard claims in that pass as authoritative until checked against CrashEdit-RE. The correct reference used for the current re-audit is [airumu/CrashEdit, branch t/net9](https://github.com/airumu/CrashEdit/tree/t/net9), the CrashEdit: Re fork. Future source comparisons for Model, Animation, renderer behavior, and editor features must use CrashEdit-RE and the appropriate t/net branch, not cbhacks legacy source.

The t/net9 source confirms the following Model implementation details:
- ModelEntryLoader obtains the serialized polygon-command count from Info offset 0x2C, color count from 0x3C, texture descriptor count from 0x34, extended-texture count from 0x48, and vertex/special-vertex counts from 0x38 / 0x4C. It expects five items, or six when compressed-position records exist.
- There is an unresolved source-level inconsistency worth auditing: ModelEntry.PolyCount reads Info offset 0x44, while ModelEntryLoader uses 0x2C as the count of item-1 polygon command words. Do not assume 0x44 is the item-1 command count without tracing its consumers.
- The polygon stream is decoded in two passes by ModelEntry.ConvertIndices(). ModelTriangle and ModelColor are distinct command structures; 0xFFFFFFFF is the footer. Original/duplicate position keys are mapped in the first pass; triangle types 0, 1 and 2 are reconstructed in the second pass with stateful per-corner color selection.
- ModelPosition is a 4-byte packed record containing X/Z/Y base values and X/Z/Y bit widths. Frame.MakeVertices(model) consumes compressed temporal bits in X-Z-Y order, sign-extends the components, accumulates/wraps coordinates to 8 bits, and uses the Model's special-vertex count to skip the directly stored prefix.
- The CrashEdit-RE ModelBox has General, Polygons, Colors, Textures, Extended Textures and (for compressed models) Positions pages. The model's polygon page exposes both the raw command stream and reconstructed triangles. The ModelBox removes its separate Vertices page for Model entries; that page is for Scenery.

These are **source-confirmed for t/net9**, not yet sample-confirmed against multiple Crash 2 NSF files and not proof that the native implementation already matches every edge case.

## Major milestone — Face Texture Selector and unrestricted triangle UV placement

**Implemented/tested — user-confirmed on 2026-11-12 (TNT Box):** the Animation Editor's in-viewport Face Texture Selector can now select previously inaccessible regions of a model's texture atlas and reposition the selected face's texture mapping there. This is the first user-confirmed completion of the direct atlas-based face UV workflow. The same test on Cr10T and additional models/color modes remains pending; do not generalize the TNT Box result to every model yet.

### What the implementation does

- The atlas is displayed inside the selector's available viewport, and its image and click target use the same fitted rectangle. This prevents visible atlas regions from falling outside the selector's hit-test area.
- A click is converted into a target atlas position. The existing face UV mapping is translated to that position rather than replacing its four UV corners with an axis-aligned rectangle. Relative UV shape is therefore preserved.
- The selected triangle's visible mapping is defined by UV1, UV2 and UV3. The auxiliary fourth UV corner (UV4) is not allowed to restrict otherwise valid triangle placement.
- UV4 is translated along with the mapping when its local byte coordinates remain representable. If it cannot be represented at the translated position, its previous raw values are retained rather than clamping the triangle's selection back toward an unrelated region.
- The existing texture descriptor assignment and fixed-size NSF write path remain in use; this milestone does not introduce a separate or alternate serializer.

### Root-cause correction trail

The first attempted repair only changed atlas sizing. It did not solve the underlying coordinate restriction and produced a misleading result where clicks in the unavailable area snapped to the nearest legal square. The actual restriction was in the UV translation constraints: UV4 was included in the bounds used to decide whether UV1-UV3 could move. Since UV4 is not one of the selected triangle's three visible corners, it must not constrain that triangle's atlas placement.

The final correction narrows the horizontal segment-fit and vertical byte-range constraints to UV1-UV3, while preserving UV4 when safely representable. The viewport-fit correction is retained as a separate hit-testing safeguard, not treated as the core UV fix.

### Implementation and verification trail

- `2d4b731` — preserve the face's existing UV shape when moving its atlas selection.
- `7043352` — fit the atlas to the selector viewport so the visible image and clickable area agree.
- `1e6a172` — remove UV4 from the triangle placement constraints and retain it safely when possible.
- GitHub Actions workflow [#1017](https://github.com/leorelic/CrashEdit-VC/actions/runs/43113110594) passed on Windows, Ubuntu 22.04 and macOS.
- **User validation:** TNT Box face atlas selection confirmed working on 2026-11-12.
- **Still pending:** repeat the full-atlas click/selection test on Cr10T, especially the regions that were previously inaccessible, and verify representative 4-bit, 8-bit and 16-bit descriptors. Also verify save/reopen behavior with a real edited NSF before claiming archive-wide validation.

This milestone establishes the direct UV-selection workflow for the tested TNT Box case. It does not complete variable-size Model serialization, topology editing, universal TPAG/CLUT resolution, or broad real-sample round-trip validation.


## Major milestone — first in-game NSF texture mod validated

**User-confirmed end-to-end test — 2026-11-12, TNT Box in Crash Bandicoot 2:** after editing the TNT Box face texture in CrashEdit-VC, the user saved the modified NSF successfully, confirmed that the saved archive opened and displayed correctly in the original CrashEdit-RE, and then tested the archive in the original game. The supplied in-game screenshot shows the changed TNT Box textures rendering correctly. This confirms that the tested Face Texture Selector edit survives the actual NSF save path and is accepted by the retail game—not merely by the editor preview.

The verified chain is: **CrashEdit-VC face texture edit → NSF save → CrashEdit-RE reopen → original Crash 2 runtime.** This is the first confirmed end-to-end game-mod validation milestone for the Model face-texture workflow.

**Scope:** validation is specific to the tested TNT Box edit. It does not establish universal compatibility across all models, texture color modes, or descriptor combinations. Cr10T and additional model/color-mode tests remain pending, as do broader archive-preservation tests and variable-size Model serialization/topology editing. The test validates the existing fixed-size texture-descriptor write path and NSF save path for this real edit.


---

## NSF chunk expansion and animation-frame growth — 2026-11-12

The Animation Timeline's existing circle-dot presentation and Append/Insert/Delete behavior remain unchanged. The new work addresses the archive-capacity boundary underneath Append and Insert.

### Container facts confirmed in the native parser

- NSF chunks decode to fixed 64 KiB buffers.
- Type-0 chunks contain a count and an offset table at byte offset `0x10`; the table has one terminal offset in addition to the per-entry offsets.
- Each entry contains its own EID, type, item count, and item-offset table. Item offsets are relative to the entry block.
- Texture pages use chunk type 1 and are not type-0 entries.
- A growing Animation entry can force the containing type-0 chunk's terminal used-data offset beyond 64 KiB even though the frame-item data itself is valid.

### Relocation implementation

Commit [`c200773`](https://github.com/leorelic/CrashEdit-VC/commit/c200773d4354096dcd84805b8868d1b0f8f8bb15) adds:
- A preflight that calculates whether duplicating the selected frame can fit after accounting for item-table growth, payload bytes, entry padding, the entry's current position, and the chunk used-data boundary.
- A confirmation prompt before moving an entry.
- Candidate-copy relocation of the complete raw entry into a new normal type-0 chunk.
- Rebuilding of the source chunk's entry table and offsets while retaining remaining raw entry blocks and padding.
- Validation of both rebuilt chunks before replacing the live archive.
- Selection/resource-location refresh and automatic retry of the requested Append or Insert.
- A synthetic regression fixture that forces the overflow, checks the other entry and retained tail bytes, and verifies append plus save/reload.

The new type-0 chunk is currently written with Chunk ID `0`. This is an explicit implementation value, not a general rule for allocating IDs; broader retail-file analysis is still needed to establish whether that field has additional semantics. The relocation API accepts normal type-0 entries, but the current user-facing overflow prompt is connected to Animation Append/Insert.

**Verification:** user confirmed the workflow works as intended. GitHub Actions workflow [workflow run](https://github.com/leorelic/CrashEdit-VC/actions/runs/43152396834) completed successful build and test jobs for Windows, Ubuntu 22.04, and macOS. Additional entry types and retail-game loading of an expanded archive remain future validation.

## TNT Box animation / GOOL relationship research — paused pending GOOL milestone

The original, unmodified TNT Box resources are sufficient to research the resource relationships; no modified animation sample is required.

Sample resources recorded in `CrashEdit-VC-Game-Samples`:
- `entry_13_2_BoxsC.bin` — BoxSC GOOL executable.
- `entry_3_0_BT10G.bin` — TNT Box Model.
- `entry_3_1_BT10V.bin` — original BT10V Animation (one frame, intentionally unmodified).

### Confirmed observations

- BoxSC contains 36 state descriptors. All 36 have Code hook locations; there are 33 unique Code-hook instruction locations.
- 14 Event-hook slots are absent (`0x3FFF`); the remaining descriptors refer to 16 unique Event-hook locations.
- Five Transition-hook slots are absent; the remaining descriptors refer to five unique Transition-hook locations.
- All 36 descriptors refer to GOOL index 0.
- The instruction stream contains 2,764 words. The observed counts include 32 `GO`, eight `GNEZ`, and one `GEQZ`; these instructions collectively reference 17 different descriptor indices.
- The stream contains 63 `ANIM` and 40 `ANIF` instructions. Their operands and runtime invocation semantics are not sufficiently established to infer a complete animation state machine.
- The BoxSC frame-group data references BT10V, and BT10V references BT10G. The uploaded BT10V is the original unmodified animation and is intentionally retained as the relationship sample.

These counts are sample observations, not a complete interpretation of BoxSC's state machine. In particular, do not infer transition direction or runtime meaning from descriptor order, hook indices, or the `GO`/`GNEZ`/`GEQZ` counts alone.

### Decision and next step

The temporary GOOL State Machine viewer experiment is paused. Keep the existing State Descriptions and GOOL Inspector unchanged; do not build a throwaway viewer or modify the decompiler/Script Editor solely to investigate the TNT Box animation. The resource-link relationship is documented, but explaining why a newly created animation is or is not played by the retail game requires tracing the relevant `ANIM`/`ANIF` behavior and GOOL execution. Resume this investigation when the planned GOOL decompilation / readable GOOL Script work is ready to handle those semantics. Continue the independent Animation/Model editor roadmap in the meantime.

This pause does not mean the original BT10V is inadequate or that it should be modified. It is the correct unmodified reference sample for the current resource-relationship investigation.

## Animation/Model editor sprint — vertex selection, Bone Rig, transforms and sample parity (2026-11-19)

### Vertex selection and camera input

- Normal vertex mode now supports click-to-replace, Shift-add, Ctrl-remove, and empty-space drag-box selection. Bone List mode retains its add/remove behavior.
- Hover picking uses a tighter screen-space radius and depth tie-break; triangle occlusion is checked only for candidates close to the cursor. Box selection rejects vertices outside the rectangle before the occlusion test.
- Selected vertices retain a distinct ring while the hovered vertex gets its own hover ring/color.
- Camera input no longer competes with left-drag box selection: middle-drag orbits, right-drag pans, the wheel zooms, and Shift-left-drag can orbit while Bone List is open. Camera input is gated while selection/transform/collision interactions own the pointer.
- Large/dense-model interaction still needs user testing; this is not a claim that X-ray selection is implemented.

### Bone Rig sidecar and per-frame rest poses

- BoneRigBone now stores a rest-position array for each Animation EID/frame, aligned with that bone's vertex membership, plus the original frame X/Y/Z offsets.
- Bone rotations are recalculated from that per-frame rest pose and the stored rig rotation rather than repeatedly rotating already-quantized positions. Preserving rest offsets is important when Crash 2 rotation requires rebasing the frame coordinate range.
- Move/Scale edits to assigned vertices, membership changes, and pivot edits invalidate the affected frame's rest cache so later rotations cannot use stale source positions.
- Sidecar version 2 persists per-frame rotations, rest positions, and rest offsets; version 1 remains readable and establishes the current visible pose as a new rest pose on the next rotation. Load validation rejects invalid frame keys, non-finite pivots/angles, duplicate vertex assignments, invalid parents, and hierarchy cycles. Save writes a temporary file and replaces the sidecar only after a successful flush.
- The rig remains editor-only. Each frame has an independent rest pose and rotation state; the edited frame is flattened through the ordinary Animation vertex encoder. Parent links are metadata only and do not yet propagate transforms to child bones.

### Transform and archive regression coverage

- Move/Scale now reject out-of-range Crash 2 results rather than clamping coordinates. The last valid pose is retained when a drag would exceed the supported range or the frame encoder cannot represent the edit.
- The selected bone pivot can be edited numerically, recentered from assigned vertices, and optionally used by Move/Scale gizmos. The ordinary selection-center pivot remains the default when the option is off.
- Synthetic tests now cover duplicate-index rejection, atomic invalid batch rejection, frame-offset rebasing, rebase failure without partial mutation, compressed multi-vertex edits, preservation of collision bytes, and save/reload.

### Real Crash 2 sample comparison

Reference source: [CrashEdit-RE Frame.cs](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Animation/Frame.cs), [ModelEntryLoader.cs](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Model/ModelEntryLoader.cs), and [ModelEntry.cs](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Model/ModelEntry.cs). Private fixtures are in `CrashEdit-VC-Game-Samples`.

| Sample | Model facts | Animation facts |
|---|---|---|
| `El1eG` / `El1eV` | 38 vertices; 64 triangles; 108 polygon words; 25 texture descriptors; 15 colors; 0 extended textures; 3 TPAG EIDs | 7 frames; 38 vertices per frame; every frame links to `El1eG` |
| `Cr10G` / `Cr10V` | 309 vertices; 574 triangles; 760 polygon words; 917 texture descriptors; 90 colors; 14 extended textures; 1 TPAG EID (`4CD8206F`) | 11 frames; 309 vertices per frame; every frame links to `Cr10G` |

The Model triangle records were compared against the `ModelEntry.ConvertIndices()` logic in the reference source. All 64 El1eG and all 574 Cr10G triangle records match in vertex indices, per-corner color indices, texture indices, animated flags, triangle type and subtype. Animation frame structure and signed X-Z-Y temporal decoding match `Frame.MakeVertices()`. The private sample workflow runs the native C++ parser/serializer against both extracted pairs, checks golden hashes for every triangle and decoded frame vertex stream, and verifies the raw Model and Animation entry bytes remain unchanged after save/reload. This is entry-level compatibility evidence; full-archive and original-game validation remain open. See [sample workflow run 44416679912](https://github.com/leorelic/CrashEdit-VC-Game-Samples/actions/runs/44416679912).
### Animation save/reload investigation — superseded correction (2026-11-26)

The 2026-11-25 note above was incorrect: restoring raw special XYZ bytes *after* temporal-word serialization overwrites compressed delta bits. CrashEdit-RE source confirms that temporal words are serialized over the overlapping prefix without restoring the old raw bytes. The native fix now keeps serialized temporal bytes authoritative, verifies the exact re-decoded pose, and refreshes the special-vertex cache from bytes actually written. See [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md) and milestone [ff5359a](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac).

Latest user report: the animation preview looks correct again. Native tests and private real-sample checks passed for the latest patch sequence; Windows and Ubuntu CI passed, with macOS queued at last check. User is continuing game testing. Do not treat this as universal animation or in-game save validation.


## Historical animation recovery checkpoint — 2026-11-26 (superseded by conversion milestone)

Checkpoint [`ff5359af45779fcd29b3827a563215b433cef1ac`](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac): user-confirmed animation preview looks correct again and saving works. This restores a stable baseline; it does not solve the original transform defect.

**Historical status at this checkpoint:** certain vertices hit a location limit on the X/Y/Z gizmo, and an incorrect chunk-capacity prompt appeared. This was the pre-conversion state. The later Complex-to-Normal conversion and edited-geometry test resolved the user's practical larger-ear editing case for the tested resource; direct Complex editing and broad compatibility remain separate open work.

Next: compare the gizmo's allowed coordinate range, frame boundary/offset metadata, compressed or uncompressed encoder constraints, and save validation for the exact failing vertex/axis. Record the first rejected coordinate and rejection reason; do not speculate or silently clamp/drop/wrap the edit. Verify movement beyond the old limit, save/reopen, then original-game behavior independently.

The temporal-prefix correction above remains valid and should not be reverted. Native regression and private sample checks are code-level evidence only; user has not yet confirmed the coordinate-limit issue fixed.


### Animation coordinate / payload-capacity source trace — 2026-11-26

New source-confirmed detail is recorded in [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md), based on CrashEdit-RE `Frame.cs` / `ModelPosition.cs` and native `src/crash/formats/nsf/nsf_archive.cpp` / `src/ui/editor_ui.cpp`:

- Crash 2 frame `HeaderSize` defines a special-vertex prefix after the 24-byte fixed header and 0x28-byte collision records; temporal words begin at the fixed/collision boundary and overlap that prefix. The frame item’s size/alignment and NSF chunk capacity are different boundaries.
- Local coordinates are unsigned byte values (0–255); compressed ModelPosition delta widths can be narrower than the full byte range. Width 7 is signed-byte delta width and resets that axis accumulator; decoded accumulators wrap modulo 256.
- A valid edited frame can be rebased into byte local coordinates only when each axis span is at most 255 and adjusted signed-16-bit offsets remain valid.
- When current compressed widths reject an edit, native save tries widening the shared Model’s position records to width 7 and rebuilding every Animation entry that references it. If an enlarged payload fails to fit a chunk, the UI raises the chunk-relocation prompt. This is an archive-capacity failure triggered by encoding fallback—not proof that the vertex coordinate itself needs a new chunk.

The previous conclusion in this paragraph was premature. The preview baseline has an asymmetric decoder/renderer mapping; the shifted decoder experiment corrupted the renderer and has been reverted. Coordinate editing remains under investigation. Preserve `ff5359a` preview behavior and do not revive the post-serialization raw-special restore.


### Real-sample animation edit coverage gap — 2026-11-26

The synthetic NSF tests already exercise narrow compressed delta rejection, width-7 expansion, coordinate 255, save-time rebasing, special-prefix serialization, and preservation of an unencodable working pose. The real `Cr10G/Cr10V` and `El1eG/El1eV` compatibility workflow currently validates decoded hashes/topology, not a compressed real-sample edit that forces width expansion and rebuilds all Animations sharing the Model. The uncompressed real edit fixture is `BT10V`.

This distinction matters: synthetic coverage proves the code path in a constructed fixture, not capacity behavior or shared-Model effects in retail data. See the regression audit in [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md). Next validation should record the actual blocked ModelPosition width/base and exact failing delta on the private sample, then test full-pose equality after expansion and every affected Animation payload rebuild.


### Correction: native unified-frame vertex mapping — 2026-11-26

The earlier conclusion that decoder destinations must be shifted was incorrect for the known-good preview path. The baseline decoder writes compressed ModelPosition i to `frame.vertices[i]`, while the renderer retains its specialVertexCount triangle offset. Do not change decoder indexing based solely on the unified frame container; encoder/edit mapping is a separate unresolved conversion.

The `0x7f` one-bit X-width fixture correction remains valid. However, the attempted shifted unified-frame save/reload assertion was withdrawn after it depended on a decoder mapping that broke the real Cr10V preview. The current synthetic special-prefix test is not evidence that arbitrary compressed/special vertex edits save correctly; see the explicit unresolved mapping note below.

Relevant commits: [fixture fix](https://github.com/leorelic/CrashEdit-VC/commit/ed21d4c97e606bb9acd6237fb3f1bf243a5aa872), [wrong shifted-index attempt](https://github.com/leorelic/CrashEdit-VC/commit/c9027db85b567a43ec25a3c18da697c04c8d195e), [source-aligned code correction](https://github.com/leorelic/CrashEdit-VC/commit/a4e015407111393ab2b81891b27e7988775b99ce), [test correction](https://github.com/leorelic/CrashEdit-VC/commit/d3aa54664ae168fb7f87d5bac71067535da5a23b), and [research correction](https://github.com/leorelic/CrashEdit-VC/commit/91c92c500c34eed0bc4ae371f734cc342d0c3adb).

This does **not** solve the original coordinate-limit/chunk-prompt issue. CI and real retail sample/game verification remain pending.

### Pending animation pose lost during NSF chunk relocation — 2026-11-26

Source inspection identified why accepting the compressed-animation payload capacity prompt could lose a vertex edit:

- Compressed Model position-width expansion can enlarge Animation frame payloads until an entry no longer fits its current 64 KiB type-0 chunk. The save path reports the exact chunk/entry and asks to relocate it.
- NsfArchive::moveEntryToNewChunk() rebuilds and reparses the source and destination chunks, then calls decodeAnimationModels(). Dirty vertex poses live in decoded NsfAnimationFrame::vertices with verticesEdited=true; they are not yet in raw entry bytes. Reparse therefore discarded pending pose edits before the retrying save.
- The initial relocation path snapshot/restoration used entry EID and frame index. Follow-up testing found that EIDs can be duplicated, making that identity ambiguous; the current implementation instead snapshots by original chunk/entry/frame location and remaps the selected entry to the new chunk and remaining entries to their shifted source indices. Failure to restore is atomic and reports an error rather than silently dropping the edit.
- Added a synthetic regression with two pending Animation poses: relocate one entry, verify both the moved pose and the pose remaining in the source chunk are retained, then save/reload and compare coordinates.

This addresses edit preservation across the relocation/retry workflow. It does not independently prove that every compressed vertex delta or every real Cr10V pose is representable; keep the coordinate/encoding investigation and retail-sample/game validation open until the user confirms the boundary edit applies and survives save/reopen.

### Special-prefix raw-byte restore could overwrite compressed deltas — 2026-11-26

A final source/code consistency check found that compressed Model expansion serialized the temporal words and then wrote the raw special XYZ bytes back over the same physical prefix. This is unsafe when byte-lane ordering maps part of that prefix to compressed bits at or after the temporal cursor. The format research already establishes that the temporal stream overlaps the special prefix and that serialized temporal bytes must remain authoritative.

- Removed the post-serialization raw XYZ restore. The initial raw prefix still seeds the temporal bit array; the final encoded words are now left intact and the rebuilt frame's decoded pose is checked for exact equality.
- Added a regression with one special vertex, a nonzero compressed X base, and a narrow second-vertex delta. Editing the special pose and forcing width-7 expansion previously made the restore overwrite the first compressed delta; the test now checks the edited pose both immediately and after save/reload.
- The private Cr10V sample has 309 frame vertices, 4 special vertices, and 305 compressed Model positions in each of its 11 frames. Its 4-vertex prefix occupies the first 96 temporal bits, so the existing raw-prefix restore does not overlap Cr10V's delta cursor in this particular sample. The new test covers the general non-word-aligned case; retain the Cr10V sample/game test as final validation.


### Duplicate-EID relocation and ImGui vertex input IDs — 2026-11-26

The relocation routine copies only the selected raw entry into the new type-0 chunk. The observed “wrong animation” behavior was traced to pending-pose restoration, not to the raw entry-copy loop: restoration previously matched dirty poses by EID/frame, which can alias duplicate Animation entries. It now maps by original chunk, entry, and frame indices, including the source-entry index shift caused by removing the moved entry. The regression now deliberately uses duplicate EIDs and checks both distinct dirty poses after save/reload.

The Decoded Vertices special-coordinate controls also used repeated visible X/Y/Z labels. Each input now has an explicit vertex-row and axis ID plus a hidden label. This is intended to eliminate the reported Dear ImGui conflicting-ID warning; user confirmation remains pending.

The coordinate limit itself remains a separate encoding issue: Crash 2 local frame coordinates are byte values (0–255), while compressed ModelPosition records store per-vertex delta widths and accumulators. A target coordinate such as X=81 is valid as a byte, but the delta from that record's base/previous accumulator can exceed its current width. Widening the shared Model's records rebuilds linked Animation frame payloads; only if that expanded payload exceeds the entry chunk's 64 KiB capacity should relocation be requested. A chunk move cannot fix a frame whose axis span exceeds 255 or whose rebased frame offsets exceed signed 16-bit.


### Shared compressed Model isolation — 2026-11-26

The previous save path widened a ModelPosition table shared by multiple Animations. If any linked Animation payload exceeded its 64 KiB chunk, commit returned failure and discarded its working archive copy. The UI then moved the reported entry in the original archive and retried from scratch; additional linked animations could fail and prompt further moves. This is why the capacity dialog could cascade through several animations without completing the requested edit.

The current patch probes the edited pose against existing widths without mutating the source. If expansion is actually required, it clones the compressed Model to a new unused EID and redirects only that Animation's frame references, then widens/rebuilds the clone. This keeps other Animations on the original Model and makes any capacity relocation target the edited Animation. Shared-model synthetic save/reload test and CI are pending.


### Root cause from native unified pose vs Model-local indices — 2026-11-26

Cr10V preview compatibility was not established by the prior synthetic indexing test. The decoder-shift patch based on this paragraph was reverted after it broke the renderer. The exact Cr10V Model-local, decoded-frame, renderer, selected-vertex, and encoder index relationship still needs to be measured against the retail sample and CrashEdit-RE source. Do not infer that vertices 305–308 are missing solely from the counts.

A coordinate such as X=81 is not a fixed-format boundary: local coordinates are bytes 0–255, while a narrow ModelPosition delta may reject a change from its base/previous accumulator. If the width probe rejects the pose, the native save path now clones the compressed Model to a new unused EID and redirects only the edited Animation before width-7 expansion. This prevents widening a shared Model from forcing unrelated Animations to be relocated. The real Cr10V save/reopen/game validation remains unverified.


## Shared Model expansion semantics and regression intent — 2026-11-26

A compressed ModelPosition table may be referenced by multiple Animation entries. Width expansion is a mutation of that table, so widening it in place can change the serialized payload size of every linked Animation and can make unrelated entries exceed their chunk capacity. The safe edit path is:

1. Test whether the requested pose can be encoded with the existing ModelPosition widths without mutating the archive.
2. If not, clone the compressed Model to a fresh EID and redirect only the Animation being edited.
3. Expand/rebuild the clone and save that Animation against its private Model; leave other Animations linked to the original Model.

The regression in `tests/nsf_archive_tests.cpp` must compare the original Model and untouched Animation against their baseline, and separately verify that the edited Animation references a different Model EID whose expanded position width is 7 and whose edited pose survives save/reload. It must not expect the shared original Model to be widened in place. This is a format/ownership invariant, not merely a test detail.

When diagnosing a failed edit, record the exact frame, unified vertex index, axis, original/requested coordinate, Model-local index after subtracting specialVertexCount, current encoded width/base or previous accumulator, and whether failure occurs in width probing, frame rebuilding, signed offset rebasing, or chunk capacity. A generic CI result or an isolated assertion does not establish the retail sample's actual coordinate limit. Preserve the `ff5359a` preview/save baseline while investigating.


### Animation preview regression: renderer lost the special-vertex index offset — 2026-11-27

The user reported that the Cr10V preview remained visibly corrupted after the renderer-offset removal. Reinspection corrected the previous diagnosis.

- Model triangle indices are Model-local indices. The Animation frame's displayed vertex array includes a special-vertex prefix, so renderer/picking/edge-overlay access must use `triangleIndex + specialVertexCount`.
- The decoder has been restored to the baseline destination `frame.vertices[i]`; renderer Model triangle lookup retains `+ specialVertexCount`. The previous shifted decode and direct-index renderer experiments were both reverted.
- Commit `51f98dc` removed that offset from animation surface picking, face selection, face drawing, and edge overlay. This caused the renderer to read special/incorrect vertices for Model triangles and explains the mesh appearing severely distorted.
- Restored the prefix offset in all those renderer paths in commit `4edbfb3690f3ff7bc4e52375778ad627a795cfd5`.

**Known-good preview invariant:** compressed ModelPosition `i` decodes to `frame.vertices[i]`; Model triangle index `t` resolves through the renderer's `frame.vertices[t + specialVertexCount]` lookup. This is the baseline behavior to preserve. The editable pose/encoder mapping is not yet proven to be identical and must be investigated separately.

CI and the user's visual validation are pending. Do not mark the preview regression resolved until Cr10V renders normally in the next build.


### Correction: preserve the baseline animation preview vertex mapping — 2026-11-27

The recent Cr10V preview regressions came from changing the decoder's destination indexing based on an incorrect interpretation of the unified editor pose. The prior notes claiming that compressed ModelPosition `i` must decode to `frame.vertices[i + specialVertexCount]` were not validated against the actual renderer and were wrong for the known-good preview path.

The known-good `ff5359a` implementation has a specific asymmetric contract:

- `decodeAnimationVertices()` decodes compressed Model positions into `frame.vertices[i]` (after reading the temporal stream starting at `specialVertexCount * 24` bits).
- The animation renderer resolves Model-local triangle indices using the existing `triangleIndex + specialVertexCount` offset.
- The save encoder uses the editable frame pose and the Model-local-to-frame offset when writing ModelPosition records.

Changing decoder destinations to `i + specialVertexCount` while leaving the established renderer offset in place made the renderer read `triangleIndex + 2 * specialVertexCount` from the decoded pose. For Cr10V's 4-special-vertex prefix this shifted the rendered lookup by 8 slots relative to Model-local indices and corrupted the preview. Removing the renderer offset was also wrong; it did not restore the original decode/render relationship.

**Recovery action:** restored the decoder function from the `ff5359a` baseline and restored the special-vertex offset in the renderer. The synthetic special-prefix decode/edit test was reset to its baseline expectations. The renderer must remain unchanged while the edit/save mapping is investigated separately.

**Important unresolved research question:** preview decode and edit/save currently appear to use different index conversions. Do not “unify” them based on the `frame.vertices` container alone. Trace CrashEdit-RE's `Frame.MakeVertices()`, `Frame.Save()`, ModelPosition ordering, special vertex header bytes, and the retail Cr10V Model triangle index range together. Build a sample-backed table for each relevant vertex: Model-local index, decoded frame slot, renderer slot, edit-selected slot, encoder source slot, ModelPosition record, and resulting saved/reloaded coordinate. This is the necessary route to understand why specific vertices hit different limits without destabilizing preview.

No claim is made that free vertex editing is fixed. This correction is about restoring the known-good preview path and identifying the remaining mapping mismatch.


Renderer audit follow-up: the prior renderer restoration missed the face-edge overlay loop, which still used raw Model triangle indices. That loop now also applies `specialVertexCount` before indexing the frame pose (`8ccb5cd`). This was an independent overlay/picking inconsistency; the primary surface-preview regression was the shifted decoder destination, now restored to the `ff5359a` baseline. Keep all renderer consumers on the same baseline offset until the edit/save mapping is proven.


### Special-prefix edit/save mismatch is now an explicit regression — 2026-11-27

With the baseline decoder restored, the synthetic special-prefix edit case no longer pretends to prove a successful save. It edits a special vertex and a compressed Model vertex in the working pose, then verifies that `commitAnimationEdits()` rejects the lossy round trip and leaves the original working pose intact. The previous shifted-index save/reload assertions were removed because they depended on the decoder mapping that broke Cr10V preview.

Source trace: baseline `decodeAnimationVertices()` writes compressed position `i` to `frame.vertices[i]`; renderer Model triangle lookup uses `triangleIndex + specialVertexCount`; `encodeAnimationFrameVertex()` addresses ModelPosition `i` from the editable frame slot `i + specialVertexCount`. This asymmetry is a concrete candidate for the edit/save mismatch, and it must be resolved with a per-vertex trace against CrashEdit-RE and Cr10V rather than by changing preview decoding again. A vertex coordinate above X=80 is not itself outside Crash 2's byte range; inspect that ModelPosition's axis width, encoded base, previous accumulator, and requested signed delta.


### User-confirmed Cr10V animation preview recovery — 2026-11-27

**Observed result:** after restoring the animation decoder to the exact `ff5359a` implementation and retaining the renderer's special-vertex index offset, the user tested the build and confirmed that the Cr10V animation preview looks correct again. This is the first successful visual confirmation after the preview regression. The earlier builds with shifted decoder destinations and/or removed renderer offsets were visibly corrupted and must not be used as a baseline.

**Verified source contract for the known-good preview (do not alter casually):**

1. `decodeAnimationVertices()` in `src/crash/formats/nsf/nsf_archive.cpp` is byte-for-byte/function-text identical to the implementation at checkpoint `ff5359a`. For compressed Model data it reads the temporal bitstream starting at `specialVertexCount * 24` bits, decodes ModelPosition records in their existing order, and assigns each decoded record to `frame.vertices[i]` (the loop index starts at zero). Do not change that destination to `i + specialVertexCount` as part of a renderer fix.
2. Animation rendering in `src/ui/editor_ui.cpp` applies `frame.specialVertexCount` to Model triangle indices when resolving frame-vertex slots. The current source has this offset in the main triangle path, face picking/selection, face-edge overlay, and vertex/surface picking path. Keep these consumers consistent.
3. The decoder destination and renderer index offset are an empirically verified pair for the currently working Cr10V preview. Do not “simplify” them into a single unified indexing convention based on assumptions about the frame vector. The data layout/edit/save mapping is a separate question and remains unresolved.
4. The current branch head at the time this note was written is `4e0ca329cf535bc055c9e4e4398673786b8f8779`. The decoder implementation was compared directly against `ff5359a` and matched exactly. The user visually confirmed the resulting Cr10V preview in the supplied screenshot.

**Regression guardrails:** before any future animation decoder/renderer change, compare the decoder function and all renderer index consumers against `ff5359a`; run the Cr10V sample regression; verify the rendered preview visually. A test that only checks decoded coordinates or save/reload is not a substitute for this visual acceptance check. Any experiment with a different vertex mapping must be isolated behind a test or branch and must not replace this working preview path until it passes the real Cr10V visual comparison.

**Still unresolved:** this recovery does not solve free vertex editing or the original coordinate-limit/chunk-relocation prompt. Investigate those in the encoder/edit path without changing the verified preview decode/render pair.


### CrashEdit-RE edit/save comparison and no-op save failure investigation — 2026-11-27

Reference baseline is **airumu/CrashEdit, branch `t/net9`** (CrashEdit: Re), as established earlier in this document. The legacy `Almamu/CrashEdit` tree contains the old fixed-width animation format and is not the relevant implementation for Crash 2 compressed frames; do not substitute it for CrashEdit-RE.

#### What CrashEdit-RE actually does

Source files inspected:
- [`Frame.cs`](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Animation/Frame.cs)
- [`FrameVertex.cs`](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Animation/FrameVertex.cs)
- [`ModelPosition.cs`](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Model/ModelPosition.cs)
- [`ModelEntry.cs`](https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Model/ModelEntry.cs)
- [`FrameBox.cs`](https://github.com/airumu/CrashEdit/blob/t/net9/CrashEdit/Controls/FrameBox.cs)
- [`AnimationRenderer.cs`](https://github.com/airumu/CrashEdit/blob/t/net9/CrashEdit/Utils/OpenTK%20Integration/AnimationRenderer.cs)

1. `Frame.MakeVertices(model)` decodes compressed ModelPosition records in order and writes record `i` into `verts[i]`. Its temporal bit cursor starts at `SpecialVertexCount * 24`, and the per-record component order is X, Z, Y. It does **not** write to `verts[i + SpecialVertexCount]`.
2. CrashEdit-RE's renderer resolves Model triangle indices as `verts[tri.Vertex + frame.SpecialVertexCount]`. The Model's converted polygon indices have a negative starting bias for compressed Models (see `ModelEntry.ConvertIndices()`). Keep this renderer convention paired with the RE decoder; do not infer the decoder's destination from the renderer lookup alone.
3. `Frame.SaveC2()` serializes the existing `Temporals` bit array. It does not regenerate compressed Model deltas from edited `Positions`.
4. The legacy `FrameBox` explicitly labels compressed Model vertices as read-only (`Vertice(s) (read-only)`). Its `UpdateTemporals()` returns immediately when `isCompresed` is true. The numeric position widgets modify cached `frame.Positions`, but they do not encode those edits into the compressed temporal stream. Thus RE's normal editor does not provide a compressed-frame vertex-edit/save implementation to port directly.
5. RE has an explicit conversion path in `ModelEntry.ConvertCompressedToUncompressed()` / `DecompressFrame()`: it creates an uncompressed Model and new uncompressed frame temporal data. This is a possible architecture for safe editing, but it must be sample-tested for EID/reference isolation, special-vertex semantics, and correct frame-to-Model linkage before adopting it.

#### Concrete native candidate for the current “change a coordinate, restore it, save fails” symptom

The current native `setAnimationFrameVertex()` writes the supplied component and sets `frame.verticesEdited = true` unconditionally, even if the supplied value equals the current value. There is no original-pose comparison in that setter. Therefore returning a coordinate to its original displayed value does not clear the dirty state; save still enters `commitAnimationEdits()`.

More importantly, the current native compressed encoder `encodeAnimationFrameVertex()` reads ModelPosition record `i` from `frame.vertices[i + frame.specialVertexCount]`, while the restored known-good decoder (and CrashEdit-RE `Frame.MakeVertices()`) writes decoded record `i` to `frame.vertices[i]`. This is a concrete decode/encode index asymmetry in the current source. `commitAnimationEdits()` then attempts a compressed encode and checks whether the decoded result reproduces the full desired pose; this is a plausible direct explanation for save rejection even when the user restores the original coordinate. It is a **source-grounded candidate, not yet a proven runtime diagnosis** until reproduced with a focused test on the Cr10V sample.

#### Next research/tests — do these before another renderer change

- Add a no-op regression: load Cr10V, decode, call `setAnimationFrameVertex` with the exact current component value, then save. It must succeed and preserve source bytes.
- Add a change-then-restore regression: edit one component to a different value, restore the exact original value, then save. It must succeed and preserve the original animation entry bytes.
- Instrument a Cr10V frame's full mapping for selected vertex indices: Model polygon index, renderer lookup index, decoder output slot, ModelPosition record index, encoder source slot, and round-trip slot. Include special-prefix boundary indices and first/last compressed records.
- Compare native compressed encoder bit writes directly against RE's `Frame.MakeVertices()` decoder and `ModelPosition` fields. Do not assume RE's normal `Frame.SaveC2()` encodes edited compressed vertices: source inspection shows it only serializes `Temporals`.
- Evaluate a deliberate conversion-to-uncompressed edit workflow as an alternative to reimplementing compressed delta mutation. Keep the source Model/Animation untouched until the converted pair can be saved/reloaded and all references are redirected safely.

**Do not change the renderer or the user-confirmed `ff5359a` preview decoder contract while doing these tests.**


### Animation Editor format label: Normal / Complex — 2026-11-27

The 3D Animation Editor now identifies the displayed frame's animation format using its linked Model's `NsfModel::compressed` flag:

- **Normal**: linked Model is uncompressed (`model.compressed == false`).
- **Complex**: linked Model uses compressed ModelPosition records (`model.compressed == true`).
- **Unknown**: the linked Model cannot currently be resolved; the UI must not guess.

The classification is evaluated for the currently displayed frame, so it follows frame playback when the animation references different Models. For Crash 3-style references the existing `linkedModelEid` path is used; otherwise the frame's `modelEid` is used.

Important terminology guardrail: frame `specialVertexCount` / special vertices are a separate frame-layout concept. They are **not** the criterion for Normal versus Complex. The label describes the linked Model's position encoding, not a claim that every other part of the Animation entry has a wholly separate binary container format.


### User-confirmed Normal / Complex classification — 2026-11-27

The 3D Animation Editor's format label was tested by the user in the running build:

- Retail sample `BT10V` / linked Model `BT10G` displays **Animation Type: Normal**.
- Retail sample `Cr10V` / linked Model `Cr10G` displays **Animation Type: Complex**; the user also confirmed the Cr10V character preview is visually correct in this build.

The UI classification is derived from the linked Model's `NsfModel::compressed` flag, follows the displayed frame's Model reference, and shows Unknown rather than guessing when the Model cannot be resolved. `specialVertexCount` is not part of this classification. The label is display-only and must not change renderer/decode/save behavior.

Checkpoint declared by the user: [e4aa8e5](https://github.com/leorelic/CrashEdit-VC/commit/e4aa8e57107e88467b3b05b9a36aa7dce975dfbc). The label's source definition is documented in the animation format research. This milestone does not resolve Complex vertex edit/serialization limitations; preserve the user-confirmed Cr10V preview contract while continuing that separate investigation.

## Separate Normal and Complex Animation editing paths — 2026-11-27

**Decision:** do not route Normal and Complex frame edits through one assumed serializer. Keep the user-confirmed Normal path as the stable baseline. Complex editing is a research-first workstream; do not change its renderer/decoder while researching its encoder or conversion.

### Classification boundary

The editor's labels describe the linked Model representation: Normal means the linked Model is uncompressed; Complex means its ModelPosition table is compressed. This is not determined by specialVertexCount, which remains a separate frame-layout property.

### CrashEdit-RE source findings (direct inspection, branch airumu/CrashEdit:t/net9)

Inspected Frame.cs, FrameVertex.cs, ModelEntry.cs, ModelPosition.cs, and FrameBox.cs:

1. Frame.MakeVertices(model) has two distinct decode paths. With a ModelPosition table it decodes compressed deltas from the temporal bitstream, starting at SpecialVertexCount * 24 bits, in X-Z-Y component order, with per-axis accumulators and width-7 resets. Without a compressed ModelPosition table it reads byte coordinates from the uncompressed temporal stream after the special prefix.
2. Frame.SaveC2() serializes Temporals as stored. It does not regenerate compressed deltas from cached Positions.
3. RE's FrameBox marks compressed vertices as read-only and UpdateTemporals() returns immediately for compressed frames. The cached position view is not evidence of a working Complex encoder/save path.
4. RE contains ModelEntry.ConvertCompressedToUncompressed(), DecompressModel(), and DecompressFrame(). The helper constructs an uncompressed Model and regenerates uncompressed frame temporals from decoded positions. This proves a conversion concept exists in RE; it does not prove that copying the helper directly is safe for our archive model.
5. RE's conversion helper constructs the converted Model with the original Model EID and the converted Animation with the original Animation EID. In an NSF where other animations share the Model EID, a native implementation must isolate/repoint references deliberately rather than replacing a shared Model globally.
6. The inspected MakeTemporals() loops over vertexCount - specialVertexCount but reads vertices[i] and inserts a prefix into the output. Special-vertex ordering/preservation during conversion therefore needs an explicit byte/slot-level test; do not assume this helper preserves every special vertex correctly merely because it exists.

Reference source files:
- https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Animation/Frame.cs
- https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Animation/FrameVertex.cs
- https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Model/ModelEntry.cs
- https://github.com/airumu/CrashEdit/blob/t/net9/Crash/Formats/Crash%20Formats/Model/ModelPosition.cs
- https://github.com/airumu/CrashEdit/blob/t/net9/CrashEdit/Controls/FrameBox.cs

### Native code implications

- Current NsfModel::compressed is the authoritative UI classification source.
- Current decodeAnimationVertices() intentionally follows the verified preview mapping: special vertices in the leading frame slots; compressed ModelPosition record i stored at frame.vertices[i]; renderer triangle consumers apply the separate special-vertex offset. Preserve this decoder/renderer pair.
- Current setAnimationFrameVertex() sets verticesEdited=true unconditionally, including when the supplied component equals its current value. Normal and Complex dirty-state handling should be audited separately; a no-op must not trigger unnecessary encoding.
- Existing Complex save code attempts to encode the compressed ModelPosition representation and has a documented decode/encode indexing asymmetry candidate. Do not assume that path is valid until a real Cr10V trace and focused regression prove it.

### Required Complex research sequence (no renderer changes)

1. Establish per-frame inventory for BT10V/BT10G (Normal) and Cr10V/Cr10G (Complex): EIDs, frame counts, vertex counts, special counts, ModelPosition counts, raw frame sizes, Model references, and all other Animations sharing each Model EID.
2. For Cr10V, trace selected vertex indices end-to-end: frame-header special slot, decoded frame.vertices slot, Model-local polygon index, ModelPosition record index, temporal bit start/end, and saved/reloaded slot. Include first and last compressed positions and vertices on both sides of the special-prefix boundary.
3. Add read-only/no-op and edit-then-restore tests before modifying the Complex encoder. Compare source bytes and decoded poses.
4. Evaluate two separate product strategies: (A) implement a true Complex ModelPosition delta encoder; (B) explicitly convert an isolated Complex Model + its target Animation to Normal representation before enabling edits. Compare fidelity, special-vertex preservation, shared EID references, chunk capacity, and save/reload.
5. Conversion acceptance must prove all frame poses are identical before/after conversion, special vertices and collision/header data are preserved, polygon indices still render correctly, only intended Animation references the converted Model, no unrelated animation changes, and save/reopen works. Also test original-game playback.
6. Only after the above should Complex vertex controls be enabled. Until then, Complex must be clearly identified and must not silently pretend edits are saveable.

**Non-negotiable guardrail:** keep the user's visually confirmed Cr10V preview behavior from ff5359a intact. Any alternate mapping or conversion experiment must be isolated in tests/a candidate archive and must not replace the working preview path without sample-backed visual verification.
## Complex-to-Normal conversion: original-game compatibility investigation — 2026-11-27

**Question:** if a Complex (compressed ModelPosition) animation is converted to Normal (uncompressed byte-coordinate frame temporals), will Crash 2 play it?

**Current conclusion:** structurally plausible, but NOT YET PROVEN in-game. This is a format-to-format conversion, not a new runtime feature: the target representation is the existing uncompressed Model + uncompressed Animation frame layout. A successful editor preview or NSF reparse is insufficient; acceptance requires the converted NSF/NSD to load and play in the original game/emulator.

### CrashEdit-RE conversion helper audit

Inspected `ModelEntry.ConvertCompressedToUncompressed`, `DecompressModel`, `DecompressFrame`, `MakeTemporals`, `Frame.MakeVertices`, `Frame.SaveC2`, and `ModelEntry.ConvertIndices` on `airumu/CrashEdit:t/net9`.

RE's helper is useful evidence that conversion was contemplated, but it is NOT a safe reference implementation to port unchanged. Two concrete red flags:

1. `DecompressModel()` removes the ModelPosition table but reuses the same `Info` and `PolyData`. `ConvertIndices()` uses a different initial vertex-index bias depending on whether `Positions` is null: compressed models start at `-Info[0x4C]`, while uncompressed models start at zero. Therefore simply nulling Positions while retaining the polygon stream changes the derived triangle-to-vertex mapping when the stored special count is nonzero. A correct conversion must explicitly preserve the rendered vertex mapping, either by transforming polygon indices/metadata as appropriate or by another proven mapping strategy.
2. `DecompressFrame()` calls `MakeVertices(model)` then `MakeTemporals()`. `MakeTemporals()` loops `vertexCount - specialVertexCount` iterations but reads `vertices[i]` starting at zero, then inserts/copies a special-prefix-sized bit region. This appears inconsistent with the uncompressed decoder, which reads ordinary vertex coordinates beginning at frame slot `specialVertexCount`, while the special vertices are separately stored in the frame header. Treat this as a likely conversion defect until a byte-level round-trip test proves otherwise.

Other required checks: preserve frame `HeaderSize` and raw special-vertex bytes consistently; preserve all collision records and frame offsets; ensure each frame's ModelEID resolves to the intended converted Model; isolate the converted Model if any other Animation/GOOL frame group shares the original Model EID; update all intended references atomically; preserve EIDs and entry/chunk integrity without accidentally changing unrelated resources.

### Game-compatibility test design

1. Start from the checked-in Crash 2 sample NSF/NSD and record hashes plus parsed resource inventory.
2. Convert a copy of Cr10V + its linked Cr10G Model to Normal. Do not overwrite the source sample.
3. For every frame, compare the decoded world-space vertex pose before/after conversion, including special vertices, first/last compressed vertices, collision records, offsets, and frame count. Compare rendered triangle vertex references, not only vertex arrays.
4. Reparse the written NSF from disk and repeat the full comparisons. Verify the Animation entry and Model entry resolve by EID and that unrelated animations retain their original linked Models and poses.
5. Load the converted NSF/NSD in the original Crash 2 game/emulator and exercise the actual Cr10V animation. Confirm it plays across all frames, with correct geometry, texture/material association, collision behavior, and no resource-load failure.
6. Only after an unchanged conversion passes game playback, edit one vertex, save/reopen, repeat the binary/pose checks, and test game playback again.

### Product decision

Keep both strategies: (A) a genuine Complex delta encoder and (B) explicit Complex-to-Normal conversion. Conversion must be an explicit archive operation with a preview/diff and safe rollback, not an invisible save fallback. It must clone/isolate shared Model resources where necessary and must not change the working Cr10V preview decoder/renderer. No claim of in-game compatibility is allowed until step 5 passes.

## Complex-to-Normal conversion: packed relocation and chunk capacity

**User-confirmed 2026-11-27:** conversion of a Complex Model and linked Animations works. The first successful implementation moved every overflowed Animation into its own type-0 chunk, wasting chunk slots and risking zone load-list limits.

- Relocate the complete Animation entry on source-chunk overflow.
- Try the latest conversion-created packing chunk before allocating another.
- Include the growing entry-offset table, prefix padding, all complete raw entries and the incoming entry in capacity calculations.
- Rebuild and reparse the table and terminal used-data offset after appending.
- Preserve pending edited poses by chunk/entry/frame identity even when the relocated entry is appended at index 1+. The converter carries the actual destination entry index through relocation and rebuilds that entry, rather than assuming every destination chunk has exactly one entry.
- Do not reorganize unrelated pre-existing chunks.
- Show Remaining space in Properties based on the terminal used-data offset, not the fixed 64 KiB buffer size.

The regression now uses two linked Animations of deliberately different frame counts: the short entry relocates first, and the longer entry's aggregate frame growth then forces a second relocation and asserts that they share one destination chunk, keep distinct EIDs, reparse, and survive Save/reopen. The source chunk retains its unrelated filler. Do not mark this correction as a milestone until CI passes.

## Complex-to-Normal conversion: user-confirmed in-game validation — 2026-11-27

**User-confirmed end-to-end:** after converting the Complex Model and its linked Animations, saving the resulting archive through CrashEdit-RE (CE-RE), the user tested that saved file in Crash 2 and reported that it worked correctly in-game.

This closes the previously pending game-compatibility validation for the tested converted model/animation set. Combined with the passing native regression for capacity-aware packing and archive Save/reopen, the conversion milestone now has both archive-level and original-game evidence.

- Accepted milestone/checkpoint: [90f4a99](https://github.com/leorelic/CrashEdit-VC/commit/90f4a995e4f399cdd71a7d951ed27171ef6b9357).
- Scope: user-tested converted Model and linked Animations, with the saved file round-tripped through CE-RE before in-game testing.
- Do not generalize this single successful test to every Complex Model, Animation layout, special-vertex configuration, or Zone load-list scenario. Retain representative regression coverage and test additional resource sets as they arise.


## User-confirmed large Crash ear edits after Complex-to-Normal conversion — 2026-11-27

**Major practical finding (user-tested):** after converting Crash's Complex Model/Animation resources to Normal, the user can move Crash's ear to substantially larger positions that were previously blocked by the Complex edit/encoding limitation. CrashEdit-VC saves the edited model successfully.

This extends the [90f4a99 in-game conversion milestone](https://github.com/leorelic/CrashEdit-VC/commit/90f4a995e4f399cdd71a7d951ed27171ef6b9357): the converted Model/linked Animations were already confirmed to survive CE-RE save and original-game playback; the new report confirms the converted Normal representation also permits larger ear-position edits and those edits save in CrashEdit-VC.

**Evidence and scope**
- User reported the larger ear positions were previously not possible before conversion.
- User reported the converted model saves successfully with those larger positions.
- This is user-tested behavior on the resource/edit case they exercised, not a claim that all vertices, axes, Models, or arbitrary coordinates are unbounded.
- The exact maximum representable coordinate and any remaining Normal-frame byte/encoding limits have not been measured.
- Preserve the known-good Complex preview decoder/renderer contract; this result supports the explicit conversion workflow rather than changing Complex decoding.

**Next validation:** save the larger ear edit, reopen the resulting NSF in CrashEdit-RE, and test the edited converted resource in Crash 2. Compare the target vertex coordinates/pose before conversion, after conversion, after edit/save/reopen, and in-game. Record the exact vertex, axis, requested coordinate, and tested resource EIDs if further limits appear.

This user-confirmed result supersedes the earlier statement that larger-coordinate editing after conversion was only a proposed strategy; it does not supersede the separate remaining-work notes for broad resource coverage, Zone load-list limits, or exhaustive coordinate-range testing.


## User-confirmed edited ear survives CE-RE and original-game playback — 2026-11-27

**End-to-end validation completed:** the user has now tested the *edited* larger Crash ear position in the original game and confirmed that it works. This closes the exact follow-up validation left open in the preceding entry.

Confirmed sequence for the tested resource:
1. Convert the Complex Model and linked Animations to Normal.
2. Move Crash's ear to a substantially larger position that was not possible in the original Complex representation.
3. Save the edited model through CrashEdit-VC.
4. Save/reopen the file through CrashEdit-RE (CE-RE).
5. Load the edited result in Crash 2 and confirm the larger ear position works in-game.

**Conclusion:** for this tested resource and edit, Complex-to-Normal conversion enables larger vertex-position edits, and the edited result survives the editor save, CE-RE round trip, and original-game loading/playback. This is stronger evidence than the earlier unchanged-conversion test: the actual modified geometry—not merely the converted baseline—has now passed in-game validation.

**Custom-animation relevance:** this validates a necessary building block for custom animation authoring: editable converted geometry can be saved and consumed by the original game. It does not yet establish a complete custom-animation workflow. Remaining work includes editing/keyframing frame poses, reliably serializing all intended frame changes, maintaining Model/Animation EID references and chunk/Zone constraints, and testing complete multi-frame custom animations in-game.

Keep the earlier scope limits: this is a confirmed test case, not proof of unlimited coordinate range or compatibility with every Model/Animation layout.
