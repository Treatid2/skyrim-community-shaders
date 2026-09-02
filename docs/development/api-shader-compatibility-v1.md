# Shader compatibility API v1

`csx.shader.compatibility` is the versioned registration and inspection service
for external inputs that change generated shader bytecode. It is deliberately
separate from the main-thread `csx.shader` controller: providers may register
from their normal SKSE startup callbacks, and registration never calls the
renderer or game objects.

The native ABI is `include/VRAPI/CSshadercompatibilityapi.h`. Discover it through
the CSXR registry and query major version 1. CSX copies every registration and
scope before returning.

## Provider contract

A provider owns a stable, lower-case identity such as
`org.example.water-integration`, and reports:

-   a shader-facing contract major and current/minimum/maximum compatible minor;
-   an optional resource fingerprint when external shader inputs have identity
    beyond the contract version;
-   one or more declarative scopes: shader family or global.

The ABI reserves shader-source and feature scope values, but version 1 rejects
them explicitly. Offline pack generation does not yet retain authoritative
per-record source and feature provenance for every ImageSpace remapping, so
accepting either scope would make the provider's stale-cache protection false.

`displayVersion` is diagnostic only. Updating a mod package without changing its
shader-facing contract must not invalidate shaders. Conversely, a shader-facing
change must update the contract or resource fingerprint even when the package
version does not change.

Register during the provider's `PostLoad` handling (or earlier after obtaining
the CSXR registry), not `PostPostLoad`; listener order at `PostPostLoad` cannot
guarantee that a provider runs before CSX freezes the set.

Registration is atomic. Repeating an identical registration is idempotent and
returns the original handle. Reusing an identity for different requirements is
an identity conflict. Providers must register before CSX validates its shader
cache; the registry then freezes. A late new registration is rejected with
`restartRequired`, while an identical replay remains successful.

## Cache identity

For each shader, CSX selects registrations whose scopes apply, sorts them by
identity, and serializes their exact shader-facing contracts into a canonical
requirement set. Its SHA-256 digest is part of that shader record's cache identity. A
Water-only provider therefore cannot invalidate Grass or Lighting records.

The canonical data, not a friendly label or timestamp, is authoritative.
Digests are lookup accelerators and corruption checks. Pack records retain the
canonical requirement set so collisions or tooling disagreements can be
detected rather than silently accepted.

## Offline/precompiled packs

The full-build cache generator consumes the same fields through its declarative
JSON manifest. Each requested compatibility variant is compiled into the same
startup pack under its distinct canonical requirement set. Runtime selection
uses the active registration set; installers do not need a compatibility FOMOD
for variants that can coexist.

The initial Water/Horizon compatibility remains a legacy adapter until its
provider adopts this API. It is an example of the contract, not a permanent
exception mechanism.

## Managed cache storage

Optimized and developer/debug shaders occupy separate two-generation A/B pack
lanes. The files are shipped by the managed cache mod and are never created,
renamed, or deleted at runtime. Writes append committed records to the active
file. Before the main menu, fragmentation-based compaction rewrites the latest
record for every logical shader identity into the inactive file, durably commits
it, and selects its higher generation; the prior file remains a searchable
fallback generation. Fragmentation is measured only within the active file, so
the intentional fallback generation does not cause endless compaction.

The managed layout is authoritative only when the manifest and all four fixed
files are present and the manifest passes the same strict identity, runtime,
ABI, variant, file-entry, lane, generation, and record-count contract used by
packaging. No managed members means the established loose cache remains active.
A partial or invalid layout is diagnosed explicitly and also retains that
fallback until the installation is repaired or cleared; one fragment never
silently suppresses an otherwise valid loose cache.

Every writable pack set has a nonzero 128-bit identity. Runtime header checks
are unconditional, and mutation ownership combines canonical process-local
exclusion with a crash-recoverable cross-process file lease. Optimized and
developer lanes open and fail independently, so damage in one lane does not
disable the other. An unavailable lane in an otherwise authoritative layout
recompiles from source without consulting or writing legacy loose blobs.

Explicit cache clearing first commits a new empty generation barrier, then
reinitializes the superseded file, rather than creating or deleting VFS entries.
The result distinguishes complete cleanup, committed-but-degraded cleanup, and
failure before commit. A committed degraded reset keeps the safe empty
generation authoritative while emitting a bounded warning for later repair.

This storage transition is `engine-cache-v3-managed-pack` in
`config/shader-cache-abi.json`. The generated pack manifest embeds the exact
derived shader-cache ABI and packaging rejects disagreement with `Info.ini`.
