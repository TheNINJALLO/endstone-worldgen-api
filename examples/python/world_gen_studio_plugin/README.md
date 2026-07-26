# Endstone WorldGen Studio Plugin

An Endstone 0.11 Python wheel that verifies the live `endstone:worldgen`
service and runs clearly labeled detached reference-buffer tests.

## Installation

Install the native WorldGen package matching the server's exact operating
system and BDS build first. Its `_endstone_worldgen_live` module must be on the
Endstone Python path, and the Endstone host must run **CPython 3.14** to match
the native bridge ABI. Then copy this wheel into the server's `plugins/` folder:

```text
endstone_worldgen_studio-0.4.5b30-py3-none-any.whl
```

Endstone discovers the `worldgen-studio` entry point at startup. All commands
require operator status or the `wg.admin` permission.

## Commands

- `/wg`: show help.
- `/wg status`: display live interceptor, queue, and native adapter counters.
- `/wg gen <flat|island|maze|ores> [cx cz]`: run one detached Python reference generator.
- `/wg structure <castle|arena> [cx cz]`: run a detached 3x3 reference-buffer test.
- `/wg benchmark [chunk_count]`: benchmark the detached Python scheduler (maximum 128).
- `/wg inspect [cx cz]`: inspect a reference buffer alongside native counters.

The detached commands do **not** commit blocks to the live world. They run only
after the native service reports active interception, and state that boundary
in-game instead of presenting reference-Python work as native success.
