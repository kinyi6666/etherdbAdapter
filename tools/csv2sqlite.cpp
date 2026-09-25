// Copyright (c) 2026 Liu jinwei <kinyi6666@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ============================================================================
// csv2sqlite — import the etherAdapter configuration CSVs into a SQLite
// configuration database (device_table / data_header_table / data_proto_table).
//
// Usage:
//   csv2sqlite <config.db> <csvDir> [--force]
//
//   csvDir must contain device_table.csv, data_header_table.csv and
//   data_proto_table.csv (exported from the plant tooling; GBK or UTF-8).
//   Without --force the tool only creates missing tables and skips rows that
//   already exist (by primary key); with --force both tables are dropped and
//   rebuilt from the CSVs.
//
// The CSVs are imported as-is (header row skipped, comment/annotation rows —
// first column empty or non-numeric — skipped).
// ============================================================================
#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// Schema (kept in sync with config/schema.sql)
// ---------------------------------------------------------------------------
static const char* kSchema =
    "CREATE TABLE IF NOT EXISTS device_table ("
    "  device_id         TEXT PRIMARY KEY,"
    "  device_name       TEXT,"
    "  device_ip         TEXT NOT NULL,"
    "  device_port       INTEGER,"
    "  conn_proto        TEXT,"
    "  local_server_port INTEGER,"
    "  data_proto_id     INTEGER NOT NULL,"
    "  device_type       TEXT,"
    "  endian            INTEGER DEFAULT 0"
    ");"
    "CREATE TABLE IF NOT EXISTS data_header_table ("
    "  data_proto_id INTEGER PRIMARY KEY,"
    "  frame_type    INTEGER,"
    "  frame_len     INTEGER,"
    "  start_flag    TEXT,"          // may be '0x01'
    "  end_flag      TEXT,"
    "  endian        INTEGER DEFAULT 0,"
    "  slave_addr    INTEGER,"        // modbus register-read request
    "  fun_code      INTEGER,"
    "  start_addr    INTEGER,"
    "  addr_num      INTEGER"
    ");"
    "CREATE TABLE IF NOT EXISTS data_proto_table ("
    "  id            INTEGER PRIMARY KEY,"
    "  data_proto_id INTEGER NOT NULL,"
    "  field_name    TEXT NOT NULL,"
    "  unit          TEXT,"
    "  field_type    INTEGER NOT NULL,"
    "  byte_offset   INTEGER NOT NULL,"
    "  bit_offset    INTEGER DEFAULT 0,"
    "  bit_len       INTEGER DEFAULT 0,"
    "  precision     INTEGER DEFAULT 0,"
    "  factor        REAL    DEFAULT 1"
    ");";

// ============================================================================
// Helpers
// ============================================================================
static void trim(std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    size_t e = s.find_last_not_of(" \t\r\n");
    if (b == std::string::npos) { s.clear(); return; }
    s = s.substr(b, e - b + 1);
}

static std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur.push_back('"'); ++i; }
                else inQuotes = false;
            } else {
                cur.push_back(c);
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == ',') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    for (auto& s : out) trim(s);
    return out;
}

static bool isNumeric(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i >= s.size()) return false;
    bool dot = false;
    for (; i < s.size(); ++i) {
        if (s[i] == '.') { if (dot) return false; dot = true; continue; }
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}

static std::string sqlQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "''";
        else out.push_back(c);
    }
    out += "'";
    return out;
}

// Column value: NULL when empty, bare number when numeric, quoted text
// otherwise (hex flag strings like '0x01' stay TEXT — the reader parses them).
static std::string colValue(const std::string& s) {
    if (s.empty()) return "NULL";
    if (isNumeric(s)) return s;
    return sqlQuote(s);
}

static std::string integerValue(const std::string& s) {
    if (s.empty()) return "NULL";
    if (isNumeric(s)) return s;
    char* end = nullptr;
    long long v = strtoll(s.c_str(), &end, 0);   // "0x..." too
    if (end && *end == '\0') return std::to_string(v);
    return "NULL";
}

