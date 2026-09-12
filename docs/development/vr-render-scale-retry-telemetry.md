# VR render-scale retry telemetry

`DEVBENCH_BRIDGE=ON` adds `status.retryTelemetry` to render-scale status and
terminal qualification receipts, and `retryTelemetry` to the saved iteration
record. Schema version 2 uses the existing stress session as its capture owner.
The bounded ring retains 1,024 events without coalescing; its session ID, QPC
frequency, count, overwrite count and producer-side drop count accompany every
snapshot. Render-side publishers use a non-blocking lock and increment
`droppedEvents` when a diagnostic reader owns the ring. Stop preserves the
evidence and marks unresolved viewport waits `capture_stopped`. Reset and the
next stress session clear the ring and drop count.

All added types, fields, locks, source locations, timestamps, serialization and
recording calls are compiled behind `DEVBENCH_BRIDGE_ENABLED`. Production
builds with the bridge disabled contain none of this instrumentation. The
diagnostics add no GPU queries, flushes, waits, resource operations, scheduling
changes or per-frame disk writes. They observe existing preparation results.

## Correlation and causes

Each event carries the stress session, immutable request ID and transition
epoch, method, quality, preset, sequence, frame and QPC timestamp. The containing
receipt supplies the producer Build ID and source commit. `Retry` preserves
each counted occurrence with its category, reason and source filename/line.
`render_target_relatch_requeued` identifies its exact call site in that source
revision; the category alone does not claim a more specific root cause.
Post-load memory-settle and cleanup-drain retries have separate reason strings.

`ViewportWaitBegin` records the first pending DLSS preparation observation for
each viewport role. It retains the cache hit/miss, selected slot, victim quality,
preset and last-use counter, whether a recycle fence already existed, and its
observed result. `ViewportWaitEnd` references the exact begin sequence and
timestamp, carries the final observation, and records the pending-observation
count. Repeated pending checks update that count without emitting events or
querying the clock. Immediate ready observations remain `ViewportReady`.
Readiness-to-failure changes are retained even after an earlier ready result.
A provider generation change closes the old wait as
`viewport_generation_changed` before observations enter the new generation;
it cannot complete the previous generation's interval.

Every viewport observation captures its request, transition epoch, guard
serial and provider generation before preparation begins. Publication succeeds
only if all four still identify the active guard after preparation returns.
Replaced or otherwise unproven work is discarded and counted rather than being
assigned to the successor guard.

Full-eye and submit-stage foveated-center waits are independent. They can
overlap, so their durations must not be summed as transition overhead. A cache
hit can proceed while an abandoned recycle fence remains pending. This is
represented by `cache_hit_superseded_recycle`, not a new wait for that target.

## Timing interpretation

-   `observedWaitMs` spans the first pending preparation result through the
    observed end. It includes service cadence and any work performed before the
    ready result; it is not a hardware timestamp of GPU completion.
-   Only `viewport_preparation_ready` establishes successful resolution. Failure,
    guard rearming/clearing, capture stop or a missing end must remain explicitly
    incomplete; they must not be presented as successful retry durations.
-   `GuardArmed`, `ProofRevoked`, `SettleGuardSatisfied`, `PromotionCandidate`, `Promoted` and
    `GuardCleared` distinguish the settling policy from preparation. They retain
    the guard start frame, minimum settling frames, observed stereo cycles and
    early-release state. `ProofRevoked` reports the resulting disabled state.
    `SettleGuardSatisfied` records the first eligible stereo observation at or
    beyond the minimum guard age. It does not add a timer or a separate poll.
    `settleGuardRequired` and `guardDeadlineFrame` identify the applicable guard;
    a proof-driven or door-handoff candidate bypasses that minimum-age check.
    Promotion and clearing events retain the candidate's door-handoff and
    required-cycle values. If candidate metadata was lost, those fields are
    `null` rather than being reconstructed as an ordinary settling guard.
-   The interval from the last ready observation to `PromotionCandidate` includes
    stereo qualification and any remaining settling guard. Six elapsed settling
    frames and the required stereo streak overlap; subtraction is not proof of
    isolated retry cost. Door handoffs identify their separate qualification path.
-   `RelatchAdmitted`, `Applied`, `Stable` and `Failure` provide surrounding
    boundaries. Requeue-to-admission measures observed scheduler backoff;
    retry-to-stable includes subsequent work and stabilization.

Missing owners, invalid clocks, overwritten events, unmatched intervals and
absent telemetry are reporting gaps. They never authorize replay, change a
render verdict, or manufacture a zero-duration retry.

Terminal qualification receipts capture the ring once. They include it only
when its stress-session ID matches the qualification baseline; reset or
replacement is reported explicitly as unavailable. Pending observations do not
serialize the ring.

## Validation status

The focused retry-telemetry unit and DevBench contract tests pass. A Release
build with `DEVBENCH_BRIDGE=ON` compiled the changed plugin sources, but the
final link was blocked by the existing `efsw.lib` toolset mismatch
(`__std_*_4` unresolved symbols). A bridge-disabled build and live DevBench
capture have not run. Excluding the DevBench preprocessor blocks still leaves
identical source tokens in the edited shared Upscaling/Streamline sources; this
is static production-exclusion evidence, not runtime validation.
