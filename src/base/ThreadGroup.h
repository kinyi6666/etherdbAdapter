
#ifndef __EtherDB_ThreadGroup_H_
#define __EtherDB_ThreadGroup_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"
#include "Mutex.h"
#include "Condition.h"
#include "Thread.h"
#include <deque>
#include <vector>
#include <memory>

namespace EtherDB
{
	class BASE_API ThreadGroup
	{
	public:
		typedef Thread::ThreadFun Task;

		explicit ThreadGroup(const std::string& nameArg = std::string("ThreadGroup"));
		~ThreadGroup();

		const std::string& name() const { return _name; }
		bool isRunning() const { return _running; }
		void setMaxQueueSize(int maxSize) { _maxQueueSize = maxSize; }
		size_t queueSize() const;
		void setThreadInitCallback(const Task& cb)
		{
			_threadInitCallback = cb;
		}

		void start(int numThreads);
		void stop();
		void run(const Task& f);

	private:
		bool isFull() const;
		void runInThread();
		Task take();

	private:
		std::string _name;
		bool		_running;
		size_t		_maxQueueSize;
		Task _threadInitCallback;
		std::deque<Task> _taskQueue;

		mutable Mutex _mutex;
		Condition _notEmpty;
		Condition _notFull;
		std::vector<std::shared_ptr<Thread> > _threads;
	};
}


#endif // !__EtherDB_ThreadGroup_H_






