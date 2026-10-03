# Render-map runtime

The render-map runtime is a bounded, opt-in diagnostic collector for observing
Community Shaders and Skyrim rendering state. It records render-pass,
technique, geometry, material, shader, D3D11 resource/view/binding, CPU-access,
resource-version, draw/dispatch, culling-observation, and accepted eye-submit
events.

The collector is inert until a controller starts a capture. Capture bounds
limit event count, byte use, string use, and frame duration; truncation and
gaps are represented explicitly. Stopping a capture produces an immutable
completed-capture snapshot which the artifact layer can serialize without
holding render-thread state.

The runtime, its D3D and engine hooks, and its integration call sites are
developer instrumentation. They are compiled only when
`DEVBENCH_BRIDGE=ON`. A normal release build with `DEVBENCH_BRIDGE=OFF`
excludes the `src/RenderMap` translation units and retains the original
non-mapping hook paths.

This runtime does not change depth-culling decisions or apply culling results.
Depth-culling events are observations of the existing upstream behaviour.

## Included here

-   `Collector`: bounded event and string storage with explicit gap accounting.
-   `Runtime`: typed observation entry points used by engine and D3D11 hooks.
-   `Controller`: single-session start, status, stop, and completed-capture
    ownership.
-   `Artifacts` and `Serialization`: deterministic JSONL event and capture
    manifest output.
-   engine, shader, D3D11 context, and OpenVR eye-submit instrumentation.
-   deferred-context recording and exact command-list execution replay, described
    in [`device-context-command-list-slice.md`](./device-context-command-list-slice.md).
-   the controlled deferred-output timing case study and its adjacent structural
    capture requirements, described in
    [`deferred-gbuffer-performance-evidence.md`](./deferred-gbuffer-performance-evidence.md).
-   focused collector, runtime, controller, and offline graph tests.
-   runtime capture-manifest, render-event, and derived render-graph schemas.

Render-event schema revision 1.17 defines fail-closed deferred-command
semantics: command-recording draw/dispatch events require typed recording and
context identities, while missing or contradictory evidence yields explicit
gaps rather than borrowing immediate-context bindings. It also standardizes the
nullable missing-recording `FinishCommandList` form and forbids failed finishes
from naming a materialized command list.

Derived graph producer `static-semantic-resource-graph-10` independently
reconciles immutable device-context, recording, and command-list declarations
across event envelopes and payloads. Contradictory ownership chains now produce
blocking gaps and no authoritative `records`, `materializes`, `finishes`, or
`executes` edge. A restore-false execution also resets all observed and
predicted immediate SRV, UAV, and target-binding state before later work is
derived.

## Deliberately separate

The DevBench registration adapter is reviewed separately because it is the
optional external control surface over this runtime. Shader dependency
analysis, generated shader manifests, engine maps, Ghidra helpers, prior-art
catalogues, and captured-analysis reports remain development tools; they do
not enter the Community Shaders binary in either build mode.

## Optional DevBench bounds contract

`communityshaders.render_map` publishes registry, status, start, stop and
completed-event paging through the shared versioned service envelope. Contract
1.20/schema revision 21 retains field-specific `invalid_bounds` errors with
the original value and independent minimum/maximum. Supplied bounds must be
unsigned JSON integers and are checked before narrowing or duration conversion.

The registry advertises every payload schema family emitted by the serializer,
including `device-context-observation-v2`, `draw-call-v4` and `dispatch-call-v2`.
The source contract checks this inventory against the serializer.

The registry byte minimum describes its default catalogue profile. Each start
request recomputes `minimumMaxBytes` using the requested catalogue sizes and
one complete event slot. A smaller budget returns field-specific details
containing `maxBytes`, `fixedCatalogueBytes`, `eventStorageUnitBytes` and
`minimumMaxBytes` before capture starts. The default collector test shares the
adapter's exact default configuration; the exact one-event minimum is admitted.

Capture start prepares its retained provenance and success response before
activating hooks, under the controller's start transaction. A failure in either
preparation leaves no active or completed capture, event page, or artifact.
Normal stop retains completed captures; it is not used for failed-start cleanup.
Stop requires the original capture-start context and returns
`capture_provenance_unavailable` if that context is absent, without rebuilding
provenance at stop time.

Artifact runtime identity uses the loaded SE/AE or VR executable family and
CommonLib's observed runtime version. Missing exact version evidence is null.
Observed shader compilation identity must agree with the loaded runtime family;
a contradiction returns `capture_provenance_unavailable` before activation.

Implemented command-recording and command-list event kinds are selectable for
bounded qualification captures. The registry continues to advertise
`deferredContexts: false` and `commandLists: false` until the documented live
deferred-vtable gate passes; selection alone does not establish hook coverage.
