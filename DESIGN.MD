# AuroraKV
### A Simple LSM-Tree Based Key-Value Store in C++

Author: Harshit Dhawan  
Language: C++17  

---

## 1. Introduction

AuroraKV is a persistent key-value storage system built as a learning
project to understand how modern databases handle large amounts of data
efficiently. The project is inspired by real-world storage engines, but
the implementation is kept simple and readable.

The main idea behind AuroraKV is to handle write-heavy workloads
efficiently by avoiding slow random disk writes and instead using a
structured, layered storage design.

---

## 2. Problem Statement

When data is written directly to disk, especially in random order,
performance becomes very slow. This problem becomes worse as the amount
of data grows.

AuroraKV is designed to solve this by:
- Writing data first to memory
- Writing data to disk only in sequential order
- Managing disk data in an organized way
- Supporting insert, read, and delete operations efficiently

---

## 3. High-Level Design Idea

AuroraKV is based on the **LSM-Tree (Log-Structured Merge Tree)** concept.

The main ideas are:
- All writes and deletes go to memory first
- Memory data is periodically flushed to disk
- Disk files are immutable (never changed after creation)
- Older files are merged using compaction
- Bloom filters are used to reduce unnecessary disk reads
- Metadata is stored using JSON files for recovery

---

## 4. Why LSM-Tree?

The LSM-Tree approach is chosen because it converts random writes into
sequential disk writes, which are much faster.

### Benefits
- Very fast write and delete operations
- Better disk performance
- Scales well with large datasets

### Drawbacks
- Reads can be slower if not optimized
- Compaction uses extra CPU and disk resources

AuroraKV handles these issues using Bloom filters and controlled
compaction.

---

## 5. System Components

AuroraKV is divided into small and clear components:

| Component | Purpose |
|--------|--------|
| KVStore | Main interface and coordinator |
| MemTable | In-memory storage for writes and deletes |
| SSTable | Immutable files stored on disk |
| BloomFilter | Quickly checks if a key may exist |
| Compaction | Merges and cleans old SSTables |
| ConfigManager | Loads system configuration |
| ManifestManager | Stores metadata for recovery |

---

## 6. Supported Operations

AuroraKV supports the following operations:
- **PUT**: Insert or update a key
- **GET**: Read a value using a key
- **DELETE**: Logically delete a key
- **FLUSH**: Write in-memory data to disk
- **COMPACTION**: Merge and optimize disk files

---

## 7. Component Details

### 7.1 KVStore
KVStore is the main entry point of the system.

It is responsible for:
- Accepting client requests
- Sending writes and deletes to the MemTable
- Searching data during reads
- Triggering flush and compaction
- Loading configuration and metadata at startup

---

### 7.2 MemTable
MemTable stores recent writes in memory.

Design points:
- Implemented using an ordered map
- Keeps data sorted by key
- Stores both values and delete markers (tombstones)
- Flushed to disk once it reaches a size limit

---

### 7.3 SSTable
SSTables are files stored on disk.

Key properties:
- Data is written in sorted order
- Files are never modified after creation
- Can contain values and tombstones
- Each SSTable has an associated Bloom filter

---

### 7.4 Bloom Filter
Bloom filters help reduce disk reads.

They:
- Quickly tell whether a key might exist
- Can return false positives
- Never return false negatives

If the Bloom filter says a key is not present, the file is skipped.

---

### 7.5 Compaction
Compaction merges multiple SSTables into fewer files.

Goals:
- Keep only the latest version of each key
- Remove deleted keys using tombstones
- Reduce the number of SSTable files
- Improve read performance over time

---

### 7.6 Config Manager
The Config Manager loads system settings from a JSON file.

It provides:
- MemTable size limits
- Bloom filter parameters
- Compaction strategy
- Directory paths

---

### 7.7 Manifest Manager
The Manifest Manager stores metadata about the system.

It keeps track of:
- SSTable file names
- File ordering and levels
- Sequence numbers

This information allows the system to recover after a restart.

---

## 8. Operation Flows

### 8.1 Write Flow (PUT)

1. Client sends a `put(key, value)` request.
2. KVStore inserts the key-value pair into the MemTable.
3. When the MemTable becomes full:
   - Its data is written to disk as an SSTable.
   - A Bloom filter is created.
   - Metadata is updated.
4. A new empty MemTable is created.

This ensures disk writes are sequential and fast.

---

### 8.2 Read Flow (GET)

1. Client sends a `get(key)` request.
2. KVStore checks the MemTable first.
3. If not found, SSTables are checked from newest to oldest.
4. Bloom filters are used to skip irrelevant files.
5. If a tombstone is found, the key is treated as deleted.
6. If nothing is found, the key does not exist.

---

### 8.3 Delete Flow (DELETE)

1. Client sends a `delete(key)` request.
2. A tombstone is inserted into the MemTable.
3. The tombstone is flushed to disk like normal data.
4. During compaction, old values are removed permanently.

Deletes are logical and efficient.

---

### 8.4 Flush Operation

- Happens when MemTable reaches its size limit
- Converts in-memory data into an SSTable
- Ensures durability of recent operations

---

### 8.5 Compaction Flow

1. Select SSTables for compaction.
2. Merge files in sorted order.
3. Keep the latest version of each key.
4. Remove deleted keys.
5. Write new compacted SSTables.
6. Delete old files.
7. Update metadata.

---

## 9. Persistence and Recovery

### Persistence
- Data is stored in SSTable files
- Metadata is saved in JSON format

### Recovery
On startup:
- Configuration is loaded
- Manifest metadata is read
- SSTables are registered
- System becomes ready for use

---

## 10. Directory Structure

AuroraKV-lsm-kv-store/
- src/        : Source code
- include/    : Header files
- config/     : Configuration files
- metadata/   : Metadata files
- data/       : SSTable data files
- build/      : Build outputs
- DESIGN.md   : Design document
- README.md   : Project overview

---

## 11. Assumptions and Limitations

- Single-node system
- Single-threaded execution
- Keys and values are strings
- Focus is on learning and correctness

---

## 12. Future Improvements

- Write-Ahead Logging (WAL)
- Multithreading
- Range queries
- Data compression
- Cache layer
- TTL-based deletes

---

## 13. Conclusion

AuroraKV is a learning-focused implementation of an LSM-Tree based
key-value store. The project demonstrates how modern storage systems
handle writes, reads, deletes, and persistence in an efficient and
structured way.
