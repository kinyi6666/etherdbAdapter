# tests/fixtures — end-to-end test configuration

These CSVs are the repository sample data (etherAdapter/*.csv) with the
data_proto_id inconsistency fixed so all three ingest paths can be tested:

  * device_table: three devices
      - dev_T100_001  raw_data(1)  ip 127.0.0.1, source port 10002, port 60382, proto 100077
      - dev_MB_401    modbus(2)    ip 127.0.0.2, request port  10004, port 60382, proto 100081
      - dev_HTTP_001  http(3)      ip 127.0.0.1, HTTP port     60390,         proto 100082
  * data_header_table:
      - 100077: raw frame 73 bytes total (frame_len counts the whole frame),
        start 0x01, end 0x03, little endian.
      - 100081: modbus — frame_type 9 (response header = MBAP+funCode+byteCount),
        request unit=1 fun_code=3 data=0 read_count=2 (two 16-bit registers).
      - 100082: http — no framing fields needed.
  * data_proto_table:
      - 100077: item1 (INT16, offset 0)                      (sample export carried
        100078 instead; the field table must join on the SAME data_proto_id)
      - 100081: reg1/reg2 (UINT16, offsets 0/2, factor 0.1)  -> DOUBLE columns
      - 100082: temp (FLOAT32), humidity (FLOAT64)           (JSON keys = field names)

## Run the tests

```bat
build\bin\Release\csv2sqlite.exe build\bin\Release\config\etherAdapter.db tests\fixtures --force
cd build\bin\Release
etherdb_dserver.exe -p 7040            (EtherDB server, separate window)
etherAdapter.exe                       (adapter; needs etherAdapter.cfg next to it)

:: 1) raw_data — 2000 frames, item1 0..1999
device_sim.exe -n 2000 -i 1

:: 2) modbus — serve 3 register requests (header mode 9), push responses
::    -B binds the push source IP so the peer matches device_ip 127.0.0.2
modbus_sim.exe -n 3 -m 9 -v 2000 -B 127.0.0.2
::    header modes 7 / 2 / 0 also work:  -m 7 | -m 2 | -m 0

:: 3) http — POST a flat JSON object
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:60390/ -Body '{"temp":12.5,"humidity":60}'
```

## Verify

```bat
query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_T100_001"   :: 2000
query_check.exe 127.0.0.1 7040 adapter "SELECT * FROM dev_MB_401"            :: reg1=(2000+k)*0.1
query_check.exe 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_HTTP_001"   :: grows per POST
```

Notes:
- Several devices share ip 127.0.0.1 (raw + http): HTTP requests are matched by
  the HTTP server port, modbus push data by the source IP 127.0.0.2.
- Re-running a simulator with the SAME source port immediately fails on Windows
  (TIME_WAIT on the 4-tuple) — use another `-b`/`-B` or wait ~2 minutes.