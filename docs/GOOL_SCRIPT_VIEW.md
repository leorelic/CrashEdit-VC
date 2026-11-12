# GOOL Script Editor documentation

The **Documentation** tab inside the GOOL Script Editor is the in-app reference for understanding the decompiler's notation. Its topic list stays separate from the code view, so it does not consume editing space. In the Selected GOOL view, Ctrl+Click a documented keyword or notation token to open the related topic.

## What the Selected GOOL view is

The Selected GOOL tab is a read-only, best-effort decompilation view of the currently selected GOOL entry. It presents meanings supported by the current IR and analysis; it is not yet an editable or recompilable representation of the selected object.

## Routines and labels

- **Routine number** is the index of a discovered routine.
- **Entry** is the GOOL instruction index where the routine begins.
- **Instruction count** is the number of instruction positions assigned to that routine.
- **Shared count** identifies positions shared with another discovered routine.
- **Labels (`Lxxxxx:`)** identify instruction positions and branch destinations.

Routine discovery currently uses supported entry sources such as local Code hooks and local calls. Discovery is not proof that every runtime entry point has been found.

## Conditions and loops

The decompiler may reconstruct `if`, `else`, `while`, and `do/while` when the control-flow pattern can be proven safely. It retains explicit labels and branches for ambiguous, irreducible, or unsupported paths instead of guessing a high-level structure. A structured block is a conservative analysis result, not a claim that the original source used the same syntax.

## Calls, returns, and state transitions

Calls and returns describe routine control flow. State transitions are emitted when the target descriptor can be resolved. Unresolved targets remain explicit diagnostics or references. Readable notation does not imply that every runtime side effect or dispatch rule is understood.

## Native MIPS

A `mips` line is a native PlayStation R3000A instruction embedded in the GOOL stream. It is assembly, not a GOOL Script command. Supported native branch targets can be mapped to exact instruction boundaries. Unsupported or malformed encodings remain represented as raw words.

## Unknown instructions

- **`.gool_word`** preserves an original 32-bit GOOL instruction whose semantics are not currently modeled.
- **`.mips_word`** preserves a native instruction that could not be decoded.

These markers are intentional. Do not replace them with guessed names or behavior. Consult the Inspector and the evidence-backed opcode audit.

## Offsets and raw words

The underlying decompiler retains original instruction positions, byte offsets, and raw 32-bit words, plus source relationships and native-instruction annotations. The readable script view hides low-level comments to keep the code uncluttered. Use Traditional GOOL Inspection / GOOL Inspector to compare a readable statement with its exact encoded instruction, operands, data pools, state metadata, and references.

## Diagnostics

Diagnostics report cases where analysis could not safely resolve an operation, branch, call, state descriptor, or instruction meaning. A diagnostic does not automatically mean the original game data is malformed. Likewise, no diagnostics does not prove that every semantic effect is understood.

## Operations and semantics

An opcode should receive source-like notation only after its behavior is established. The ongoing audit covers GOOL opcodes, operand roles, object/process fields, state and hook references, and native MIPS/GTE behavior. Unknown and partially understood operations remain low-level until verified against reliable evidence and regression tests.

## Current limitations

- The selected GOOL decompilation is read-only; selected-object source editing and recompilation are not implemented.
- Structured control flow is conservative.
- Real-object validation is still limited; additional Crash 2 GOOL entries are needed.
- The next research stage is an evidence-backed inventory of unknown opcodes, operand roles, process fields, hooks/references, and native MIPS/GTE cases.
