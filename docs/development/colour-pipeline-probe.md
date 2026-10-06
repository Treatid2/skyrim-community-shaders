# VR FSR colour-pipeline probe

The optional `communityshaders.colour_pipeline_probe` DevBench tool captures
one engine frame across five seams and both eyes: FSR input, raw FSR output,
combined main output, ImageSpace input, and ImageSpace output. Each stage/eye
retains a 17 by 17 grid of raw little-endian pixel bytes, numeric samples,
texture/view formats, source rectangles and observed resource flow.
Copy-back observations occur immediately after each actual copy call and
match its source texture and destination rectangle. Skipped finalization
cannot produce complete flow evidence. The calls establish queued work;
they do not independently prove that a D3D copy succeeded.

Use `communityshaders.fsr_color_contract` for revisioned FSR settings and
their coherent requested/effective dispatch status. The probe reuses those
controls; it adds no second setter or NVIDIA/DLSS selection policy. The older
PR47 proposal placed probe actions in `communityshaders.upscaling_api` and
duplicated colour setters. Clients should use this separate probe tool and
the existing colour-contract tool instead.

Call `arm` with the exact `expectedBuildId`, a `captureId` of 1–128 UTF-8
bytes, and unsigned `expectedRevision` from the colour-contract status.
Optional object metadata is limited to 16 KiB when serialized. The arm
receipt binds the accepted capture ID and generation; status reports current
state separately. A capture starts only on the VR main-pass stereo FSR path.
Submit-only/foveated paths, unsupported layouts or absent FSR work cannot
produce a complete capture. SE and AE reject arm without graphics mutation.

`combined_main` samples the confirmed `kMAIN` FSR copy-back source for
ImageSpace. The original post-processing call names its VR destination,
`kVR_FRAMEBUFFER`: `imagespace_input` samples that destination before the
call and `imagespace_output` samples it afterwards. Their `engineTarget`
reports the actual destination and its observed `matchesKMain` value; the
destination is not relabeled as the main source.

After status reports `complete`, call `read` for each of the ten stage/eye
pages with the same `captureId` and `generation`, the stage name and eye 0
or 1. Stage names are `fsr_input`, `fsr_output`, `combined_main`,
`imagespace_input`, and `imagespace_output`. A replaced identity/generation
is rejected, so callers cannot silently mix pages from different captures.
`reset` requires the matching ID/generation and exact build identity; it
releases only a terminal capture. Arm can replace a terminal capture directly.

Each staging dimension is at most 8192, one slot is at most 256 MiB, and a
capture is at most 1 GiB of texture payload. These are admission bounds,
not measured graphics allocation: row alignment and driver overhead remain
unknown. Rectangle and byte checks precede allocation/copy. Names use the
standard D3D resource naming helper, ownership is RAII, and retained source
references prevent pointer reuse during capture. Capture can add substantial
frame cost; it is an opt-in diagnostic, with no performance benefit claimed.

The completion query never flushes. CPU mapping uses `DO_NOT_WAIT` and retries
pending maps only within the capture deadline. Ten mapped pages are required
for completion. Missing stages across engine frames fail; asynchronous
readback expires after 120 frames, and every active state also expires after
15 wall-clock seconds. Status/read/reset/arm and render servicing enforce the
wall deadline, including an armed request that never observes FSR. Failure
releases graphics references and exposes partial queued/mapped evidence.
Status includes all ten `stageEyeSlots`, each with its stage, numeric eye,
eye name and queued/mapped flags. `missingStageEyeSlots` identifies the
unqueued stage/eye pairs and survives terminal failure until reset or a new
arm. Unqueued read pages retain the requested stage/eye identity. A count
alone does not identify which seams were observed; missing entries are not
substituted from a later engine frame.

