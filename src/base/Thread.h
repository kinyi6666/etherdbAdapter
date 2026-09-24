//////////////////////////////////////////////////////////////////////////////////
//�ļ���Thread.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������߳�
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Thread_H_
#define __EtherDB_Thread_H_

#include "EtherDBConfig.h"
#include "AtomicInt.h"

#ifndef INWINDOWS
#include "Thread_POSIX.h"
#else
#include "Thread_WIN.h"
#endif // !INWINDOWS

namespace EtherDB
{
	class BASE_API Thread : Noncopyable
	{
	public:
		typedef ThreadImpl::ThreadFun ThreadFun;
		
		Thread(const ThreadFun& fun);
		Thread(const ThreadFun& fun, const std::string& name);
		~Thread();

		//�߳�ID
		int id() const;
		//�߳�TID
		int tid() const;
		//�߳�����
		std::string name() const;
		//�����̶߳�ջ��С
		void setStackSize(int size);
		//��ȡ�̶߳�ջ��С
		int getStackSize() const;

		void start();
		void join();
		void join(long milliseconds);
		bool tryJoin(long milliseconds);
		bool isRunning() const;

		static void sleep(long milliseconds);
		static void yield();
		static int currentTid();
		static std::string currentName();
		static bool isMainThread();
		static int numCreated() { return _numCreated.get(); }

	private:
		ThreadImpl _impl;
		int _id;

		static AtomicInt32 _numCreated;
	};
}

#endif // !__EtherDB_Thread_H_



