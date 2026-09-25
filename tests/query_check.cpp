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
// query_check — tiny EtherDB query tool (verification helper)
//
// Usage:
//   query_check [host] [port] [db] [sql...]
// Defaults: 127.0.0.1 7040 adapter "SELECT * FROM dev_T100_001 LIMIT 10"
//
// Example:
//   query_check 127.0.0.1 7040 adapter "SELECT COUNT(*) FROM dev_T100_001"
// ============================================================================
#include <EtDBClient.h>

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
    std::string host = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t    port = (argc > 2) ? (uint16_t)atoi(argv[2]) : 7040;
    std::string db   = (argc > 3) ? argv[3] : "adapter";
    std::string sql  = (argc > 4) ? argv[4] : "SELECT * FROM dev_T100_001 LIMIT 10";

    ETDB::Client::EtDBClient client;
    if (!client.connect(host, port, "root", "etherdbdata", "")) {
        printf("ERROR: cannot connect to %s:%u\n", host.c_str(), (unsigned)port);
        return 2;
    }
    if (!db.empty()) {
        auto u = client.query("USE " + db);
        if (!u.error().empty()) {
            printf("ERROR: USE %s: %s\n", db.c_str(), u.error().c_str());
            return 2;
        }
    }

    auto r = client.query(sql);
    if (!r.error().empty()) {
        printf("ERROR: %s\n", r.error().c_str());
        client.close();
        return 1;
    }
    r.print();
    client.close();
    return 0;
}
