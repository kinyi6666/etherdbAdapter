//////////////////////////////////////////////////////////////////////////////////
//�ļ���EventLoopThread.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//�������¼�ѭ���߳�
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_EventLoopThread_H_
#define __EtherDB_Net_EventLoopThread_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/Condition.h>
#include <base/Thread.h>

namespace EtherDB
{
	namespace Net
	{
		class EventLoop;
		class AMNET_API EventLoopThread
			:Noncopyable
		{
		public:
			typedef std::function<void(EventLoop*)> ThreadInitCallback;

			EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
				const std::string& name = std::string());
			~EventLoopThread();
			EventLoop* startLoop();

		private:
			void threadFunc();

		private:
			EventLoop * _loop;
			bool _exiting;
			Thread _thread;
			Mutex _mutex;
			Condition _cond;
			ThreadInitCallback _callback;
		};
	}
}


#endif // !__EtherDB_Net_EventLoopThread_H_




