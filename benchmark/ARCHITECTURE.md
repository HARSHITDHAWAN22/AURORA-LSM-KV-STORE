# ARCHITECTURE.md

# AuroraKV Architecture

AuroraKV is a **Log-Structured Merge Tree (LSM-tree) based key-value store** built in C++ to explore the internal design of modern write-optimized storage engines.

The system is designed around the same core principles used in engines such as **LevelDB**, **RocksDB**, and other **LSM-based storage systems**:

- **Append-first durability**
- **In-memory write buffering**
- **Immutable sorted disk files**
- **Read path pruning**
- **Background compaction**

Its architecture prioritizes:

- **High write throughput**
- **Efficient point lookups**
- **Crash recoverability**
- **Scalable on-disk organization**
- **Performance-aware compaction**

---

# 1. Architectural Goals

AuroraKV was designed with the following goals:

## 1.1 Fast Writes
Avoid random disk updates by converting writes into **sequential append operations**.

## 1.2 Durable Storage
Ensure writes are recoverable across crashes using a **Write-Ahead Log (WAL)**.

## 1.3 Efficient Reads
Reduce lookup cost using:

- **MemTable-first search**
- **Bloom Filters**
- **LRU Cache**
- **Sorted SSTables**

## 1.4 Scalable Growth
Allow the engine to continue performing as data volume grows using:

- **Flush**
- **Compaction**
- **Level organization**

## 1.5 Real Storage-Engine Behavior
Model the trade-offs found in production systems:

- Read amplification
- Write amplification
- Space amplification
- Compaction overhead

---

# 2. High-Level System View

AuroraKV is organized into four major layers:

## 2.1 Request Processing Layer
Handles external operations:

- `PUT(key, value)`
- `GET(key)`
- `DELETE(key)`

## 2.2 In-Memory Data Layer
Responsible for low-latency access and write buffering:

- **MemTable**
- **LRU Cache**

## 2.3 Persistent Storage Layer
Responsible for durable and scalable on-disk storage:

- **Write-Ahead Log (WAL)**
- **SSTables**
- **Manifest**

## 2.4 Background Maintenance Layer
Responsible for keeping the system efficient over time:

- **Flush Engine**
- **Compaction Engine**
- **Recovery Logic**

---

# 3. High-Level Architecture Diagram

```text
                          ┌──────────────────────────┐
                          │       Client API         │
                          │   PUT / GET / DELETE     │
                          └─────────────┬────────────┘
                                        │
                    ┌───────────────────┼───────────────────┐
                    │                   │                   │
                    ▼                   ▼                   ▼
             ┌────────────┐      ┌────────────┐      ┌────────────┐
             │    WAL     │      │  MemTable  │      │ LRU Cache  │
             └─────┬──────┘      └─────┬──────┘      └─────┬──────┘
                   │                   │                   │
                   │            (flush when full)          │
                   │                   ▼                   │
                   │          ┌──────────────────┐         │
                   │          │     SSTables     │◄────────┘
                   │          └────────┬─────────┘
                   │                   │
                   ▼                   ▼
            ┌────────────┐      ┌──────────────┐
            │ Manifest   │      │ Bloom Filter │
            └────────────┘      └──────┬───────┘
                                        │
                                        ▼
                             ┌──────────────────────┐
                             │  Compaction Engine   │
                             │ Leveling / Tiering   │
                             └──────────────────────┘
```

---

# 4. Core Data Structures and Components

## 4.1 Write-Ahead Log (WAL)

The WAL is the **first durability boundary** in the system.

Before any update is considered accepted, it is appended to the WAL.

### Responsibilities

- Persist every `PUT` and `DELETE`
- Preserve operation ordering
- Support crash recovery through replay
- Minimize durability overhead through append-only writes

### Why it matters

Without the WAL, data in the MemTable would be lost on crash before flush.

---

## 4.2 MemTable

The MemTable is the **active in-memory write buffer** and the primary destination for new writes.

It stores the latest version of recently written keys.

### Responsibilities

- Accept new writes with low latency
- Store the latest state of keys before disk flush
- Serve recent reads before disk access
- Hold tombstones for deleted keys

### Design Role

