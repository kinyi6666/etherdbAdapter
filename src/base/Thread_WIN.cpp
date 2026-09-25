//////////////////////////////////////////////////////////////////////////////////
//�ļ���Thread_WIN32.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������߳�
////////////////////////////////////////////////////////////////////////////////////

#include "Thread_WIN.h"

#ifdef INWINDOWS
#include "Exception.h"
#include "ThreadLocal.h"
#include <assert.h>

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
			typedef ThreadImpl::ThreadFun ThreadFun;

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

		__thread ThreadData *t_threadData = NULL;

		DWORD WINAPI startThread(void* obj)
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
		_ptid(NULL)
	{

	}

	ThreadImpl::~ThreadImpl()
	{
		if (_ptid != NULL)
			CloseHandle(_ptid);
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
		_ptid = CreateThread(NULL, _stackSize, detail::startThread, data, 0, NULL);
		if (_ptid == NULL)
		{
			*_state = 0;
			delete data;
			return;
		}
		_begin->wait();
	}

	void ThreadImpl::joinImpl()
	{
		assert((*_state > 0) && (*_state != 3));
		_end->wait();

		*_state = 3;
		WaitForSingleObject(_ptid, INFINITE);
		CloseHandle(_ptid);
		_ptid = NULL;
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
				WaitForSingleObject(_ptid, INFINITE);
				CloseHandle(_ptid);
				_ptid = NULL;
				return true;
			}
			CloseHandle(_ptid);
			_ptid = NULL;
			return false;
		}
		catch (...)
		{

		}
		CloseHandle(_ptid);
		_ptid = NULL;
		return false;
	}

	bool ThreadImpl::isRunningImpl() const
	{
		return (*_state == 1);
	}


	void ThreadImpl::sleepImpl(long milliseconds)
	{
		Sleep(milliseconds);
	}

	void ThreadImpl::yieldImpl()
	{
		Sleep(0);
	}

	int ThreadImpl::currentTidImpl()
	{
		return ::GetCurrentThreadId();
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
#endif // INWINDOWS



