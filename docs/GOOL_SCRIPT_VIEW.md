# Understanding the GOOL Script decompilation view

The Selected GOOL tab is a **read-only, best-effort decompilation view** of the currently selected GOOL entry. It presents the decompiler's verified understanding in a more readable form; it is not yet an editable or recompilable representation of that selected object.

## Routine information above the code

When routine discovery succeeds, the editor shows:

- **Routine number** — the index of the discovered routine in this GOOL entry.
- **Entry** — the GOOL instruction index where the routine begins (shown as an `Lxxxxx` label).
- **Instruction count** — the number of instruction positions assigned to the routine.
- **Shared count** — instruction positions that are shared with another discovered routine.
- **Previous/next arrows** — move between discovered routines.

The decompiler also records how an entry was found (for example, from a local Code hook or a local call), exit classifications, and routine diagnostics. These details remain part of the decompiler result; the compact editor chrome does not display every comment.

## Code body

- **Labels (`Lxxxxx:`)** identify branch destinations. A `goto` or conditional branch can refer to one of these labels.
- **`if`, `else`, `while`, and `do/while`** are reconstructed control-flow structures. They are emitted only when the current conservative analysis can prove the pattern; uncertain or irreducible paths stay explicit.
- **Source-like expressions and assignments** are used where the opcode and operand semantics are sufficiently understood.
- **Calls, returns, and state transitions** use dedicated readable forms when their targets or descriptors can be resolved. Unresolved targets are stated explicitly.
- **`mips` lines** are native PlayStation R3000A instructions embedded in the GOOL stream. They are assembly, not GOOL Script commands.
- **`.gool_word` lines** preserve an unknown GOOL instruction's original 32-bit word. **`.mips_word` lines** preserve a native instruction that could not be decoded.

## Source mapping and raw inspection

The underlying decompiler output associates instructions with their original byte offsets and raw 32-bit words. It also records native MIPS annotations and value-definition analysis. The readable editor hides low-level comment annotations to keep the code uncluttered; the small status line below the editor reports how many were hidden.

Use Traditional GOOL Inspection / the GOOL Inspector when you need the raw instruction fields, data pools, state metadata, native decoding, and detailed references. The readable view is not a replacement for that evidence.

## Diagnostics

IR/decompiler diagnostics report cases where the analysis could not safely resolve an operation, branch, call, state descriptor, or instruction meaning. A diagnostic does not automatically mean the original game data is malformed. Likewise, a missing diagnostic does not prove that every semantic effect is understood.

Unknown instructions and unsupported data must remain visible and loss-preserved until their meanings are established from reliable evidence and regression tests.

## Current limitations

- The selected GOOL decompilation is read-only; selected-object source editing and recompilation are not implemented.
- Structured control flow is conservative. Irreducible regions, unsupported native edges, and ambiguous paths are not guessed into high-level constructs.
- Real-object validation is still limited. Additional Crash 2 GOOL entries are needed before treating current output as broadly representative.
- The next research stage is an evidence-backed inventory and audit of unknown opcodes, operand roles, process fields, hooks/references, and native MIPS/GTE cases.
