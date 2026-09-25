//////////////////////////////////////////////////////////////////////////////////
//�ļ���Condition.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Condition_H_
#define __EtherDB_Condition_H_

#include "EtherDBConfig.h"

#ifndef INWINDOWS
#include "Condition_POSIX.h"
#else
#include "Condition_WIN.h"
#endif // !INWINDOWS

namespace EtherDB
{
	class BASE_API Condition : Noncopyable
	{
	public:
		Condition(Mutex& mutex);
		~Condition();

		void wait();
		void wait(long milliseconds);
		bool tryWait(long milliseconds);
		void notify();
		void notifyAll();

	private:
		ConditionImpl _impl;
	};
}


#endif //!__EtherDB_Condition_H_