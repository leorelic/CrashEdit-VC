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
