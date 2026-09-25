//////////////////////////////////////////////////////////////////////////////////
//�ļ���IOCPPoller.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#include "IOCPPoller.h"

#ifdef INWINDOWS
#include "EventLoop.h"
#include "Channel.h"
#include "NetEnvInit.h"
#include <base/Logging.h>
#include <assert.h>

namespace EtherDB
{
	namespace Net
	{
		const int kNew = -1;
		const int kAdded = 1;
		const int kDeleted = 2;

		IOCPPoller::IOCPPoller(EventLoop *loop)
			:Poller(loop),
			_comPort(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0))
		{

		}

		IOCPPoller::~IOCPPoller()
		{
			::CloseHandle(_comPort);
		}

		Timestamp IOCPPoller::poll(int timeoutMs, ChannelList* activeChannels)
		{
			DWORD BytesTransferred;
			LPOVERLAPPED IpOverlapped;
			ULONG_PTR key;
			BOOL bret = ::GetQueuedCompletionStatus(_comPort, &BytesTransferred, (PULONG_PTR)&key, (LPOVERLAPPED*)&IpOverlapped, timeoutMs);

			do
			{
				if (bret == TRUE)
				{
					WSATransData *data = (WSATransData *)CONTAINING_RECORD(IpOverlapped, WSATransData, overlapped);
					switch (data->event)
					{
					case IOCPPOST:
					{
						delete data;
					}break;
					case IOCPREAD:
					{
						if (_channels.find(data->fd) == _channels.end())
						{
							delete data;
							return Timestamp::now();
						}
						else
						{
							Channel *channel = _channels[data->fd];
							if (channel->events() & data->event)
							{
								data->translen = BytesTransferred;
								channel->set_revents(data->event);
								activeChannels->push_back(channel);
							}
							else
							{
								delete data;
							}
						}
					}break;

					case IOCPWRITE:
					{
						if (_channels.find(data->fd) == _channels.end())
						{
							delete data;
							return Timestamp::now();
						}
						else
						{
							Channel *channel = _channels[data->fd];
							if (channel->events() & data->event)
							{
								data->translen = BytesTransferred;
								channel->set_revents(data->event);
								activeChannels->push_back(channel);
							}
							else
							{
								delete data;
							}

						}
					}break;
					default: {}break;
					}
				}
				else //����
				{
					if (GetLastError() == WAIT_TIMEOUT)
					{
						break;
					}

					if (IpOverlapped != NULL)
					{
						WSATransData *data = (WSATransData *)CONTAINING_RECORD(IpOverlapped, WSATransData, overlapped);
						if (_channels.find(data->fd) == _channels.end())
						{
							delete data;
							break;
						}
						else
						{
							Channel *channel = _channels[data->fd];
							if (channel->events() & data->event)
							{
								data->translen = 0;
								channel->set_revents(data->event);
								activeChannels->push_back(channel);
							}
							else
							{
								delete data;
							}
							break;
						}
					}


					LOG_FATAL << "IOCP Error...";
				}

			} while (0);
			return Timestamp::now();
		}

		void IOCPPoller::wakeup()
		{
			WSATransData *data = new WSATransData;
			memset(&(data->overlapped), 0, sizeof(OVERLAPPED));
			data->event = IOCPPOST;
			::PostQueuedCompletionStatus(_comPort, 0, NULL, (LPOVERLAPPED)data);
		}

		void IOCPPoller::updateChannel(Channel* channel)
		{
			if (channel->index() == kNew)
			{
				::CreateIoCompletionPort((HANDLE)(channel->fd()), _comPort, NULL, 0);
				channel->set_index(kAdded);
				_channels[channel->fd()] = channel;
			}
		}

		void IOCPPoller::removeChannel(Channel* channel)
		{
			assertInLoopThread();
			int fd = channel->fd();
			assert(_channels.find(fd) != _channels.end());
			assert(_channels[fd] == channel);
			assert(channel->isNoneEvent());
			int index = channel->index();
			assert(index == kAdded || index == kDeleted);
			size_t n = _channels.erase(fd);
			(void)n;
			assert(n == 1);

			channel->set_index(kNew);
		}

	}
}


#endif // INWINDOWS


