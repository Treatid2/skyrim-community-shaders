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

CPU-access payloads use JSON null for unavailable visibility and publication
boundaries. Successful readable maps establish CPU visibility; only matched
writable unmaps establish GPU publication.

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
1.23/schema revision 25 retains field-specific `invalid_bounds` errors with
the original value and independent minimum/maximum. Supplied bounds must be
unsigned JSON integers and are checked before narrowing or duration conversion.

The registry and serializer share the current payload schema catalogue. It
contains exactly the reachable outputs, including both geometry-boundary
versions, `device-context-observation-v2`, `draw-call-v4` and `dispatch-call-v2`.
Obsolete, unreachable versions are omitted. Source checks enforce both
inventory directions; the controller fixture serializes every payload variant
and compares the emitted set with the advertised catalogue.

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

## Original post-processing observations

Render-event 1.18 adds scoped original `Main_PostProcessing` boundaries,
native shader bindings, raster viewport/scissor slots, resource/view access
candidates, copy regions and accepted OpenVR publication records. Selecting
eye-submit or transfer events expands the draw/dispatch/flow and declaration
dependencies. The boundary names the main resource as a candidate and the
existing destination as destination-before; neither establishes an actual
shader source. Draw/dispatch reads are queried before the native command and
write-capable bindings after it. Copy/resolve observations follow their
native calls. Command-stream order and operation IDs correlate these records.

The version ledger holds at most 256 resource command epochs per capture.
Epochs are admitted only after their defining event is recorded and are
withheld across frames, captures, unobserved execution and failed admission.
If a completed write cannot admit its resource or view observation, the
bounded ledger is invalidated before that call returns. A later publication
then reports a null command version and epoch, rather than an older write.
Accepted per-eye publication records carry the actual current render-target
lease generation and nullable same-frame command epoch alongside the existing
OpenVR bounds. These are observed bindings and queued commands: they do not
establish shader pixel reads/writes, GPU completion, native object lifetime,
subresource versions or a causal pixel transfer to the headset.

Capture summary and manifest 1.8 retain the version-resource capacity and
observed admission failures. Those failures mark the artifact incomplete;
ordinary event/catalogue losses retain their existing separate counters.
Summary `completion.truncated` reports lost event or structural evidence,
including catalogue capacity, scope pairing, and transfer admission failures.
Manifest `completion.truncated` specifically reports lost events, represented
by a synthetic gap. Both outputs use the same reason model: summary
`completion.incompleteReasons` and manifest extension
`csx.captureIncompleteReasons` enumerate those failures and shutdown/failure
termination. Lifecycle failure alone makes evidence incomplete without
claiming truncation. The summary's `state: complete` means the capture has
finished; `completion.incomplete` reports whether its evidence is incomplete.
Frame/time bounds and intentional selector filtering remain separate counters.

When `executionWithinSelectedGeometry` selects draws or dispatches, the
resolved event mask includes paired geometry boundaries and their semantic
declarations. The requested mask retains the user's selection. An eligible
immediate draw can consume its prepared geometry identity after setup returns;
deferred execution requires the selected geometry scope to be active.

No new capture setter, D3D state mutation or renderer selection is introduced.
Instrumentation is opt-in and may add frame cost. Native fixture execution
and live SE/AE/VR qualification are separate from source checks.

## Late main-pass window

In Skyrim VR, start with `activation: "main_post_processing"`, requested
`eventKinds: ["eye-submitted"]`, and an explicit `maxActivationWaitMs`
(1..10000; default 2000). The normal immediate selector remains the default.
The late selector rejects geometry-restricted execution and requires the eye
family; its existing dependency closure remains intact.

The collector allocates its bounded catalogues and event buffer when armed,
but admits no prefix events or catalogue entries. The armed wait is independent
of `maxDurationMs`, which begins at the original main-target
`Upscaling::Main_PostProcessing` call. At that boundary, the activation thread
queries the immediate context, native shaders, output targets, SRV/UAV bindings,
viewport/scissor state and resource descriptors using the existing getters.
This bounded bootstrap shares the active event/catalogue budget. Its dedicated
`native-pipeline-snapshot-v1` payload records state without inventing a draw or
dispatch. Other threads cannot populate the bootstrap.

The active window retains dependent events until both native accepted eyes
are recorded in its exact CPU frame, compositor cycle and resource publication
generation. Duplicate or mismatched eyes cannot complete it. Activation wait,
active duration, frame changes, bootstrap failure, missing eyes, event/byte
limits and explicit stop yield an incomplete window. Hook/status observations
latch deadlines; the caller still issues `stop` to use the existing bounded
in-flight drain and durable finalization. An armed or terminal unfinalized
capture retains single-owner admission and cannot be replaced by another start.

Manifest revision 1.9 and controller/summary `captureWindow` retain phase,
failure, bootstrap thread/count, activation frame, publication, accepted eye
mask and half-open omitted-prefix QPC bounds. Prefix count is null and prior
producer history is unobserved. Completion proves structural coverage within
this window; it does not establish the earlier pipeline, producer freshness,
native object lifetime, pixel transfer or GPU completion. Historical
`RequiredStorageBytes` coefficients from another binary must not be reused:
the current binary recomputes admission from its compiled structure sizes.

## Persistent shader diagnostic storage

Bridge-enabled shader provenance is retained independently of a live capture.
Registry/status `shaderMetadata` declare separate limits: 65,536 bytecode
identities, 65,536 stage identities, eight bounded aliases per stage identity,
and 64 MiB of retained optional dump bytes. These limits are not included in
the live capture's `maxBytes`; neither is a measured process-memory limit.
Creation metadata retires through an attached D3D private-data reference whose
cleanup retains only weak catalogue ownership. Failed attachment withholds
metadata. Retirement reclaims identity/dump capacity; unavailable dumps never
reuse an older byte sequence. Engine aliases require admitted creation metadata.

Vertex, pixel and compute creation observations run behind a catch-all boundary
that returns the original native HRESULT and output without modification. Missing
outputs and failed native calls skip diagnostics. Hash/storage/registration/map
exceptions and admission or cleanup failures increment the allocation-free
`failureCount`; no logging or allocation is performed by the exception handler.
COM teardown and native lifetime behavior still require live qualification.
