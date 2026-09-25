//////////////////////////////////////////////////////////////////////////////////
//�ļ���NetEnvInit.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//���������绷����ʼ��
////////////////////////////////////////////////////////////////////////////////////

#include "NetEnvInit.h"

#ifdef INWINDOWS

#include <base/AmWindows.h>

namespace EtherDB
{
	namespace Net
	{
		AtomicInt64 _wsatransdata::s_wsacount;

		class WsaInit
		{
		public:
			WsaInit()
			{
				WORD wVersionRequested = MAKEWORD(2, 2);
				WSADATA wsaData;
				DWORD err = WSAStartup(wVersionRequested, &wsaData);
			}

			~WsaInit()
			{
				WSACleanup();
			}

		private:
		};

		WsaInit _wsainit;
	}
}

#else

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

namespace EtherDB
{
	namespace Net
	{
		class IgnoreSigPipe
		{
		public:
			IgnoreSigPipe()
			{
				::signal(SIGPIPE, SIG_IGN);
			}
		};

		IgnoreSigPipe initObj;
	}
}

#endif // INWINDOWS


