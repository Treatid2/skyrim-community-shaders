# Testing integration

`codex/testing-integration-VR` contains the clean local acceptance baseline
plus current compatible development candidates. It supplies experimental
builds for live testing before those candidates complete acceptance.

The clean branch is `codex/local-main-VR`. Testing admission does not promote
work to that branch or establish review, runtime or release qualification.
Individual PR histories and acceptance gates remain separate.

[The composition manifest](testing-integration.json) records the accepted
baseline, exact included candidate revisions, dependency preservation,
composition corrections and excluded work. Its pre-manifest commit binds
the integrated source; each Broker delivery separately binds the final
commit, tree, profile, artifacts and manifest hash.

The starting mapping candidate already contains corrected PR45, PR46,
corrected PR44 and the reconciled PR47 scope. Integration merges add current
PR48 and PR51, retaining mapping services and the required CommonLib capture
conversions. Old published PR44/45 revisions and the obsolete PR47 branch
are not merged again. NVIDIA/DLSS and FidelityFX implementations retain the
starting mapping candidate's source and dependency pins.

A focused followup to the initial combined build adds fixed-size late-window
activation-attempt, refusal-count and last-input diagnostics. The composition
manifest records the deliberate RenderMap tree change. Target, frame and
publication eligibility guards and rendering behavior remain unchanged.
Source contracts and scoped hooks passed; the followup needs its own native
build, assertion results, package and Mapping admission. It does not establish
or fix the cause of the earlier zero-event activation timeout.
CSX Code Builder owns the committed composition and updates it deliberately
as candidate revisions and the clean acceptance baseline change. Build
Broker owns exact-source compilation and artifact verification. Mapping
owns supported runtime admission, loaded-identity verification and scoped
live tests, after safely closing its previous owner-controlled work.

Every live result must identify the final integration commit, Broker build
identity, composition manifest and tested scenario. General mapping results
do not substitute for render-scale qualification or screenshot-worker tests.

No review PASS is required to request an experimental build. Exact source,
dependency admission, artifact custody and runtime ownership still apply.
