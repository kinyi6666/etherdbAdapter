# tests/fixtures — end-to-end test configuration

These CSVs are a self-consistent version of the repository sample data
(`etherAdapter/*.csv`) so that every ingest path can be exercised:

  * device_table: four rows; **two of them share ONE endpoint**
      - dev_T100_001    custom_data(1)  ip 127.0.0.1, source port 10002,
                        port 60382, proto 100077   -> status table
      - dev_T100_001_E  custom_data(1)  same ip / same source port 10002,
                        proto 100078              -> EVENT table, selected by the
                        frame type (0xE) in the custom header
      - dev_MB_401      modbus(2)       ip 127.0.0.2, request port 10004,
                        port 60382, proto 100081
      - dev_HTTP_001    http(3)         ip 127.0.0.1, HTTP port 60390, proto 100082
  * data_header_table:
      - 100077: custom_data STATUS frame — frame_type 0xC, frame_len 73
        (payload bytes of ONE record), no delimiter.
      - 100078: custom_data EVENT frame  — frame_type 0xE, frame_len 62,
        start_flag 0x01 / end_flag 0x03 (event delimiters).
      - 100081: modbus — frame_type 9 (response header = MBAP+funCode+byteCount),
        request slave_addr=1 fun_code=3 start_addr=0 addr_num=2 (two 16-bit regs).
      - 100082: http — no framing fields needed.
  * data_proto_table:
      - 100077: item1 (INT16, offset 0)
      - 100078: item1/item2 (INT16, offsets 0/2)  <- different point count than the
        status stream, on the same device endpoint
      - 100081: reg1/reg2 (UINT16, offsets 0/2)   <- `factor 0.1` is IGNORED:
        the raw register value is stored in an INT column
      - 100082: temp (FLOAT32), humidity (FLOAT64) (JSON keys = field names)

## Run the tests

```bat
build\bin\Release\csv2sqlite.exe build\bin\Release\config\etherAdapter.db tests\fixtures --force
cd build\bin\Release
etherdb_dserver.exe -p 7040            (EtherDB server, separate window)
etherAdapter.exe                       (adapter; needs etherAdapter.cfg next to it)

:: 1) custom_data — status frames only (frameType 0xC, source port 10002)
device_sim.exe -n 2000 -i 1
::    -> dev_T100_001.item1 = 0..1999

:: 2) custom_data — status + event frames on ONE TCP connection (0xC / 0xE)
device_sim.exe -n 60 -i 20 -t C,E -l 73,62 -f 0,01 -e 0,03 -v 100
::    -> dev_T100_001   item1 = 100,102,104... (status frames)
::    -> dev_T100_001_E item1 = 101,103,105... (event frames, 2 fields)
::    -m 3 puts 3 records under one header (frameLen = 3 * record bytes)

:: 3) modbus — serve 3 register requests (header mode 9), push responses
::    -B binds the push source IP so the peer matches device_ip 127.0.0.2
modbus_sim.exe -n 3 -m 9 -v 2000 -B 127.0.0.2
::    header modes 7 / 2 / 0 also work:  -m 7 | -m 2 | -m 0

:: 4) http — POST a flat JSON object
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:60390/ -Body '{"temp":12.5,"humidity":60}'
```

## Verify

```bat
query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_T100_001"   :: grows per status frame
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001_E"        :: event rows
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_MB_401"            :: reg1=2000+k, reg2=2100+k (raw)
query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_HTTP_001"   :: grows per POST
```

Notes:
- Several rows share ip 127.0.0.1: the custom_data status + event rows share
  **127.0.0.1:10002** (routed by frame type — one TCP connection, two tables),
  while the HTTP device is matched by the HTTP server port.
- Re-running a simulator with the SAME source port immediately fails on Windows
  (TIME_WAIT on the 4-tuple) — use another `-b`/`-B` or wait ~2 minutes.