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

Requests are limited to 64 KiB and responses to 128 KiB; reads return one
stage/eye page. Native policy tests cover rectangle overflow, dimensions,
pixel size, slot budget and aggregate-budget boundaries. Compilation and
test execution use the registered Build Broker/test-owner workflow. A source
review or complete sample capture does not qualify runtime ABI, visual
fidelity, render-scale stability or performance. Those require their separate
exact-DLL protocols and accepted fixture/baseline evidence.
