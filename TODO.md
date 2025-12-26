# Time Series Database

## 2. Durability (Write Ahead Log)
Ensure data survives a crash.

**Implement WAL Writer**
* [ ] Create class `WAL`.
* [ ] On init: `std::ofstream log_file("wal.log", std::ios::binary | std::ios::app);`.
* [ ] Implement `append(string key, DataPoint dp)`.
* [ ] Use `reinterpret_cast` to write raw bytes (timestamp + value) to the stream. Don't use text/JSON!
* [ ] Call `log_file.flush()` (or rely on OS buffers if optimizing for speed).

**Implement WAL Replayer**
* [ ] Implement `recover()` method.
* [ ] Read `wal.log` from start to finish on startup.
* [ ] Re-insert parsed data back into the `MemTable`.

**Integration Test**
* [ ] Start DB -> Run Load Gen -> Kill DB (CTRL+C) -> Restart DB -> Verify count of points in RAM.


## 3. Storage Engine (SSTables)
Move data from RAM to Disk efficiently.

**Implement Flush Logic**
* [ ] Monitor `MemTable` size (e.g., when vector size > 10,000 points).

**Snapshoting:**
* [ ] Move the current `std::vector` to a "Immutable MemTable" and start a new fresh vector for writes.

**Sorting:**
* [ ] Sort the immutable vector by timestamp:

**File Format Design**
* [ ] Write a Header (Magic Number `TSDB`, Version).
* [ ] Write the Data Block (Sequential binary dump of sorted points).
* [ ] Write a Footer (Offset to the index, checksum).
* [ ] Use `std::filesystem` to name files uniquely (e.g., `timestamp_segment.tsm`).


## 4. Compression (Gorilla Algorithm)
Implement bit-packing in C++.

**BitWriter Class**
* [ ] Implement a class to buffer individual bits into a `uint8_t` or `uint64_t` buffer.
* [ ] Methods: `writeBit(bool)`, `writeBits(uint64_t value, int count)`.

**Timestamp Compression (Delta-of-Delta)**
* [ ] Calculate .
* [ ] Calculate .
* [ ] Store  using variable bit-width encoding (e.g., if 0, write '0', if small, write '10' + value).

**Value Compression (XOR)**
* [ ] Cast double to uint64: `uint64_t val_int = *reinterpret_cast<uint64_t*>(&value);`.
* [ ] Calculate `xor_val = val_int ^ prev_val_int`.
* [ ] Implement Leading Zero / Trailing Zero counting (`__builtin_clzll` in GCC/Clang is fast for this).
* [ ] Store only the meaningful XOR bits.


## 5. Query Engine
Retrieve the data.

**Index Implementation**
* [ ] Maintain an in-memory map: `MetricName -> std::vector<std::string> filenames`.

**File Cursor**
* [ ] Create `TSMIterator` class.
* [ ] Uses `mmap` (memory mapping) or `std::ifstream` to read file blocks.
* [ ] Decompresses the bitstream on the fly, returning `DataPoint` structs.

**Merge Logic**
* [ ] Implement a query: `select(metric, start_time, end_time)`.
* [ ] Fetch data from `MemTable` (recent).
* [ ] Fetch data from Disk (historical).
* [ ] Merge the two sorted lists and return.


## 6: Performance & Profiling
Optimize the C++ code.

**Benchmarking**
* [ ] Run the Load Gen with 1M points.
* [ ] Measure ingestion rate.
* [ ] Measure disk footprint (Compression Ratio).

**Profiling**
* [ ] Use `perf` (Linux) or `valgrind` to find bottlenecks.
* [ ] Check for memory leaks.