static bool execSql(sqlite3* db, const std::string& sql, std::string* err) {
    char* em = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &em) != SQLITE_OK) {
        if (err) *err = em ? em : "sql error";
        if (em) sqlite3_free(em);
        return false;
    }
    return true;
}

// GBK -> UTF-8 on Windows so Chinese text lands readable in the database.
#ifdef _WIN32
static std::string toUtf8(const std::string& in) {
    if (in.empty()) return in;
    int wlen = MultiByteToWideChar(CP_ACP, 0, in.c_str(), (int)in.size(), nullptr, 0);
    if (wlen <= 0) return in;
    std::wstring w((size_t)wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, in.c_str(), (int)in.size(), &w[0], wlen);
    int ulen = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, nullptr, 0, nullptr, nullptr);
    if (ulen <= 0) return in;
    std::string u((size_t)ulen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, &u[0], ulen, nullptr, nullptr);
    return u;
}
#else
static std::string toUtf8(const std::string& in) { return in; }
#endif

// ============================================================================
// CSV import
// ============================================================================
struct ImportStats { int rows = 0; int skipped = 0; };

static bool readLines(const std::string& path, std::vector<std::string>& lines) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string all;
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) all.append(buf, n);
    fclose(f);

    // Strip UTF-8 BOM
    if (all.size() >= 3 && (unsigned char)all[0] == 0xEF &&
        (unsigned char)all[1] == 0xBB && (unsigned char)all[2] == 0xBF) {
        all.erase(0, 3);
    }

    std::string cur;
    for (char c : all) {
        if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty()) lines.push_back(cur);
    return true;
}

static bool importDeviceTable(sqlite3* db, const std::string& csv, ImportStats* st, std::string* err) {
    std::vector<std::string> lines;
    if (!readLines(csv, lines)) { if (err) *err = "cannot open " + csv; return false; }

    for (size_t i = 0; i < lines.size(); ++i) {
        auto f = splitCsvLine(lines[i]);
        if (f.size() < 7) continue;
        // device_id is the key: skip the header row and annotation rows.
        const std::string& did = f[0];
        if (did.empty() || did == "device_id") { if (i > 0) ++st->skipped; continue; }

        std::string sql =
            "INSERT OR REPLACE INTO device_table"
            "(device_id, device_name, device_ip, device_port, conn_proto,"
            " local_server_port, data_proto_id, device_type, endian) VALUES(" +
            colValue(toUtf8(did)) + "," +
            colValue(toUtf8(f.size() > 1 ? f[1] : "")) + "," +
            colValue(toUtf8(f.size() > 2 ? f[2] : "")) + "," +
            integerValue(f.size() > 3 ? f[3] : "") + "," +
            colValue(toUtf8(f.size() > 4 ? f[4] : "")) + "," +
            integerValue(f.size() > 5 ? f[5] : "") + "," +
            integerValue(f.size() > 6 ? f[6] : "") + "," +
            colValue(toUtf8(f.size() > 7 ? f[7] : "")) + "," +
            integerValue(f.size() > 8 ? f[8] : "") + ")";
        if (!execSql(db, sql, err)) return false;
        ++st->rows;
    }
    return true;
}

static bool importHeaderTable(sqlite3* db, const std::string& csv, ImportStats* st, std::string* err) {
    std::vector<std::string> lines;
    if (!readLines(csv, lines)) { if (err) *err = "cannot open " + csv; return false; }

    for (size_t i = 0; i < lines.size(); ++i) {
        auto f = splitCsvLine(lines[i]);
        if (f.size() < 2) continue;
        if (!isNumeric(f[0])) { if (i > 0) ++st->skipped; continue; }

        std::string sql =
            "INSERT OR REPLACE INTO data_header_table"
            "(data_proto_id, frame_type, frame_len, start_flag, end_flag, endian,"
            " slave_addr, fun_code, start_addr, addr_num) VALUES(" +
            integerValue(f[0]) + "," + integerValue(f.size() > 1 ? f[1] : "") + "," +
            integerValue(f.size() > 2 ? f[2] : "") + "," +
            colValue(f.size() > 3 ? f[3] : "") + "," + colValue(f.size() > 4 ? f[4] : "") + "," +
            integerValue(f.size() > 5 ? f[5] : "") + "," +
            integerValue(f.size() > 6 ? f[6] : "") + "," +
            integerValue(f.size() > 7 ? f[7] : "") + "," +
            integerValue(f.size() > 8 ? f[8] : "") + "," +
            integerValue(f.size() > 9 ? f[9] : "") + ")";
        if (!execSql(db, sql, err)) return false;
        ++st->rows;
    }
    return true;
}

