//////////////////////////////////////////////////////////////////////////////////
//�ļ���NetEnvInit.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//���������绷����ʼ��
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_EnvInit_H_
#define __EtherDB_Net_EnvInit_H_

#include "AmNetConfig.h"

#ifdef INWINDOWS

#include <base/AmWindows.h>
#include <base/AtomicInt.h>

namespace EtherDB
{
	namespace Net
	{
		enum WSASTATUS
		{
			IOCPREAD = 1,
			IOCPWRITE = 2,
			IOCPERROE = 4,
			IOCPPOST = 8
		};

		typedef struct _wsatransdata
		{
			_wsatransdata()
			{
				//s_wsacount.incrementAndGet();
			}

			~_wsatransdata()
			{
				//printf("wsacount:%d\n", s_wsacount.decrementAndGet());
			}

			OVERLAPPED overlapped;
			WSABUF databuff;
			int event;
			int fd;
			int translen;
			int ptr;

			static AtomicInt64 s_wsacount;
		}WSATransData;
	}
}

#else

#endif // INWINDOWS

#endif // !__EtherDB_Net_EnvInit_H_