Numeric samples represent storage values without gamma conversion. Typeless
textures require compatible typed view evidence; ambiguous/unsupported
numeric interpretation yields null decoded values while retaining raw bytes.
CPU frame and immediate-context order correlate the seams. Scene/submission
epochs remain unknown. Schema 3 retains each eye's successful dispatch
serial, selected host/runtime/fallback path, context generation, configured
and effective sharpness, sharpening enablement and dispatch QPC. Input bytes
are sampled before path selection; their effective metadata stays null until
the same frame, revision and eye dispatch succeeds. Finalization preserves
the original sample timestamp. The ImageSpace input is destination-before
evidence, not an established shader source. Headset pixel lineage remains
unverified; the separate Render Map records accepted eye publication and
nullable observed command epochs.

Each stage/eye page also includes additive `sceneObservation` schema 1.
Only the active probe copies these CPU observations, immediately before
that slot's staging-copy call. `observationCpuFrame`, numeric eye,
`beginQpc`, `endQpc` and `qpcFrequency` describe the sequential observation
bracket. The parent slot supplies stage/eye identity and `queuedQpc` after
the copy call; these are separate timestamps. No GPU completion or atomic
engine snapshot is implied. Unqueued slots expose unavailable/null values.

`cameraCache` contains native per-eye view, projection, unjittered
projection, current and previous unjittered view-projection matrices,
plus current and previous position-adjust vectors. Matrices use four rows
of four columns in `Matrix::m` storage order without transposition or
convention conversion. Adjusted positions are not absolute world-camera
coordinates. Every component must be finite; otherwise the camera group
is unavailable. Finite values do not establish geometric validity. The
native framebuffer Unmap cache has no content-frame/QPC or initialization
stamp, so those fields remain null. World-render started/completed CPU
markers are associations and do not prove continuous camera equivalence.

`imageSpaceParameters` observes the runtime-aware typed
`ImageSpaceManager::GetImageSpaceData().baseData` accessor. HDR parameters
are `eyeAdaptSpeed`, `eyeAdaptStrength`, `bloomBlurRadius`, `bloomThreshold`,
`bloomScale`, `receiveBloomThreshold`, `white`, `sunlightScale` and
`skyScale`. Cinematic parameters are saturation, brightness and contrast;
tint retains amount and RGB. Values use native engine units. A missing
manager or any nonfinite value makes its parameter group unavailable/null.
These are image-space parameters, not measured adaptive exposure or SDK
exposure history. `internalAdaptiveExposure` is explicitly unavailable;
individual scene-light objects are not sampled. The observation adds no
settings or SDK setter, simulation freeze, scheduling or rendering change.

Both colour-contract successful-dispatch status and page `dispatch` include
an additive `submittedInputs` object with `schemaVersion: 1`, boolean
`available`, nullable boolean `reset`, nullable two-number
`jitterOffsetPixels` in X/Y order, and nullable numeric
`frameTimeDeltaMilliseconds`. These are the exact SDK descriptor values,
including the submitted jitter sign, captured before dispatch. All three
inputs become null together for absent, unsuccessful or nonfinite evidence;
a negative frame time is also unavailable. Existing schema 3 fields remain.
The typed object contract is [submitted input schema](fsr-dispatch-inputs.schema.json).
An older build may omit this object; clients must distinguish absence from a
schema-1 object with `available: false` and reject unsupported versions.

Status is explicitly the last successful dispatch. Availability certifies
retained input evidence, not current-frame freshness: consumers must still
qualify its frame, serial, eye and context generation. Failed SDK calls clear
the affected diagnostic success record; runtime batch failure clears both
attempted eyes. Context changes invalidate records through the existing
colour-contract lifecycle. Probe input attribution binds the successful
same-frame, revision and eye record and preserves it in immutable pages.
No additional per-frame lock or non-atomic cross-thread frame read is added.
The fields do not expose internal exposure, history age or convergence;
neither successful submission nor a reset flag establishes vendor settling.

Requests are limited to 64 KiB and responses to 128 KiB; reads return one
stage/eye page. Native policy tests cover rectangle overflow, dimensions,
pixel size, slot budget and aggregate-budget boundaries. Compilation and
test execution use the registered Build Broker/test-owner workflow. A source
review or complete sample capture does not qualify runtime ABI, visual
fidelity, render-scale stability or performance. Those require their separate
exact-DLL protocols and accepted fixture/baseline evidence.
