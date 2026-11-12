# DispC: first real Crash 2 GOOL routine study

This note records the first routine inspected directly from the extracted Crash 2 GOOL entry available to the project. It is evidence for the decoder and decompiler, not a claim that every instruction's runtime semantics are understood.

## Sample identity and parser layout

- Entry: DispC, chunk 7 / entry 0 (as identified when the sample was extracted)
- EID: `0x4E938CCD`
- GOOL format: 1
- Decoding target: Crash 2 / GOOL Version 2
- Header fields: ID `4`, class ID `512`, format `1`, heap base `81`, event count `80`, entry count `2`
- Item count: 6

The extracted entry is 20,492 bytes. Its item-offset table resolves to:

| Item | Start | End | Size | Parser interpretation |
|---:|---:|---:|---:|---|
| 0 | 44 | 68 | 24 | GOOL header |
| 1 | 68 | 13,168 | 13,100 | 3,275 32-bit instruction words |
| 2 | 13,168 | 13,692 | 524 | 131 data-pool words |
| 3 | 13,692 | 13,908 | 216 | 108 state-map entries |
| 4 | 13,908 | 14,500 | 592 | 37 state descriptors |
| 5 | 14,500 | 20,492 | 5,992 | frame-group data |

These boundaries agree with the current `NsfArchive::decodeGools` requirements: a 24-byte header, code and data items made of 32-bit words, a 16-bit state map, 16-byte state descriptors, and an optional frame-group item.

## Entry-point evidence

The first state descriptors contain these local Code hook offsets:

| Descriptor | Code hook instruction |
|---:|---:|
| 0 | 0 |
| 1 | 20 |
| 2 | 39 |
| 3 | 70 |

Descriptor 3 also has Event hook 85 and Transition hook 99. Its GOOL data-pool index is 0, which resolves to this entry's own EID, `0x4E938CCD`.

The first Code hook therefore gives us a compact real sample beginning at instruction 0. The next Code hook begins at instruction 20, immediately after a RET at instruction 19. That makes instructions 0–19 a useful initial routine-sized region, although the precise engine-level call/hook behavior still requires validation.

## Code hook 0: original words

All offsets below are relative to the code item, matching `NsfGoolInstruction::byteOffset`.

| Instruction | Byte offset | Raw word | Decoded opcode |
|---:|---:|---:|---|
| 0 | 0x0000 | `0x2086480D` | GVAW |
| 1 | 0x0004 | `0x11014E1B` | SETF |
| 2 | 0x0008 | `0x07002C38` | ANDL |
| 3 | 0x000C | `0x08003E1F` | ORL |
| 4 | 0x0010 | `0x11E1FC38` | SETF |
| 5 | 0x0014 | `0x11004E0E` | SETF |
| 6 | 0x0018 | `0x11802E0F` | SETF |
| 7 | 0x001C | `0x11802E10` | SETF |
| 8 | 0x0020 | `0x11882E08` | SETF |
| 9 | 0x0024 | `0x118BEE18` | SETF |
| 10 | 0x0028 | `0x11800E0A` | SETF |
| 11 | 0x002C | `0x11800E4A` | SETF |
| 12 | 0x0030 | `0x11841E4B` | SETF |
| 13 | 0x0034 | `0x11800E4C` | SETF |
| 14 | 0x0038 | `0x11800E19` | SETF |
| 15 | 0x003C | `0x38C41D00` | ANIM |
| 16 | 0x0040 | `0x1C305E4B` | MISC |
| 17 | 0x0044 | `0x1C400E4B` | MISC |
| 18 | 0x0048 | `0x35494003` | GO |
| 19 | 0x004C | `0x31894000` | RET |

The regression test added alongside this note feeds these exact twenty words through `buildGoolIR` and `decompileGoolIR`, and checks that the raw words, offsets, state-descriptor target, resolved GOOL EID, and RET remain represented. It is deliberately a focused IR/decompiler test: it does not replace a full `NsfArchive` binary-parser test.

## What we can say, and what remains open

- The opcode boundaries and operand fields above are decoded from the existing Version 2 format table.
- GO at instruction 18 encodes state descriptor 3 with zero arguments. Descriptor 3 resolves back to the same GOOL EID.
- RET follows GO at instruction 19. In the original CrashEdit source at commit `1bdd8fc2b420bd9bb4d388c22c56b9e782301016`, `Go` derives from `GOOLInsOpcode`, not `GOOLInsBranch`; its decompiler represents it as a state-change statement. The original `GOOLDecompiler` gives sequential continuation to ordinary instructions (including GO), while treating BRA/BNEZ/BEQZ and RET as explicit control-flow cases. This is strong evidence against dropping the sequential edge after GO, though it is still the old decompiler's model rather than a direct proof of interpreter scheduling.
- CrashEdit-VC now retains both facts for GO: a dynamic state-transition edge and a sequential fallthrough edge to the following instruction. Instruction 19 remains a separate basic block, so the edge is explicit and testable rather than silently merged into the transition.
- GVAW, SETF, ANDL, ORL, ANIM, and MISC are retained with their decoded operands, but this short study does not assign higher-level gameplay intent to them.
- The Code hooks at 0, 20, and 39 suggest several compact entrypoint regions near the start of the stream. Their exact boundaries should be established by tracing control flow and all incoming references, not merely by stopping at the next hook offset.
- The sample has 197 words classified as native MIPS by the current parser's MIPS-mode rules. Those instructions are outside this first 20-word routine and remain a separate audit item.

## Next evidence-driven steps

1. Compare GO/RET sequencing and the first routine's operand side effects with the original CrashEdit implementation and any reliable Crash 2 execution evidence.
2. Apply the same analysis to the next local Code hook at instruction 20, then instruction 39, to see whether the apparent routine pattern repeats.
3. Extend the real-sample regression only when the expected decoding is grounded in the parser and source evidence.
4. Return to the planned BoxsC Code hook at instruction 558 once its extracted entry is accessible; do not infer its contents from the remembered offset alone.
