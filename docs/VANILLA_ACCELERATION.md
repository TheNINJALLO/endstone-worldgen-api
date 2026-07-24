# Vanilla and custom generation acceleration

## Active in v0.4.5-alpha.9

`PostProcessing` is connected to real BDS chunk requests. Registered custom populators run in parallel over detached chunks and return through a primary-thread commit gate.

## Not yet claimed

`Hybrid` and `NativeExperimental` remain disabled for Mojang's opaque base-terrain stages. Moving the original BDS generator call to a foreign worker would also move hidden lighting, structure, cache, storage and neighbor ownership. That is not safe without exact internal stage symbols and live corruption testing.

## Practical speedups available now

- custom caves and carvers
- custom structures
- ores and feature placement
- vegetation and decoration
- chunk analysis
- pregeneration post-processing
- expensive calculations used to produce chunk patches

The hook unit makes those jobs automatic when BDS requests new chunks. It is no longer a disconnected worker pool.
