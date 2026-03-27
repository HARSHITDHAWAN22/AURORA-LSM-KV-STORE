AuroraKV
A Persistent, Write-Optimized Key-Value Storage Engine in C++17
![Language](https://en.cppreference.com/w/cpp/17)
[![Build](https://img.shields.io/badge/Build-g%2B%2B-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)]()
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)]()
AuroraKV is a fully functional, persistent key-value storage engine built from scratch in C++17, modeled after the internal architecture of production systems like LevelDB and RocksDB. It implements Write-Ahead Logging, MemTable buffering, immutable SSTables, Bloom filters, a three-layer cache hierarchy, and a dual-strategy compaction engine — all benchmarked under real workloads at scale.
---
Table of Contents
Motivation
What This Project Demonstrates
Architecture
Core Components
Operation Flows
Compaction Strategies
Benchmark Results
Project Structure
Build and Run
Configuration
Running Benchmarks
Observability
Key Learnings
Roadmap
References
Author
---
Motivation
Most projects use databases. This one builds one.
AuroraKV was built to answer questions that cannot be answered by reading documentation:
Why do write-heavy systems use LSM-trees instead of B-trees?
How does a storage engine guarantee durability without sacrificing write speed?
When do Bloom filters actually matter, and by how much?
How do Leveling and Tiering behave differently as dataset size grows — and why does the answer change at scale?
What does read amplification actually look like in practice?
The project is not a CRUD wrapper. It is a ground-up implementation of the subsystems that make modern key-value stores fast, durable, and tunable — built to the point where those questions have measurable, benchmarked answers.
---
What This Project Demonstrates
Systems Design
LSM-tree architecture with multi-level SSTable organization
Write-Ahead Logging for crash durability and WAL replay on recovery
Tombstone-based logical deletes consistent with how production LSM engines handle removal
Background flush and compaction threads decoupled from the write path
Performance Engineering
Bloom filters that eliminate 90%+ of unnecessary SSTable reads at scale
Three-layer cache hierarchy (BlockCache, LRUCache, TableCache) absorbing the hot read path
Sequential disk writes via MemTable buffering, avoiding random I/O entirely
Switchable compaction strategies with measured throughput results across workload scales
Software Engineering
Clean component boundaries — each class owns exactly one responsibility
JSON-driven configuration with a dedicated ConfigManager
JSON-backed manifest enabling full state recovery without data file re-scanning
Structured logging and persistent runtime statistics across sessions
---
Architecture
AuroraKV is structured as a layered pipeline. Writes flow down through the WAL, MemTable, and flush pipeline. Reads flow through the cache hierarchy before touching disk. A background compaction engine continuously maintains the SSTable layout.
```
+----------------------------------------------------+
|                   Client / Shell                   |
|          put()  get()  delete()  scan()            |
+----------------------------------------------------+
|                   KVStore (API)                    |
|       Routes operations, manages threads,          |
|       accumulates stats, coordinates lifecycle     |
+----------------------+-----------------------------+
|      Write Path      |        Read Path            |
|                      |                             |
|  WAL.logPut()        |  BlockCache                 |
|       |              |       |                     |
|  MemTable.put()      |  LRUCache                   |
|       |              |       |                     |
|  \[if full: flush]    |  TableCache                 |
|       |              |       |                     |
|  SSTable written     |  MemTable                   |
|  Bloom filter built  |       |                     |
|  Manifest updated    |  SSTable + Bloom filter     |
|       |              |                             |
|  \[if threshold met]  |                             |
|  Background compact  |                             |
+----------------------+-----------------------------+
|      ManifestManager     (Persistence / Recovery)  |
|      ConfigManager       (Runtime Settings)        |
|      Logger              (Diagnostics)             |
+----------------------------------------------------+
```
---
Core Components
KVStore
The central coordinator and public API surface. Routes all `put`, `get`, `delete`, and `scan` calls, manages two background threads (flush and compaction), accumulates the full `KVStats` structure, and coordinates the component lifecycle from startup to clean shutdown.
MemTable
An `std::map`-backed in-memory write buffer. Stores entries in key-sorted order and records both values and tombstone markers. When the configured entry limit is reached (default: 50 entries), the MemTable is atomically flushed to a new SSTable on disk and replaced with an empty one.
SSTable
Write-once, read-many disk files storing key-value records in sorted order. Each file carries:
A magic number header (`0x4155524F52414B56`) for file integrity validation
An embedded Bloom filter for O(1) probabilistic membership tests
A sparse index mapping key ranges to byte offsets for efficient binary seeks
Support for both value entries and tombstone markers
Files are never modified after creation. All updates produce new files; old ones are cleaned up by compaction.
Write-Ahead Log (WAL)
All `put` and `delete` mutations are appended to a binary WAL before touching the MemTable. On startup, the WAL is replayed to restore any operations that were in-flight at the time of an unclean shutdown. Writes are batched and buffered (configurable batch size) to minimize I/O overhead while maintaining full durability guarantees.
Bloom Filter
Each SSTable carries a 10,000-bit Bloom filter using 3 hash functions. Provides O(1) probabilistic membership testing on the read path: if the filter definitively reports a key absent, the entire SSTable file is skipped without any disk I/O. At 10K ops scale, this results in a 0% false positive rate. At 100K ops, fewer than 25 false positives were observed across the entire run — meaning the vast majority of file skips are correct.
Cache Hierarchy
Three cooperating layers absorb the hot read path before any disk access occurs:
BlockCache — caches raw disk blocks; eliminates repeated page reads for the same byte ranges
LRUCache — caches deserialized key-value pairs with least-recently-used eviction
TableCache — caches open SSTable file descriptors; eliminates repeated `open()` syscalls for active files
At small scale (10K ops), this hierarchy achieves a cache hit rate above 99%, meaning nearly all reads are served from memory without touching disk.
Compaction Engine
Merges SSTables, removes stale versions of overwritten keys, and permanently eliminates tombstoned entries. Runs on a dedicated background thread at a configurable interval. Supports two strategies — see Compaction Strategies.
ManifestManager
Persists the engine's structural metadata — SSTable file paths, level assignments, key ranges, and file sizes — in a JSON-backed manifest file. On startup, the manifest is read to fully reconstruct the SSTable layout without scanning data files.
ConfigManager
Loads all engine parameters from `config/system\_config.json` at startup. All tuning is done through this file — no recompilation required.
---
Operation Flows
Write (PUT)
```
put(key, value)
      |
      v
  WAL.logPut()              -- persisted to disk before MemTable is touched
      |
      v
  MemTable.put()            -- O(log n) insert into sorted map
      |
  \[MemTable entry count >= max\_entries]
      v
  flushMemTable()
      |-- write SSTable to disk (sequential I/O)
      |-- build and embed Bloom filter
      |-- update ManifestManager
      |-- reset MemTable
      |
  \[level file count >= compaction threshold]
      v
  backgroundCompaction()    -- runs on dedicated thread, non-blocking to writer
```
Read (GET)
```
get(key)
      |
      v
  BlockCache.get()          -- hit? return in microseconds
      | miss
      v
  LRUCache.get()            -- hit? return from memory
      | miss
      v
  MemTable.get()            -- check latest in-memory state first
      | not found
      v
  For each SSTable (newest to oldest):
      |
      +-- BloomFilter.check()    -- negative? skip entire file, zero disk I/O
      +-- sparseIndex lookup     -- seek to approximate byte offset
      +-- binary record scan     -- found? write to cache and return value
      +-- tombstone found?       -- key is deleted, return NOT FOUND
```
Delete (DELETE)
```
delete(key)
      |
      v
  WAL.logDelete()
      |
      v
  MemTable.insertTombstone()    -- logical delete, O(log n), no disk I/O
      |
  \[on flush / compaction]
      v
  Tombstone propagates to SSTable, eliminates all prior versions during compaction
```
---
Compaction Strategies
Leveling
SSTables are merged into discrete levels with strictly bounded file counts per level. When a level overflows its threshold, files are merged with the next level, maintaining sorted, non-overlapping key ranges.
Lower read amplification — fewer SSTables to check per read
Higher write amplification — files are rewritten more frequently
Best suited for read-heavy workloads and smaller datasets
Tiering
SSTables accumulate in groups (tiers) and are compacted less aggressively. Files within a tier may have overlapping key ranges, but compaction is deferred until a tier is full.
Lower write amplification — less frequent rewriting
Higher read amplification — more files to consult per read
Best suited for write-heavy ingestion and large-scale workloads
The strategy is switchable at runtime without modifying the codebase.
---
Benchmark Results
Benchmarks were executed across three workload scales using a configurable workload generator, with each strategy run independently from a clean state.
Mixed Workload Throughput (ops/sec)
Scale	Leveling	Tiering	Observation
10K ops	5,827	1,243	Leveling dominates; compaction overhead minimal
50K ops	5,092	5,553	Tiering begins to overtake as dataset grows
100K ops	1,188	3,794	Tiering outperforms by 3.2x at large scale
![Throughput Comparison](assets/throughput_comparison.png)
---
Cache Hit Rate & Bloom Filter Efficiency
Scale	Leveling Cache Hit	Tiering Cache Hit	Bloom False Positives (Leveling / Tiering)
10K ops	99.84%	99.93%	0 / 0
50K ops	83.81%	84.08%	0 / 4
100K ops	62.00%	61.88%	22 / 10
![Cache and Bloom Analysis](assets/cache_bloom_analysis.png)
---
PUT vs GET Throughput Breakdown
![PUT vs GET Breakdown](assets/put_get_breakdown.png)
---
Analysis
Scale changes everything. Leveling's advantage at 10K ops — where aggressive compaction keeps SSTable count low — becomes a liability at 100K ops, where the compaction overhead itself costs throughput. Tiering defers that cost and scales write ingestion far more gracefully, outperforming Leveling by 3.2x at 100K ops.
Bloom filters matter most when the cache no longer fits. At 10K ops, cache hit rates above 99% mean Bloom filters are rarely exercised. At 100K ops, where cache hit rate drops to ~62%, Bloom filters are actively eliminating the majority of SSTable reads. Without them, every cache miss would require scanning multiple files on disk.
Cache hit rate follows a predictable decay. As the working set outgrows the cache capacity, hit rate drops across both strategies. Both converge to ~62% at 100K ops, confirming that cache sizing should be planned relative to expected working set size, not total dataset size.
---
Project Structure
```
Aurora-lsm-kv-store/
|
+-- src/
|   +-- main.cpp                  CLI shell and entry point
|   +-- KVStore.cpp               Core engine — routing, threads, stats
|   +-- MemTable.cpp              In-memory sorted write buffer
|   +-- SSTable.cpp               On-disk sorted file — write, read, index
|   +-- SSTableBuilder.cpp        Incremental SSTable construction
|   +-- SSTableIterator.cpp       Sequential SSTable traversal
|   +-- BloomFilter.cpp           Probabilistic membership filter
|   +-- Compaction.cpp            Leveling and tiering compaction logic
|   +-- WAL.cpp                   Write-Ahead Log with binary encoding
|   +-- ManifestManager.cpp       Metadata persistence and recovery
|   +-- ConfigManager.cpp         JSON configuration loader
|   +-- LRUCache.cpp              LRU eviction cache
|   +-- MergeIterator.cpp         Multi-SSTable sorted merge cursor
|   +-- RangeIterator.cpp         Range scan support
|   +-- Logger.cpp                Structured logging
|
+-- include/                      Header files for all components
|
+-- benchmark/
|   +-- benchmark\_main.cpp        Benchmark runner and entry point
|   +-- Workload.cpp              Configurable workload generator
|   +-- Config.h                  Benchmark parameters
|
+-- benchmark\_results/
|   +-- raw/                      Per-run raw timing measurements
|   +-- summary/                  Aggregated results and analysis notes
|
+-- assets/                       Benchmark charts (embedded in README)
|   +-- throughput\_comparison.png
|   +-- cache\_bloom\_analysis.png
|   +-- put\_get\_breakdown.png
|
+-- config/
|   +-- system\_config.json        Engine configuration (all tuning here)
|
+-- data/sstables/                SSTable .dat files (generated at runtime)
+-- metadata/                     WAL log, manifest, strategy, stats files
+-- Makefile
+-- DESIGN.MD
```
---
Build and Run
Requirements
`g++` with C++17 support
`make`
Windows (MinGW/MSYS2) or Linux
Build the Engine
```bash
git clone https://github.com/your-username/Aurora-lsm-kv-store.git
cd Aurora-lsm-kv-store
make
```
Binary produced: `aurorakv` (Linux) or `aurorakv.exe` (Windows).
Launch the Interactive Shell
```bash
./aurorakv shell
```
```
AuroraKV Shell Mode
Type 'exit' to quit
>>
```
Shell Commands
Command	Description	Example
`put <key> <value>`	Insert or update a key	`put name harshit`
`get <key>`	Retrieve a value by key	`get name`
`delete <key>`	Logically delete a key	`delete name`
`scan <start> <end>`	Range scan over a key interval	`scan a z`
`flush`	Force flush MemTable to disk	`flush`
`compact`	Trigger manual compaction	`compact`
`stats`	Print runtime statistics	`stats`
`strategy <n>`	Switch compaction strategy	`strategy tiering`
`exit`	Exit the shell	`exit`
Start with a Specific Compaction Strategy
```bash
./aurorakv start leveling
./aurorakv start tiering
```
---
Configuration
All parameters are set in `config/system\_config.json`. No recompilation needed.
```json
{
  "storage": {
    "memtable": { "max\_entries": 50 },
    "sstable":  { "data\_directory": "data/sstables" }
  },
  "bloom\_filter": {
    "bit\_size": 10000,
    "hash\_functions": 3
  },
  "compaction": {
    "strategy": "leveling",
    "max\_files\_per\_level": 4,
    "interval\_seconds": 5,
    "l0\_threshold": 4
  },
  "flush": { "interval\_seconds": 2 }
}
```
Parameter	Effect
`memtable.max\_entries`	Entries before MemTable flushes to SSTable
`bloom\_filter.bit\_size`	Larger = fewer false positives, more memory per SSTable
`bloom\_filter.hash\_functions`	3 is optimal for this bit size
`compaction.strategy`	`leveling` or `tiering`
`compaction.max\_files\_per\_level`	File count threshold that triggers level compaction
`compaction.interval\_seconds`	How often the background compaction thread checks
`flush.interval\_seconds`	How often the background flush thread checks
---
Running Benchmarks
```bash
g++ -std=c++17 -Iinclude -Ibenchmark \\
    benchmark/benchmark\_main.cpp benchmark/Workload.cpp \\
    src/KVStore.cpp src/MemTable.cpp src/WAL.cpp \\
    src/SSTable.cpp src/SSTableBuilder.cpp src/SSTableIterator.cpp \\
    src/BloomFilter.cpp src/Compaction.cpp src/ManifestManager.cpp \\
    src/LRUCache.cpp src/ConfigManager.cpp src/Logger.cpp \\
    src/MergeIterator.cpp src/RangeIterator.cpp \\
    -pthread -o benchmark\_runner

./benchmark\_runner
```
Configure workload in `benchmark/benchmark\_main.cpp`:
```cpp
config.total\_ops    = 100000;
config.workload     = WorkloadType::WRITE\_HEAVY;  // WRITE\_HEAVY | READ\_HEAVY | MIXED
config.key\_dist     = KeyDistribution::RANDOM;
config.put\_ratio    = 0.80;
config.get\_ratio    = 0.15;
config.delete\_ratio = 0.05;

KVStore db("config/system\_config.json", "tiering");
```
Results are written to `benchmark\_results/raw/` and `benchmark\_results/summary/`.
---
Observability
Runtime statistics are accessible via the `stats` shell command and persisted to `metadata/stats.dat` across sessions:
```
Total PUTs          : 50000
Total GETs          : 45000
Total Flushes       : 1000
Total Compactions   : 48
Total Bytes Written : 52428800

Bloom Checks        : 120000
Bloom Negatives     : 108000     (90.0% skip rate)
Bloom False Pos     : 4

Cache Hits          : 41850
Cache Misses        : 3150
Cache Hit Rate      : 93.0%
```
---
Key Learnings
Building AuroraKV produced concrete answers to questions that are easy to hand-wave in theory:
On write design: Buffering writes in memory and flushing sequentially is not just faster — it transforms the I/O pattern from random to sequential, which is the single biggest factor in write throughput on both HDDs and SSDs.
On Bloom filters: Their value is invisible at small scale (where the cache handles everything) and becomes critical at large scale (where cache hit rate decays and every miss risks a disk read). Implementing them from scratch made the false positive / false negative trade-off tangible rather than theoretical.
On compaction strategies: Leveling and Tiering are not universally better or worse — they have a crossover point that depends on dataset size and workload mix. Observing Tiering outperform Leveling by 3.2x at 100K ops, after underperforming at 10K ops, made this trade-off concrete in a way no paper could.
On cache behavior: Cache hit rate decays predictably as working set size grows relative to cache capacity. This makes cache sizing a dataset-relative decision, not an absolute one.
On system design: Clean component boundaries are not just aesthetic — they make it possible to swap compaction strategies, tune configuration, and reason about performance without touching unrelated code.
---
Roadmap
[ ] Block-level compression (Snappy or LZ4) for reduced SSTable footprint
[ ] Concurrent read access with reader-writer lock separation
[ ] CRC32 checksums per SSTable block for corruption detection
[ ] TTL-based automatic key expiration
[ ] Compaction rate limiting to prevent write stalls under heavy load
[ ] Point-in-time snapshot reads
[ ] TCP server interface for network-accessible operation
[ ] Metrics visualization dashboard
---
References
LevelDB — Google's LSM-tree storage engine; primary design reference
RocksDB — Meta's production LSM engine; tiering and cache hierarchy inspiration
The Log-Structured Merge-Tree (LSM-Tree) — O'Neil et al., 1996
Bigtable: A Distributed Storage System for Structured Data — Chang et al., Google, 2006
---
Author
Harshit Dhawan
B.Tech CSE, Delhi Technological University (DTU)
AuroraKV was built to go beyond using databases and understand how they actually work — from the write path to the cache layer to the compaction engine. Every component was implemented, benchmarked, and analyzed to produce answers, not just code.
---
Licensed under the MIT License.