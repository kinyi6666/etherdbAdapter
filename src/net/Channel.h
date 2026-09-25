//////////////////////////////////////////////////////////////////////////////////
//�ļ���Channel.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Channel
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_Channel_H_
#define __EtherDB_Net_Channel_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/Timestamp.h>
#include <base/AtomicInt.h>
#include <functional>
#include <memory>

namespace EtherDB
{
	namespace Net
	{
		class EventLoop;
		class AMNET_API Channel : Noncopyable
		{
		public:
			typedef std::function<void()> EventCallback;
			typedef std::function<void(Timestamp)> ReadEventCallback;

			Channel(EventLoop* loop, int fd);
			~Channel();

			void handleEvent(Timestamp receiveTime);
			void tie(const std::shared_ptr<void>&);
			void remove();

			std::string reventsToString() const;
			std::string eventsToString() const;

			EventLoop* ownerLoop() { return _loop; }
			int fd() const { return _fd; }
			int64_t amfd() const { return _amfd; }
			int events() const { return _events; }
			void set_revents(int revt) { _revents = revt; }

			bool isNoneEvent() const { return _events == kNoneEvent; }
			void enableReading() { _events |= kReadEvent; update(); }
			void disableReading() { _events &= ~kReadEvent; update(); }
			void enableWriting() { _events |= kWriteEvent; update(); }
			void disableWriting() { _events &= ~kWriteEvent; update(); }
			void disableAll() { _events = kNoneEvent; update(); }
			bool isWriting() const { return _events & kWriteEvent; }
			bool isReading() const { return _events & kReadEvent; }

			int index() { return _index; }
			void set_index(int idx) { _index = idx; }
			void doNotLogHup() { _logHup = false; }

			void setReadCallback(const ReadEventCallback& cb) { _readCallback = cb; }
			void setWriteCallback(const EventCallback& cb) { _writeCallback = cb; }
			void setCloseCallback(const EventCallback& cb) { _closeCallback = cb; }
			void setErrorCallback(const EventCallback& cb) { _errorCallback = cb; }

		private:
			void update();
			void handleEventWithGuard(Timestamp receiveTime);

			static std::string eventsToString(int fd, int ev);

		private:
			static const int kNoneEvent;
			static const int kReadEvent;
			static const int kWriteEvent;

			EventLoop *_loop;
			const int _fd;
			const int64_t _amfd;
			int _events;
			int _revents;
			int _index;
			bool _logHup;

			std::weak_ptr<void> _tie;
			bool _tied;
			bool _eventHandling;
			bool _addedToLoop;

			ReadEventCallback _readCallback;
			EventCallback _writeCallback;
			EventCallback _closeCallback;
			EventCallback _errorCallback;

			static AtomicInt64 s_Count;
		};
	}
}


#endif // !__EtherDB_Net_Channel_H_

