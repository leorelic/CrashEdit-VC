# GOOL Development Roadmap

This roadmap tracks the progression from reliable GOOL inspection toward readable decompilation and, eventually, safe editing and recompilation. Items are staged deliberately: later authoring features depend on understanding the instruction set and validating the underlying representation first.

## 1. Inspection and format understanding

- [x] Parse and inspect GOOL headers, instructions, data pools, state maps, state descriptors, and frame groups.
- [x] Decode Crash 2 instruction opcodes and operand fields using a version-scoped instruction table.
- [x] Resolve supported data-pool references and clearly label unresolved or special references.
- [x] Build basic-block control-flow analysis with branch destinations and diagnostics.
- [x] Add a visual Blueprint view of basic blocks and execution relationships.
- [ ] Continue auditing instruction execution semantics against the original CrashEdit source and real Crash 2 samples, including opcode-specific side effects and stack behavior.
- [x] Add initial per-instruction operand access analysis from encoded reference roles, with explicit unknown handling for unsupported GOOL, native MIPS, and unaudited process-field roles.
- [x] Add an initial control-flow-aware reaching-definitions pass for stable stack, argument, and object references; unknown/native instructions and engine call/event opcodes break propagation, while call targets and dynamic state-transition paths remain outside this first pass.
- [ ] Extend data-flow analysis with validated value propagation, call/return context, and state-transition/event relationships without guessing at undocumented opcode behavior.
- [ ] Improve explanations of calls, state transitions, event dispatch, and relationships between GOOL data structures.
- [ ] Expand regression coverage across more real GOOL entries and unusual control-flow patterns.
- [x] Discover routine entry candidates generically from resolved local Code hooks and local GOOL calls; trace conservative CFG reachability, stop at other known entry points, identify exact shared instruction indices, and report instructions not reached by any discovered entry point.

## 2. Native MIPS and PlayStation instruction support

