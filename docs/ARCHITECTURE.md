# Architecture

The scheduler runs only detached data on workers.

1. Main thread captures or creates a detached `ChunkBuffer`.
2. A deterministic stage seed is calculated from world seed, dimension, chunk and stage.
3. Generator/populator work runs in the worker pool.
4. Cross-chunk populators acquire ordered neighborhood locks.
5. A result is queued for a bounded main-thread commit.

Live `LevelChunk`, `Dimension`, `BlockSource`, players, actors and Endstone API objects must never enter a worker.
