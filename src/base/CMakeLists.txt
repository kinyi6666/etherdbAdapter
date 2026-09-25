
#set(base_SRCS
#    Mutex.h
#    Mutex.cpp
#  )
  
 add_definitions(-DNO_FSEEKO)
 add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
 add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
 add_definitions(-DZLIB_DLL)
 add_definitions(-Dbase_EXPORTS)
aux_source_directory(. base_SRCS)
aux_source_directory(./zlib Zlib_Src)
add_library(base SHARED ${base_SRCS}
						${Zlib_Src})

set(LIBRARY_OUTPUT_PATH ../../bin)
set_target_properties(base PROPERTIES OUTPUT_NAME "base") 

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION ${PROJECT_BINARY_DIR}/../include/am/base)

#add_subdirectory(tests)
  
  
  
  
  
  
  
  
  
 
