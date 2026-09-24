#set(net_SRCS
#    EventLoop.h
#    EventLoop.cpp
#  )
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-Dnet_EXPORTS)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}     # net ????
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # src ????®LæÈ? base ö´????
)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../bin)
link_directories(${CMAKE_CURRENT_SOURCE_DIR})
aux_source_directory(. net_SRCS)
add_library(net SHARED ${net_SRCS})

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
target_link_libraries(net base )
else ()
target_link_libraries(net base Ws2_32 Kernel32 )
endif ()
set(LIBRARY_OUTPUT_PATH ../../bin)
set_target_properties(net PROPERTIES OUTPUT_NAME "net")



file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION ${PROJECT_BINARY_DIR}/../include/am/net)

#add_subdirectory(tests)

