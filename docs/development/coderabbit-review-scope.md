# CodeRabbit review scope

CodeRabbit's documentation describes unmatched files as included by default.
An actual incremental review of this repository reported unmatched changed
files as "included by none" and omitted them after positive filters were
present. The repository therefore maintains an explicit source allow-list.

Positive path filters cover:

-   code, shaders, tests, scripts, configuration, and documentation by source
    extension;
-   known extensionless source and policy files;
-   root and nested hidden policy files that require literal hidden-path
    coverage; and
-   the two checked-in render-scale CSV evidence ledgers.

Custom negative filters independently exclude build, dependency, generated,
binary, lock, minified, source-map, media, and game-asset outputs. They are
retained even where CodeRabbit currently supplies the same default so a broad
source-extension positive cannot re-admit those files.

When a file matches both a source positive and an output negative, exclusion is
the required outcome. This is the bounded mixed-rule behavior to re-check after
configuration changes.

## Representative outcomes

| Path                                                     | Outcome | Reason                           |
| -------------------------------------------------------- | ------- | -------------------------------- |
| `src/Features/Wetness.cpp`                               | Include | C++ source positive              |
| `.github/workflows/build.yml`                            | Include | Literal hidden-policy positive   |
| `.githooks/pre-commit`                                   | Include | Literal hook-name positive       |
| `.clang-format`                                          | Include | Literal root-policy positive     |
| `tests/shaders/.gitignore`                               | Include | Literal nested-policy positive   |
| `package/Shaders/Common/Color.hlsli`                     | Include | Shader-source positive           |
| `features/Upscaling/Shaders/FSR2/foo.hlsl`               | Include | Shader-source positive           |
| `docs/development/vr-render-scale-comparison-ledger.csv` | Include | Checked-in evidence exception    |
| `out/Release/output.hlsl`                                | Exclude | Configured output exclusion      |
| `.github/actions/build/build/output.hlsl`                | Exclude | Configured build exclusion       |
| `.githooks/helper.exe`                                   | Exclude | Configured and default exclusion |
| `.github/actions/view.min.css`                           | Exclude | Configured and default exclusion |
| `.github/actions/view.min.js.map`                        | Exclude | Configured and default exclusion |
| `.github/actions/package-lock.json`                      | Exclude | Configured and default exclusion |
| `.githooks/helper.dll`                                   | Exclude | Configured and default exclusion |
| `package/example.nif`                                    | Exclude | Configured game-asset exclusion  |

The output examples deliberately exercise positive/negative overlap where
their extensions also look like source. CodeRabbit's selected-file response is
the final integration check; a schema pass alone does not establish selection
semantics.
