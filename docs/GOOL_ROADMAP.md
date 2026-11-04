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
- [ ] Extend the initial access facts into inter-instruction data-flow analysis (definitions, uses, and value propagation) without guessing at undocumented opcode behavior.
- [ ] Improve explanations of calls, state transitions, event dispatch, and relationships between GOOL data structures.
- [ ] Expand regression coverage across more real GOOL entries and unusual control-flow patterns.

## 2. Native MIPS and PlayStation instruction support

- [ ] Restore the original CrashEdit MIPS-mode return sentinel behavior and add regression tests for the exact raw-word conventions.
- [ ] Complete the R3000A decoder audit, including accurate branch and jump target presentation.
- [ ] Audit the PlayStation COP2/GTE instruction space: command words, command flags, register transfers, and load/store forms.
- [ ] Decode supported GTE instructions while preserving raw words and clearly identifying unknown or reserved encodings.
- [ ] Connect native MIPS/GTE instructions back to their position and role in the surrounding GOOL stream.

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

- [ ] Define a structured intermediate representation that preserves instruction semantics, control flow, data references, and state relationships.
- [ ] Develop a readable, source-like GOOL representation rather than exposing raw instruction words as the primary editing surface.
- [ ] Represent branches, loops, calls, returns, and state transitions in a way that can be traced back to their original instruction locations.
- [ ] Preserve unknown or not-yet-understood instructions without silently changing their meaning.
- [ ] Validate decompiled output against the original instruction stream and known execution relationships.

## 5. Editing and recompilation

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

## Design direction

The long-term GOOL workflow should provide three complementary ways to understand and work with an entry:

1. **GOOL Inspector** — detailed decoded data, instruction fields, references, and diagnostics.
2. **GOOL Blueprint** — a visual explanation of execution flow and relationships, eventually with visual authoring.
3. **GOOL Script Editor** — a readable source-like representation, eventually with manual editing.

These are different views of the same underlying GOOL model, not separate implementations of the format. The Inspector remains useful for precise technical inspection; Blueprint emphasizes structure and flow; the Script Editor emphasizes readability and authoring.

## Progress policy

- Mark an item complete only when the implementation and its evidence are in place.
- Keep uncertain interpretations explicitly identified as pending research.
- Prefer small, independently testable changes.
- Do not enable GOOL recompilation or routine NSF saving based only on successful parsing or display.
