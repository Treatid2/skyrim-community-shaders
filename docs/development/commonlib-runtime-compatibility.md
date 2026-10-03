# CommonLib runtime compatibility

CSX pins the owned CommonLib correction branch at
`4c770e6903e44aaf4c0eb966c30357e5b812f367`, based on release v10.1.0
(`39f9d07a6ffabea8fb559eee87ab7d27cd463e8a`). The submodule URL uses
`Treatid2/CommonLibSSE-NG` so recursive consumers can retrieve the exact
corrected dependency after that branch is published.

The correction preserves unsigned normalized weather bytes and strict
precipitation transition thresholds. Weather editing, interpolation, and
rain/snow consumers therefore share the same byte semantics. RaceSexMenu
and Inventory3DManager runtime views return nullable pointers: callers
must check the selected runtime view before dereferencing it. The VR
inventory exposes only its confirmed prefix; the latest release's
`ToggleItemZoom(VR_DEVICE)` signature and concrete Havok types remain intact.

CSX uses `REX::W32::AsReal` at native graphics API boundaries while keeping
the existing owners alive. Those representation bridges preserve the
existing NVIDIA/DLSS resource lifetime and selection policy. The submit
input freshness contract checks the corresponding native resource identity.

Integrated diagnostic compositions must retain these consumer bridges when
selecting this dependency. Its renderer slots use REX graphics pointers;
native D3D descriptors, COM queries and dispatch arguments use the Windows
SDK representations. Slot assignments bridge back with `AsW32`, without
transferring ownership. Plugin exports use the dependency's current SKSE
macros, and the material texture-set spin lock uses `textureSetLock`.

Post-processing payload flags explicitly widen booleans before combining
them with 64-bit operation identifiers. This preserves their existing wire
bits while satisfying the production warning policy. The runtime fixture
checks both scissor states, operation identity and resource admission bits.

The dependency's `tests/RE/RuntimeWeatherSafety.test.cpp` covers all 256
precipitation bytes, equality and adjacent transition boundaries, mixed
and missing weather, direct normalized-byte use, and guarded mutable/const
runtime views. Weather cases invoke the production data-only precipitation
helper with constructed weather data. Accessor cases use constructed hosts,
the production pointer-accessor macros and shared offsets, with separate
static checks for engine layouts and signatures. An empty constructed VR
inventory prefix checks its count-base offset without game allocation.
These fixtures do not fabricate polymorphic engine objects in raw storage.

Compile these tests for all supported runtime presets and
execute them through the qualified build/test service. Source review and
formatting do not establish ABI correctness in a running Skyrim process;
SE, AE, and VR engine qualification remain separate evidence.
