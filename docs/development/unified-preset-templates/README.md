# Unified preset baseline templates

These files are the complete, vendor-neutral starting state for the three
Unified preset tiers. They preserve settings that are not yet derived by the
policy or exported from the runtime settings schema.

They are generator inputs only. They are not installed as separate mods, are
not AMD or NVIDIA presets, and are not copied into the runtime package except
through the generated Unified preset outputs.

The policy pins every baseline by SHA-256. To change a baseline intentionally:

1. edit the relevant baseline file;
2. update its `baselineSha256` in `unified-preset-policy.json`;
3. run `tools/generate-unified-presets.ps1` to regenerate the outputs;
4. run the same command with `-Check` to verify deterministic reproduction;
5. review both the baseline and generated-output diffs.

Do not use a generated Unified output or a vendor preset as a baseline. A
future complete settings-schema exporter can supersede these snapshots.