static bool importProtoTable(sqlite3* db, const std::string& csv, ImportStats* st, std::string* err) {
    std::vector<std::string> lines;
    if (!readLines(csv, lines)) { if (err) *err = "cannot open " + csv; return false; }

    for (size_t i = 0; i < lines.size(); ++i) {
        auto f = splitCsvLine(lines[i]);
        if (f.size() < 3) continue;
        if (!isNumeric(f[0])) { if (i > 0) ++st->skipped; continue; }

        std::string sql =
            "INSERT OR REPLACE INTO data_proto_table"
            "(id, data_proto_id, field_name, unit, field_type, byte_offset,"
            " bit_offset, bit_len, precision, factor) VALUES(" +
            integerValue(f[0]) + "," + integerValue(f[1]) + "," + colValue(toUtf8(f[2])) + "," +
            colValue(toUtf8(f.size() > 3 ? f[3] : "")) + "," +
            integerValue(f.size() > 4 ? f[4] : "") + "," + integerValue(f.size() > 5 ? f[5] : "") + "," +
            integerValue(f.size() > 6 ? f[6] : "") + "," + integerValue(f.size() > 7 ? f[7] : "") + "," +
            integerValue(f.size() > 8 ? f[8] : "") + "," +
            colValue(f.size() > 9 ? f[9] : "") + ")";
        if (!execSql(db, sql, err)) return false;
        ++st->rows;
    }
    return true;
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif

    std::string dbPath, csvDir;
    bool force = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--force") == 0) force = true;
        else if (dbPath.empty()) dbPath = argv[i];
        else if (csvDir.empty()) csvDir = argv[i];
    }

    if (dbPath.empty() || csvDir.empty()) {
        printf("Usage: csv2sqlite <config.db> <csvDir> [--force]\n");
        printf("  Imports device_table.csv / data_header_table.csv /\n");
        printf("  data_proto_table.csv from <csvDir> into the SQLite config db.\n");
        printf("  --force  drop + rebuild the tables before importing.\n");
        return 1;
    }

    if (!csvDir.empty() && (csvDir.back() == '/' || csvDir.back() == '\\'))
        csvDir.pop_back();

    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        printf("ERROR: cannot open/create %s: %s\n", dbPath.c_str(),
               db ? sqlite3_errmsg(db) : "unknown");
        if (db) sqlite3_close(db);
        return 2;
    }

    std::string err;
    if (force) {
        execSql(db, "DROP TABLE IF EXISTS device_table;"
                    "DROP TABLE IF EXISTS data_header_table;"
                    "DROP TABLE IF EXISTS data_proto_table;", nullptr);
    }
    if (!execSql(db, kSchema, &err)) {
        printf("ERROR: creating schema: %s\n", err.c_str());
        sqlite3_close(db);
        return 2;
    }

    struct Job { const char* file; const char* table;
                 bool (*fn)(sqlite3*, const std::string&, ImportStats*, std::string*); };
    const Job jobs[] = {
        { "device_table.csv",      "device_table",      importDeviceTable },
        { "data_header_table.csv", "data_header_table", importHeaderTable },
        { "data_proto_table.csv",  "data_proto_table",  importProtoTable  },
    };

    bool ok = true;
    for (const Job& j : jobs) {
        const std::string path = csvDir + "/" + j.file;
        ImportStats st;
        if (!j.fn(db, path, &st, &err)) {
            printf("ERROR: importing %s: %s\n", j.file, err.c_str());
            ok = false;
            break;
        }
        printf("  %-24s %5d row(s) imported, %d annotation row(s) skipped\n",
               j.table, st.rows, st.skipped);
    }

    sqlite3_close(db);
    if (!ok) return 3;

    printf("OK: configuration database written to %s\n", dbPath.c_str());
    return 0;
}
