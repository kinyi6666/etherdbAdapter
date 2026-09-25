//////////////////////////////////////////////////////////////////////////////////
//�ļ���Event.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Event_H_
#define __EtherDB_Event_H_

#include "CountDownLatch.h"

namespace EtherDB
{
	class BASE_API Event : Noncopyable
	{
	public:
		Event(bool autoReset = true);
		~Event();

		void set();
		void wait();
		void wait(long milliseconds);
		bool tryWait(long milliseconds);
		void reset();

	private:
		mutable Mutex _mutex;
		Condition _condition;
		int _count;
		bool _auto;
	};
}

#endif // !__EtherDB_Event_H_

