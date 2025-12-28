# Time Series Database

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