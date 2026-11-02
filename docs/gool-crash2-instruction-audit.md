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

## Decoder guardrails

1. Keep the raw 32-bit word and raw 24-bit operand payload authoritative.
2. Do not resolve a reference into a pointer/address unless the relevant pool and reference class are known.
3. Keep unresolved and out-of-range references visible as raw values plus a diagnostic.
4. Keep opcode recognition and operand-layout recognition separate: an unidentified opcode may still have a documented layout.
5. Do not use Crash 1 opcode assignments to fill Crash 2 gaps; the registry is version-specific.
6. Native MIPS payload handling is a separate concern from GOOL operand decoding.

## Next audit step

Use representative real Crash 2 GOOL entries to validate the runtime behavior of the documented reference classes and the control-flow fields (especially the branch/call family). The current inventory is a format/registry audit, not a claim that every instruction's gameplay semantics are fully understood.
