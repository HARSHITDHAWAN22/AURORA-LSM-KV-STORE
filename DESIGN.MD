# Design Decisions in AuroraKV

This document explains the major design decisions made while building AuroraKV and the trade-offs behind them. The goal was to keep the system practical, understandable, and aligned with the design principles used in modern LSM-tree based storage engines.

---

## 1. Why LSM-Tree Instead of B-Tree?

AuroraKV uses an **LSM-tree (Log-Structured Merge Tree)** instead of a **B-Tree**.

### Why not B-Tree?

B-Trees are effective for many workloads, especially when in-place updates and balanced read performance are needed. However, they usually involve:

- random disk writes
- page splits and rebalancing
- frequent in-place modifications

These are less suitable for write-optimized systems.

### Why LSM-tree was chosen

LSM-trees are designed to optimize write-heavy workloads by converting updates into:

- append-only log writes
- in-memory buffering
- batched sequential disk writes

### Decision

LSM-tree was chosen because it provides:

- higher write efficiency
- better batching behavior
- simpler immutable disk layout
- a realistic foundation for studying compaction trade-offs

---

## 2. Why Use a Write-Ahead Log (WAL)?

The MemTable is memory-resident, which makes it fast but volatile. Without durability protection, recent writes would be lost if the system crashed before flush.

### Decision

AuroraKV uses a **Write-Ahead Log (WAL)** as the first durability boundary.

Every `PUT` and `DELETE` is appended to the WAL **before** being inserted into the MemTable.

### Why this was necessary

The WAL guarantees that:

- accepted writes are recoverable
- operation ordering is preserved
- in-memory state can be reconstructed after restart

### Trade-off

This adds an extra disk write per operation.

However, since the WAL is append-only, the overhead is generally small and predictable.

---

## 3. Why Use a MemTable?

Writing every update directly to disk in sorted order would be too expensive.

### Decision

AuroraKV uses a **MemTable** as the active mutable in-memory state.

### Why this was chosen

The MemTable provides:

- low-latency inserts
- efficient updates to recent keys
- a batching point before SSTable creation

It also allows recent reads to be served without disk access.

### Trade-off

Because the MemTable is volatile:

- it requires WAL for durability
- it must be flushed when full
- memory usage must be controlled

---

## 4. Why Use Immutable SSTables?

Once data is flushed from the MemTable, AuroraKV writes it to disk as an **immutable SSTable**.

### Why not update files in-place?

In-place disk updates introduce:

- random I/O
- fragmentation
- harder crash recovery
- more complex consistency handling

### Why immutable files were chosen

Immutable SSTables provide:

- sequential file generation
- simple on-disk structure
- easier recovery guarantees
- clean merge behavior during compaction

### Trade-off

Immutability means stale versions remain on disk until compaction removes them.

This introduces:

- temporary duplication
- extra storage usage
- background maintenance requirements

---

## 5. Why Use Bloom Filters?

As the number of SSTables grows, point lookups become more expensive. Without optimization, each lookup may need to check many files.

### Decision

AuroraKV associates a **Bloom Filter** with each SSTable.

Before searching a file, the Bloom Filter is checked to determine whether the key could exist.

### Why this was chosen

Bloom Filters help quickly reject negative lookups and reduce unnecessary disk reads.

### Trade-off

Bloom Filters introduce:

- additional memory usage
- false positives

However, they do not produce false negatives, which preserves correctness.

---

## 6. Why Use an LRU Cache?

Repeated reads of the same keys should not always traverse the full storage hierarchy.

### Decision

AuroraKV uses an **LRU (Least Recently Used) Cache** to retain recently accessed values.

### Why this was chosen

The cache improves performance by:

- accelerating repeated key lookups
- reducing MemTable and SSTable traversal
- improving latency under skewed workloads

### Trade-off

Caching introduces:

- additional memory consumption
- cache update responsibility
- eviction overhead

---

## 7. Why Use Tombstones for Deletes?

Deletes in an LSM-tree cannot be handled by immediately removing the key from disk because older versions may still exist in multiple SSTables.

### Decision

AuroraKV represents deletions using **tombstones**.

A tombstone is a delete marker written like a normal record.

### Why this was chosen

Tombstones ensure that:

- deletes become visible immediately
- older values are masked correctly
- physical cleanup can happen safely later

### Trade-off

Tombstones temporarily increase:

- storage usage
- read complexity
- compaction workload

---

