# Concurrent-KV

A multi-threaded, network-enabled, in-memory key-value store written from scratch in **C++17** — inspired by Redis, built to understand systems programming (sockets, concurrency, memory management) end to end rather than through a framework.

Built entirely on the C++ standard library — no external networking, hashing, or concurrency dependencies.

## Features

* **Custom TCP server** built directly on POSIX sockets (`socket`/`bind`/`listen`/`accept`/`recv`/`send`), with manual byte-stream framing, partial I/O handling, and `EINTR` recovery
* **Thread-per-connection concurrency**, with a single shared store protected by mutex synchronization (RAII `lock_guard`)
* **O(1) LRU eviction** via a hash map + doubly linked list, bounding memory by key count
* **TTL-based key expiration**, combining lazy checks on access with a background sweep thread for keys that are never accessed again
* **Binary snapshot persistence** (`SAVE` command) for crash recovery, with correct TTL reapplication across process restarts
* **Custom load-testing tool** (`bench`) measuring real throughput and latency percentiles under concurrent load

## Getting Started

### Prerequisites

* A C++17 compiler (GCC/Clang)
* CMake ≥ 3.10
* Linux or macOS (uses POSIX sockets)

### Build

```bash
git clone https://github.com/vikasjha2003/concurrent-kv.git
cd concurrent-kv
mkdir build && cd build
cmake ..
cmake --build .
```

This produces two executables: `concurrent-kv` (the server) and `bench` (the load-testing client).

### Run the server

```bash
./concurrent-kv
```

By default the server listens on port `6380` and loads `snapshot.db` at startup if one exists.

### Connect as a client

```bash
nc localhost 6380
```

## Protocol / Commands

Concurrent-KV speaks a simple newline-delimited text protocol.

| Command              | Effect                                          | Response               |
| -------------------- | ----------------------------------------------- | ---------------------- |
| `SET key value`      | Insert or overwrite a key                       | `OK`                   |
| `GET key`            | Fetch a value                                   | `VALUE <v>` or `NIL`   |
| `DEL key`            | Remove a key                                    | `OK` or `NIL`          |
| `EXISTS key`         | Check existence (does **not** count as LRU use) | `VALUE 1` or `VALUE 0` |
| `EXPIRE key seconds` | Set/refresh a TTL on a key                      | `VALUE 1` or `VALUE 0` |
| `SAVE`               | Write a binary snapshot to disk                 | `OK` or `ERR <reason>` |
| `PING`               | Liveness check                                  | `PONG`                 |

**Response convention:** every real value returned is prefixed with `VALUE`, so it can never be confused with a status word (`OK` / `NIL` / `ERR` / `PONG`) — even if the stored value happens to be one of those words.

> **Note:** values cannot contain whitespace, since the protocol tokenizes on whitespace. A production version would use length-prefixed framing (as Redis's RESP protocol does) to lift this restriction.

## Benchmarking

A standalone load-testing client (`bench`) opens its own TCP connections and drives concurrent SET/GET traffic against the server, reporting throughput and latency percentiles (p50/p95/p99).

```bash
./bench --threads 16 --ops 1000 --host 127.0.0.1 --port 6380
```

**Sample result** (4 threads, 1000 ops/thread):

```text
Completed ops:  4000
Wall time:      0.189859 s
Throughput:     21068.3 ops/sec
Latency min:    0.093254 ms
Latency p50:    0.168166 ms
Latency p95:    0.244985 ms
Latency p99:    0.392293 ms
Latency max:    1.5066 ms
```

**Sample result** (1 thread, 4000 ops/thread):

```text
Completed ops:  4000
Wall time:      0.620407 s
Throughput:     6447.38 ops/sec
Latency min:    0.091059 ms
Latency p50:    0.136829 ms
Latency p95:    0.207571 ms
Latency p99:    0.271047 ms
Latency max:    1.3034 ms
```

**Sample result** (16 threads, 1000 ops/thread):

```text
Completed ops:  16000
Wall time:      0.200352 s
Throughput:     79859.5 ops/sec
Latency min:    0.033883 ms
Latency p50:    0.057757 ms
Latency p95:    0.319062 ms
Latency p99:    1.19945 ms
Latency max:    14.7938 ms
```

**Sample result** (64 threads, 1000 ops/thread):

```text
Completed ops:  64000
Wall time:      1.32146 s
Throughput:     48431.3 ops/sec
Latency min:    0.03198 ms
Latency p50:    0.245372 ms
Latency p95:    1.3657 ms
Latency p99:    2.97803 ms
Latency max:    31.0193 ms
```

Scaling across thread counts revealed the server's actual bottleneck: throughput scales ~12x from 1 → 16 concurrent threads, but **drops** at 64 threads as contention on the single global mutex outweighs the added parallelism — direct evidence of the coarse-grained locking trade-off described below.

## Design Notes & Known Limitations

This project prioritizes **correctness and understanding over raw performance** at every stage — each of the following was a deliberate, documented trade-off, not an oversight:

* **Coarse-grained locking** — a single mutex protects the entire store. Simple to reason about and prove correct; benchmarking confirms it becomes the scalability bottleneck past ~16 concurrent clients. Fix: shard the store into independently-locked partitions, or use a reader-writer lock.
* **No graceful shutdown** — the process exits abruptly on `Ctrl+C`; in-flight connections aren't drained and there's no auto-save on exit.
* **Single node only** — no replication or clustering.
* **Manual `SAVE` only** — no periodic auto-snapshotting or write-ahead log yet, so data since the last `SAVE` is lost on a crash.
* **No authentication** — any client that can open a TCP connection has full access.

## Project Structure

```text
concurrent-kv/
├── src/
│   ├── store/       # KVStore — in-memory map, LRU, TTL, persistence
│   ├── command/     # CommandProcessor — protocol parsing & dispatch
│   ├── server/      # TcpServer — sockets, connection handling
│   ├── bench/       # Standalone load-testing client
│   └── main.cpp
├── CMakeLists.txt
└── README.md
```
