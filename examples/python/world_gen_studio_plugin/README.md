# Endstone WorldGen Studio Plugin

An Endstone 0.11 Python wheel that exercises the live `endstone:worldgen:v2`
capture/commit service and clearly labeled detached reference-buffer tests.

## Installation

Install the native WorldGen package matching the server's exact operating
system and BDS build, then copy the matching platform wheel into the server's
`plugins/` folder. The wheel bundles `_endstone_worldgen_live`; no `PYTHONPATH`
or manual `site-packages` copy is required. Endstone must run **CPython 3.14**.

```text
endstone_worldgen_studio-0.4.5b32-cp314-cp314-linux_x86_64.whl
endstone_worldgen_studio-0.4.5b32-cp314-cp314-win_amd64.whl
```

Endstone discovers the `worldgen-studio` entry point at startup. All commands
require operator status or the `wg.admin` permission.

## Commands

- `/wg`: show help.
- `/wg status`: display live-recipe, interceptor, queue, failure, and native adapter counters.
- `/wg gen <flat|island|maze|ores> [cx cz]`: apply one bounded live recipe.
- `/wg structure <castle|arena> [cx cz]`: apply a bounded live recipe across a 3x3 chunk area.
- `/wg buffer <flat|island|maze|ores> [cx cz]`: run one detached Python reference generator.
- `/wg benchmark [chunk_count]`: benchmark the detached Python scheduler (maximum 128).
- `/wg inspect [cx cz]`: inspect a detached reference buffer.

`gen` and `structure` capture every target chunk on the primary thread, resolve
the recipe's namespaced `BlockData` descriptors against the exact server, alter
only the recipe's selected block cells, and leave biomes untouched. Success is
reported only after every changed chunk passes native commit and flush. Use a
world backup and disposable coordinates: these two commands intentionally edit
the live world.

`flat`, `island`, `maze`, `castle`, and `arena` use
`floor(sender.location.y) - 1` as their visible surface/floor anchor, even when
chunk X/Z are supplied explicitly. The start message shows the bounded recipe Y
band, and every native result reports the actual changed Y range (or explicitly
states that no write range exists). A recipe that does not fit the captured
vertical bounds fails before commit. `ores` ignores the visible anchor and only
replaces natural stone/deepslate found while scanning the captured chunk, so its
reported changes may be underground.

`buffer`, `benchmark`, and `inspect` are detached. They do **not** capture,
change, or commit live chunks. Commands without explicit coordinates require an
in-game sender; console and malformed/partial coordinate defaults are rejected.
Non-finite, non-numeric, and out-of-native-range sender Y anchors are also
rejected before the bridge is called.

ChunkSource interception and manual live recipes have separate readiness. Zero
registered `IPopulator` objects disables useful automatic intercepted work, but
does not disable the manual built-in recipe path. `/wg status` reports both.
