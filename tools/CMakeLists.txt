# ============================================================================
# tools — configuration helpers
# ============================================================================
add_executable(csv2sqlite csv2sqlite.cpp)
target_link_libraries(csv2sqlite sqlite3)

if(WIN32)
    target_link_libraries(csv2sqlite Ws2_32)
endif()
