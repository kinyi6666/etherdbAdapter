#!/bin/sh
# ============================================================================
# etherAdapter — Linux build
#
#   ./scripts/build_linux.sh            (Release by default)
#
# Output: build/bin/etherAdapter (+ libbase.so / libnet.so),
#         build/bin/csv2sqlite, build/bin/device_sim
# ============================================================================
set -e
cd "$(dirname "$0")/.."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

echo
echo "Build OK. Binaries in build/bin:"
ls -1 build/bin
