# GPU-unified presets

The three `CSX Unified` MGO presets use one settings file per quality tier on
both AMD and NVIDIA. They do not select a vendor-specific preset directory.

The capability boundary is the existing pair of upscaling settings:

- `upscaleMethod=3` requests DLSS when Streamline reports DLSS available;
- `upscaleMethodNoDLSS=2` selects FSR when DLSS is unavailable.

Provider-specific tuning remains in the same JSON file. DLSS reads its preset
and sharpener values, while FSR reads its own sharpness and runtime-provider
settings. Shader-quality policy is otherwise shared.

[`unified-preset-policy.json`](./unified-preset-policy.json) is the
machine-readable source. Each tier starts from a complete, vendor-neutral
[`SettingsUser.json` baseline](./unified-preset-templates/README.md). The
baseline supplies settings that the policy does not derive programmatically;
it is not an AMD or NVIDIA preset and it is not a claim about engine defaults.

[`generate-unified-presets.ps1`](../../tools/generate-unified-presets.ps1)
pins each baseline by SHA-256, then applies enforced defaults, shared policy,
and tier overrides in that order. It rejects vendor-named baselines and output
directories, rejects baselines outside the dedicated neutral directory, and
checks required provider-selection values. `-Check` also requires the complete
generated files to match the checked-in presets byte for byte.

Run the non-writing validation from the repository root:

```powershell
pwsh -NoProfile -File tools/generate-unified-presets.ps1 -Check
```

This structure deliberately has one authority path, not separate AMD and
NVIDIA source branches. A future settings-schema exporter may replace the
complete snapshots once it can emit every setting and stable default needed by
the presets.

The generated candidates are intentionally marked provisional. The same
Quality file has passed a live AMD fallback smoke test: DLSS was unavailable,
FSR4 was selected, both eyes were valid, and the render-scale controller
stabilized without lifecycle or fidelity failures. Static policy and schema
validation covers the NVIDIA selection path, but release qualification still
requires a native NVIDIA run proving that Streamline selects DLSS and presents
both eyes correctly.
