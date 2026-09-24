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
// device_sim — raw_data device simulator for end-to-end testing
//
// Connects to the etherAdapter ingest port as a device from device_table
// (default: dev_T100_001, device ip 127.0.0.1, source port 10002) and sends
// frames matching data_header_table 100077:
//
//   [start 0x01][type 0x00][frame_len 2B LE][payload ...][end 0x03]
//   frame_len counts the WHOLE frame (73 bytes by default)
//   payload[0..1] = item1 (INT16 little endian, data_proto_table)
//
// The source port matters: the adapter matches devices by (ip, source port)
// from device_table, so the simulator binds it explicitly.
//
// Usage:
//   device_sim [options]
//     -h <host>        adapter host          (default 127.0.0.1)
//     -p <port>        adapter listen port   (default 60382)
//     -b <bindPort>    local source port     (default 10002; 0 = ephemeral)
//     -n <frames>      frames to send        (default 1000)
//     -i <intervalMs>  interval between frames (default 10)
//     -v <startValue>  first item1 value     (default 0)
//     -l <frameLen>    frame_len field       (default 73)
//     -m <mode>        frame_len semantics: "total" (default, counts the whole
//                      frame) or "payload" (counts the payload only)
//     -q               quiet (no per-frame output)
// ============================================================================
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>

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
    int frameLen  = 73;
    bool payloadMode = false;
    bool quiet    = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) host = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) bindPort = atoi(argv[++i]);
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) frames = atoi(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) intervalMs = atoi(argv[++i]);
        else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) startValue = atoi(argv[++i]);
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) frameLen = atoi(argv[++i]);
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) payloadMode = (strcmp(argv[++i], "payload") == 0);
        else if (strcmp(argv[i], "-q") == 0) quiet = true;
        else {
            printf("unknown option: %s (see source header for usage)\n", argv[i]);
            return 1;
        }
    }

    // frame_len field value vs. actual frame size
    const int totalLen = payloadMode ? (frameLen + 5) : frameLen;
    if (frameLen < 1 || totalLen < 5 || totalLen > 4096) {
        printf("invalid frame length %d (total %d must be 5..4096)\n", frameLen, totalLen);
        return 1;
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

    printf("device_sim: connected to %s:%d (source port %d), sending %d frame(s) of %d bytes (frame_len=%d, %s semantics)\n",
           host.c_str(), port, bindPort, frames, totalLen, frameLen,
           payloadMode ? "payload" : "total");
    if (!quiet) {
        printf("  frame: [0x01][0x00][len %d][payload %d B][0x03], item1 INT16 LE at payload[0]\n",
               frameLen, totalLen - 5);
    }

    // Build the frame template: [0x01][type][len LE][payload][0x03]
    std::string frame((size_t)totalLen, '\0');
    frame[0] = (char)0x01;
    frame[1] = (char)0x00;
    frame[2] = (char)(frameLen & 0xFF);
    frame[3] = (char)((frameLen >> 8) & 0xFF);
    frame[totalLen - 1] = (char)0x03;

    int sent = 0;
    for (int i = 0; i < frames; ++i) {
        const int v = startValue + i;

        // item1: INT16 little endian at payload offset 0 (frame offset 4)
        frame[4] = (char)(v & 0xFF);
        frame[5] = (char)((v >> 8) & 0xFF);

        // fill the rest of the payload with a ramp so mis-decoding is visible
        for (int k = 6; k <= totalLen - 2; ++k)
            frame[k] = (char)((i + k) & 0xFF);

        int n = send(s, frame.data(), (int)frame.size(), 0);
        if (n != totalLen) {
            printf("ERROR: send() returned %d (expected %d) at frame %d\n",
                   n, totalLen, i);
            break;
        }
        ++sent;

        if (!quiet && (i < 5 || (i + 1) % 100 == 0))
            printf("  frame %5d: item1 = %d\n", i, (short)v);

        sleepMs(intervalMs);
    }

    sleepMs(100);
    CLOSE_SOCK(s);

    printf("device_sim: %d frame(s) sent\n", sent);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
