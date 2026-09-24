//////////////////////////////////////////////////////////////////////////////////
//�ļ���ThreadPool.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������̳߳�
////////////////////////////////////////////////////////////////////////////////////

#include "ThreadPool.h"
#include "Exception.h"
#include "Singleton.h"
#include <ctime>
#include <assert.h>
#include <sstream>

namespace EtherDB
{
	class PooledThread
	{
	public:
		PooledThread(const std::string& name, int stackSize = 0)
			:_idle(true),
			_idleTime(0),
			_name(name),
			_targetCompleted(false)
		{
			assert(stackSize >= 0);
			_thread = new Thread(std::bind(&PooledThread::run, this), _name);
			_idleTime = std::time(NULL);
		}

		~PooledThread()
		{
			delete _thread;
		}

		void start()
		{
			_thread->start();
			_started.wait();
		}

		void start(const ThreadPool::Task &task)
		{
			MutexLock lock(_mutex);
			assert(!_task);
			_task = task;
			_targetReady.set();
		}

		bool idle()
		{
			MutexLock lock(_mutex);
			return _idle;
		}

		int idleTime()
		{
			MutexLock lock(_mutex);
			return (int)(time(NULL) - _idleTime);
		}

		void join()
		{
			ThreadPool::Task task;
			{
				MutexLock lock(_mutex);
				task = _task;
			}
			if (task)
				_targetCompleted.wait();
		}

		void activate()
		{
			MutexLock lock(_mutex);
			assert(_idle);
			_idle = false;
			_targetCompleted.reset();
		}

		void release()
		{
			{
				MutexLock lock(_mutex);
				_task = 0;
			}
			if (_thread->isRunning())
				_targetReady.set();
			_thread->join(100);
			delete this;
		}

	private:
		void run()
		{
			_started.set();
			while (true)
			{
				_targetReady.wait();
				_mutex.lock();
				if (_task)
				{
					_mutex.unlock();

					try
					{
						_task();
					}
					catch (...)
					{

					}
					MutexLock lock(_mutex);

					_task = 0;
					_idleTime = time(NULL);
					_idle = true;
					_targetCompleted.set();
				}
				else
				{
					_mutex.unlock();
					break;
				}
			}

		}

	private:
		volatile bool _idle;
		volatile std::time_t _idleTime;
		std::string _name;
		Event _targetReady;
		Event _targetCompleted;
		Event _started;
		Mutex _mutex;
		ThreadPool::Task _task;

		Thread *_thread;
	};

	ThreadPool::ThreadPool(const std::string& name,
		int minCapacity,
		int maxCapacity,
		int idleTime,
		int stackSize)
		:_name(name),
		_minCapacity(minCapacity),
		_maxCapacity(maxCapacity),
		_idleTime(idleTime),
		_serial(0),
		_age(0),
		_stackSize(stackSize)
	{
		assert(minCapacity >= 1 && maxCapacity >= minCapacity && idleTime > 0);
		for (int i = 0; i < _minCapacity; i++)
		{
			PooledThread* pThread = createThread();
			_threads.push_back(pThread);
			pThread->start();
		}
	}

	ThreadPool::~ThreadPool()
	{
		try
		{
			stopAll();
		}
		catch (...)
		{

		}
	}

	void ThreadPool::addCapacity(int n)
	{
		MutexLock lock(_mutex);
		assert(_maxCapacity + n >= _minCapacity);
		_maxCapacity += n;
		housekeep();
	}

	int ThreadPool::capacity() const
	{
		MutexLock lock(_mutex);
		return _maxCapacity;
	}

	void ThreadPool::setStackSize(int stackSize)
	{
		_stackSize = stackSize;
	}

	int ThreadPool::getStackSize() const
	{
		return _stackSize;
	}

	const std::string& ThreadPool::name() const
	{
		return _name;
	}

	int ThreadPool::used() const
	{
		MutexLock lock(_mutex);

		int count = 0;
		for (ThreadVec::const_iterator it = _threads.begin(); it != _threads.end(); ++it)
		{
			if (!(*it)->idle()) ++count;
		}
		return count;
	}

	int ThreadPool::allocated() const
	{
		MutexLock lock(_mutex);

		return int(_threads.size());
	}

	int ThreadPool::available() const
	{
		MutexLock lock(_mutex);
		int count = 0;
		for (ThreadVec::const_iterator it = _threads.begin(); it != _threads.end(); ++it)
		{
			if ((*it)->idle()) ++count;
		}
		return (int)(count + _maxCapacity - _threads.size());
	}

	void ThreadPool::start(const Task& task)
	{
		getThread()->start(task);
	}

	void ThreadPool::stopAll()
	{
		MutexLock lock(_mutex);

		for (ThreadVec::iterator it = _threads.begin(); it != _threads.end(); ++it)
		{
			(*it)->release();
		}
		_threads.clear();
	}

	void ThreadPool::joinAll()
	{
		MutexLock lock(_mutex);

		for (ThreadVec::iterator it = _threads.begin(); it != _threads.end(); ++it)
		{
			(*it)->join();
		}
		housekeep();
	}

	void ThreadPool::collect()
	{
		MutexLock lock(_mutex);
		housekeep();
	}

	PooledThread* ThreadPool::getThread()
	{
		MutexLock lock(_mutex);
		if (++_age == 32)
			housekeep();
		PooledThread* pThread = NULL;
		for (ThreadVec::iterator it = _threads.begin(); !pThread && it != _threads.end(); ++it)
		{
			if ((*it)->idle())
				pThread = *it;
		}

		if (!pThread)
		{
			if (_threads.size() < _maxCapacity)
			{
				pThread = createThread();
				try
				{
					pThread->start();
					_threads.push_back(pThread);
				}
				catch (...)
				{
					delete pThread;
					throw;
				}
			}
			else
			{
				throw NoThreadAvailableException();
			}
		}
		pThread->activate();
		return pThread;
	}

	PooledThread* ThreadPool::createThread()
	{
		std::ostringstream name;
		name << _name << "[#" << ++_serial << "]";
		return new PooledThread(name.str(), _stackSize);
	}

	void ThreadPool::housekeep()
	{
		_age = 0;
		if (_threads.size() <= _minCapacity)
			return;

		ThreadVec idleThreads;
		ThreadVec expiredThreads;
		ThreadVec activeThreads;
		idleThreads.reserve(_threads.size());
		activeThreads.reserve(_threads.size());

		for (ThreadVec::iterator it = _threads.begin(); it != _threads.end(); ++it)
		{
			if ((*it)->idle())
			{
				if ((*it)->idleTime() < _idleTime)
					idleThreads.push_back(*it);
				else
					expiredThreads.push_back(*it);
			}
			else
			{
				activeThreads.push_back(*it);
			}
		}

		int n = (int)activeThreads.size();
		int limit = (int)idleThreads.size() + n;
		if (limit < _minCapacity)
		{
			limit = _minCapacity;
		}
		idleThreads.insert(idleThreads.end(), expiredThreads.begin(), expiredThreads.end());
		_threads.clear();
		for (ThreadVec::iterator it = idleThreads.begin(); it != idleThreads.end(); ++it)
		{
			if (n < limit)
			{
				_threads.push_back(*it);
				++n;
			}
			else
			{
				(*it)->release();
			}
		}
		_threads.insert(_threads.end(), activeThreads.begin(), activeThreads.end());
	}

	ThreadPool& ThreadPool::defaultPool()
	{
		return Singleton<ThreadPool>::instance();
	}
}



