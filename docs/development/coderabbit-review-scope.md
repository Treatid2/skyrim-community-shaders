# CodeRabbit review scope

CodeRabbit reviews files that do not match an exclusion by default. The
repository therefore does not maintain a positive allow-list for ordinary
source, tests, scripts, documentation, configuration, hooks, or hidden policy
files.

Positive path filters have only two purposes:

-   restore review for HLSL and HLSLI files under the repository's authoritative
    shader source roots; and
-   restore review for the two checked-in render-scale CSV evidence ledgers.

CodeRabbit's default filters continue to exclude build, dependency, generated,
binary, lock, minified, media, and other output classes. The repository adds
negative filters only for game-specific asset extensions not covered by those
defaults, plus the tracked uppercase PNG variant.

Generated output must not be written beneath an authoritative shader source
root. A shader below one of those roots is treated as reviewable source even if
its filename or an intermediate directory resembles generated output.

## Representative outcomes

| Path                                                     | Outcome | Reason                              |
| -------------------------------------------------------- | ------- | ----------------------------------- |
| `src/Features/Wetness.cpp`                               | Include | Unmatched ordinary source           |
| `.github/workflows/build.yml`                            | Include | Unmatched hidden policy source      |
| `.githooks/pre-commit`                                   | Include | Unmatched extensionless hook source |
| `.clang-format`                                          | Include | Unmatched root policy file          |
| `tests/shaders/.gitignore`                               | Include | Unmatched nested policy file        |
| `package/Shaders/Common/Color.hlsli`                     | Include | Authoritative shader source         |
| `features/Upscaling/Shaders/FSR2/foo.hlsl`               | Include | Authoritative shader source         |
| `docs/development/vr-render-scale-comparison-ledger.csv` | Include | Checked-in evidence exception       |
| `out/Release/output.hlsl`                                | Exclude | Default output and HLSL exclusions  |
| `.github/actions/build/build/output.hlsl`                | Exclude | Default build and HLSL exclusions   |
| `.githooks/helper.exe`                                   | Exclude | Default executable exclusion        |
| `.github/actions/view.min.css`                           | Exclude | Default minified CSS exclusion      |
| `.github/actions/view.min.js.map`                        | Exclude | Default source-map exclusion        |
| `.github/actions/package-lock.json`                      | Exclude | Default lock-file exclusion         |
| `.githooks/helper.dll`                                   | Exclude | Default binary exclusion            |
| `package/example.nif`                                    | Exclude | Repository-specific asset exclusion |

The custom positive and negative filters do not overlap. The only override of a
CodeRabbit default exclusion is the deliberate positive inclusion of shader
source and the two named evidence ledgers.
