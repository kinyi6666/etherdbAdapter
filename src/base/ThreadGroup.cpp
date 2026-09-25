//////////////////////////////////////////////////////////////////////////////////
//�ļ���ThreadGroup.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���
////////////////////////////////////////////////////////////////////////////////////

#include "ThreadGroup.h"
#include <assert.h>

namespace EtherDB
{
	ThreadGroup::ThreadGroup(const std::string& nameArg)
		:_name(nameArg),
		_running(false),
		_maxQueueSize(0),
		_mutex(),
		_notEmpty(_mutex),
		_notFull(_mutex)
	{

	}

	ThreadGroup::~ThreadGroup()
	{
		if (_running)
		{
			stop();
		}
	}

	size_t ThreadGroup::queueSize() const
	{
		MutexLock lock(_mutex);
		return _maxQueueSize;
	}

	void ThreadGroup::start(int numThreads)
	{
		assert(_threads.empty());
		_running = true;
		_threads.reserve(numThreads);
		for (int i = 0; i < numThreads; ++i)
		{
			char id[32];
			snprintf(id, sizeof id, "%d", i + 1);
			_threads.push_back(std::shared_ptr<Thread>(new Thread(std::bind(&ThreadGroup::runInThread, this), _name + id)));
			_threads[i]->start();
		}
		if (numThreads == 0 && _threadInitCallback)
		{
			_threadInitCallback();
		}
	}

	void ThreadGroup::stop()
	{
		{
			MutexLock lock(_mutex);
			_running = false;
			_notEmpty.notifyAll();
		}
		for (auto it = _threads.begin(); it != _threads.end(); ++it)
		{
			try
			{
				(*it)->join();
			}
			catch (...)
			{

			}

		}
		_threads.clear();
	}

	void ThreadGroup::run(const Task& task)
	{
		if (_threads.empty())
		{
			task();
		}
		else
		{
			MutexLock lock(_mutex);
			while (isFull())
			{
				_notFull.wait();
			}
			assert(!isFull());

			_taskQueue.push_back(task);
			_notEmpty.notifyAll();
		}
	}


	bool ThreadGroup::isFull() const
	{
		return _maxQueueSize > 0 && _taskQueue.size() >= _maxQueueSize;
	}

	void ThreadGroup::runInThread()
	{
		try
		{
			if (_threadInitCallback)
			{
				_threadInitCallback();
			}
			while (_running)
			{
				Task task(take());
				if (task)
				{
					task();
				}
			}
		}
		catch (...)
		{

		}
	}

	ThreadGroup::Task ThreadGroup::take()
	{
		MutexLock lock(_mutex);
		while (_taskQueue.empty() && _running)
		{
			_notEmpty.wait();
		}
		Task task;
		if (!_taskQueue.empty())
		{
			task = _taskQueue.front();
			_taskQueue.pop_front();
			if (_maxQueueSize > 0)
			{
				_notFull.notifyAll();
			}
		}
		return task;
	}

}










