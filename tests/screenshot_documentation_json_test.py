"""Parse normative screenshot JSON examples and their capability inventory."""

import json
import re
from pathlib import Path


def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result = {}
    for name, value in pairs:
        if name in result:
            raise ValueError(f"duplicate JSON member: {name}")
        result[name] = value
    return result


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    text = (root / "docs/development/screenshot-api-and-sequences.md").read_text(encoding="utf-8")
    blocks = re.findall(r"^```json\s*\n(.*?)^```\s*$", text, re.MULTILINE | re.DOTALL)
    if not blocks:
        raise RuntimeError("normative screenshot document has no JSON examples")
    for ordinal, block in enumerate(blocks, 1):
        try:
            json.loads(block, object_pairs_hook=reject_duplicate_keys)
        except ValueError as error:
            raise RuntimeError(f"screenshot JSON example {ordinal} is invalid: {error}") from error
    section = text.split("### Capabilities\n", 1)[1].split("\n### ", 1)[0]
    capabilities_blocks = re.findall(r"^```json\s*\n(.*?)^```\s*$", section, re.MULTILINE | re.DOTALL)
    if len(capabilities_blocks) != 1:
        raise RuntimeError("capabilities must have one complete JSON example")
    capabilities = json.loads(capabilities_blocks[0], object_pairs_hook=reject_duplicate_keys)
    expected = {
        "activeSourceCaptures", "outstandingCaptureJobs", "outstandingArtifacts",
        "maximumOutputsPerCaptureJob", "pendingOperations", "maximumOutputsPerFrame",
        "maximumSequenceFrames", "maximumSequenceDurationMs",
        "maximumRetainedTerminalRequests", "maximumRetainedEvents", "retentionSeconds",
    }
    limits = capabilities["limits"]
    if set(limits) != expected:
        raise RuntimeError("capabilities limits do not match the canonical/compatibility inventory")
    if limits["outstandingArtifacts"] != limits["outstandingCaptureJobs"]:
        raise RuntimeError("outstandingArtifacts must alias the canonical capture-job limit")
    print(f"PASS: {len(blocks)} JSON examples; one canonical capabilities object with matching alias")


if __name__ == "__main__":
    main()