- [x] Restore the original CrashEdit MIPS-mode return sentinel behavior and add regression tests for both exact raw-word conventions (`0x03E0A809` and `0x03E00008`).
- [x] Show PC-relative MIPS branch destinations as GOOL-stream byte offsets; show the encoded jump index while explicitly retaining the unresolved upper PC bits.
- [x] Preserve the pseudo-direct low 28-bit J/JAL target in the IR without deriving the runtime upper address nibble from GOOL-relative offsets.
- [x] Record conservative R3000A GPR/HI/LO read-write effects plus memory-access and control-transfer categories for supported instruction forms; leave unsupported/reserved encodings explicitly unanalyzed.
- [x] Validate reserved-field constraints for common R3000A SPECIAL-format instructions and recognize SYSCALL/BREAK without inventing register or memory effects.
- [x] Validate reserved `rt` fields for BLEZ/BGTZ and the reserved `rs` field for LUI; keep malformed immediate-format words unanalyzed.
- [x] Model validated COP0 MFC0/MTC0 transfers with separate coprocessor-register read/write effects; leave unsupported COP0 operations and malformed transfer encodings unanalyzed.
- [x] Record the R3000A one-instruction GPR load delay for memory loads and COP0/COP2 MFC/CFC transfers, excluding discarded writes to `$zero`; keep GTE-register pipeline timing separate.
- [x] Record GPR read/write overlap between an R3000A delayed load and the immediately following native instruction; identify the complementary same-register LWL/LWR pair exception in either order, while retaining the second load's own delay before later consumers.
- [x] Preserve scalar and GTE load/store address metadata in the IR: base GPR, signed 16-bit offset, access width, signed-load extension, and LWL/LWR/SWL/SWR merge classification.
- [x] Distinguish all eight validated R3000A HI/LO operations in the IR: signed/unsigned multiply and divide, plus moves to/from HI and LO; retain independent read/write effects.
- [x] Track the PlayStation's two-instruction LWC2/SWC2 GTE data-register access delay separately from the CPU GPR load delay, flagging only same-register COP2 accesses within the window. Require contiguous, word-aligned native instruction addresses before inferring pipeline relationships.
- [x] Mark the architectural one-instruction delay slot on every validated R3000A branch/jump control transfer, including JR/JALR and COP0/COP2 condition branches; leave non-control instructions unmarked.
- [x] Mark the following native MIPS instruction as the delay-slot instruction for each validated control transfer, without changing CFG behavior or inventing a missing slot at the end of a stream.
- [x] Preserve the specific validated R3000A control-transfer operation in the IR: direct/register jumps and links, integer conditional branches (including REGIMM link forms), and COP0/COP2 condition branches; retain the generic control-transfer and delay-slot flags.
- [x] Model R3000A BC0 condition branches and RFE status-mode restoration separately from GPR effects; reject malformed branch encodings and leave other COP0 operations unsupported.
- [x] Model validated COP2 MFC2/CFC2/MTC2/CTC2 transfers with separate data/control register effects, plus BC2 condition branches; keep malformed encodings and command words explicitly unanalyzed.
- [x] Model PlayStation LWC2/SWC2 memory transfers as GTE data-register writes/reads, retaining the base GPR dependency and explicit memory read/write classification; leave other coprocessor load/store encodings unsupported.
- [x] Decode the structural fields of COP2/GTE command words (command number, ignored command bits, sf/mx/v/cv/lm, and reserved-bit snapshots) without claiming command-specific execution effects.
- [x] Identify documented GTE command numbers by mnemonic while preserving unknown command IDs and leaving command-specific register effects unanalyzed.
- [x] Model the verified register dependencies and outputs of MVMVA (selector-dependent rotation/light/color matrix, V0/V1/V2/IR vector, translation/background/far-color/zero inputs; conservatively leave the documented selector-3 hardware-quirk matrix unknown), RTPS, RTPT, NCLIP, OP, CC, CDP, NCDS, NCDT, NCS, NCT, NCCS, DPCS, INTPL, DCPL, DPCT, SQR, GPF, GPL, AVSZ3, and AVSZ4, including RTPS's V0, rotation matrix, translation vector, projection offsets/plane distance, depth-cue parameters, IR0/MAC/SXY/SZ FIFO outputs and FLAG effects, RTPT's corresponding three-vertex V0–V2 inputs and FIFO pipeline effects, OP's rotation-matrix diagonal inputs, CC/CDP color matrix and background inputs, CDP's IR0 and far-color depth cue, NCS/NCT/NCCS/NCCT normal and light matrix inputs plus background and color matrix lighting stages (NCT/NCCT additionally consume V1–V2; NCCS/NCCT additionally consume RGBC; NCS/NCT do not), NCDS/NCDT normal-lighting inputs plus RGBC, IR0, and far-color depth-cue dependencies (NCDT additionally consumes V0–V2), DPCS/INTPL/DCPL far-color inputs, INTPL vector and interpolation inputs, DCPL color and vector inputs, DPCT's three RGB FIFO input passes and RGBC code dependency, GPF/GPL RGB FIFO effects, GPL's accumulated MAC inputs, and command FLAG writes; leave other GTE commands structurally decoded but semantically unknown.
- [x] Complete the supported R3000A MIPS-I integer decoder audit, including reserved-field validation and control-flow edge cases; unsupported coprocessor/FPU forms remain explicitly unanalyzed.
- [x] Audit the PlayStation COP2/GTE instruction space: command words and selector/reserved fields, register transfers, condition branches, and load/store forms.
- [x] Decode verified GTE instruction effects while preserving raw words and retaining unknown commands or nonzero reserved encodings as structurally decoded but semantically unknown.
- [x] Resolve validated native MIPS PC-relative branch byte targets from PC+4 and signed word offsets; map only unique, exact native instruction boundaries, while preserving valid but unresolved byte offsets.
- [x] Connect native MIPS/GTE instructions back to their position and role in the surrounding GOOL stream, including source offsets, resolved branch targets, unresolved jump address bits, delay-slot ownership, load/GTE pipeline hazards, memory operands, and GTE command selectors.

The native-instruction audit covers the supported PlayStation R3000A integer subset and modeled COP0/COP2/GTE forms. FPU/COP1 instructions, unsupported privileged operations, unknown GTE commands, and malformed/reserved encodings remain loss-preserving and explicitly unanalyzed rather than being assigned speculative semantics.

## 3. Blueprint graph usability and navigation

### Near-term improvements

- [x] Make node text layout robust: wrap long instruction summaries and size node content using measured text dimensions.
- [ ] Verify node sizing and text layout at different UI scaling levels and window sizes; add clipping if testing exposes overflow.
- [x] Keep the graph focused on explaining execution and relationships rather than duplicating the detailed GOOL Inspector tables.

### Longer-term canvas improvements

- [ ] Replace the fixed grid with a horizontally expandable, execution-flow-oriented layout.
- [ ] Arrange blocks along the main execution path so users can follow the flow without tracing long wires around unrelated nodes.
- [ ] Give conditional branches, loops, calls, and returns clearer paths and routing, reducing wire crossings and unnecessary detours.
- [ ] Add canvas panning and zoom controls, including zoom in/out, fit-to-view, and reset-view actions.
- [ ] Support cursor-centered mouse-wheel zoom and preserve useful navigation when the graph becomes large.
- [ ] Consider a minimap or overview for navigating large graphs.
- [ ] Adapt node detail to zoom level: prioritize readable structure at a distance and show fuller instruction summaries when zoomed in.

