# Crash 2 GOOL instruction audit

## Scope and evidence

This is the first-pass instruction-format audit for **Crash Bandicoot 2 GOOL (Version2)**. The reference for the current mnemonic/format inventory is the CrashEdit instruction registry in `Crash/Formats/Crash Formats/GOOL/GOOLInstructionList.cs`, together with its generic bitfield parser in `GOOLInstruction.cs`. The C++ decoder must stay version-gated and preserve the original 32-bit instruction word.

The registry explicitly marks some opcodes as unidentified. Those entries are not promoted to guessed mnemonics. Their known bitfield layouts are still useful and are retained where the reference registry supplies one.

## Encoding rules confirmed by the reference implementation

- Each instruction is a 32-bit word: the upper 8 bits are the opcode; the lower 24 bits are the operand/format field.
- Format strings are read from the least-significant operand bit upward. Each character position consumes one bit, including fixed `0`/`1` bits and unused `-` bits.
- `[...]` denotes a GOOL reference.
- `{...}` denotes a writable/destination GOOL reference.
- `(...)` denotes a process/object field.
- `<...>` denotes a signed immediate field.
- Unbracketed letter fields are immediate values. Repeated letters form one contiguous field.
- GOOL references in the standard two-reference formats are 12 bits each. The parser retains raw reference values even if the reference cannot be resolved.

## Important correction: SETF

Crash 2 opcode `0x11` (`SETF`) uses `DefaultFormatDS2`, i.e. **`{DDDDDDDDDDDD} [SSSSSSSSSSSS]`**. The destination is therefore a writable/destination reference, not an ordinary source reference. This is now explicitly covered by the C++ unit test.

The same destination-reference form is used by the Crash 2 definitions for opcodes 18 (NOT), 20 (LEA), 23 (NOTL), 39 (ANIS), 43 (SIN), 44 (COS), and 46 (unidentified), where the registry specifies that format.

The reference implementation decodes `{D}` differently from `[S]`: destination values with bit `0x400` clear are signed 7-bit stack/argument indices; with bit `0x400` set, they address an object field, a linked object's field, or the special stack pointer field. The C++ resolver now follows that split and displays the resolved destination alongside its raw 12-bit value. It does not route a destination reference through the data-pool resolver.

## Source-reference special encodings

The source resolver also needs to preserve the distinctions in the original `GOOLInstruction.GetRefVal` implementation. When bit `0x800` is set, the value is not a pool offset:

- With `0x400` clear and `0x200` clear, the low 9 bits are a signed immediate.
- With `0x400` clear, `0x200` set, and `0x100` clear, the low 8 bits are a signed immediate.
- The exact values `0xBE0` and `0xBF0` are the null and secondary stack-pointer markers.
- With `0x400` set and `0x200` clear, the reference names a linked object's field. The original formatter displays the link explicitly, including `self->field` for link zero.
- With `0x400` and `0x200` set, `0x1F` in the low 9 bits is `[sp]`; other values identify object fields.

The C++ resolver now formats these cases instead of collapsing all of them into an opaque raw special-reference label. Unit tests cover both signed-immediate widths, the two exact markers, self/parent links, `[sp]`, and a memory field.

## Crash 2 opcode inventory

| Opcode range / value | Registry name(s) | Audit status |
|---|---|---|
| 0x00-0x0F | ADD, SUB, MUL, DIV, EQL, AND, OR, ANDL, ORL, LT, LTE, GT, GTE, MOD, XORL, TSTA | Named; two 12-bit reference operands |
| 0x10 | RAND | Named; two 12-bit references in B/A order |
| 0x11 | SETF | Named; writable D reference + source S reference |
| 0x12 | NOT | Named; writable D reference + source S reference |
| 0x13 | LOOP | Named; L/R references |
| 0x14 | LEA | Named; writable D reference + source S reference |
| 0x15 | ASH | Named; L/R references |
| 0x16 | PUSH | Named; A/B references |
| 0x17 | NOTL | Named; writable D reference + source S reference |
| 0x18 | SETC | Named; 14-bit immediate, 1-bit selector, 6-bit process field |
| 0x19 | ABS | Named; writable D reference + source S reference |
| 0x1A | CPAD | Named; fixed immediate subfields |
| 0x1B-0x2D | VEL, MISC, PSIN, SYNC, GVAR, GVAW, DEGD, SEEK, COLR, COLW, DSEK, PSHP, ANIS, EFLR, EFLV, ARRL, SIN, COS, ATAN | Named; per-opcode layouts in the C++ format table |
| 0x2E | UNK46 | Explicitly unidentified; D/S destination/source format retained |
| 0x2F | NOP | Named; fixed marker bits |
| 0x30-0x47 | DBG, RET, BRA, BNEZ, BEQZ, GO, GNEZ, GEQZ, ANIM, ANIF, VEC, CALL, SEVT, RJEV, ACEV, SPWN, CHNK, SNDP, SNDA, COLL, BEVT, CEVT, SPWF, CALR | Named; per-opcode layouts in the C++ format table |
| 0x48 | UNK72 | Explicitly unidentified; generic A/B reference layout |
| 0x49 | MIPS | Named native-code marker; not a GOOL operand layout |
| 0x4A-0x4D | UNK74-UNK77 | Explicitly unidentified; generic A/B reference layout |
| 0x4E | ARRS | Named; L/R references |

