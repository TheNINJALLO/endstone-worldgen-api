# Endstone WorldGen Studio Plugin

An Endstone Python wheel plugin for testing custom chunk generators, multi-threaded generation schedulers, and neighborhood chunk boundary locks in Minecraft Bedrock Edition.

## Features
- `/wg gen <flat|island|maze|ores> [cx] [cz]`: Custom terrain chunk generator
- `/wg structure <castle|arena> [cx] [cz]`: 3x3 multi-chunk neighborhood lock test
- `/wg benchmark [chunk_count]`: Parallel multi-threaded generation stress test
- `/wg inspect [cx] [cz]`: Query chunk buffer fingerprint, heightmap & layers

## Installation
```bash
pip install endstone_worldgen_studio-0.4.5a9-py3-none-any.whl
```
