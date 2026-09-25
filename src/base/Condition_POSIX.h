//////////////////////////////////////////////////////////////////////////////////
//�ļ���Condition_POSIX.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���POSIX�ӿ�
//////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Condition_POSIX_H_
#define __EtherDB_Condition_POSIX_H_

#include "EtherDBConfig.h"

#ifndef INWINDOWS

#include "Mutex.h"

namespace EtherDB
{
	class BASE_API ConditionImpl : Noncopyable
	{
	public:
		ConditionImpl(MutexImpl &impl);
		~ConditionImpl();

		void waitImpl();
		bool tryWaitImpl(long milliseconds);
		void notifyImpl();
		void notifyAllImpl();

	private:
		MutexImpl & _mutex;
		pthread_cond_t _pcond;
	};
}

#endif // !INWINDOWS

#endif // !__EtherDB_Condition_POSIX_H_