The registry's final named entry is opcode 78 (0x4E, ARRS); opcode 73 (0x49) is MIPS. The five opcodes 72 and 74-77 are explicitly registered as unidentified and use the generic A/B reference format. Opcode 46 is also explicitly unidentified, with the D/S format. These are shown as unknown in the UI while retaining their operand structure.

## Entry-loader comparison and current parser boundary

The original `GOOLEntryLoader.LoadGOOL` confirms the item order used by the C++ parser:

1. 24-byte header
2. instruction words
3. 32-bit data-pool words
4. optional 16-bit state map
5. optional 16-byte state descriptors
6. optional frame-group stream

The original `GOOLEntry` constructor decodes the instruction stream from item 2 in that list's one-based wording (array index 1), not from the temporary integer array built from item index 2 in the loader. That temporary array is not passed to the constructor; the C++ parser correctly reads instructions from item index 1 and data from item index 2.

The C++ parser currently validates and decodes the header, instruction words, data pool, state map, and state descriptors. It validates the frame-group item's bounds but currently records only its byte length. This is an explicit coverage gap, not a fully decoded frame-group implementation. For Crash 2, the reference loader dispatches frame-group records by a 16-bit type:
- type 1: 12-byte vertex-group header (`type`, frame count, EID, interpolation flag)
- type 2: 8-byte header followed by 16 bytes per sprite frame
- type 3: 8-byte header followed by 20 bytes per font frame
- type 4: text-group record (still to be audited)
- type 5: 12-byte header followed by `frameCount * partCount` records of 24 bytes each

The source loader also supports a Crash 3-to-Crash 2 compatibility path for type 1 vertex groups; that variant consumes 20 bytes rather than the standard 12-byte record. Its selection depends on a user setting and heuristics over the following fields, so it should not be silently assumed for every Crash 2 entry.

The reference loader accepts entry item counts of 3, 5, or 6 before interpreting the optional items. The C++ parser additionally checks the header's `format` field and currently requires the canonical 3-item layout for format 0 and 5/6-item layout for format 1. Keep this stricter behavior documented until representative files establish whether noncanonical combinations occur in Crash 2.

## Decoder guardrails

1. Keep the raw 32-bit word and raw 24-bit operand payload authoritative.
2. Do not resolve a reference into a pointer/address unless the relevant pool and reference class are known.
3. Keep unresolved and out-of-range references visible as raw values plus a diagnostic.
4. Keep opcode recognition and operand-layout recognition separate: an unidentified opcode may still have a documented layout.
5. Do not use Crash 1 opcode assignments to fill Crash 2 gaps; the registry is version-specific.
6. Native MIPS payload handling is a separate concern from GOOL operand decoding.

## Next audit step

Use representative real Crash 2 GOOL entries to validate the runtime behavior of the documented reference classes and the control-flow fields (especially the branch/call family). The current inventory is a format/registry audit, not a claim that every instruction's gameplay semantics are fully understood.


## Real Crash 2 sample validation: `entry_7_0_DispC.bin`

A user-supplied exported Crash 2 GOOL entry (20,492 bytes) was inspected against the original loader and frame-group record readers. Its entry item table contains six items with boundaries:

| Item | Offset | Size | Interpretation |
|---|---:|---:|---|
| 0 | 0x2C | 24 | GOOL header |
| 1 | 0x44 | 13,100 | 3,275 instruction words |
| 2 | 0x3370 | 524 | 131 data-pool words |
| 3 | 0x357C | 216 | 108 signed 16-bit state-map values |
| 4 | 0x3654 | 592 | 37 state descriptors (16 bytes each) |
| 5 | 0x38A4 | 5,992 | Frame-group stream |

The header decodes to ID 4, class 0x200, format 1, heap base 81, event count 80, and entry count 2. All 37 state descriptors' GOOLIndex values are within the 131-word data pool.

The frame-group stream consumes exactly 5,992 bytes when decoded with the original Crash 2 record sizes: two type-2 sprite groups (14 and 1 frames), seven type-1 vertex groups (one frame each), two type-3 font groups (65 frames each), and one type-4 text group (151 NUL-terminated strings, padded to a 4-byte boundary). All 12 records fit exactly; there are no trailing bytes or truncated records.

This sample also exercises the native-code marker (opcode 0x49 / MIPS) and embedded MIPS load/store instructions. The current native inspector still needs explicit MIPS-mode tracking/disassembly; treating every word after the marker as an ordinary GOOL opcode is not sufficient. This is a separate follow-up from the byte-layout parser.

