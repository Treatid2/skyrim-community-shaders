"""Check the configured controller suite without executing its binaries."""

import argparse
import json
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", required=True)
    parser.add_argument("--ctest-command", required=True)
    parser.add_argument("--expected-tests", nargs="+", required=True)
    args = parser.parse_args()

    def enumerate_tests(label: str | None = None) -> list[dict]:
        command = [args.ctest_command, "--test-dir", args.build_directory, "-N", "--show-only=json-v1"]
        if label is not None:
            command.extend(["-L", label])
        result = subprocess.run(command, capture_output=True, text=True, check=True, timeout=30)
        return json.loads(result.stdout)["tests"]

    tests = enumerate_tests()
    names = [test["name"] for test in tests]
    if len(names) != len(set(names)):
        raise RuntimeError("configured CTest names are not unique")
    expected = set(args.expected_tests)
    if len(expected) != len(args.expected_tests):
        raise RuntimeError("authored controller registrations contain duplicate names")
    required = {"FeaturePresetCompatibilityContract", "WetternessPuddleStrengthContract", "AioInstallResetPolicy"}
    if not required <= expected:
        raise RuntimeError(f"required controller contracts are missing: {sorted(required - expected)}")
    by_name = {test["name"]: test for test in tests}
    for name in sorted(expected):
        if name not in by_name:
            raise RuntimeError(f"authored controller test is not configured: {name}")
        properties = {item["name"]: item["value"] for item in by_name[name].get("properties", [])}
        if "ControllerTests" not in properties.get("LABELS", []):
            raise RuntimeError(f"authored controller test lacks ControllerTests label: {name}")
    selected = {test["name"] for test in enumerate_tests("^ControllerTests$")}
    if selected != expected:
        raise RuntimeError(f"controller label selection differs: missing={sorted(expected - selected)}, extra={sorted(selected - expected)}")
    print(f"PASS: {len(names)} unique configured tests; {len(expected)} labelled controller tests")


if __name__ == "__main__":
    main()
