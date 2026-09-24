//////////////////////////////////////////////////////////////////////////////////
//�ļ���Mutex_WIN.h
//���ߣ�LSPZ
//ʱ�䣺2018-03-02
//������WINDOWS�������ӿ�
///////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Mutex_WIN_H_
#define __EtherDB_Mutex_WIN_H_

#include "EtherDBConfig.h"

#ifdef INWINDOWS

#include "AmWindows.h"
#include "Noncopyable.h"

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
		CRITICAL_SECTION _cs;
		friend ConditionImpl;
	};
}



#endif // INWINDOWS


#endif // !__EtherDB_Mutex_WIN_H_




