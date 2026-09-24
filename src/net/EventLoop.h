//////////////////////////////////////////////////////////////////////////////////
//�ļ���EventLoop.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������ʱ��ѭ��
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_EventLoop_H_
#define __EtherDB_Net_EventLoop_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/Timestamp.h>
#include <base/Mutex.h>
#include <base/Any.h>
#include "TimerId.h"
#include "Callbacks.h"
#include <functional>
#include <memory>
#include <vector>
#include <map>


namespace EtherDB
{
	namespace Net
	{
		class Channel;
		class Poller;
		class TimerQueue;

		class AMNET_API EventLoop : Noncopyable
		{
		public:
			typedef std::function<void()> Functor;

			EventLoop();
			~EventLoop();

			void loop();
			void quit();

			Timestamp pollReturnTime() const;
			int64_t iteration() const;
			void runInLoop(const Functor& cb);
			void queueInLoop(const Functor& cb);
			size_t queueSize() const;

			TimerId runAt(const Timestamp& time, const TimerCallback& cb);
			TimerId runAfter(double delay, const TimerCallback& cb);
			TimerId runEvery(double interval, const TimerCallback& cb);
			void cancel(TimerId timerId);

			void wakeup();
			void updateChannel(Channel* channel);
			void removeChannel(Channel* channel);
			bool hasChannel(Channel* channel);

			void assertInLoopThread();
			bool isInLoopThread() const;
			bool eventHandling() const;
			void setContext(std::string key, const Any& value);
			const Any& getContext(std::string key) const;

			static EventLoop* getEventLoopOfCurrentThread();

		private:
			void abortNotInLoopThread();
			void doPendingFunctors();
			void printActiveChannels() const;

		private:
			typedef std::vector<Channel*> ChannelList;

			bool _looping;
			bool _quit;
			bool _eventHandling;
			bool _callingPendingFunctors;
			int64_t _iteration;
			const int _threadId;
			Timestamp _pollReturnTime;

			std::unique_ptr<Poller> _poller;
			std::unique_ptr<TimerQueue> _timerQueue;
			ChannelList _activeChannels;
			Channel* _currentActiveChannel;
			mutable std::map<std::string, Any> _context;

			mutable Mutex _mutex;
			std::vector<Functor> _pendingFunctors;
		};

	}
}


#endif // !__EtherDB_Net_EventLoop_H_

