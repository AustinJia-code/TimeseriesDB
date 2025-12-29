## Time Series Database
Receives and stores timeseries data (logs, sensors, etc.) over http.

Multithreaded to handle many input feeds, robust to crashes using a write-ahead-log, and memory-friendly using periodic writes from memory to disk (gorilla compression, ~30% ratio).

Queries (`read` endpoint with device `tag`) return data from both long-term storage and RAM.

### Build:
```
mkdir build && cd build
cmake ..
make
```

### Run:
* Terminal 1: `./tsdb_server`
* Terminal 2: `./load_gen`