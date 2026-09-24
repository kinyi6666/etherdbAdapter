-- ============================================================================
-- etherAdapter configuration database schema (SQLite)
--
-- This mirrors what tools/csv2sqlite.cpp creates. The adapter itself opens
-- the database READ-ONLY; maintain it with the plant tooling, csv2sqlite, or
-- any SQLite client.
--
-- Three tables (same names as the plant exports):
--   device_table      one row per device (ip/port/protocol/proto id)
--   data_header_table one row per data_proto_id: framing (flags/len/endian)
--   data_proto_table  one row per sensor field of a data_proto_id
-- ============================================================================

CREATE TABLE IF NOT EXISTS device_table (
    channelID         INTEGER PRIMARY KEY,  -- measurement point / channel id
    device_id         TEXT,                 -- device id (EtherDB table name)
    device_name       TEXT,                 -- display name
    device_ip         TEXT NOT NULL,        -- device ip (peer address)
    device_port       INTEGER,              -- device source port (raw_data) /
                                            -- request port (modbus); empty for http
    conn_proto        TEXT,                 -- raw_data(1) modbus(2) http(3) mttq(4)
    local_server_port INTEGER NOT NULL,     -- ingest port served by the adapter
    data_proto_id     INTEGER NOT NULL,     -- protocol id -> data_header/data_proto
    device_type       TEXT,
    endian            INTEGER DEFAULT 0     -- 0 = little, 1 = big (0 = use header)
);

CREATE TABLE IF NOT EXISTS data_header_table (
    data_proto_id INTEGER PRIMARY KEY,
    frame_type    INTEGER,        -- raw_data: status/event frame type (informational);
                                  -- modbus:   response header length 9/7/2/0
    frame_len     INTEGER,        -- frame length (see [parse].frameLenMode)
    start_flag    TEXT,           -- frame start byte, e.g. '0x01' or 1
    end_flag      TEXT,           -- frame end byte, e.g. '0x03' or 3
    endian        INTEGER DEFAULT 0,  -- raw_data: 0=little/1=big; modbus: 0/absent=big, 1=little
    reserve       TEXT,
    -- Modbus register-read request (conn_proto = modbus(2) devices only):
    unit          INTEGER,        -- slave / unit id
    fun_code      INTEGER,        -- function code (e.g. 3 = read holding registers)
    data          INTEGER,        -- start register address
    read_count    INTEGER         -- number of registers to read
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
    byte_offset   INTEGER NOT NULL,  -- offset inside the frame payload
    bit_offset    INTEGER DEFAULT 0, -- sub-byte field extraction
    bit_len       INTEGER DEFAULT 0, -- 0 = whole bytes
    precision     INTEGER DEFAULT 0, -- decimal digits (informational)
    factor        REAL    DEFAULT 1  -- physical value = raw * factor
);

-- ---------------------------------------------------------------------------
-- The adapter inserts parsed data into one EtherDB table per device
-- (<device_id>: ts TIMESTAMP + one column per data_proto_table field); these
-- tables are created automatically when [etherdb].createTables = true.
-- ---------------------------------------------------------------------------