The expanded canvas is a future usability milestone, not a prerequisite for the current decoding work. First establish accurate control-flow and instruction semantics; then use those semantics to produce a more meaningful graph layout.

## 4. Readable decompilation

- [x] Introduce an initial loss-preserving GOOL IR snapshot for decoded instructions, operands, data pools, state metadata/references/hooks, frame groups, and original instruction locations; preserve unknown opcodes and native MIPS words.
- [x] Add source-mapped IR control-operation records for local/external calls, dynamic returns, and state transitions; resolve local instruction targets and state descriptor/data-pool EIDs only when bounds-checked, retaining diagnostics for unresolved destinations.
- [x] Add structural R3000A instruction fields to the shared IR (format, register fields, shift, function, immediate, and jump target) while retaining the raw word; these are encoding facts, not fully audited execution semantics.
- [x] Add typed, source-mapped state-hook dispatch records for Event, Transition, and Code descriptor slots, preserving encoded values, local/external destinations, target entry locations, and all parser resolution states without inferring invocation timing or hook behavior.
- [x] Add a first source-mapped decompiler pass for selected instruction ranges: label relative branches and local calls, render calls/returns/state transitions, show decoded operands, and retain original byte offsets/raw words plus diagnostics for unknown instructions. This is a faithful listing/pseudocode foundation, not the final GOOL Script grammar.
- [x] Reconstruct a conservative forward conditional pattern as a structured `if` block when the fallthrough arm ends in an unconditional branch to a later join; retain raw offsets/words on body instructions and leave ambiguous control flow unstructured.
- [x] Reconstruct simple comparison expressions (`==`, `!=`, `<`, `<=`, `>`, `>=`) when a comparison instruction directly feeds a conditional branch; preserve the stack-test fallback when the producer cannot be proven.
- [x] Reconstruct adjacent `else if` clauses when the preceding branch targets the next comparison and both clauses share a proven join point.
- [x] Reconstruct a plain `else` arm when a final conditional skips to a unique fallback body and both paths reach a proven join.
- [x] Reconstruct simple pre-test loops when a conditional exit and a single backward branch prove the loop region; reject interior entries and additional control flow.
- [ ] Extend structured control-flow reconstruction to nested conditionals and more complex loops, with explicit fallback when region boundaries or branch conditions cannot be proven.
- [x] Apply the IR/decompiler to the first 20-word real Crash 2 sample region: DispC's Code hook at instruction 0; record its decoded words, descriptor-3 state transition, following RET, and unresolved execution question in `docs/DISPC_INITIAL_ROUTINE.md`.
- [ ] Apply the decompiler to real Crash 2 routines, beginning with the `BoxsC` Code hook at instruction 558; use the resulting output to identify missing semantics rather than inventing high-level behavior.
- [ ] Validate routine discovery against additional real GOOL objects and local-call patterns; investigate ambiguous/shared code and entry points not represented by local Code hooks or local calls.
- [x] Report when a discovered routine's control flow reaches another known entry point, preserving it as an explicit routine boundary instead of silently absorbing it or misclassifying it as shared code.
- [ ] Evolve the initial snapshot into a fully game-independent semantic IR; keep Crash 2 binary decoding/encoding in a separate backend.
- [ ] Develop a readable, source-like GOOL representation rather than exposing raw instruction words as the primary editing surface.
- [ ] Represent branches, loops, calls, returns, and state transitions in a way that can be traced back to their original instruction locations.
- [ ] Preserve unknown or not-yet-understood instructions without silently changing their meaning.
- [ ] Validate decompiled output against the original instruction stream and known execution relationships.

## 5. Editing and recompilation

- [x] Add a read-only GOOL Script decompilation preview to the Inspector, with instruction-range controls and a shortcut to the first resolved local Code hook.
- [ ] Design a readable GOOL Script Editor for manual source-like editing.
- [ ] Develop Blueprint into a visual authoring interface, allowing users to understand and eventually edit execution flow visually.
- [ ] Keep the Script Editor and Blueprint backed by the same structured GOOL representation so changes remain consistent between views.
- [ ] Add validation and diagnostics before allowing modified GOOL data to be written.
- [ ] Implement recompilation only when instruction encoding, control flow, references, and preservation of unknown data are sufficiently understood.
- [ ] Add round-trip tests and real-file validation before enabling routine GOOL save workflows.

## 6. File integration and validation

- [ ] Establish safe GOOL entry replacement and serialization rules.
- [ ] Test modified entries against representative Crash 2 NSF files.
- [ ] Verify that saved files reopen correctly and, where practical, validate behavior in-game.
- [ ] Keep GOOL editing and NSF save capabilities gated by the maturity of the decoder, compiler, and regression suite.

## Product direction: approachable Crash 2 modding

