//////////////////////////////////////////////////////////////////////////////////
//�ļ���Thread_POSIX.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������߳�
////////////////////////////////////////////////////////////////////////////////////

#include "Thread_POSIX.h"

#ifndef INWINDOWS

#include "Exception.h"
#include "ThreadLocal.h"
#include <assert.h>
#include <unistd.h>

namespace EtherDB
{
	__thread const char *t_threadName = "UnKnown";
	__thread ThreadLocalStorage *t_localStorage = NULL;

	namespace detail
	{

		int  g_MainTid = 0;

		struct ThreadNameInitializer
		{
			ThreadNameInitializer()
			{
				t_threadName = "main";
				g_MainTid = ThreadImpl::currentTidImpl();
				t_localStorage = new ThreadLocalStorage;
			}
			~ThreadNameInitializer()
			{
				delete t_localStorage;
			}
		};
		ThreadNameInitializer _threadNameInitializer;

		struct ThreadData
		{
			typedef EtherDB::ThreadImpl::ThreadFun ThreadFun;

			ThreadFun _func;
			std::string _name;
			std::weak_ptr<Event>  _begin;
			std::weak_ptr<Event>  _end;
			std::weak_ptr<int> _tid;
			std::weak_ptr<int> _state;

			ThreadData(const ThreadFun &fun,
				const std::string &name,
				const std::shared_ptr<Event> &begin,
				const std::shared_ptr<Event> &end,
				const std::shared_ptr<int> &tid,
				const std::shared_ptr<int> &state)
				:_func(fun),
				_name(name),
				_begin(begin),
				_end(end),
				_tid(tid),
				_state(state)
			{

			}

			void runInThread()
			{
				int tid = ThreadImpl::currentTidImpl();
				std::shared_ptr<int> ptid = _tid.lock();
				if (ptid)
				{
					*ptid = tid;
					ptid.reset();
				}

				t_threadName = _name.empty() ? "AmThread" : _name.c_str();

				std::shared_ptr<Event> bevent = _begin.lock();
				if (bevent)
				{
					bevent->set();
				}

				try
				{
					_func();
					t_threadName = "finished";
				}
				catch (...)
				{
					t_threadName = "crashed";
				}

				std::shared_ptr<int> state = _state.lock();
				if (state)
				{
					*state = 2;
				}

				std::shared_ptr<Event> eevent = _end.lock();
				if (eevent)
				{
					eevent->set();
				}
			}
		};

		__thread EtherDB::detail::ThreadData *t_threadData = NULL;

		void* startThread(void* obj)
		{
			t_localStorage = new ThreadLocalStorage;
			ThreadData* data = static_cast<ThreadData*>(obj);

			t_threadData = data;
			data->runInThread();
			delete data;
			delete t_localStorage;
			return NULL;
		}
	}

	ThreadImpl::ThreadImpl(const ThreadFun& fun, const std::string name)
		:_fun(fun),
		_name(name),
		_stackSize(0),
		_begin(new Event),
		_end(new Event),
		_tid(new int(0)),
		_state(new int(0)),
		_ptid(0)
	{

	}

	ThreadImpl::~ThreadImpl()
	{
		if (*_state == 1)
		{
			pthread_detach(_ptid);
		}
		else if (*_state == 2)
		{
			joinImpl();
		}
	}

	int ThreadImpl::tidImpl() const
	{
		return *_tid;
	}

	std::string ThreadImpl::nameImpl() const
	{
		return _name;
	}

	void ThreadImpl::setStackSizeImpl(int size)
	{
		if (size % 4096 != 0)
		{
			size = (size / 4096 + 1) * 4096;
		}
		_stackSize = size;
	}

	int ThreadImpl::getStackSizeImpl() const
	{
		return _stackSize;
	}


	void ThreadImpl::startImpl()
	{
		assert(*_state == 0);
		*_state = 1;

		detail::ThreadData* data = new detail::ThreadData(_fun, _name, _begin, _end, _tid, _state);

		pthread_attr_t attributes;
		pthread_attr_init(&attributes);
		if (_stackSize != 0)
		{
			if (0 != pthread_attr_setstacksize(&attributes, _stackSize))
			{
				*_state = 0;
				delete data;
				pthread_attr_destroy(&attributes);
				throw SystemException("cannot set thread stack size");
			}
		}

		if (pthread_create(&_ptid, &attributes, detail::startThread, data))
		{
			*_state = 0;
			delete data;
			pthread_attr_destroy(&attributes);
			throw SystemException("cannot create thread");
		}
		_begin->wait();
	}

	void ThreadImpl::joinImpl()
	{
		assert((*_state > 0) && (*_state != 3));
		_end->wait();
		*_state = 3;
		if (pthread_join(_ptid, NULL))
			throw SystemException("cannot join thread");
	}

	void ThreadImpl::joinImpl(long milliseconds)
	{
		if (!tryJoinImpl(milliseconds))
			throw TimeoutException();
	}

	bool ThreadImpl::tryJoinImpl(long milliseconds)
	{
		assert((*_state > 0) && (*_state != 3));
		try
		{
			if (_end->tryWait(milliseconds))
			{
				*_state = 3;
				if (pthread_join(_ptid, NULL) == 0)
				{
					return true;
				}
				else
				{
					throw SystemException("cannot join thread");
				}
			}
			return false;
		}
		catch (...)
		{

		}
		return false;
	}

	bool ThreadImpl::isRunningImpl() const
	{
		return (*_state == 1);
	}


	void ThreadImpl::sleepImpl(long milliseconds)
	{
		::usleep(milliseconds * 1000);
	}

	void ThreadImpl::yieldImpl()
	{
		::sleep(0);
	}

	int ThreadImpl::currentTidImpl()
	{
		return ::pthread_self();
	}

	std::string ThreadImpl::currentNameImpl()
	{
		return t_threadName;
	}

	bool ThreadImpl::isMainThreadImpl()
	{
		return currentTidImpl() == detail::g_MainTid;
	}
}




#endif // !INWINDOWS


