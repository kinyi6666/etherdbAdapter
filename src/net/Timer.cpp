//////////////////////////////////////////////////////////////////////////////////
//�ļ���Timer.cpp  
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//��������ʱ��
////////////////////////////////////////////////////////////////////////////////////

#include "Timer.h"

namespace EtherDB
{
	namespace Net
	{
		AtomicInt64 Timer::_s_numCreated;

		void Timer::restart(Timestamp now)
		{
			if (_repeat)
			{
				_expiration = addTime(now, _interval);
			}
			else
			{
				_expiration = Timestamp::invalid();
			}
		}
	}
}