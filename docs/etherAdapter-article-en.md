# One TCP Connection, Two Samplings: Feeding Plant Data into EtherDB with etherAdapter

> The devices stay as they are. The database stays as it is. In between you add
> one config-driven adapter: devices speak their own protocol, EtherDB stores
> raw values, column by column.

---

## 1. Five real-world annoyances

Anyone who has wired plant equipment into a database has met all of these:

1. **Every device speaks a different dialect.** Each new device means new parsing
   code, a rebuild, and downtime on site.
2. **One device has two kinds of data.** Normally it samples 20 points at 1 Hz.
   When an event fires, the rate goes up and the point count may drop to 8 — or
   change to a different set entirely. The classic answer is two connections and
   two programs, with changes on both the device and the gateway.
3. **Frame length is a guess.** With no standard header, is the length field the
   whole frame or just the payload? Probing at runtime gets it wrong, and a wrong
   guess means misaligned frames and lost data.
4. **Batching adds latency.** A 1 Hz status point should land immediately, but
   batching waits for 4,000 rows — the dashboard lags.
5. **Scaling throws away the raw value.** The moment you multiply by a factor,
   the code the device actually reported is gone. Calibration, traceability and
   later re-scaling lose their reference.

etherAdapter exists for exactly those five problems: **one process, one
configuration, one TCP connection — plant data into EtherDB as it was sent.**

---

## 2. One connection, two samplings — selected by frameType

A device reports both data kinds over **one TCP connection**:

- **status frames** — periodic sampling, typically 1 Hz, fixed points;
- **event frames** — burst sampling, higher rate, a different (often smaller)
  point count.

They are told apart by the first byte of a **6-byte custom header**:

```
[frameType 1B][flag 1B][frameLen 2B][sequenceId 2B][payload ...][flag 1B?]
```

- `frameType` — which measurement set this is. To the adapter it **is another
  logical device**: its own field table, its own parsing rule, its own EtherDB
  table.
- `flag` — the event stream delimiter. It is meaningful for **event frames only**:
  it is how the adapter recognises "a different parsing rule starts here" inside a
  shared TCP stream. Status frames carry no delimiter and the byte is ignored.
- `frameLen` — payload bytes in this frame. One header can carry several records
  (`frameLen / record size`), so multiple samples share a single header.
- `sequenceId` — reserved. It is currently **not validated and not decoded**;
  it is there for future extensions.

On the configuration side you simply list more rows for the same endpoint:

| device_id | ip | source port | data_proto_id | frame_type |
|---|---|---|---|---|
| dev_T100_001 | 127.0.0.1 | 10002 | 100077 | 0xC (status) |
| dev_T100_001_E | 127.0.0.1 | 10002 | 100078 | 0xE (event) |

The first row is the status device, the rest are event devices. **The device
changes one byte in its header** and the data lands in `dev_T100_001` and
`dev_T100_001_E` respectively.

> Why it matters: adding an event stream needs no change to connection handling
> in the firmware and no extra listening port on the gateway side.

---

## 3. Status data is not batched — it is submitted on arrival

Most status points are 1 Hz and not worth waiting for. So:

- **status frames: every received chunk is flushed immediately** — no batching,
  no timer, fastest possible time-to-dashboard;
- **event frames: written in batches** — bursts are high-rate and high-volume,
  where batching is the right throughput trade.

Both coexist in the same pipeline; getting it right is a matter of configuring the
device rows correctly.

---

## 4. Raw values only — factors are set aside

A time-series database earns its keep by faithfully preserving the field, so the
adapter performs **no unit conversion**:

- `data_proto_table.factor` is currently **not applied**; if a non-1 factor is
  configured, the adapter logs one warning at start-up and moves on;
- integer fields are stored as the raw code value (with an appropriate column width
  for signed/unsigned types), `FLOAT32/FLOAT64` as their raw value;
- want engineering units? Multiply at query time, or in the layer above — the raw
  value is always still there.

---

## 5. The path from device to EtherDB

```
device ──TCP──> muduo network layer ──lock-free queue──> single-thread parser
                                                              │
                                    RowBatch ──┬──────────────┘
                                               ▼
                                 write queue ──> column-bound prepared INSERT ──> EtherDB
                                               └─> publish queue ──> ZMQ PUB (reserved)
```

A few deliberate choices:

- **One parser thread.** All protocol parsing happens on a single thread, fully
  config-driven and lock-free; network threads only move bytes into the queue.
- **Column-bound batch inserts.** One prepared `INSERT` per device, one contiguous
  buffer per column, many rows bound and executed at once. Slow HTTP data uses
  plain SQL so it never occupies a prepared statement.
- **Configuration is the product.** Three SQLite tables (devices / frame headers /
  fields) decide everything: a new device is a new row, a protocol change is an
  edited row.

EtherDB's own numbers are published (see `EtherDB/docs/EtherDB-performance.md`):
about **2.14M rows/s** for batch inserts on one machine, **6.18M rows/s**
aggregated across 8 writer processes, `COUNT(*)` in **0.34 ms**, paging through
tens of millions of rows in **0.82 ms**, and a **1.07 MB** single-file server.
The adapter's job is to feed that path correctly, not to change it.

---

## 6. A complete end-to-end run, verified locally

Using the fixtures shipped in the repository:

```bat
csv2sqlite.exe config\etherAdapter.db tests\fixtures --force
etherdb_dserver.exe -p 7040
etherAdapter.exe

:: status and event frames alternating on ONE TCP connection
device_sim.exe -n 60 -i 5 -t C,E -l 73,62 -f 0,01 -e 0,03 -v 100
```

Querying the database directly (`query_check.exe`):

| table | contents | meaning |
|---|---|---|
| `dev_T100_001` | item1 = 100,102,104… | status frames, one field, **flushed on arrival** |
| `dev_T100_001_E` | item1 = 101,103,105… plus item2 | event frames, **two points, a different table** |
| `dev_MB_401` | reg1=2000+k, reg2=2100+k | MODBUS polled every 3 s, **raw register values** |
| `dev_HTTP_001` | temp=12.5, humidity=60 | HTTP POST of JSON, plain INSERT SQL |

The runtime statistics show clean routing:
`frames +60 rows +60 (event +30) written +61 drop 0 err 0`.

The same fixtures also cover several records under one header
(`device_sim -m 3`), all four MODBUS response headers (9/7/2/0), HTTP rows with
missing fields stored as NULL, and a `sequenceId` that can hold anything without
affecting parsing.

---

## 7. Who it is for

- Sites where the devices **cannot be changed** (or cannot be changed again) but
  the data still has to reach a time-series database;
- A device that needs **both periodic status and burst events** without opening a
  second connection;
- Edge deployments that want a **small footprint** (both the adapter and EtherDB
  are single binaries with no external dependencies);
- Anyone who wants **raw values preserved** and calibration left for later.

---

## 8. Summary

etherAdapter does the dirtiest part of the last mile: **translating whatever the
device says into EtherDB columns, verbatim and with minimal latency.**

- One TCP connection, two samplings — selected by `frameType`;
- status data submitted on arrival, event data batched into its own table;
- raw values stored, factors deferred;
- configuration as the integration surface: three tables, one process.

The devices stay as they are, the database stays as it is, and one process sits in
between.
