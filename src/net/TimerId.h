//////////////////////////////////////////////////////////////////////////////////
//�ļ���TimerId.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//��������ʱ��ID
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_TimerId_H_
#define __EtherDB_Net_TimerId_H_

#include "AmNetConfig.h"

namespace EtherDB
{
	namespace Net
	{
		class Timer;

		class AMNET_API TimerId
		{
		public:
			TimerId()
				:_timer(NULL),
				_sequence(0)
			{
			}

			TimerId(Timer* timer, int64_t seq)
				:_timer(timer),
				_sequence(seq)
			{

			}

			friend class TimerQueue;
		private:
			Timer * _timer;
			int64_t _sequence;
		};
	}
}


#endif // !__EtherDB_Net_TimerId_H_



