#!/usr/bin/env python3
"""Fail when release/API versions drift across project metadata."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIGS = {
    "endstone-blockdata-api": {
        "package_init": "python/endstone_blockdata/__init__.py",
        "wheel_pyproject": "examples/python/block_data_inspector_plugin/pyproject.toml",
        "wheel_plugin": (
            "examples/python/block_data_inspector_plugin/src/"
            "endstone_blockdata_inspector/plugin.py"
        ),
        "version_macro": "ENDSTONE_BLOCKDATA_VERSION",
    },
    "endstone-worldgen-api": {
        "exact_targets": {"1.26.33": "v0.11.6"},
        "package_init": "python/endstone_worldgen/__init__.py",
        "wheel_pyproject": "examples/python/world_gen_studio_plugin/pyproject.toml",
        "wheel_plugin": (
            "examples/python/world_gen_studio_plugin/src/"
            "endstone_worldgen_studio/plugin.py"
        ),
        "version_macro": "ENDSTONE_WORLDGEN_VERSION",
    },
}


def capture(path: str, pattern: str, label: str) -> str:
    text = (ROOT / path).read_text(encoding="utf-8")
    match = re.search(pattern, text, re.MULTILINE)
    if not match:
        raise SystemExit(f"Could not find {label} in {path}")
    return match.group(1)


def pep440_version(release: str) -> tuple[str, str]:
    match = re.fullmatch(r"(\d+\.\d+\.\d+)(?:-(alpha|beta|rc)\.(\d+))?", release)
    if not match:
        raise SystemExit(f"Unsupported SOURCE_RELEASE.json version: {release!r}")
    base, phase, serial = match.groups()
    if phase is None:
        return base, base
    marker = {"alpha": "a", "beta": "b", "rc": "rc"}[phase]
    return base, f"{base}{marker}{serial}"


def main() -> int:
    source = json.loads((ROOT / "SOURCE_RELEASE.json").read_text(encoding="utf-8"))
    slug = source.get("name")
    if slug not in CONFIGS:
        raise SystemExit(f"Unknown project in SOURCE_RELEASE.json: {slug!r}")
    config = CONFIGS[slug]
    release = source.get("version")
    if not isinstance(release, str):
        raise SystemExit("SOURCE_RELEASE.json version must be a string")
    base, python_version = pep440_version(release)

    compatibility = json.loads(
        (ROOT / "compatibility/versions.json").read_text(encoding="utf-8")
    )
    checks = {
        "CMake project version": (
            capture("CMakeLists.txt", r"project\([^\n]*\bVERSION\s+([^\s\)]+)", "project version"),
            base,
        ),
        "Python package version": (
            capture(config["package_init"], r'^__version__\s*=\s*"([^"]+)"', "__version__"),
            python_version,
        ),
        "root wheel version": (
            capture("pyproject.toml", r'^version\s*=\s*"([^"]+)"', "project version"),
            python_version,
        ),
        "test wheel version": (
            capture(config["wheel_pyproject"], r'^version\s*=\s*"([^"]+)"', "test wheel version"),
            python_version,
        ),
        "test wheel Python version": (
            capture(config["wheel_pyproject"], r'^requires-python\s*=\s*"([^"]+)"', "requires-python"),
            "==3.14.*",
        ),
        "test wheel Endstone dependency": (
            capture(
                config["wheel_pyproject"],
                r'^dependencies\s*=\s*\[\s*"([^"]+)"\s*,?\s*\]',
                "test wheel dependency",
            ),
            "endstone==0.11.6",
        ),
        "test plugin version": (
            capture(config["wheel_plugin"], r'^\s+version\s*=\s*"([^"]+)"', "plugin version"),
            release,
        ),
        "test plugin Endstone API": (
            capture(config["wheel_plugin"], r'^\s+api_version\s*=\s*"([^"]+)"', "api_version"),
            "0.11",
        ),
        "workflow release version": (
            capture(".github/workflows/ci.yml", r"^\s*RELEASE_VERSION:\s*([^\s]+)", "RELEASE_VERSION"),
            release,
        ),
        "compatibility API version": (compatibility.get("api"), base),
        "build status version": (
            capture("BUILD_STATUS.md", r"^Version:\s*\*\*([^*]+)\*\*", "build status version"),
            release,
        ),
        "installation guide tag": (
            capture("docs/INSTALL.md", r"`v([^`]+)`", "installation release tag"),
            release,
        ),
        "release guide tag": (
            capture("docs/GITHUB_RELEASES.md", r"git tag v([^\s]+)", "release guide tag"),
            release,
        ),
        "version header source": (
            capture(
                "include/version.h.in",
                rf'#define\s+{config["version_macro"]}\s+"([^"]+)"',
                "version macro",
            ),
            f"@{config['version_macro'].removesuffix('_VERSION')}_RELEASE_VERSION@",
        ),
    }
    failures = [
        f"{label}: expected {expected!r}, got {actual!r}"
        for label, (actual, expected) in checks.items()
        if actual != expected
    ]
    workflow_text = (ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
    workflow_python_versions = re.findall(
        r'^\s*python-version:\s*"([^"]+)"', workflow_text, re.MULTILINE
    )
    if workflow_python_versions != ["3.14"] * 5:
        failures.append(
            "All metadata, portable, exact, test-wheel, and release jobs must use Python 3.14; "
            f"got {workflow_python_versions!r}"
        )
    supported_bds = source.get("supported_bds", [])
    endstone_tags = source.get("endstone_tags", [])
    if len(supported_bds) != len(endstone_tags):
        failures.append("supported_bds and endstone_tags must have a one-to-one mapping")
    expected_tag_map = dict(zip(supported_bds, endstone_tags))
    configured_exact_targets = config.get("exact_targets")
    if configured_exact_targets is not None and expected_tag_map != configured_exact_targets:
        failures.append(
            f"SOURCE_RELEASE exact BDS/tag mapping: expected {configured_exact_targets!r}, "
            f"got {expected_tag_map!r}"
        )
    cmake_text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    configured_tag_map = dict(
        re.findall(
            r'(?:if|elseif)\(ENDSTONE_BDS_BUILD STREQUAL "([^"]+)"\)\s*'
            r'set\(ENDSTONE_EXACT_TAG "([^"]+)"\)',
            cmake_text,
        )
    )
    if configured_tag_map != expected_tag_map:
        failures.append(
            f"CMake exact BDS/tag mapping: expected {expected_tag_map!r}, "
            f"got {configured_tag_map!r}"
        )
    if configured_exact_targets is not None:
        adapters = compatibility.get("verified_source_adapters", [])
        compatibility_tag_map = {
            adapter.get("bds"): f"v{str(adapter.get('endstone', '')).removeprefix('v')}"
            for adapter in adapters
            if isinstance(adapter, dict)
        }
        if (
            len(compatibility_tag_map) != len(adapters)
            or compatibility_tag_map != configured_exact_targets
        ):
            failures.append(
                "compatibility exact BDS/tag mapping: expected "
                f"{configured_exact_targets!r}, got {compatibility_tag_map!r}"
            )

        workflow_text = (ROOT / ".github/workflows/ci.yml").read_text(
            encoding="utf-8"
        )
        workflow_bds = sorted(
            re.findall(r'^\s+bds:\s*"([^"]+)"\s*$', workflow_text, re.MULTILINE)
        )
        platforms = source.get("github_actions", {}).get("platforms", [])
        source_workflow_bds = source.get("github_actions", {}).get("bds", [])
        if source_workflow_bds != supported_bds:
            failures.append(
                f"SOURCE_RELEASE workflow BDS list: expected {supported_bds!r}, "
                f"got {source_workflow_bds!r}"
            )
        expected_workflow_bds = sorted(supported_bds * len(platforms))
        if workflow_bds != expected_workflow_bds:
            failures.append(
                f"workflow exact BDS matrix: expected {expected_workflow_bds!r}, "
                f"got {workflow_bds!r}"
            )

        conan_options = re.findall(
            r'"([^"]+)"',
            capture(
                "conanfile.py",
                r'options\s*=\s*\{\s*"bds_build":\s*\[([^\]]*)\]',
                "Conan bds_build options",
            ),
        )
        if conan_options != supported_bds:
            failures.append(
                f"Conan exact BDS options: expected {supported_bds!r}, "
                f"got {conan_options!r}"
            )

        powershell_options = re.findall(
            r'"([^"]+)"',
            capture(
                "scripts/build_exact.ps1",
                r'\[ValidateSet\(([^\)]*)\)\]\[string\]\$BdsBuild',
                "PowerShell BDS ValidateSet",
            ),
        )
        if powershell_options != supported_bds:
            failures.append(
                f"PowerShell exact BDS options: expected {supported_bds!r}, "
                f"got {powershell_options!r}"
            )

        result_format_anchor = (
            'set(ENDSTONE_RESULT_ERROR_CODE_FORMAT '
            '"std::format(\\"{}\\", error_info.error)")'
        )
        if result_format_anchor not in cmake_text:
            failures.append(
                "CMake must guard the Endstone v0.11.6 std::error_code format expression"
            )
        if 'set(ENDSTONE_RESULT_ERROR_CODE_MESSAGE "error_info.error.message()")' not in cmake_text:
            failures.append(
                "CMake must replace Endstone result std::error_code formatting with message()"
            )

        install_contracts = {
            "worldgen_api": (
                r"RUNTIME\s+DESTINATION\s+plugins\s+COMPONENT\s+worldgen_package",
                r"LIBRARY\s+DESTINATION\s+plugins\s+COMPONENT\s+worldgen_package",
                r"ARCHIVE\s+DESTINATION\s+lib\s+COMPONENT\s+worldgen_package",
            ),
            "_endstone_worldgen_live": (
                r"RUNTIME\s+DESTINATION\s+python\s+COMPONENT\s+worldgen_package",
                r"LIBRARY\s+DESTINATION\s+python\s+COMPONENT\s+worldgen_package",
            ),
        }
        for target, required_patterns in install_contracts.items():
            install_match = re.search(
                rf"install\(TARGETS\s+{re.escape(target)}(?P<body>.*?)^\s*\)",
                cmake_text,
                re.MULTILINE | re.DOTALL,
            )
            if install_match is None:
                failures.append(f"CMake install block is missing for {target}")
                continue
            for pattern in required_patterns:
                if not re.search(pattern, install_match.group("body")):
                    failures.append(
                        f"CMake install block for {target} does not scope every "
                        "artifact kind to worldgen_package"
                    )
                    break
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    if f"v{release}" not in readme:
        failures.append(f"README.md does not reference release tag v{release}")
    if failures:
        raise SystemExit("Metadata verification failed:\n- " + "\n- ".join(failures))

    print(f"Verified metadata for {slug} {release}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
