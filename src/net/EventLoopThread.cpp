//////////////////////////////////////////////////////////////////////////////////
//�ļ���EventLoopThread.cpp  
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#include "EventLoopThread.h"
#include "EventLoop.h"
#include <assert.h>




namespace EtherDB
{
	namespace Net
	{
		EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
			const std::string& name)
			:_loop(NULL),
			_exiting(false),
			_thread(std::bind(&EventLoopThread::threadFunc, this), name),
			_mutex(),
			_cond(_mutex),
			_callback(cb)
		{

		}

		EventLoopThread::~EventLoopThread()
		{
			_exiting = true;
			if (_loop != NULL)
			{
				_loop->quit();
				_thread.join();
			}
		}

		EventLoop* EventLoopThread::startLoop()
		{
			assert(!_thread.isRunning());
			_thread.start();

			{
				MutexLock lock(_mutex);
				while (_loop == NULL)
				{
					_cond.wait();
				}
			}
			return _loop;
		}

		void EventLoopThread::threadFunc()
		{
			EventLoop loop;

			if (_callback)
			{
				_callback(&loop);
			}

			{
				MutexLock lock(_mutex);
				_loop = &loop;
				_cond.notify();
			}

			loop.loop();
			_loop = NULL;
		}

	}
}