The MemTable allows AuroraKV to avoid writing each update directly to disk in sorted order, which would be expensive.

---

## 4.3 SSTables

SSTables (**Sorted String Tables**) are immutable, sorted files stored on disk.

They are created by flushing the MemTable and later reorganized through compaction.

### Responsibilities

- Persist flushed key-value data
- Support sorted storage layout
- Enable merge-based compaction
- Serve disk-based point lookups

### Design Role

Because SSTables are immutable:

- writes are efficient
- crash consistency is easier
- compaction becomes predictable

---

## 4.4 Bloom Filters

Each SSTable has an associated Bloom Filter.

Before searching an SSTable, AuroraKV checks its Bloom Filter to determine whether the key **could exist**.

### Responsibilities

- Quickly reject non-existent key lookups
- Reduce unnecessary SSTable reads
- Lower disk lookup cost at scale

### Design Role

Bloom Filters are critical once the number of SSTables increases, especially under read-heavy workloads.

---

## 4.5 LRU Cache

AuroraKV includes an **LRU (Least Recently Used) Cache** to speed up repeated accesses.

### Responsibilities

- Cache recently accessed keys
- Reduce repeated traversal of MemTable and SSTables
- Improve hot-key lookup performance

### Design Role

The cache is especially valuable for:

- read-heavy workloads
- skewed key access patterns
- benchmark stability at smaller scales

---

## 4.6 Manifest

The Manifest tracks metadata about all active SSTables.

### Responsibilities

- Track file creation and organization
- Maintain level metadata
- Restore engine state during startup

### Design Role

The Manifest acts as the metadata control plane of the storage engine.

---

## 4.7 Compaction Engine

Compaction is the process that prevents the storage engine from degrading over time.

### Responsibilities

- Merge overlapping SSTables
- Remove obsolete versions of keys
- Delete tombstones when safe
- Maintain healthy SSTable layout

### Design Role

Without compaction:

- reads would become slower
- storage would grow inefficiently
- stale data would accumulate

---

# 5. Request Lifecycle

AuroraKV supports three primary operations:

- `PUT`
- `GET`
- `DELETE`

Each operation follows a distinct internal execution path.

---

# 6. Write Path (`PUT`)

A write operation prioritizes **durability first**, then **in-memory visibility**, and finally **eventual disk persistence**.

## Write Flow

```text
PUT(key, value)
   ↓
Append record to WAL
   ↓
Insert/update key in MemTable
   ↓
Update LRU Cache
   ↓
If MemTable threshold reached:
   → Freeze MemTable
   → Flush to SSTable
   → Register SSTable in Manifest
```

## Write Path Guarantees

- **Durable** once WAL append succeeds
- **Visible immediately** after MemTable update
- **Persisted to disk** after flush

## Why this design works

This path transforms many small writes into:

- sequential log appends
- batched sorted disk writes

This is the fundamental reason LSM systems achieve high write throughput.

---

# 7. Read Path (`GET`)

The read path is optimized to avoid unnecessary disk work.

AuroraKV resolves lookups in the following order:

## Read Flow

```text
GET(key)
   ↓
Check LRU Cache
   ↓
Check MemTable
   ↓
Check SSTables using Bloom Filters
   ↓
Return latest visible value or NOT_FOUND
```

## Read Path Strategy

### Step 1 — Cache Lookup
If the key is in cache, the read completes immediately.

### Step 2 — MemTable Lookup
If recently written, the latest value is returned directly from memory.

### Step 3 — Bloom Filter Screening
Before touching an SSTable, AuroraKV checks whether the key may exist.

### Step 4 — SSTable Search
Only candidate SSTables are searched.

## Why this design works

This layered lookup path reduces:

- unnecessary disk I/O
- read amplification
- latency under repeated access patterns

---

# 8. Delete Path (`DELETE`)

AuroraKV does not physically remove keys immediately.

Instead, it uses **tombstones**.

## Delete Flow

```text
DELETE(key)
   ↓
Append tombstone to WAL
   ↓
Insert tombstone into MemTable
   ↓
Flush tombstone into SSTable
   ↓
Physically remove during compaction
```

## Why tombstones are necessary

