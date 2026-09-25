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
// device_sim — custom_data device simulator for end-to-end testing
//
// Connects to the etherAdapter ingest port as a device from device_table
// (default: dev_T100_001, device ip 127.0.0.1, source port 10002) and sends
// custom_data frames:
//
//   [frameType 1B][flag 1B][frameLen 2B LE][sequenceId 2B LE][payload ...][flag 1B?]
//
//   frameType  selects the measurement set (0xC = status, 0xE = event);
//   flag       is the delimiter byte of the EVENT stream (data_header_table
//              start_flag / end_flag, only meaningful for event frames);
//   frameLen   = records * record payload bytes — one header can carry several
//              records of data_header_table.frame_len bytes each;
//   sequenceId reserved for future use, sent as 0 here.
//
// payload[0..1] of every record = item1 (INT16 little endian, data_proto_table).
//
// The source port matters: the adapter matches devices by (ip, source port)
// from device_table, so the simulator binds it explicitly.
//
// Usage:
//   device_sim [options]
//     -h <host>        adapter host          (default 127.0.0.1)
//     -p <port>        adapter listen port   (default 60382)
//     -b <bindPort>    local source port     (default 10002; 0 = ephemeral)
//     -t <types>       frame type(s), hex without 0x, comma separated
//                      (default "C" = status; "C,E" alternates the status and
//                      the event stream on ONE connection)
//     -l <sizes>       record payload bytes per type = data_header_table
//                      .frame_len (default "73"; e.g. "73,62")
//     -f <flags>       leading event delimiter per type, hex (default 0)
//     -e <flags>       trailing delimiter per type, hex (default: same as -f)
//     -m <records>     records carried by one frame (default 1)
//     -n <frames>      frames to send        (default 1000)
//     -i <intervalMs>  interval between frames (default 10)
//     -v <startValue>  first item1 value     (default 0)
//     -q               quiet (no per-frame output)
//
// Example — status + event on the SAME TCP connection (the deployment case:
// one device, two point counts, two EtherDB tables):
//   device_sim -n 60 -t C,E -l 73,62 -f 0,01 -e 0,03 -v 100
// ============================================================================
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define CLOSE_SOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCK close
#endif

