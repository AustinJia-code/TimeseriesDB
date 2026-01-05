## Time Series Database
Receives and stores timeseries data (logs, sensors, etc.) over http.

Multithreaded to handle many input feeds, robust to crashes using a write-ahead-log, and memory-friendly using periodic writes from memory to disk (gorilla compression, as good as ~15% ratio).

Queries (`read` endpoint with device `tag`) return data from RAM.

### Build:
```
cd build
cmake ..
make
```

### Run:
* Terminal 1: `./tsdb_server`
* Terminal 2: `./load_gen`
* Sample CLI poll: `curl -s "http://localhost:9090/read?tag=device_1" | python3 -m json.tool`

* See how to run vibe-coded dashboard in ```./dashboard/README.md``` :).

### Make
```make wipe``` - clear .wal and .db from disk
