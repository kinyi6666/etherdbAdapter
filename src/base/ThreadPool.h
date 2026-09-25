//////////////////////////////////////////////////////////////////////////////////
//�ļ���ThreadPool.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������̳߳�
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_ThreadPool_H_
#define __EtherDB_ThreadPool_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"
#include "Thread.h"
#include <string>
#include <vector>

namespace EtherDB
{
	class PooledThread;
	class BASE_API  ThreadPool : Noncopyable
	{
	public:
		typedef Thread::ThreadFun Task;

		explicit ThreadPool(const std::string& name = std::string("ThreadPool"),
			int minCapacity = 2,
			int maxCapacity = 16,
			int idleTime = 60,
			int stackSize = 0);
		~ThreadPool();

		void addCapacity(int n);
		int capacity() const;

		void setStackSize(int stackSize);
		int getStackSize() const;

		int used() const;
		int allocated() const;
		int available() const;

		void start(const Task& task);

		void stopAll();

		void joinAll();

		void collect();
		const std::string& name() const;
		static ThreadPool& defaultPool();

	private:
		PooledThread * getThread();
		PooledThread* createThread();

		void housekeep();

	private:
		typedef std::vector<PooledThread*> ThreadVec;

		std::string _name;
		int _minCapacity;
		int _maxCapacity;
		int _idleTime;
		int _serial;
		int _age;
		int _stackSize;
		ThreadVec _threads;
		mutable Mutex _mutex;
	};
}



#endif // !__EtherDB_ThreadPool_H_




