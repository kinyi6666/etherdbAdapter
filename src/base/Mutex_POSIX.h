//////////////////////////////////////////////////////////////////////////////////
//�ļ���Mutex_POSIX.h 
//���ߣ�LSPZ
//ʱ�䣺2018-03-02
//������POSIX�������ӿ�
///////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Mutex_POSIX_H_
#define __EtherDB_Mutex_POSIX_H_

#include "EtherDBConfig.h"

#ifndef INWINDOWS

#include "Noncopyable.h"
#include <pthread.h>
#include <errno.h>

namespace EtherDB
{
	class ConditionImpl;
	class BASE_API MutexImpl : Noncopyable
	{
	public:
		MutexImpl();
		~MutexImpl();
		void lockImpl();
		bool tryLockImpl();
		bool tryLockImpl(long milliseconds);
		void unlockImpl();

	private:
		pthread_mutex_t _mutex;
		friend ConditionImpl;
	};
}



#endif // !INWINDOWS



#endif // !__EtherDB_Mutex_POSIX_H_