static void sleepMs(int ms) {
    if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// "C,E" -> {12, 14}; values missing for a type fall back to the last one.
static std::vector<int> parseIntList(const char* text, int base) {
    std::vector<int> out;
    const std::string s = text ? text : "";
    size_t b = 0;
    while (b <= s.size()) {
        size_t e = s.find(',', b);
        if (e == std::string::npos) e = s.size();
        if (e > b) out.push_back((int)strtol(s.substr(b, e - b).c_str(), nullptr, base));
        b = e + 1;
    }
    return out;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    std::string host = "127.0.0.1";
    int port      = 60382;
    int bindPort  = 10002;
    int frames    = 1000;
    int intervalMs = 10;
    int startValue = 0;
    int records   = 1;
    std::vector<int> typeList = parseIntList("C", 16);    // 0x0C = status frame
    std::vector<int> sizeList = parseIntList("73", 10);
    std::vector<int> flagList = parseIntList("0", 16);
    std::vector<int> endList;                              // empty = follow the flag
    bool quiet    = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) host = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) bindPort = atoi(argv[++i]);
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) intervalMs = atoi(argv[++i]);
        else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) startValue = atoi(argv[++i]);
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) records = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) typeList = parseIntList(argv[++i], 16);
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) sizeList = parseIntList(argv[++i], 10);
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) flagList = parseIntList(argv[++i], 16);
        else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) endList  = parseIntList(argv[++i], 16);
        else if (strcmp(argv[i], "-q") == 0) quiet = true;
        else {
            printf("unknown option: %s (see source header for usage)\n", argv[i]);
            return 1;
        }
    }
    if (typeList.empty()) typeList.push_back(0x0C);
    if (sizeList.empty()) sizeList.push_back(73);
    if (flagList.empty()) flagList.push_back(0);
    if (records < 1) records = 1;

    // ── one template frame per stream (type / record size / delimiters) ──
    const size_t headerSize = 6;   // frameType + flag + frameLen(2) + sequenceId(2)
    struct Stream {
        int    frameType   = 0;
        int    leadFlag    = 0;
        int    endFlag     = 0;
        int    recordBytes = 0;
        size_t payloadBytes = 0;
        std::string frame;
    };
    std::vector<Stream> streams;
    for (size_t k = 0; k < typeList.size(); ++k) {
        Stream st;
        st.frameType   = typeList[k];
        st.recordBytes = (int)((k < sizeList.size()) ? sizeList[k] : sizeList.back());
        st.leadFlag    = (int)((k < flagList.size()) ? flagList[k] : flagList.back());
        st.endFlag     = (k < endList.size()) ? endList[k] : st.leadFlag;
        st.payloadBytes = (size_t)st.recordBytes * (size_t)records;
        const size_t total = headerSize + st.payloadBytes + (st.endFlag ? 1 : 0);
        if (st.recordBytes < 2 || total > 65535) {
            printf("invalid frame geometry for type 0x%X (record %d B)\n",
                   (unsigned)st.frameType, st.recordBytes);
            return 1;
        }
        st.frame.assign(total, '\0');
        st.frame[0] = (char)(st.frameType & 0xFF);
        st.frame[1] = (char)(st.leadFlag & 0xFF);
        st.frame[2] = (char)(st.payloadBytes & 0xFF);
        st.frame[3] = (char)((st.payloadBytes >> 8) & 0xFF);
        st.frame[4] = 0;   // sequenceId — reserved, ignored by the adapter
        st.frame[5] = 0;
        if (st.endFlag) st.frame[total - 1] = (char)(st.endFlag & 0xFF);
        streams.push_back(std::move(st));
    }

    int s = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        printf("ERROR: socket() failed\n");
        return 2;
    }

    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));

    if (bindPort > 0) {
        sockaddr_in local = {};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons((unsigned short)bindPort);
        if (bind(s, (sockaddr*)&local, sizeof(local)) != 0) {
            printf("ERROR: cannot bind source port %d (in use?)\n", bindPort);
            CLOSE_SOCK(s);
            return 2;
        }
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        printf("ERROR: invalid host %s\n", host.c_str());
        CLOSE_SOCK(s);
        return 2;
    }

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        printf("ERROR: cannot connect to %s:%d (is etherAdapter running?)\n",
               host.c_str(), port);
        CLOSE_SOCK(s);
        return 2;
    }

    printf("device_sim: connected to %s:%d (source port %d), %zu stream(s), %d frame(s), "
           "%d record(s)/frame\n",
           host.c_str(), port, bindPort, streams.size(), frames, records);
    if (!quiet) {
        for (const Stream& st : streams) {
            printf("  stream 0x%02X: [type][flag 0x%02X][len %zu][seqId 0][payload %zu B][0x%02X], "
                   "item1 INT16 LE of every record\n",
                   (unsigned)st.frameType, (unsigned)st.leadFlag, st.payloadBytes,
                   st.payloadBytes, (unsigned)st.endFlag);
        }
    }

    int sent = 0;
    int64_t next = (int64_t)startValue;
    for (int i = 0; i < frames; ++i) {
        Stream& st = streams[(size_t)i % streams.size()];
        for (int r = 0; r < records; ++r) {
            const int v = (int)next++;
            char* p = &st.frame[headerSize + (size_t)r * (size_t)st.recordBytes];

            // item1: INT16 little endian at record offset 0
            p[0] = (char)(v & 0xFF);
            p[1] = (char)((v >> 8) & 0xFF);
            // fill the rest of the record with a ramp so mis-decoding is visible
            for (int k = 2; k < st.recordBytes; ++k)
                p[k] = (char)((i + k + r) & 0xFF);
        }

        const int n = send(s, st.frame.data(), (int)st.frame.size(), 0);
        if (n != (int)st.frame.size()) {
            printf("ERROR: send() returned %d (expected %zu) at frame %d\n",
                   n, st.frame.size(), i);
            break;
        }
        ++sent;

        if (!quiet && (i < 5 || (i + 1) % 100 == 0))
            printf("  frame %5d: type 0x%02X item1 = %d\n", i,
                   (unsigned)st.frameType, (short)(next - records));

        sleepMs(intervalMs);
    }

    sleepMs(100);
    CLOSE_SOCK(s);

    printf("device_sim: %d frame(s) sent (%lld record(s))\n", sent, (long long)(next - startValue));
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
