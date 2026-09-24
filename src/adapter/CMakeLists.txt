# ============================================================================
# etherAdapter application
#   device --> muduo TCP server --> ingest queue --> parser thread
#          --> write queue  --> EtherDB batch writer
#          --> publish queue --> ZMQ PUB (reserved / stub)
# ============================================================================
set(ADAPTER_SRCS
    AdapterConfig.cpp
    AdapterLog.cpp
    ConfigDB.cpp
    FrameCodec.cpp
    IngestServer.cpp
    ParserWorker.cpp
    EtherDBWriter.cpp
    PublishQueue.cpp
    IngestStubs.cpp
    AdapterApp.cpp
    main.cpp
)

set(ADAPTER_HEADERS
    AdapterConfig.h
    AdapterLog.h
    ConfigDB.h
    DeviceModel.h
    Pipeline.h
    FrameCodec.h
    IngestServer.h
    ParserWorker.h
    EtherDBWriter.h
    PublishQueue.h
    IngestStubs.h
    AdapterApp.h
)

# The EtherDB client SDK requires C++17 (std::string_view).
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src            # <base/...> and <net/...>
    ${ETHERDB_SDK_DIR}/include
)

add_executable(etherAdapter ${ADAPTER_SRCS} ${ADAPTER_HEADERS})

target_link_libraries(etherAdapter
    net
    base
    sqlite3
)

# ── EtherDB client SDK ───────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(etherAdapter
        "${ETHERDB_SDK_DIR}/lib/etherdb_client.lib"
        Ws2_32)
else()
    target_link_libraries(etherAdapter
        "${ETHERDB_SDK_DIR}/lib/libetherdb_client.a"
        pthread)
endif()

# The SDK is built with the dynamic CRT (/MD); keep this target consistent.
if(MSVC)
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                     CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
        if(${flag_var} MATCHES "/MT")
            string(REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
    set_property(TARGET etherAdapter PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()
