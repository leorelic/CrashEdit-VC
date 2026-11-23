# CrashEdit-VC milestones

This file records verified project checkpoints and the remaining acceptance work at each milestone. A checkpoint is a known-good rollback/reference point, not a claim that the entire editor or every game-data variant is complete.

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
