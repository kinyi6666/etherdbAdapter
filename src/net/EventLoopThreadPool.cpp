//////////////////////////////////////////////////////////////////////////////////
//�ļ���EventLoopThreadPool.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#include "EventLoopThreadPool.h"
#include "EventLoop.h"
#include "EventLoopThread.h"
#include <assert.h>

namespace EtherDB
{
	namespace Net
	{
		EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
			:_baseLoop(baseLoop),
			_name(nameArg),
			_started(false),
			_numThreads(0),
			_next(0)
		{

		}

		EventLoopThreadPool::~EventLoopThreadPool()
		{

		}

		void EventLoopThreadPool::setThreadNum(int numThreads)
		{
			_numThreads = numThreads;
		}

		void EventLoopThreadPool::start(const ThreadInitCallback& cb)
		{
			assert(!_started);
			_baseLoop->assertInLoopThread();

			_started = true;

			for (int i = 0; i < _numThreads; i++)
			{
				char *buf = new char[_name.size() + 32];
				snprintf(buf, sizeof buf, "%s%d", _name.c_str(), i);
				EventLoopThread* t = new EventLoopThread(cb, buf);
				_threads.push_back(std::shared_ptr<EventLoopThread>(t));
				_loops.push_back(t->startLoop());
				delete[] buf;   // new[] 必须配 delete[]（ASan alloc-dealloc-mismatch）
			}
			if (_numThreads == 0 && cb)
			{
				cb(_baseLoop);
			}
		}

		EventLoop* EventLoopThreadPool::getNextLoop()
		{
			_baseLoop->assertInLoopThread();
			assert(_started);
			EventLoop* loop = _baseLoop;

			if (!_loops.empty())
			{
				loop = _loops[_next];
				++_next;
				if (static_cast<size_t>(_next) >= _loops.size())
				{
					_next = 0;
				}
			}
			return loop;
		}

		EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode)
		{
			_baseLoop->assertInLoopThread();
			EventLoop* loop = _baseLoop;

			if (!_loops.empty())
			{
				loop = _loops[hashCode % _loops.size()];
			}
			return loop;
		}

		std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
		{
			_baseLoop->assertInLoopThread();
			assert(_started);
			if (_loops.empty())
			{
				return std::vector<EventLoop*>(1, _baseLoop);
			}
			else
			{
				return _loops;
			}
		}


		bool EventLoopThreadPool::started() const
		{
			return _started;
		}


		const std::string& EventLoopThreadPool::name() const
		{
			return _name;
		}

	}
}