## 8. Why Compaction is Required

An LSM-tree naturally accumulates files over time. Without compaction, the system would degrade as:

- SSTable count grows
- duplicate keys accumulate
- tombstones remain indefinitely
- read amplification increases

### Decision

AuroraKV includes a **background compaction engine**.

### Why this was chosen

Compaction is required to:

- merge overlapping SSTables
- keep only the latest key versions
- remove obsolete entries
- eliminate tombstones when safe
- improve future read performance

### Trade-off

Compaction introduces:

- background CPU usage
- disk I/O overhead
- write amplification

---

## 9. Why Support Both Leveling and Tiering?

AuroraKV supports both **leveling** and **tiering** compaction strategies.

### Why not just implement one?

A major goal of the project was to study the behavior of different storage policies. Leveling and tiering represent two important but different approaches to compaction.

Implementing both makes the project more meaningful from a systems perspective.

---

## 10. Why Choose Leveling?

In **leveling**, SSTables are merged aggressively into structured levels with limited overlap.

### Advantages

- better read performance
- fewer files to search
- lower read amplification

### Trade-off

Leveling increases:

- write amplification
- compaction frequency
- background maintenance cost

### Decision

Leveling was included because it represents the read-optimized side of LSM compaction design.

---

## 11. Why Choose Tiering?

In **tiering**, SSTables are grouped and compacted less aggressively.

### Advantages

- better write throughput
- lower write amplification
- reduced compaction pressure

### Trade-off

Tiering increases:

- overlap between SSTables
- read amplification
- lookup complexity

### Decision

Tiering was included because it represents the write-optimized side of LSM compaction design.

---

## 12. Why Run Flush and Compaction in the Background?

Foreground operations should remain responsive. If flush and compaction were performed inline with user operations, write latency would become inconsistent and the system would stall under heavier workloads.

### Decision

AuroraKV separates:

#### Foreground path
- PUT
- GET
- DELETE

#### Background path
- MemTable flush
- Compaction
- maintenance work

### Why this was chosen

This separation improves:

- responsiveness
- throughput stability
- system realism

### Trade-off

Background execution introduces:

- thread coordination complexity
- scheduling considerations
- concurrency management overhead

---

## 13. Why Use a Manifest?

AuroraKV needs a persistent way to track active SSTables and their organization.

### Decision

AuroraKV maintains a **Manifest** to store storage metadata.

### Why this was chosen

The Manifest allows the system to:

- track active SSTables
- maintain structural organization
- restore storage state on startup

### Trade-off

This adds metadata management complexity, but it is necessary for a realistic persistent engine.

---

## 14. Why Prioritize Sequential I/O?

A recurring design principle in AuroraKV is the preference for **sequential disk access over random disk access**.

This decision influenced:

- WAL design
- MemTable flush behavior
- SSTable generation
- compaction strategy

### Why this matters

Sequential writes are generally:

- faster
- more predictable
- easier to batch
- better suited for storage devices

This is one of the key reasons LSM-based systems perform well under write-heavy workloads.

---

## 15. Key Trade-Off Summary

| Design Area | Chosen Approach | Benefit | Trade-Off |
|------------|----------------|---------|-----------|
| Storage Model | LSM-tree | Fast writes | Compaction complexity |
| Durability | WAL | Crash safety | Extra append |
| Write Buffering | MemTable | Low-latency writes | Requires flush |
| Disk Format | Immutable SSTables | Sequential I/O | Stale versions remain |
| Read Optimization | Bloom Filters | Fewer disk reads | Extra memory |
| Read Optimization | LRU Cache | Faster repeated lookups | Cache overhead |
| Deletes | Tombstones | Correctness | Delayed cleanup |
| Maintenance | Background compaction | Long-term efficiency | Write amplification |
| Compaction Policy | Leveling / Tiering | Tunable behavior | Different trade-offs |

---

## 16. Design Philosophy

AuroraKV was built around the following principles:

- prefer sequential writes over random updates
- use immutability to simplify correctness
- accept background maintenance as the cost of write optimization
- improve reads with targeted probabilistic and caching structures
- preserve durability before performance shortcuts
- expose trade-offs rather than hiding them

---

## 17. Summary

The design decisions in AuroraKV were chosen to balance:

- performance
- durability
- implementation clarity
- real-world relevance

AuroraKV was designed not just as a functional key-value store, but as a compact and practical representation of how modern **LSM-tree based storage engines** are structured and optimized.
