// ============================================================================
// etherAdapter — entry point
//
//   etherAdapter [-c <cfg file>]
//
// Default config file: <exe dir>/etherAdapter.cfg (a ready-to-edit example
// lives in the repository under config/etherAdapter.cfg).
// ============================================================================
#include "AdapterApp.h"
#include "AdapterConfig.h"

#include <cstdio>
#include <cstring>
#include <string>

static void usage(const char* prog) {
    printf("etherAdapter — device sampling data adapter for EtherDB\n");
    printf("Usage: %s [options]\n", prog);
    printf("  -c <file>   Config file path (default: <exe dir>/etherAdapter.cfg)\n");
    printf("  -h          Print this help\n");
    printf("  -V          Print version\n");
}

int main(int argc, char* argv[]) {
    std::string cfgFile;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            cfgFile = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("etherAdapter version 0.1.0\n");
            return 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (cfgFile.empty()) {
        const std::string exeDir = EtherAdapter::getExeDir();
        cfgFile = (exeDir.empty() ? std::string(".") : exeDir) + "/etherAdapter.cfg";
    }

    EtherAdapter::AdapterApp app;
    return app.run(cfgFile);
}
