# etherAdapter

**etherAdapter** — a device sampling-data ingest adapter: it receives the data
devices send, parses it, and writes it into the
[EtherDB](https://github.com/kinyi6666/EtherDB) time-series database.

> 🌏 中文: [README.md](README.md)
>
> Design document: `etherAdapter.txt` (this directory).
> Data flow: `device → muduo → moodycamel → single-thread parser → write queue → EtherDB`
> (the publish queue → ZMQ PUB is reserved and not implemented yet).
> Ingest protocols: custom_data (custom header, device push, status + event frames),
> modbus (the adapter polls registers every 3 s), http (one HTTP server, POST JSON);
> mqtt is still a stub.

---

## 1. Architecture

```
  custom_data device ─┐ (one TCP connection per ip:port — status AND event frames)
                      ▼
           ┌───────────────── muduo (net) ─────────────────┐
 modbus device ◄─request─ ModbusPoller (3 s poll, long conn) │
           │  └─response─► TcpServer(60382, ...) onMessage → RawChunk │
           └─────────────► TcpServer(60382, ...)  ↑ (responses on the unified port)
                      └───────────────────────────┬─────────────────┘
                                                  │ ingest queue (moodycamel)
 http client ──POST JSON──► HttpIngest(own port)   │
                            │ parse JSON → RowBatch│
                            └──────────► write queue
                                                  ▼
                                  ┌─ ParserWorker (single thread: slice + decode) ─┐
                                  │  custom_data: 6-byte custom header, frameType  │
                                  │  → status device table / event device table    │
                                  │  modbus: response header + big-endian registers│
                                  │  per-field byte_offset/bit decode (raw values) │
                                  └───────┬──────────────────────┬────────────────┘
                                          │ RowBatch             │ RowBatch (copy)
                                          ▼                      ▼
                             write queue (moodycamel)      publish queue
                                          │                      │
                                          ▼                      ▼
                                EtherDBWriter (column bind / plain SQL)   ZMQ PUB (stub)
                                          │
                                          ▼
                                      EtherDB (127.0.0.1:7040)
```

- **Networking**: reuses EtherDB's muduo-style network library (`src/net`) plus the
  base library (`src/base`) — the same code the EtherDB server runs, buildable on
  Windows and Linux.
- **Queues**: moodycamel `BlockingConcurrentQueue`
  (`src/base/blockingconcurrentqueue.h`) with the same traits as the EtherDB server
  (larger blocks + block recycling).
- **Parsing**: **one thread** performs all protocol parsing (fully configuration
  driven, no hard-coded protocol): custom_data frames and modbus responses both
  enter the parser thread from the unified listening port.
- **Writing**: custom_data/modbus devices use the SDK's column-bound prepared
  statements (`EtDBStmt::bindParamBatch + execute` — the fastest insert path);
  slow HTTP data is written with plain `INSERT INTO ... VALUES(...)` (no statement
  binding needed).

## 2. Directory layout

```
etherAdapter/
├── CMakeLists.txt            top-level build (base/net/sqlite3/adapter/tools/tests)
├── config/
│   ├── etherAdapter.cfg      process-level configuration sample (INI)
│   └── schema.sql            SQLite configuration schema (three tables)
├── deps/sqlite3/             SQLite amalgamation (statically linked)
├── src/
│   ├── base/                 reused EtherDB muduo base (logging/threads/queue)
│   ├── net/                  reused EtherDB muduo net (TcpServer/EventLoop...)
│   ├── wincompat/            Windows compatibility layer (posix_compat.h ...)
│   └── adapter/              ★ this project's application code
│       ├── AdapterConfig.*   INI configuration loading
│       ├── ConfigDB.*        SQLite three tables → runtime device/protocol registry
│       ├── DeviceModel.h     field/frame/device descriptions (materialized tables)
│       ├── FrameCodec.*      streaming frame slicing + per-field decoding
│       ├── Pipeline.h        RawChunk / RowBatch / queues and statistics
│       ├── IngestServer.*    muduo TCP ingest (matched by ip:port)
│       ├── ParserWorker.*    single-thread parsing (frames → batches)
│       ├── EtherDBWriter.*    batch inserts into EtherDB (column bind + prepared)
│       ├── PublishQueue.*     publish queue (ZMQ PUB reserved stub)
│       ├── HttpIngest.*      one HTTP server (POST JSON → write queue)
│       ├── ModbusPoller.*    periodic modbus register requests (3 s, reconnect backoff)
│       ├── IngestStubs.*    mqtt ingest (stub)
│       └── AdapterApp.*      assembly and lifecycle; main.cpp is the entry point
├── tools/csv2sqlite.cpp      configuration CSV → SQLite import tool
├── tests/device_sim.cpp      custom_data device simulator (status + event frames)
├── tests/modbus_sim.cpp      modbus device simulator (request/response + data push)
├── tests/query_check.cpp     tiny query tool (verification, links the SDK)
├── tests/fixtures/           test configuration CSVs (corrected sample data)
├── docs/                     articles / write-ups (Chinese and English)
└── scripts/build_windows.bat / build_linux.sh
```

## 3. Configuration

### 3.1 Process configuration `etherAdapter.cfg`

See `config/etherAdapter.cfg`. It is looked up **next to the executable** by
default (or pass `-c <file>`). Key entries:

| Section | Key | Meaning |
|---|---|---|
| `[server]` | `extraListenPorts` | extra listening ports (normally empty — ports come from device_table) |
| `[sqlite]` | `path` | configuration database path (relative to the exe directory) |
| `[etherdb]` | `host/port/user/password/db` | EtherDB connection and target database |
| | `createDatabase / createTables` | create the database / tables at start-up |
| | `batchRows / flushIntervalMs` | event batch size / idle flush interval (status data ignores these — it is submitted on arrival) |
| `[http]` | `port` | HTTP listening port; 0 = take `local_server_port` from the http device rows (exactly **one** HTTP server) |
| `[modbus]` | `pollIntervalMs` | register-read request interval (default 3000 ms) |
| | `connectTimeoutMs` | connect timeout for the device link (non-blocking connect, default 1500 ms) |
| `[log]` | `dir / level` | async file log directory and level |
| `[zmq]` | `enabled / pubEndpoint` | publish path reserved (currently a stub) |

### 3.2 Device configuration (three SQLite tables)

All parsing configuration comes from the SQLite configuration database (opened
read-only); the three tables match the plant exports:

- `device_table`: one row per device, primary key `device_id` (also the EtherDB
  table name). `(device_ip, device_port)` matches a connection to its device:
  the device's source port for custom_data, the **request port** for modbus (the
  adapter connects to it to send read commands), empty for http. Rows that share
  one `(device_ip, device_port)` form **one peer group**: the first row (lowest
  `data_proto_id`) is the status device, the remaining rows are event devices
  told apart by the frame type — so a single device can report both periodic
  status sampling and event sampling over **one TCP connection**, landing in
  different EtherDB tables. `local_server_port` is the adapter's listening port
  (custom_data/modbus share the unified data port, http uses its own port — the
  two kinds must not collide); `data_proto_id` joins the other two tables.
