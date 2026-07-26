# In-game command and studio suite

The release includes the `endstone_worldgen_studio` test-plugin wheel.

## Install

1. Install the exact native WorldGen bundle for the server's BDS build and platform.
2. Ensure the bundle's `_endstone_worldgen_live` module is importable by Endstone.
3. Copy `endstone_worldgen_studio-0.4.5b30-py3-none-any.whl` to `plugins/`.
4. Restart Endstone and run `/wg status`.

The wheel registers Endstone entry point `worldgen-studio`, command `/wg`, and
permission `wg.admin` with operator default.

## Commands

### `/wg status`

Displays native interception state, populator count, queue state, and adapter counters.

### `/wg gen <flat|island|maze|ores> [cx cz]`

Runs a detached Python reference generator and reports its fingerprint.

### `/wg structure <castle|arena> [cx cz]`

Runs a detached 3x3 reference-buffer test.

### `/wg benchmark [chunk_count]`

Benchmarks up to 128 detached reference buffers.

### `/wg inspect [cx cz]`

Displays reference-buffer bounds, block probes, fingerprint, and native counters.

The generation, structure, benchmark, and buffer-inspection commands do not
commit blocks to the live world. They run only after the native bridge confirms
that `endstone:worldgen` is active, and explicitly label their detached scope.
