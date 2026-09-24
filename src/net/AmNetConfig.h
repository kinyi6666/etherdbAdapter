//////////////////////////////////////////////////////////////////////////////////
//�ļ���AmNetConfig.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////


#ifndef __EtherDB_NetConfig_H_
#define __EtherDB_NetConfig_H_

#include <base/EtherDBConfig.h>

#ifdef INWINDOWS

#ifdef net_EXPORTS//_DLL //
#define AMNET_API __declspec(dllexport)
#else
#define AMNET_API __declspec(dllimport)
#endif

#else

#define AMNET_API
#define ADDRESS_FAMILY sa_family_t
#define SOCKET int 

#endif // INWINDOWS



#endif // !__EtherDB_NetConfig_H_
