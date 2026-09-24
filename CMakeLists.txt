# ============================================================================
# etherAdapter — device sampling data adapter for EtherDB
#
#   device --> muduo (TCP ingest) --> moodycamel queue --> parser thread
#          --> EtherDB write queue --> EtherDB   (raw_data protocol path)
#          --> publish queue        --> ZMQ PUB (reserved, stubbed today)
#
# Build (Windows / VS2022):
#   scripts\build_windows.bat
# Build (Linux / gcc):
#   mkdir build && cd build && cmake .. && make -j
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(etherAdapter CXX C)

# ── Build type: default to Release ──────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── EtherDB client SDK location (include/ + lib/) ────────────────────────────
# Defaults to the in-tree EtherDB build output; override with
#   cmake -DETHERDB_SDK_DIR=<path>/EtherDB/src/bin/sdk ..
set(ETHERDB_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../EtherDB/src/bin/sdk"
    CACHE PATH "EtherDB client SDK directory (include/ + lib/)")

# ── Global compile flags ─────────────────────────────────────────────────────
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS
                    -D_CRT_NONSTDC_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS
                    -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    if(MSVC)
        add_compile_options(/utf-8)
        add_compile_options(/wd4251 /wd4200 /wd4275 /wd4996 /wd4244 /wd4267 /wd4305 /wd4101)
    endif()
else()
    add_definitions(-DNO_FSEEKO -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

# All executables/shared libs land in <build>/bin so that base.dll / net.dll
# sit next to etherAdapter.exe at runtime.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(deps/sqlite3)
add_subdirectory(src)          # base + net + adapter
add_subdirectory(tools)
add_subdirectory(tests)
