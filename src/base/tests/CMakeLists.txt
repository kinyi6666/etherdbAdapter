
#set(baseTest_Src
#    Main.cpp
#)
aux_source_directory(. baseTest_Src)
add_executable(basetest 
              ${baseTest_Src}
              )
 


IF (CMAKE_SYSTEM_NAME MATCHES "Linux") 
target_link_libraries(basetest base)
else ()
target_link_libraries(basetest base)
ENDIF ()
  
  
  
  
  
  
  
 