Because older copies of a key may still exist in lower SSTables, immediate deletion is not safe.

Tombstones ensure that:

- the delete is visible immediately
- stale copies are ignored
- final cleanup happens during compaction

---

# 9. Flush Pipeline

The flush pipeline converts volatile in-memory state into durable sorted disk state.

## Flush Trigger

Flush occurs when the MemTable reaches a configured threshold.

## Flush Steps

1. Mark current MemTable as immutable  
2. Serialize entries in sorted order  
3. Build SSTable file  
4. Build Bloom Filter  
5. Register file in Manifest  
6. Activate a fresh MemTable  

## Flush Design Goals

- Keep writes fast
- Avoid blocking foreground operations
- Preserve sorted disk layout

---

# 10. Compaction Pipeline

Compaction is the mechanism that keeps AuroraKV performant as data grows.

It resolves the natural side effects of LSM systems:

- duplicate keys across files
- tombstones
- overlapping SSTables
- increasing read amplification

---

## 10.1 Why Compaction Exists

As more flushes occur:

- more SSTables are created
- more files must be searched
- more obsolete versions accumulate

Compaction rewrites this fragmented state into a more efficient layout.

---

## 10.2 Compaction Responsibilities

Compaction:

- merges sorted SSTables
- keeps the latest version of each key
- discards overwritten versions
- removes tombstones when safe
- reduces future lookup cost

---

# 11. Supported Compaction Strategies

AuroraKV supports two compaction strategies:

---

## 11.1 Leveling

In leveling, data is progressively merged into larger levels with limited overlap.

### Properties

- Fewer SSTables per level
- Better lookup efficiency
- Lower read amplification
- Higher compaction cost

### Best suited for

- read-heavy workloads
- latency-sensitive lookup paths

---

## 11.2 Tiering

In tiering, SSTables are accumulated and merged less aggressively.

### Properties

- Better write throughput
- Lower write amplification
- More overlapping files
- Higher read amplification

### Best suited for

- write-heavy ingestion
- append-dominant workloads

---

# 12. Recovery Architecture

AuroraKV is designed to recover cleanly after restart or crash.

## Recovery Flow

```text
Engine Startup
   ↓
Load Manifest metadata
   ↓
Open WAL
   ↓
Replay operations in order
   ↓
Reconstruct MemTable state
   ↓
Resume normal operation
```

## Recovery Guarantees

- recently acknowledged writes are recoverable
- MemTable state can be rebuilt
- SSTable metadata remains consistent

This is a critical property for any storage engine claiming durability.

---

# 13. Background Execution Model

AuroraKV separates foreground operations from maintenance work.

## Foreground Path

Handles:

- PUT
- GET
- DELETE

## Background Path

Handles:

- MemTable flush
- SSTable compaction
- maintenance operations

## Why this matters

This separation improves:

- responsiveness
- throughput consistency
- operational stability under load

---

# 14. Scalability Characteristics

AuroraKV scales by converting many random updates into structured sequential storage operations.

As the dataset grows:

- MemTable remains fast for recent writes
- SSTables increase in count
- Bloom Filters become more valuable
- Compaction becomes more important
- read/write trade-offs become more visible

This is where leveling vs tiering behavior becomes meaningful.

---

# 15. Architectural Trade-Offs

AuroraKV intentionally exposes the same trade-offs seen in production LSM systems.

## Write Optimization vs Read Cost
Fast ingestion creates more SSTables, which increases read complexity.

## Tiering vs Leveling
Tiering favors write throughput; leveling favors read efficiency.

## Memory vs Disk Efficiency
Larger caches and filters improve performance but consume more memory.

## Simplicity vs Realism
The design remains educational and implementable while still reflecting real engine behavior.

---

# 16. Design Summary

AuroraKV is a practical implementation of a modern **LSM-tree storage engine architecture**.

Its design combines:

- **WAL-backed durability**
- **MemTable-based write buffering**
- **Immutable SSTable persistence**
- **Bloom Filter assisted reads**
- **LRU-based caching**
- **Background compaction**
- **Recovery support**

Together, these components form a storage engine that is not only functional, but also representative of the engineering principles used in real-world backend and database systems.