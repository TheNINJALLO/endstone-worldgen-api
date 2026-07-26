# In-game command and studio suite

The release includes the `endstone_worldgen_studio` test-plugin wheel.

## Install

1. Stop the server and remove every older WorldGen Studio wheel from `plugins/`.
2. Remove any manually copied top-level `_endstone_worldgen_live` file from `plugins/.local`.
3. Extract the BDS 1.26.33 ZIP matching the server platform and copy both files from its `plugins/` directory into the server's `plugins/` directory.
4. Restart Endstone and run `/wg status`.

The wheel registers Endstone entry point `worldgen-studio`, command `/wg`, and
permission `wg.admin` with operator default.

## Commands

### `/wg status`

Displays manual live-recipe totals, native interception state, populator count,
capture/commit/empty-pipeline failures, queue state, and exact-adapter diagnostics.
An inactive hook or zero populators degrades automatic interception but does not
disable the manual recipe capture/commit path.

### `/wg gen <flat|island|maze|ores> [cx cz]`

Captures the target live chunk and applies a bounded built-in recipe. BlockData
descriptors are resolved by the exact server, untouched cells are preserved,
and biomes are not edited. The green success message is sent only after native
commit and flush both succeed, and includes the confirmed changed-block count
and exact changed Y range. `flat`, `island`, and `maze` use the block below the
sender (`floor(sender Y) - 1`) as their visible surface/floor. `ores` instead
scans natural stone/deepslate throughout the captured chunk and may write
underground; its actual changed Y range is reported after the scan.

### `/wg structure <castle|arena> [cx cz]`

Applies a bounded live castle or arena recipe across the 3x3 area centered on
the target chunk. All chunks are captured and validated before the first write.
The castle or arena floor is anchored at `floor(sender Y) - 1`, and the command
shows both the bounded recipe band and actual native changed Y range.

### `/wg buffer <flat|island|maze|ores> [cx cz]`

Runs a detached Python reference generator and reports its fingerprint. It does
not inspect or edit the live world.

### `/wg benchmark [chunk_count]`

Benchmarks up to 128 detached reference buffers.

### `/wg inspect [cx cz]`

Displays detached reference-buffer bounds, block probes, and fingerprint.

`gen` and `structure` intentionally edit live blocks; use a backup and disposable
test area. `buffer`, `benchmark`, and `inspect` remain detached and explicitly
label that scope. Commands without explicit coordinates require an in-game
sender and use mathematical floor division for negative player coordinates.
Malformed, partial, out-of-range, non-finite-Y, and console-default targets fail
closed. If the anchor does not leave enough room inside any captured chunk's
vertical bounds, no commit is attempted.
