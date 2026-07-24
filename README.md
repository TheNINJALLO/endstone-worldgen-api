# Endstone WorldGen API

> **CI update required:** If GitHub still shows `Portable tests (ubuntu-22.04)`, it is running the old hidden workflow. Follow [`APPLY_CI_FIX.md`](APPLY_CI_FIX.md).


A hooked, detached and parallel custom generation/post-processing service for Endstone.

## Exact Minecraft Bedrock 26.30-family support

- BDS 1.26.32 with Endstone v0.11.5
- BDS 1.26.33 with Endstone v0.11.6

## What v0.4.5-alpha.9 adds

- Exact `ChunkSource::createNewChunk` interception.
- Optional exact `ChunkSource::getOrLoadChunk` interception for diagnostics and controlled processing; it is disabled by default to avoid reprocessing ordinary chunk loads.
- Per-dimension native request queues.
- Request deduplication and bounded backpressure.
- Primary-thread detached chunk capture.
- Worker-thread custom generator/populator pipeline.
- Ordered neighborhood locking.
- Primary-thread changed-block commit and BDS thread-batch flush.
- Native Endstone service name: `endstone:worldgen`.
- Live stats for intercepted, dispatched, committed, retried and dropped work.

Other native plugins register worker-safe `IPopulator` implementations through Endstone's `ServiceManager`. See `examples/cpp/parallel_ore_populator.cpp`.


## Automatic GitHub builds

Every push and pull request builds and tests the portable core, then compiles exact native packages for Windows x64 and Linux x64 against both supported BDS builds. The completed `.dll`, `.so`, ZIP package, and per-package SHA-256 file are available from the workflow run's **Artifacts** section for 30 days.

Pushing the exact tag `v0.4.5-alpha.9` also creates or updates a GitHub Release and attaches all four platform/BDS packages plus combined checksums. See `docs/GITHUB_RELEASES.md`.

## Thread ownership

The hook intercepts actual BDS chunk requests. It does not pass live `LevelChunk`, `BlockSource`, player, actor or Endstone objects to workers. Workers receive only detached `ChunkBuffer` data; commits return through the owning BDS/Endstone thread.

## Important boundary

This release makes the worker system functional by connecting it to real chunk requests. It accelerates registered custom generation and post-processing. It does **not** call Mojang's opaque vanilla base-terrain generator from foreign threads, because that would require verified internal generator-stage symbols and ownership rules beyond the published 26.30 headers.

See `docs/BDS_26_30_ADAPTER.md` and `docs/VANILLA_ACCELERATION.md`.
