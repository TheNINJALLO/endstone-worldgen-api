# Architecture

The scheduler runs only detached data on workers.

1. The main thread captures or creates a detached `ChunkBuffer`, with a default budget of one capture per tick.
2. A deterministic stage seed is calculated from world seed, dimension, chunk and stage.
3. Generator/populator work runs in the worker pool.
4. Cross-chunk populators acquire ordered neighborhood locks.
5. A result is queued for a main-thread commit, also limited to one per tick by default. The adapter rejects detached biome edits and preflights every used block palette descriptor before its first live block mutation.

Live `LevelChunk`, `Dimension`, `BlockSource`, players, actors and Endstone API objects must never enter a worker.
Capture and commit therefore still consume primary-thread time; increasing either budget trades tick latency for throughput.
The interceptor waiting queue defaults to 256 entries. Requests beyond that cap
are dropped and counted in `waiting_overflow_drops` instead of growing memory
without a bound.
