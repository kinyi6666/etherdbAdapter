# ============================================================================
# src — wraps the reused muduo-style base/net libraries and the adapter app.
#
# On Windows the wincompat directory is added to the include path and
# posix_compat.h is force-included into every translation unit (same approach
# as the EtherDB dserver build): winsock2 ordering, NOMINMAX, missing POSIX
# names (ssize_t, sleep, strcasecmp, ...).
# ============================================================================
if(WIN32)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/wincompat)
    add_compile_options(/FI"${CMAKE_CURRENT_SOURCE_DIR}/wincompat/posix_compat.h")
endif()

add_subdirectory(base)
add_subdirectory(net)
add_subdirectory(adapter)
