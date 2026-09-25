-- ============================================================================
-- etherAdapter configuration database schema (SQLite)
--
-- This mirrors what tools/csv2sqlite.cpp creates. The adapter itself opens
-- the database READ-ONLY; maintain it with the plant tooling, csv2sqlite, or
-- any SQLite client.
--
-- Three tables (same names as the plant exports):
--   device_table      one row per device (ip/port/protocol/proto id); rows
--                     sharing an (device_ip, device_port) endpoint form ONE peer
--                     group: the first (lowest data_proto_id) is the periodic
--                     STATUS device, the others are EVENT devices selected by
--                     the frame type carried in the custom_data header.
--   data_header_table one row per data_proto_id: framing + frame type
--   data_proto_table  one row per sensor field of a data_proto_id
-- ============================================================================

CREATE TABLE IF NOT EXISTS device_table (
    device_id         TEXT PRIMARY KEY,  -- device id (EtherDB table name)
    device_name       TEXT,              -- display name
    device_ip         TEXT NOT NULL,     -- device ip (peer address)
    device_port       INTEGER,           -- device source/bound port (peer port);
                                         -- request port for modbus; empty for http
    conn_proto        TEXT,              -- custom_data(1) modbus(2) http(3) mttq(4)
    local_server_port INTEGER,           -- ingest port served by the adapter
    data_proto_id     INTEGER NOT NULL,  -- protocol id -> data_header/data_proto
    device_type       TEXT,
    endian            INTEGER DEFAULT 0  -- 0 = little, 1 = big (0 = use header)
);

CREATE TABLE IF NOT EXISTS data_header_table (
    data_proto_id INTEGER PRIMARY KEY,
    frame_type    INTEGER,   -- custom_data: frame type byte in the custom header
                             --   (picks status vs event measurement set)
                             -- modbus:      response header length 9/7/2/0
    frame_len     INTEGER,   -- custom_data: payload bytes of ONE record
    start_flag    TEXT,      -- frame delimiter byte; start_flag == end_flag
                             --   (a frame opens AND closes with this byte)
    end_flag      TEXT,      -- same byte as start_flag
    endian        INTEGER DEFAULT 0,  -- custom_data: 0=little/1=big;
                                      -- modbus: 0/absent=big, 1=little
    -- Modbus register-read request (conn_proto = modbus(2) devices only):
    slave_addr    INTEGER,   -- modbus slave / unit id
    fun_code      INTEGER,   -- function code (e.g. 3 = read holding registers)
    start_addr    INTEGER,   -- register start address
    addr_num      INTEGER    -- number of registers to read
);

CREATE TABLE IF NOT EXISTS data_proto_table (
    id            INTEGER PRIMARY KEY,
    data_proto_id INTEGER NOT NULL,  -- protocol id (joins data_header_table)
    field_name    TEXT NOT NULL,     -- sensor value name -> EtherDB column name
    unit          TEXT,
    field_type    INTEGER NOT NULL,  -- see ConfigDB.cpp mapFieldType():
                                     --   0 BOOL, 1 INT16, 2 INT32, 3 FLOAT32,
                                     --   4 FLOAT64, 5 INT8, 6 UINT8, 7 UINT16,
                                     --   8 UINT32, 9 INT64, 10 UINT64
    byte_offset   INTEGER NOT NULL,  -- offset inside one record payload
    bit_offset    INTEGER DEFAULT 0, -- sub-byte field extraction
    bit_len       INTEGER DEFAULT 0, -- 0 = whole bytes
    precision     INTEGER DEFAULT 0, -- decimal digits (informational)
    factor        REAL    DEFAULT 1  -- INFORMATIONAL ONLY: factors are ignored,
                                     -- the database stores raw values
);

-- ---------------------------------------------------------------------------
-- The adapter inserts parsed data into one EtherDB table per device
-- (<device_id>: ts TIMESTAMP + one column per data_proto_table field); these
-- tables are created automatically when [etherdb].createTables = true.
-- Status rows and event rows share the ts + field layout but live in different
-- tables (the status device_id and each event device_id).
-- ---------------------------------------------------------------------------