- `data_header_table`: the frame format + frame type for each `data_proto_id`.
  - **custom_data** (6-byte custom header, little endian):
    `[frameType 1B][flag 1B][frameLen 2B][sequenceId 2B][payload ...][flag 1B?]`
    - `frameType` **is this table's `frame_type`**: for one `(ip, device_port)`
      endpoint the first row is the status device (periodic sampling, usually
      1 Hz) and the other rows are **event devices** (burst sampling, different
      point count/rate), each writing its own EtherDB table (the event table).
    - `flag` is `start_flag` (= `end_flag`, the same byte): it is **only
      meaningful for event frames**, and it is how a different parsing rule /
      another table is recognized inside a shared TCP stream. Status frames carry
      no delimiter and the byte is ignored. When the two columns differ they are
      used as the leading / trailing delimiter respectively (tolerant of
      `0x01…0x03`).
    - `frameLen` = payload bytes of this frame; one header can carry
      `frameLen / frame_len` records (several records share a header; 0 = one
      record).
    - `sequenceId` is reserved: **not validated, not decoded**, kept for future
      extensions.
  - **modbus**: `frame_type` **is the response header length** (9/7/2/0):
    9 = MBAP(7B)+function code+byte count, 7 = MBAP only, 2 = function code+byte
    count, 0 = raw register bytes without a header. The request parameters
    `slave_addr / fun_code / start_addr / addr_num` are configured here too:
    `[transId 2][0x0000 2][length=6 2][slave_addr 1][fun_code 1][start_addr 2][addr_num 2]`
    (big endian, transId cycles 1..0xFE).
- `data_proto_table`: the sensor field table per `data_proto_id` (one field = one
  column): `field_type` (type code, see `ConfigDB.cpp::mapFieldType`),
  `byte_offset`, `bit_offset/bit_len` (bit fields).
  **`factor` is currently not applied**: the database stores raw values (at least
  all non-float types; a non-1 factor only produces one warning at start-up).