The ultimate purpose of GOOL work in CrashEdit-VC is to make Crash 2 object modding approachable to newcomers, not merely to provide a more convenient way for experts to inspect instructions. The editing experience should eventually offer three complementary levels:

1. **GOOL Blueprints — visual editing for newcomers.** Users should be able to assemble behaviour from discoverable, pre-made event, condition, and action/function nodes; connect execution pins; configure exposed values; and receive clear validation feedback. The experience should take inspiration from the accessibility of Unreal Engine Blueprints. The current Blueprint tab is an analysis/visualization foundation, not yet this visual authoring system.
2. **GOOL Script — readable text editing.** A purpose-built, Crash 2-targeted scripting language should let users understand and modify object behaviour without dealing directly with packed operands, instruction indices, or raw machine code. GDScript is a usability reference for clarity and low boilerplate, not a syntax or semantics template. The language must be derived from verified Crash 2 GOOL behaviour rather than invented abstractions.
3. **GOOL Inspector — legacy, low-level editing.** This is the expert workflow for inspecting and modifying actual GOOL instructions, operands, data pools, state descriptors, and supported native MIPS. It preserves the precise instruction-level workflow familiar to users of traditional CrashEdit tools. This is not a separate new scripting tier; it is the low-level/legacy editing experience.

All three experiences must ultimately operate on the same underlying GOOL representation. A user should be able to create or modify behaviour in Blueprints, refine it in GOOL Script, and inspect the resulting low-level instructions in the GOOL Inspector without maintaining three independent implementations. The views should remain synchronized through the shared representation and its validation rules.

Do not implement the full visual authoring system or finalize GOOL Script syntax ahead of the semantic and representation work. First establish what Crash 2 GOOL actually does, build a faithful shared representation, and prove readable decompilation and recompilation on real objects. Design the authoring abstractions from those verified capabilities.

## Four major technical goals

The product direction above is served by four major technical goals. These are the priority for current development; new authoring UI, language syntax, and other polish must not displace them:

1. **Correct GOOL execution semantics.** Establish opcode meanings, operand effects, MIPS interactions, calls/returns, state transitions, event dispatch, and relevant side effects using evidence from source, binaries, and tests. Unknown semantics must remain explicitly unknown rather than guessed.
2. **A reusable intermediate representation (IR).** Separate Crash 2 binary decoding/encoding from shared GOOL semantics. The first loss-preserving IR snapshot is now in place; it carries original instruction words/locations, decoded operands, pools, states, references, hooks, frame groups, and unknown/native instructions. It is a foundation, not a completed architecture: explicit semantic control-flow/call/state/event models and a clean game-specific backend boundary remain to be built.
3. **Readable decompilation.** Produce genuinely understandable source-like output from real Crash 2 GOOL routines. Start with a small, representative routine and expand coverage incrementally; do not wait for every opcode to be understood. Unsupported instructions must remain visible and faithfully preserved.
4. **Recompilation and validation.** Rebuild valid GOOL data from the representation/source, preserve unsupported content where possible, and prove correctness through round-trip tests, real NSF samples, reopen tests, and eventual in-game validation before routine save workflows are enabled.

Every proposed milestone should pass this test: **Does it bring us closer to readable, editable, recompilable GOOL and easier Crash 2 modding?** If not, defer it. Prefer small, independently testable steps and evidence-backed semantics over broad UI expansion.

## Design direction

The long-term GOOL workflow should provide three complementary ways to understand and work with an entry:

1. **GOOL Inspector** — detailed decoded data, instruction fields, references, and diagnostics; eventually the low-level legacy editing surface.
2. **GOOL Blueprint** — currently a visual explanation of execution flow and relationships; eventually a visual authoring interface.
3. **GOOL Script** — a readable source-like representation, eventually with manual editing.

These are different views of the same underlying GOOL model, not separate implementations of the format. The Inspector remains useful for precise technical inspection; Blueprint emphasizes visual structure and approachable authoring; GOOL Script emphasizes readability and text-based authoring.

- [x] Model GTE data-register aliases in COP2 transfers: IRGB reads and ORGB reads depend on IR1–IR3, IRGB writes update IR1–IR3, and LZCS writes update LZCR; cover register and memory transfer forms.
- [x] Model the SXYP screen-coordinate FIFO alias in COP2 transfers: reads mirror SXY2, while writes shift SXY0–SXY2; cover register and memory transfer forms.
- [x] Record the PlayStation's fixed-false COP2 condition input in native MIPS effects, distinguishing the architectural condition source from ordinary data dependencies for BC2F/BC2T.

## Progress policy

- Mark an item complete only when the implementation and its evidence are in place.
- Keep uncertain interpretations explicitly identified as pending research.
- Prefer small, independently testable changes.
- Do not enable GOOL recompilation or routine NSF saving based only on successful parsing or display.
