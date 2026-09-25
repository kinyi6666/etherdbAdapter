//////////////////////////////////////////////////////////////////////////////////
//�ļ���TcpConnection.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_TcpConnection_H_
#define __EtherDB_Net_TcpConnection_H_

#include "AmNetConfig.h"
#include <base/Noncopyable.h>
#include <base/Any.h>
#include "InetAddress.h"
#include "Channel.h"
#include "TcpSocket.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "Buffer.h"
#include <memory>
#include <map>

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API TcpConnection : public std::enable_shared_from_this<TcpConnection>,
			Noncopyable
		{
		public:
			TcpConnection(EventLoop *loop,
				const std::string name,
				int sockfd,
				const InetAddress &localAddr,
				const InetAddress &peerAddr);
			~TcpConnection();

			void connectEstablished();
			void connectDestroyed();

			void send(const void* message, int len);
			void send(const std::string& message);
			void send(Buffer* message);
			void shutdown();
			void forceClose();
			void forceCloseWithDelay(double seconds);
			void setTcpNoDelay(bool on);

			int fd() { return _fd; }
			EventLoop* getLoop() const { return _loop; }
			const std::string& name() const { return _name; }
			const InetAddress& localAddress() const { return _localAddr; }
			const InetAddress& peerAddress() const { return _peerAddr; }
			bool connected() const { return _state == kConnected; }
			bool disconnected() const { return _state == kDisconnected; }
			Buffer* inputBuffer() { return &_inputBuffer; }
			Buffer* outputBuffer() { return &_outputBuffer; }

			void setConnectionCallback(const ConnectionCallback& cb) { _connectionCallback = cb; }
			void setMessageCallback(const MessageCallback& cb) { _messageCallback = cb; }
			void setWriteCompleteCallback(const WriteCompleteCallback& cb) { _writeCompleteCallback = cb; }
			void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { _highWaterMarkCallback = cb; _highWaterMark = highWaterMark; }
			void setCloseCallback(const CloseCallback& cb) { _closeCallback = cb; }

			void setContext(std::string key, const Any& value)
			{
				_context[key] = value;
			}

			Any& getContext(std::string key) const
			{
				if (_context.find(key) == _context.end())
				{
					_context[key] = Any();
				}
				return _context[key];
			}

		private:
			enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
			void handleRead(Timestamp receiveTime);
			void handleWrite();
			void handleClose();
			void handleError();

			void sendInLoop(const std::string& message);
			void sendInLoop(const void* message, int len);
			void shutdownInLoop();
			void forceCloseInLoop();

			void setState(StateE s) { _state = s; }
			const char* stateToString() const;

		private:
			EventLoop * _loop;
			const std::string _name;
			int _fd;
			const InetAddress _localAddr;
			const InetAddress _peerAddr;

			StateE _state;
			std::unique_ptr<TcpSocket> _socket;
			std::unique_ptr<Channel> _channel;

			ConnectionCallback _connectionCallback;
			MessageCallback _messageCallback;
			WriteCompleteCallback _writeCompleteCallback;
			HighWaterMarkCallback _highWaterMarkCallback;
			CloseCallback _closeCallback;

			size_t _highWaterMark;
			int _rdlen;
			Buffer _inputBuffer;
			Buffer _outputBuffer;
			mutable std::map<std::string, Any> _context;
		};
	}
}

#endif // !__EtherDB_Net_TcpConnection_H_



