//////////////////////////////////////////////////////////////////////////////////
//�ļ���EventLoopThreadPool.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_EventLoopThreadPool_H_
#define __EtherDB_Net_EventLoopThreadPool_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <vector>
#include <memory>
#include "EventLoopThread.h"

namespace EtherDB
{
	namespace Net
	{
		class EventLoop;
		class EventLoopThread;

		class AMNET_API EventLoopThreadPool : Noncopyable
		{
		public:
			typedef EventLoopThread::ThreadInitCallback ThreadInitCallback;

			EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
			~EventLoopThreadPool();

			void setThreadNum(int numThreads);
			void start(const ThreadInitCallback& cb = ThreadInitCallback());

			EventLoop* getNextLoop();
			EventLoop* getLoopForHash(size_t hashCode);
			std::vector<EventLoop*> getAllLoops();

			bool started() const;
			const std::string& name() const;

		private:
			EventLoop * _baseLoop;
			std::string _name;
			bool _started;
			int _numThreads;
			int _next;
			std::vector< std::shared_ptr<EventLoopThread> > _threads;
			std::vector<EventLoop*> _loops;
		};

	}
}



#endif // !__EtherDB_Net_EventLoopThreadPool_H_