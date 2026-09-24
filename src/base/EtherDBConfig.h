
#ifndef __EtherDB_Config_H_
#define __EtherDB_Config_H_

#include <stdio.h>
#include <stdint.h>
#include <string>


#if (defined(_WIN32) || defined(_WIN64))

////////////////////Windows//////////////////////////

#ifdef base_EXPORTS //_DLL//
#define BASE_API __declspec(dllexport)
#else
#define BASE_API __declspec(dllimport)
#endif

//#if defined(_MSC_VER) && (_MSC_VER >= 1700) && !defined(AM_ENABLE_CPP11)
//#define AM_ENABLE_CPP11
//#endif

#define snprintf _snprintf 
// Windows native build (VS2022/CMake): enable the INWINDOWS code paths in
// base/net (Mutex_WIN, Condition_WIN, Thread_WIN, IOCPPoller, ...).
// This block is only compiled on Windows, so Linux is unaffected.
#define INWINDOWS
#define  __thread __declspec(thread)

/////////////////////////////////////////////////////

#else

//////////////////Linux/////////////////////////////

#define  BASE_API

/////////////////////////////////////////////////////

#endif


#endif // !__EtherDB_Config_H_




