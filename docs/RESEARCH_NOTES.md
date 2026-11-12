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

## Independent development and licensing boundary

**Hard project constraint — preserve CrashEdit-VC's MIT license.** CrashEdit-VC must remain an independently implemented MIT-licensed project. The following repositories are deliberately excluded as sources of code, implementation, dependencies, design templates, feature checklists, or implementation inspiration:

- **Airumu / Blender_CrashXform** — GPL-3.0. Do not inspect or restudy this project, use its video or repository as a design reference, port or adapt its code, reproduce its implementation, or incorporate its dependencies, assets, or generated output.
- **ManDude / GOOC** — its licensing has not been established as compatible with CrashEdit-VC's MIT distribution. Treat it as unavailable for reuse. Do not inspect it for GOOL decompilation guidance, copy or adapt its source, compiler/decompiler logic, tools, dependencies, assets, generated output, or implementation patterns.

These are deliberate exclusions, not tasks to revisit. Do not add features to CrashEdit-VC merely to match either project, and do not use either repository as a roadmap or comparison checklist. Build features from CrashEdit-VC's own code and accomplishments, independent analysis of Crash 2 data and runtime behavior, primary technical evidence that can be used consistently with the MIT goal, and our own regression fixtures and tests.

When planning or implementing a feature, check this boundary first. If a proposed approach depends on either excluded project, stop and design an independent approach instead. Do not assume that public availability grants permission to reuse code or other copyrighted implementation material.

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

## Native MIPS mode: source comparison and current correction needed

The original CrashEdit implementation switches from GOOL decoding into native MIPS decoding after opcode 73 (`0x49`). Its GOOL entry parser recognizes two exact 32-bit words as MIPS-mode return patterns:
- `0x03E00008` (`jr $ra`)
- `0x03E0A809` (a special raw-word sentinel in this parser)

The parser includes the instruction after the return pattern as the delay-slot instruction, then resumes GOOL decoding. This is an exact raw-word convention in the original parser; it must not be replaced by a guess based only on conventional MIPS instruction semantics.

**Important distinction:** `0x03E0A809` conventionally disassembles as `jalr $s5,$ra`, not `jalr $ra,$ra`. The canonical encoding for `jalr $ra,$ra` is `0x03E0F809`. Therefore, the disassembler should display the instruction according to its actual fields, while the GOOL stream parser must independently honor the original CrashEdit sentinel convention.

**Correction to apply:** the current native parser was recently changed to use `0x03E0F809` as the mode-return sentinel. That diverges from the original source behavior and should be restored to `0x03E0A809`, with a regression test that separately verifies:
- `0x03E0A809` displays as `jalr $s5,$ra`;
- `0x03E0F809` displays as `jalr $ra,$ra`;
- the GOOL/MIPS parser exits MIPS mode on the original raw sentinel `0x03E0A809` (and `jr $ra`) after consuming the delay slot.

The native MIPS decoder now covers common R3000A operations including shifts, register jumps/calls, syscall/break, HI/LO transfers, multiply/divide, arithmetic/logical operations, branches/jumps, immediate operations, and load/store instructions. Register operand ordering for `jalr` and source-register selection for `mthi`/`mtlo` were corrected during source/encoding review. Keep raw words visible for verification.

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

## Documentation and evidence policy

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