> The `field_type` code table is centralized in
> `src/adapter/ConfigDB.cpp::mapFieldType()`: `0=BOOL, 1=INT16, 2=INT32,
> 3=FLOAT32, 4=FLOAT64, 5=INT8, 6=UINT8, 7=UINT16, 8=UINT32, 9=INT64,
> 10=UINT64`. If a plant protocol differs, **only this place needs changing**.

### 3.3 Importing the configuration database

```bat
:: [sqlite].path defaults to config/etherAdapter.db (relative to the exe dir),
:: so generate the database into the config\ folder next to etherAdapter.exe:
build\bin\Release\csv2sqlite.exe build\bin\Release\config\etherAdapter.db <csv dir>
```

This imports `device_table.csv / data_header_table.csv / data_proto_table.csv`
(header and annotation rows are skipped; GBK is converted to UTF-8; `--force`
drops and rebuilds the tables).

## 4. Build

```bat
:: Windows (VS2022)
scripts\build_windows.bat
:: artifacts: build\bin\Release\{etherAdapter.exe, csv2sqlite.exe, device_sim.exe, query_check.exe, base.dll, net.dll}
```

```sh
# Linux
./scripts/build_linux.sh
```

Prerequisites:

- **EtherDB client SDK**: taken from `../EtherDB/src/bin/sdk` (include + lib) by
  default; override with `-DETHERDB_SDK_DIR=<...>`. Build the EtherDB SDK once
  (`EtherDB\scripts\build_sdk.bat`) — repository:
  https://github.com/kinyi6666/EtherDB.
- CMake ≥ 3.16; on Windows use VS2022 (`/MD`, consistent with the SDK).

## 5. Running

```bat
:: 1) start the EtherDB server (separate window)
EtherDB\src\bin\Release\etherdb_dserver.exe -p 7040

:: 2) prepare the configuration database (first time)
build\bin\Release\csv2sqlite.exe build\bin\Release\etherAdapter.db <csv dir>

:: 3) copy config\etherAdapter.cfg next to etherAdapter.exe (build\bin\Release\),
::    adjust it, then start the adapter
build\bin\Release\etherAdapter.exe            :: or etherAdapter.exe -c <cfg>
```

At start-up the adapter: connects to EtherDB → `CREATE DATABASE IF NOT EXISTS` +
`USE` → creates one table per device (including event devices)
`CREATE TABLE IF NOT EXISTS <device_id> (ts TIMESTAMP, ...)` → prepares the INSERT
statements (http devices are skipped) → then:

- **custom_data**: listens on `local_server_port` (the unified port) for device
  pushes; the header's `frameType` decides the target table — status frames
  (periodic sampling, usually 1 Hz) are **submitted as soon as they arrive**, while
  event frames (burst/high-rate, possibly a different point count) are written to
  the event table in batches;
- **modbus**: every 3 s (`[modbus].pollIntervalMs`) sends a register-read request
  to the device's request port (long connection + non-blocking connect + backoff
  reconnect); the device response arrives on the unified port as well;
- **http**: listens on its own HTTP port (`[http].port` or the http device's
  `local_server_port`); a device POSTing JSON is written straight to the database.

Press `Ctrl+C` for a graceful shutdown (stop ingesting → parse what is queued →
flush the write queue → persist).

Table layout: `<device_id>(ts TIMESTAMP, <field1> <type>, ...)` where `ts` is the
adapter's **receive time** (milliseconds). Status and event data share the same
field layout but live in separate tables (e.g. `dev_T100_001` /
`dev_T100_001_E`). Runtime statistics are logged every 5 seconds
(`log\etherAdapter*.log`, including event/modbus/http counters).

## 6. End-to-end testing

```bat
build\bin\Release\csv2sqlite.exe build\bin\Release\etherAdapter.db tests\fixtures --force
cd build\bin
etherdb_dserver.exe -p 7040      :: separate window
etherAdapter.exe
device_sim.exe -n 2000 -i 1      :: simulates dev_T100_001 connecting to 60382, 2000 frames
```

`device_sim` connects from **source port 10002** (matching device_table). Status
frames carry 73 B per record, event frames 62 B — they can be sent separately, or
(recommended) interleaved on **one connection**:

```bat
:: 1a) status frames only (frameType 0xC → dev_T100_001)
device_sim.exe -n 2000 -i 1

:: 1b) status + event interleaved on ONE TCP connection (the real deployment case)
::     -t C,E = frame types 0xC/0xE; -l 73,62 = record sizes; -f/-e = event delimiters
device_sim.exe -n 60 -i 20 -t C,E -l 73,62 -f 0,01 -e 0,03 -v 100
```

