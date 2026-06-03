# WorldIndex

`WorldIndex` is the indexed query view over entity IDs, owner buckets, tile buckets, occupancy layers, and resource tiles. It is passed through `GameContext` so command and AI paths can avoid repeated whole-entity scans.

## Allowed dependencies

Command dispatch, AI execution, placement checks, and tests may build or pass `WorldIndex`. Query helpers may provide compatibility wrappers while older code migrates.

## Forbidden dependencies

Do not keep a `WorldIndex` across mutations without rebuilding it. Do not add new hot-path scans for entity lookup, owner counts, tile lookup, or occupancy when an index-backed query exists.
