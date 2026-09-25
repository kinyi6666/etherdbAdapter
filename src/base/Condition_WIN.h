//////////////////////////////////////////////////////////////////////////////////
//�ļ���Condition_WIN.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���WINDOWS�ӿ�
//////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Condition_WIN_H_
#define __EtherDB_Condition_WIN_H_

#include "EtherDBConfig.h"

#ifdef INWINDOWS

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
		CONDITION_VARIABLE _pcond;
	};
}


#endif // INWINDOWS



#endif // !__EtherDB_Condition_WIN_H_