Verify:

```bat
query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_T100_001"
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001 LIMIT 10"
:: status table item1 = 0,2,4... (values increment per record in interleaved mode)
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001_E LIMIT 10"
:: event table dev_T100_001_E item1/item2 = 1,3,5... (routed by frameType 0xE)
```

(`tests/fixtures/README.md` has the full step-by-step procedure.)

### modbus test (fixtures: dev_MB_401, ip 127.0.0.2, request port 10004)

```bat
modbus_sim.exe -n 3 -m 9 -v 2000 -B 127.0.0.2
:: -m 9/7/2/0 = response header mode (matches data_header_table.frame_type)
:: -B sets the source IP of the push link (simulating a device on its own IP;
::    -b sets the source port)
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_MB_401"
:: reg1/reg2 = 2000+k / 2100+k (big-endian decode, raw values — factor is not applied)
```

### http test (fixtures: dev_HTTP_001, HTTP port 60390)

```powershell
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:60390/ -Body '{"temp":12.5,"humidity":60}'
# -> {"ok":true,"device":"dev_HTTP_001","matched":2}
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_HTTP_001"
```

## 7. Current scope and extension points

| Capability | Status |
|---|---|
| custom_data TCP ingest (custom header + status/event frame routing) + EtherDB batch writes | ✅ implemented |
| status data (~1 Hz) submitted on arrival; event frames batched into the event table | ✅ implemented |
| MODBUS ingest (`conn_proto=modbus(2)`, 3 s polling + response headers 9/7/2/0) | ✅ implemented |
| HTTP ingest (`conn_proto=http(3)`, one server + JSON → plain INSERT SQL) | ✅ implemented |
| MQTT subscription ingest (`conn_proto=mttq(4)`) | ⏳ stub (`IngestStubs.h`) |
| publish queue → ZMQ PUB | ⏳ reserved (`PublishQueue`, copies + drops when enabled) |
| sequenceId parsing/validation | ⏳ reserved (placeholder only — not validated, not decoded) |
| factor scaling | ⏳ not implemented (raw values are stored) |

Extension points (the data flow stays the same; only add producers/consumers):

- **MQTT**: follow `HttpIngest` (parse → write queue) or `ModbusPoller`
  (request/subscribe → data on the unified port) and reuse the existing
  parse/write path.
- **ZMQ PUB**: implement the PUB send marked `TODO(zmq)` in
  `PublishQueue::run()`.
- **More event streams**: add one device_table row per stream (plus its
  `frame_type`) under the same `(ip, device_port)` — no code changes needed
  (event type 2, 3, …).
- **Protocol maintenance**: only the three SQLite tables change; the `field_type`
  code table is centralized in `ConfigDB.cpp::mapFieldType()`.

## 8. Notes and caveats

- Device matching order: exact `(ip, source port)` match → IP match when that IP
  has exactly one peer group; otherwise the data is dropped and a rate-limited
  warning is logged (watch the `drop` column in the statistics). When testing
  several devices on the same IP, bind the device's source port/IP (see
  `device_sim -b` / `modbus_sim -b/-B`).
- **One `(ip, source port)` may map to several device_table rows** (status +
  multiple event streams). They form a **peer group** routed by
  `data_header_table.frame_type`; each `device_id` is its own EtherDB table name
  (the event table).
- One `local_server_port` can serve several devices (distinguished by peer);
  `extraListenPorts` is only for testing/temporary ports. **The HTTP port must
  differ from every other listening port.**
- modbus register data is **big endian by default** (protocol convention): an
  `endian` of 0/absent means big endian, 1 selects little endian. custom_data
  keeps its original endian semantics (0 = little endian).
- `start_flag`/`end_flag` are meaningful for event frames only (they identify
  another parsing rule / another table inside one TCP stream); the two columns
  normally hold the same byte, and when they differ each is used as the leading /
  trailing delimiter.
- `data_proto_table.factor` is currently **not applied** — the database stores raw
  values; unsigned integers are widened to the next **signed** column type (to
  avoid overflow). HTTP JSON numbers are stored as-is.
- Missing HTTP fields are written as SQL NULL; `WHERE col IS NULL` matches them
  correctly (the list rendering path shows NULL as 0, a known property of EtherDB
  query responses having no NULL bitmap).
- On Windows, reconnecting from the same source port soon after a previous run
  hits TIME_WAIT (the 4-tuple) — use another `-b/-B` or wait ~2 minutes.
  `[parse].frameLenMode` has been removed (the custom header carries the frame
  length).
