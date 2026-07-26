# GitHub Actions troubleshooting

## Hardened in 0.4.5-beta.29

Earlier exact-build workflows could fail with:

- Linux exit code `126`, because GitHub's browser upload did not retain the executable bit on `scripts/build_exact.sh`.
- Native libraries could retain machine-specific runner RPATH entries or depend on untyped weak Bedrock ABI shims.

The workflow now calls `python scripts/build_exact.py` on both platforms, uses Clang 18 with libc++ 18 on Ubuntu 22.04, and uses `clang-cl`, `lld-link`, and Ninja from a Visual Studio developer environment on Windows. The native adapter links the matching Endstone Bedrock implementation. Both the native build and release verifier reject strong unresolved Bedrock ABI symbols, and release verification rejects machine-specific RPATH/RUNPATH entries.

## Reading a failed build

If an exact job still fails, open the workflow run and download the artifact whose name begins with:

```text
endstone-worldgen-api-diagnostics-
```

It contains the CMake cache and available configure/build logs. This makes adapter-source failures visible instead of ending with only a generic exit code.
