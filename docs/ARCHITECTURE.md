# Architecture

The scheduler runs only detached data on workers.

1. The main thread captures or creates a detached `ChunkBuffer`, with a default budget of one capture per tick.
2. A deterministic stage seed is calculated from world seed, dimension, chunk and stage.
3. Generator/populator work runs in the worker pool.
4. Cross-chunk populators acquire ordered neighborhood locks.
5. A result is queued for a main-thread commit, also limited to one per tick by default. The adapter rejects detached biome edits and preflights every used block palette descriptor before its first live block mutation.

Automatic worker results never full-commit their original snapshot. Unchanged
results skip the commit and flush path. Changed results recapture the live chunk
on the primary thread with bounded retries, require every changed block runtime
ID and supported biome cell to still match its captured value, merge only those
changes into the fresh snapshot, and abort before writing if any target
conflicts. Unrelated edits made while a worker was running are therefore
preserved. The exact adapter rejects changed cells containing block actors
because `ChunkBuffer` does not carry their NBT.

Live `LevelChunk`, `Dimension`, `BlockSource`, players, actors and Endstone API objects must never enter a worker.
Capture and commit therefore still consume primary-thread time; increasing either budget trades tick latency for throughput.
The interceptor waiting queue defaults to 256 entries. Requests beyond that cap
are dropped and counted in `waiting_overflow_drops` instead of growing memory
without a bound.

Manual built-in recipes use a separate synchronous primary-thread service path:
resolve exact server BlockData descriptors, capture every target chunk, mutate a
bounded set of block cells in those detached copies, then commit and flush each
changed chunk before returning. They never edit biome cells. This path remains
available when the optional ChunkSource interception hook is inactive; automatic
intercepted population still requires both an active hook and at least one
registered `IPopulator`. Visible recipes carry the command sender's
`floor(Y) - 1` anchor through the ABI and reject a target unless their entire
vertical band fits every captured chunk. Ore generation deliberately ignores
that visible anchor and scans only natural stone/deepslate. Results carry a
nullable union of the actual confirmed or flush-unconfirmed changed Y range.
The exact adapter requires `ChunkState::Loaded`, rejects changed block-actor
cells, and reads every changed runtime ID back through Endstone's public virtual
BlockData boundary before the recipe can report a successful commit.
