# ============================================================================
# tests — device simulator (end-to-end ingest testing) + query check tool
# ============================================================================
add_executable(device_sim device_sim.cpp)

if(WIN32)
    target_link_libraries(device_sim Ws2_32)
else()
    target_link_libraries(device_sim pthread)
endif()

# query_check — tiny query tool built against the EtherDB client SDK
add_executable(query_check query_check.cpp)
target_include_directories(query_check PRIVATE ${ETHERDB_SDK_DIR}/include)
set_target_properties(query_check PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set_property(TARGET query_check PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

if(WIN32)
    target_link_libraries(query_check "${ETHERDB_SDK_DIR}/lib/etherdb_client.lib" Ws2_32)
else()
    target_link_libraries(query_check "${ETHERDB_SDK_DIR}/lib/libetherdb_client.a" pthread)
endif()
