//////////////////////////////////////////////////////////////////////////////////
//�ļ���Mutex.h 
//���ߣ�LSPZ
//ʱ�䣺2018-03-02
//������������
///////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Mutex_H_
#define __EtherDB_Mutex_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"

#ifndef INWINDOWS
#include "Mutex_POSIX.h"
#else
#include "Mutex_WIN.h"
#endif // !INWINDOWS


namespace EtherDB
{
	class Condition;
	class BASE_API Mutex : Noncopyable
	{
	public:
		//����,�����׳��쳣
		void lock();
		//����������milliseconds���룬��ʱ�׳��쳣
		void lock(long milliseconds);
		//��������,�����׳��쳣
		bool tryLock();
		//��������������milliseconds���룬��ʱ����false
		bool tryLock(long milliseconds);
		void unlock();

	private:
		MutexImpl _impl;
		friend Condition;
	};

	class BASE_API MutexLock
	{
	public:
		explicit MutexLock(Mutex &m)
			:_m(m)
		{
			_m.lock();
		}

		~MutexLock()
		{
			_m.unlock();
		}

	private:
		Mutex & _m;
	};

}


#endif // !__EtherDB_Mutex_H_


