# VR animation pose binding guard

## Problem

Skyrim VR's animation pose copier trusts every entry in
`BShkbAnimationGraph::boneNodes`. The helper writes a 48-byte local transform
to each resolved destination without validating that the pointer is still a
live scene object.

Two full dumps contain that exact overwrite shape in unrelated victims. One
dump proves that the player graph's Shield entry pointed at a
`BSLightingShaderProperty`; the copied Shield translation is bit-identical to
the corrupt bytes. A later Papyrus victim contains another 48-byte transform.
The immediate engine writer is established, but the component that poisoned
the binding remains unknown.

An offline audit of all 126 entries in the captured player graph found 122
non-null bindings whose parent chains reached `graph->rootNode`, three null
entries, and exactly one binding outside the graph: entry 42, the shader
property victim. No legitimate captured binding violated the scene-membership
invariant used by this guard.

The captured flattened tree reports 535 bones and legitimately uses offsets
through 534. Some offsets exceed `numPopulatedBones`, so the guard deliberately
uses the native storage bound rather than imposing that narrower count.

## Guard

On Skyrim VR 1.4.15, CSX verifies and hooks the call at RVA `0xB26DAD` to the
native helper at RVA `0xB3C260`. For each invocation it derives the owning
animation graph, walks the graph's current scene tree and validates every
direct binding against that live object set. Entries with a non-negative
flattened-tree offset must also identify a `BSFlattenedBoneTree`, remain below
its `numBones` bound and resolve to readable `BoneEntry` storage.

If every entry is valid, the original helper receives the original arguments.
If an entry is invalid, the guard logs bounded graph, binding, pose and stack
evidence and calls the helper with a temporary native-layout array view backed
by a standard vector in which only invalid entries are null. The view exposes
the data pointer and size fields read by the disassembled helper; it does not
claim ownership or invoke the game's terminating `BSTArray` allocator. The
graph's original table is not changed. If the scene tree cannot be validated
or the temporary storage cannot be allocated, pose application is skipped
rather than permitting an unvalidated write.

The hook refuses installation if the runtime or original call instruction is
not an exact match. It also exposes the ownership name
`VRAnimationPoseBindingGuard`, allowing CSX to stand down after Engine Fixes VR
ships and reports the equivalent patch.

## Scope

This is containment and causal telemetry for `ISSUE-CSX-LIGHT-VALIDATION`.
It is not evidence that CSX caused the stale binding, and it is not a repair
for the unidentified producer. The final engine-level patch belongs in Engine
Fixes VR; any identified producer should also be corrected at source.

## Producer investigation and qualification limits

The native binding builder at VR RVA `0xB3BF90` resolves skeleton bone names
through `0xCAFC40`, `0xCB0680` and `0xCB0910`, then copies each 16-byte result
into the graph table. The matcher can return either a named scene object or
the node pointer stored in a flattened-tree entry. This path does not use the
separate `BOM` cache used by the generic bone-name lookup helper.

In the September 14 03:28 dump, flattened-tree entry 382 (`SHIELD`) names a
live `BSFadeNode` at `0x2B1D4653880` under the graph root. Graph binding 42
instead names the shader-property victim at `0x2B3111BFE80`. This is concrete
evidence of inconsistent lookup structures, consistent with node replacement
without rebinding. It does not record the historical free or pointer write.

A bounded code audit also found pose-copy callers at RVAs `0xB269D7` and
`0xB2769F`. The present draft only hooks `0xB26DAD`; coverage of these other
paths remains a qualification requirement before claiming general containment.
The September 28 Papyrus dump establishes a matching transform-shaped
overwrite, but has not yet linked its victim to a specific surviving graph
binding. Its producer attribution therefore remains weaker than the older
Shield captures.

Addresses embedded in reconstructed symbol names are not necessarily VR
RVAs. Module-base subtraction and the decoded relative call independently
verify `0xB26DAD + 5 + 0x154AE == 0xB3C260`. The call bytes were checked in
both full dumps; a compile-time check keeps these constants consistent.