During this audit, comparison with the original `GOOLInstruction.GetRefVal` exposed a missing source-reference case: when bits 0x800, 0x200, and 0x100 are set but bit 0x80 is clear, the low 7 bits encode a signed stack/argument index. The native resolver now handles this case and has regression tests for positive stack and negative argument indices.


## Second real Crash 2 sample: `entry_13_1_WillC.bin`

A second user-supplied entry was checked independently (28,184 bytes). It uses the same six-item format-1 layout but has a different GOOL class and a substantially larger state table:

| Item | Offset | Size | Interpretation |
|---|---:|---:|---|
| 0 | 0x2C | 24 | GOOL header |
| 1 | 0x44 | 22,124 | 5,531 instruction words |
| 2 | 0x56B0 | 1,664 | 416 data-pool words |
| 3 | 0x5D30 | 172 | 86 signed 16-bit state-map values |
| 4 | 0x5DDC | 2,128 | 133 state descriptors (16 bytes each) |
| 5 | 0x662C | 2,028 | Frame-group stream |

Header values: GOOL ID 0, class 0x100, format 1, heap base 108, event count 80, entry count 86. The 133 descriptor GOOLIndex values observed in this sample range from 0 to 11; this is recorded as an observation, not interpreted as a data-pool index.

The frame-group stream consists of exactly 169 Crash 2 type-1 vertex groups. Each uses the 12-byte `VertexGroup2` header (type, frame count, EID, interpolation flag). The 169 records consume all 2,028 bytes exactly. This confirms the 12-byte type-1 layout on a second real entry and exercises both interpolated and non-interpolated groups.

The instruction stream contains 5,531 words, including eight opcode-0x49 MIPS markers. Native MIPS mode and the meaning of these markers still require a separate instruction-level audit.

The native parser now records Crash 2 frame-group boundaries and metadata for types 1–5. Type 1 is parsed as the 12-byte Crash 2 `VertexGroup2` header; type 2 sprite records use 16-byte frames; type 3 font records use 20-byte frames; type 4 text records are NUL-terminated and 4-byte aligned; and type 5 image records use a 12-byte header plus 24 bytes per frame-part. Unknown, truncated, or out-of-range records leave the GOOL entry decoded but set a frame-group-specific diagnostic, preserving the raw item for inspection.


## Project-authored extension: PlayStation COP2/GTE disassembly

The original CrashEdit MIPS decoder does not provide PlayStation COP2/GTE instruction decoding; these words are left unidentified by that reference implementation. CrashEdit-VC is therefore extending the native MIPS disassembler with COP2/GTE awareness as **new project-authored functionality**, rather than porting an existing CrashEdit feature.

This is a disassembly/inspection feature, not GTE emulation. The implementation will distinguish GTE command instructions from COP2 register transfers and load/store forms, decode recognized command functions and their encoded flags/parameters, and retain unknown or reserved encodings as raw words. The command/function names and bitfield interpretation must be checked against PlayStation R3000A/COP2 and Geometry Transformation Engine documentation, not inferred from the GOOL opcode registry.

A real word observed in the DispC native instruction stream, `0x49BE0BE0`, is a motivating test case: its primary opcode is COP2 and its GTE function field identifies `NCT` (Normal Color Triple). It should no longer appear as an undifferentiated `???` once this extension is implemented. This section records planned work; it does not imply that COP2/GTE decoding is already implemented.

## Control-flow operand semantics confirmed from CrashEdit source

The original Crash 2 opcode classes in `GOOLInstructionList.cs` distinguish three control-flow families:

- `BRA`, `BNEZ`, and `BEQZ` use a signed 10-bit `I` displacement described by the source as moving a number of **instructions**, a 4-bit `V` stack-pop count, and a 6-bit process-field `R` condition for the conditional forms. The C++ inspector reports the relative instruction displacement and stack-pop count; it does not invent an absolute instruction address.
- `GO`, `GNEZ`, and `GEQZ` use a 10-bit `S` state number, a 4-bit `V` argument count, and a 6-bit process-field `R` condition for the conditional forms. These are state transitions, not instruction-relative branches.
- `CALL` uses a 14-bit `I` subroutine number, one-bit `E` local/external selector, and 4-bit `V` argument count. It is not a 14-bit byte or instruction offset.

The inspector now presents those meanings in the instruction argument column while retaining the original instruction word and operand bits in the parsed record. Target-address calculation and state/subroutine resolution remain deferred until their indexing bases and cross-entry relationships are established.


## Local control-flow target validation

Control-flow formatting now receives the parsed GOOL entry and current instruction index. Relative branch destinations use the original CrashEdit rule `instruction index + 1 + signed displacement`, and are checked against the decoded instruction stream. Local CALL targets are checked against the same stream; external calls remain explicitly external and are not assigned a local destination.

GO/GNEZ/GEQZ state operands are checked against the state-descriptor table. For a valid descriptor, its GOOL data-pool index is validated and the referenced EID is shown. Invalid state indices or data-pool indices are reported rather than dereferenced. This is metadata-level resolution only: mapping external EIDs to other parsed NSF entries and resolving external hooks is a separate archive-level step.
