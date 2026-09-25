//////////////////////////////////////////////////////////////////////////////////
//�ļ���ConsoleChannel.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//����������̨
////////////////////////////////////////////////////////////////////////////////////

#include "ConsoleChannel.h"
#include "EventLoop.h"
#include <base/Logging.h>

#ifndef INWINDOWS
#include <unistd.h>
#endif // !INWINDOWS


namespace EtherDB
{
	namespace Net
	{
		AtomicInt32 ConsoleChannel::s_count;

#ifndef INWINDOWS
		ConsoleChannel::ConsoleChannel(EventLoop *loop)
			:_loop(loop),
			_fd(0),
			_channel(new Channel(loop, _fd))
		{
			_channel->setReadCallback(std::bind(&ConsoleChannel::handleRead, this, std::placeholders::_1));
			_channel->enableReading();
		}

		ConsoleChannel::~ConsoleChannel()
		{
			_channel->disableAll();
			_channel->remove();
		}

		void ConsoleChannel::handleRead(Timestamp receiveTime)
		{
			int n = ::read(_fd, _readBuffer.beginWrite(), _readBuffer.writableBytes());
			_readBuffer.hasWritten(n);
			if (_readCallback)
			{
				_readCallback(&_readBuffer);
			}
		}
#else
		ConsoleChannel::ConsoleChannel(EventLoop *loop)
			:_loop(loop),
			_fd(0),
			_rdata(new WSATransData),
			_reading(false)
		{
			if (s_count.incrementAndGet() > 1)
			{
				LOG_FATAL << "open console agin!";
			}
			_fd = (int)::CreateFile("conin$", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0);
			if (_fd != NULL)
			{
				_channel.reset(new Channel(loop, _fd));
				_channel->setReadCallback(std::bind(&ConsoleChannel::handleRead, this, std::placeholders::_1));
				_channel->enableReading();

				_reading = true;
				memset(&(_rdata->overlapped), 0, sizeof(OVERLAPPED));
				_rdata->databuff.len = _readBuffer.writableBytes();
				_rdata->databuff.buf = _readBuffer.beginWrite();
				_rdata->event = 1;
				_rdata->fd = _fd;
				_rdata->translen = 0;
				_rdata->ptr = 0;
				DWORD byte;
				::ReadFile((HANDLE)_fd, _rdata->databuff.buf, _rdata->databuff.len, &byte, (LPOVERLAPPED)_rdata);
			}
			else
			{
				LOG_FATAL << "Open console fail!";
			}

		}

		ConsoleChannel::~ConsoleChannel()
		{
			if (!_reading)
				delete _rdata;
			_channel->disableAll();
			_channel->remove();
			::CloseHandle((HANDLE)_fd);
		}

		void ConsoleChannel::handleRead(Timestamp receiveTime)
		{
			_reading = false;
			_readBuffer.hasWritten(_rdata->translen);
			if (_readCallback)
			{
				_readCallback(&_readBuffer);
			}

			_reading = true;
			_readBuffer.ensureWritableBytes(1024);
			memset(&(_rdata->overlapped), 0, sizeof(OVERLAPPED));
			_rdata->databuff.len = _readBuffer.writableBytes();
			_rdata->databuff.buf = _readBuffer.beginWrite();
			_rdata->event = 1;
			_rdata->fd = _fd;
			_rdata->translen = 0;
			_rdata->ptr = 0;
			DWORD byte;
			::ReadFile((HANDLE)_fd, _rdata->databuff.buf, _rdata->databuff.len, &byte, (LPOVERLAPPED)_rdata);
		}
#endif //!INWINDOWS


		
	}
}



