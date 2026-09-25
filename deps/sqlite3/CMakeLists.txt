# SQLite3 — amalgamation compiled as a static library (no external DLL needed)
add_library(sqlite3 STATIC sqlite3.c)

target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_compile_definitions(sqlite3 PRIVATE
        _CRT_SECURE_NO_WARNINGS
        SQLITE_THREADSAFE=1
        SQLITE_OMIT_LOAD_EXTENSION)
else()
    target_compile_definitions(sqlite3 PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_OMIT_LOAD_EXTENSION)
    target_link_libraries(sqlite3 PUBLIC pthread dl)
endif()

# sqlite3.c is huge; skip warnings for it
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /w)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()
