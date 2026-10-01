# CommonLib runtime compatibility

CSX pins the owned CommonLib correction branch at
`284818e0c1c87d863337aeeb50cb493c01dd9a7f`, based on release v10.1.0
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

The dependency's `tests/RE/RuntimeWeatherSafety.test.cpp` covers all 256
precipitation bytes, equality and adjacent transition boundaries, mixed
and missing weather, direct normalized-byte use, and guarded mutable/const
runtime views. Compile these tests for all supported runtime presets and
execute them through the qualified build/test service. Source review and
formatting do not establish ABI correctness in a running Skyrim process;
SE, AE, and VR engine qualification remain separate evidence.
