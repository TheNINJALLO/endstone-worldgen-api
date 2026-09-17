# Exact Bedrock 26.30 WorldGen interception adapter

| BDS build | Endstone tag | Runtime result |
|---|---|---|
| 1.26.51 | v0.11.11 | accepted |
| 1.26.51 with any other Endstone version | mismatch | refused |
| anything else | none | refused |

## Native hook path

The adapter installs exact ABI-pinned detours on `ChunkSource::createNewChunk` and `ChunkSource::getOrLoadChunk`. Windows and Linux use separate vtable ordinals because their destructor layouts differ.

After BDS completes the original chunk request, the detour records the dimension, chunk coordinates, request type, read-only state and sequence number. The main-thread pump then:

1. Drains the bounded native queue.
2. Deduplicates repeated chunk requests.
3. Captures a palette-preserving detached `ChunkBuffer` on the primary thread (one per pump by default).
4. Sends registered populators to the worker pool.
5. Waits without blocking the server tick.
6. Skips unchanged results; otherwise recaptures the loaded chunk with bounded retries, rejects changed-cell conflicts, and merges only the populator delta into the fresh snapshot.
7. Rejects biome edits, resolves and verifies every used palette descriptor, then commits changed blocks on the primary thread (one per pump by default).
8. Calls `ChunkSource::flushThreadBatch()`.

## Service access

Native clients compiled for service ABI 2 load `endstone:worldgen:v2` through Endstone's `ServiceManager` and register one or more worker-safe `IPopulator` instances. The versioned name makes mixed bridge/native releases fail lookup safely. A pipeline with no registered populators intentionally performs no world edits; this is the expected state when the API enables before its dependent consumers.

## Safety

- Live BDS pointers never enter worker jobs.
- The native hook queue and interceptor waiting queue are bounded. Waiting
  overflow is reported through `waiting_overflow_drops`.
- In-flight requests are bounded and deduplicated.
- Captures retry when the requested chunk is not ready yet.
- Capture and commit require exact `ChunkState::Loaded`; invalid, generating, decoration, and lighting states are refused.
- Changed cells containing block actors are refused because detached buffers do not preserve their NBT.
- Initial captures and commits have separate per-tick budgets. A changed worker result performs one additional primary-thread recapture immediately before its conflict-checked delta merge; raising either budget may increase tick latency.
- Exact runtime gating rejects unsupported builds.
- The adapter restores its exact vtable entries when the final interceptor instance is disabled.

## Build

```bash
./scripts/build_exact.sh 1.26.51
```
