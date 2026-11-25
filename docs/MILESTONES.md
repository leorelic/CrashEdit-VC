# CrashEdit-VC milestones

> Active investigation: animation vertex transform limits remain unresolved. Read [ANIMATION_FORMAT_RESEARCH.md](ANIMATION_FORMAT_RESEARCH.md) and [RESEARCH_NOTES.md](RESEARCH_NOTES.md) before changing frame boundaries, coordinate limits, or chunk-expansion behavior.

This file records verified project checkpoints and the remaining acceptance work at each milestone. A checkpoint is a known-good rollback/reference point, not a claim that the entire editor or every game-data variant is complete.

## 2026-11-26 — Animation preview/save recovery checkpoint

**Checkpoint:** [`ff5359af45779fcd29b3827a563215b433cef1ac`](https://github.com/leorelic/CrashEdit-VC/commit/ff5359af45779fcd29b3827a563215b433cef1ac)

**Confirmed status**
- User reports the animation preview looks correct again.
- User subsequently confirmed that saving works.
- This is a stable recovery/reference checkpoint, **not a fix for the original vertex-transform limitation**.
- Code-level native regression and private real-sample checks passed for representative `Cr10G/Cr10V` and `El1eG/El1eV` data. These tests do not substitute for user testing in the editor or original game.

**Original issue still open**
- Certain vertices cannot be moved beyond a particular X/Y/Z gizmo location.
- The editor incorrectly prompts to move the Animation entry into a new NSF chunk, but the requested vertex edit still is not applied after that operation.
- The working hypothesis to investigate is a mismatch between frame coordinate/encoding limits, gizmo transform bounds, and the save/validation path—not NSF chunk capacity. Do not treat chunk relocation as a solution unless evidence shows the entry itself cannot fit in its chunk.
- Research Animation frame boundaries and coordinate limits from CrashEdit-RE source, real frames, and controlled edits. Identify the exact rejected coordinate and reason before changing bounds.

**Guardrails**
- Preserve `ff5359a` as the current known-good preview/save baseline while investigating.
- Do not claim the coordinate-limit issue is fixed until a vertex can be moved past the previously blocked location, the edit is applied, saved, reopened, and tested in-game as applicable.
- Do not silently clamp, wrap, drop, or redirect the requested edit.

## 2026-11-23 — Animation vertex save reliability

**Checkpoint** [`4b6592852d761d742358313b0250add7112e411e`](https://github.com/leorelic/CrashEdit-VC/commit/4b6592852d761d742358313b0250add7112e411e)

**Verified progress**
- Fixed uncompressed Animation frame saving. The encoder re-decodes the frame after each component write; the save path now preserves the complete intended pose between writes and verifies the final decoded coordinates before accepting the edit.
- Added a synthetic regression that edits later vertices on multiple axes, saves and reloads the NSF, and checks every resulting coordinate.
- User-confirmed in-game: edits to Crash's `Cr10V` ear vertices save and work when representable by the frame encoding.
- User-confirmed in-game: edits to `BT10V` now persist after saving instead of reverting to the default pose.
- The earlier working `Cr10V` path remains intact; this fix addresses the uncompressed frame path rather than replacing the compressed/special-vertex implementation.

**Still open**
- `Cr10V` ear edits can still be rejected when they exceed the frame encoder's representable range, even when the coordinates appear to be inside the viewer's displayed boundary. Reconcile the visual boundary with the true serialization constraints without silently clamping or dropping edits.
- Continue testing other Crash 2 animations and model/frame layouts; success on `Cr10V` and `BT10V` does not establish universal compatibility.
- Validate full NSF/NSD archive preservation and broader original-game loading after edits.
- Keep topology changes and variable-size Model serialization disabled until dependent resources can be updated and round-tripped safely.

**Previous reference checkpoint:** [`bf646ab4c18cc7a4a9111cd556774816b62c4bbd`](https://github.com/leorelic/CrashEdit-VC/commit/bf646ab4c18cc7a4a9111cd556774816b62c4bbd). The November 23 checkpoint supersedes it for ongoing work.
