//////////////////////////////////////////////////////////////////////////////////
//�ļ���TcpConnection.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#include "TcpConnection.h"
#include <base/Logging.h>
//#include <base/WeakCallback.h>

namespace EtherDB
{
	namespace Net
	{
		void defaultConnectionCallback(const TcpConnectionPtr& conn)
		{
			LOG_TRACE << conn->localAddress().toIpPort() << " -> "
				<< conn->peerAddress().toIpPort() << " is "
				<< (conn->connected() ? "UP" : "DOWN");
		}

		void defaultMessageCallback(const TcpConnectionPtr&,
			Buffer* buf,
			Timestamp)
		{
			buf->retrieveAll();
		}
	}
}

#ifndef INWINDOWS

namespace EtherDB
{
	namespace Net
	{
		TcpConnection::TcpConnection(EventLoop *loop,
			const std::string name,
			int sockfd,
			const InetAddress &localAddr,
			const InetAddress &peerAddr)
			:_loop(loop),
			_name(name),
			_fd(sockfd),
			_localAddr(localAddr),
			_peerAddr(peerAddr),
			_state(kConnecting),
			_socket(new TcpSocket(_fd)),
			_channel(new Channel(loop, _fd)),
			_highWaterMark(64 * 1024 * 1024),
			_rdlen(2048)
		{
			_channel->setReadCallback(
				std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
			_channel->setWriteCallback(
				std::bind(&TcpConnection::handleWrite, this));
			_channel->setCloseCallback(
				std::bind(&TcpConnection::handleClose, this));
			_channel->setErrorCallback(
				std::bind(&TcpConnection::handleError, this));
			LOG_TRACE << "TcpConnection::ctor[" << _name << "] at " << this
				<< " fd=" << sockfd;
			_socket->setKeepAlive(true);
		}

		TcpConnection::~TcpConnection()
		{
			LOG_TRACE << "TcpConnection::dtor[" << _name << "] at " << this
				<< " fd=" << _channel->fd()
				<< " state=" << stateToString();
			assert(_state == kDisconnected);
		}

		void TcpConnection::connectEstablished()
		{
			_loop->assertInLoopThread();
			assert(_state == kConnecting);
			setState(kConnected);
			_channel->tie(shared_from_this());
			_channel->enableReading();
			_connectionCallback(shared_from_this());
		}

		void TcpConnection::connectDestroyed()
		{
			_loop->assertInLoopThread();
			if (_state == kConnected)
			{
				setState(kDisconnected);
				_channel->disableAll();

				_connectionCallback(shared_from_this());
			}
			_channel->remove();
		}

		void TcpConnection::sendInLoop(const void* message, int len)
		{
			_loop->assertInLoopThread();
			if (_state == kDisconnected)
			{
				LOG_TRACE << "disconnected, give up writing";
				return;
			}

			ssize_t nwrote = 0;
			size_t remaining = len;
			bool faultError = false;

			if (!_channel->isWriting() && _outputBuffer.readableBytes() == 0)
			{
				nwrote = _socket->write((char *)message, len);
				if (nwrote >= 0)
				{
					remaining = len - nwrote;
					if (remaining == 0 && _writeCompleteCallback)
					{
						_loop->queueInLoop(std::bind(_writeCompleteCallback, shared_from_this()));
					}
				}
				else
				{
					nwrote = 0;
					if (errno != EWOULDBLOCK)
					{
						LOG_SYSERR << "TcpConnection::sendInLoop";
						if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
						{
							faultError = true;
						}
					}
				}
			}

			assert(remaining <= len);
			if (!faultError && remaining > 0)
			{
				size_t oldLen = _outputBuffer.readableBytes();
				if (oldLen + remaining >= _highWaterMark
					&& oldLen < _highWaterMark
					&& _highWaterMarkCallback)
				{
					_loop->queueInLoop(std::bind(_highWaterMarkCallback, shared_from_this(), oldLen + remaining));
				}
				_outputBuffer.append(static_cast<const char*>(message) + nwrote, remaining);
				if (!_channel->isWriting())
				{
					_channel->enableWriting();
				}
			}
		}

		void TcpConnection::handleRead(Timestamp receiveTime)
		{
			_loop->assertInLoopThread();
			int savedErrno = 0;
			_inputBuffer.ensureWritableBytes(_rdlen);
			int len = _socket->read(_inputBuffer.beginWrite(), _inputBuffer.writableBytes());
			if (len > 0)
			{
				_inputBuffer.hasWritten(len);
				_messageCallback(shared_from_this(), &_inputBuffer, receiveTime);
			}
			else if (len == 0)
			{
				handleClose();
			}
			else
			{
				errno = savedErrno;
				LOG_SYSERR << "TcpConnection::handleRead";
				handleError();
			}
		}

		void TcpConnection::handleWrite()
		{
			_loop->assertInLoopThread();
			if (_channel->isWriting())
			{
				ssize_t n = _socket->write((char *)_outputBuffer.peek(), _outputBuffer.readableBytes());
				if (n > 0)
				{
					_outputBuffer.retrieve(n);
					if (_outputBuffer.readableBytes() == 0)
					{
						_channel->disableWriting();
						if (_writeCompleteCallback)
						{
							_loop->queueInLoop(std::bind(_writeCompleteCallback, shared_from_this()));
						}
						if (_state == kDisconnecting)
						{
							shutdownInLoop();
						}
					}
				}
				else
				{
					LOG_SYSERR << "TcpConnection::handleWrite";
				}
			}
			else
			{
				LOG_TRACE << "Connection fd = " << _channel->fd()
					<< " is down, no more writing";
			}
		}

		void TcpConnection::handleClose()
		{
			_loop->assertInLoopThread();
			LOG_TRACE << "fd = " << _channel->fd() << " state = " << stateToString();
			assert(_state == kConnected || _state == kDisconnecting);
			setState(kDisconnected);
			_channel->disableAll();

			TcpConnectionPtr guardThis(shared_from_this());
			_connectionCallback(guardThis);
			_closeCallback(guardThis);
		}

		void TcpConnection::shutdownInLoop()
		{
			_loop->assertInLoopThread();
			if (!_channel->isWriting())
			{
				_socket->shutdownWrite();
			}
		}

		void TcpConnection::forceCloseInLoop()
		{
			_loop->assertInLoopThread();
			if (_state == kConnected || _state == kDisconnecting)
			{
				handleClose();
			}
		}

		void TcpConnection::send(const void* message, int len)
		{
			send(std::string(static_cast<const char*>(message), len));
		}

		void TcpConnection::send(const std::string& message)
		{
			if (_state == kConnected)
			{
				if (_loop->isInLoopThread())
				{
					sendInLoop(message);
				}
				else
				{
					_loop->runInLoop(
						std::bind((void(TcpConnection::*)(const std::string&)) &TcpConnection::sendInLoop,
							this,
							message));
				}
			}
		}

		void TcpConnection::send(Buffer* message)
		{
			if (_state == kConnected)
			{
				if (_loop->isInLoopThread())
				{
					sendInLoop(message->peek(), message->readableBytes());
					message->retrieveAll();
				}
				else
				{
					_loop->runInLoop(
						std::bind((void(TcpConnection::*)(const std::string&))&TcpConnection::sendInLoop,
							this,     // FIXME
							message->retrieveAllAsString()));
				}
			}
		}

		void TcpConnection::sendInLoop(const std::string& message)
		{
			sendInLoop(message.data(), message.size());
		}



		void TcpConnection::shutdown()
		{
			if (_state == kConnected)
			{
				setState(kDisconnecting);
				_loop->runInLoop(std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
			}
		}

		void TcpConnection::forceClose()
		{
			if (_state == kConnected || _state == kDisconnecting)
			{
				setState(kDisconnecting);
				_loop->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
			}
		}

		void TcpConnection::forceCloseWithDelay(double seconds)
		{
			if (_state == kConnected || _state == kDisconnecting)
			{
				//setState(kDisconnecting);
				//_loop->runAfter(
				//	seconds,
				//	makeWeakCallback(shared_from_this(),
				//		&TcpConnection::forceClose));
			}
		}

		void TcpConnection::setTcpNoDelay(bool on)
		{
			_socket->setTcpNoDelay(on);
		}

		void TcpConnection::handleError()
		{
			LOG_ERROR << "TcpConnection::handleError fd=" << _channel->fd();
			if (_state == kConnected || _state == kDisconnecting)
				handleClose();
		}

		const char* TcpConnection::stateToString() const
		{
			switch (_state)
			{
			case kDisconnected:
				return "kDisconnected";
			case kConnecting:
				return "kConnecting";
			case kConnected:
				return "kConnected";
			case kDisconnecting:
				return "kDisconnecting";
			default:
				return "unknown state";
			}
		}
	}
}

#else

namespace EtherDB
{
	namespace Net
	{
		TcpConnection::TcpConnection(EventLoop *loop,
			const std::string name,
			int sockfd,
			const InetAddress &localAddr,
			const InetAddress &peerAddr)
			:_loop(loop),
			_name(name),
			_fd(sockfd),
			_localAddr(localAddr),
			_peerAddr(peerAddr),
			_state(kConnecting),
			_socket(new TcpSocket(_fd)),
			_channel(new Channel(loop, _fd)),
			_highWaterMark(64 * 1024 * 1024),
			_rdlen(2048)
		{
			_channel->setReadCallback(
				std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
			_channel->setWriteCallback(
				std::bind(&TcpConnection::handleWrite, this));
			_channel->setCloseCallback(
				std::bind(&TcpConnection::handleClose, this));
			_channel->setErrorCallback(
				std::bind(&TcpConnection::handleError, this));
			LOG_TRACE << "TcpConnection::ctor[" << _name << "] at " << this
				<< " fd=" << sockfd;
			_socket->setKeepAlive(true);
		}

		TcpConnection::~TcpConnection()
		{
			LOG_TRACE << "TcpConnection::dtor[" << _name << "] at " << this
				<< " fd=" << _channel->fd()
				<< " state=" << stateToString();
			assert(_state == kDisconnected);
		}

		void TcpConnection::connectEstablished()
		{
			_loop->assertInLoopThread();
			assert(_state == kConnecting);
			setState(kConnected);
			_channel->tie(shared_from_this());
			_channel->enableReading();

			_connectionCallback(shared_from_this());

			if (_state == kConnected)
			{
				if (!_socket->readRequest((char*)_inputBuffer.beginWrite(), _inputBuffer.writableBytes()))
				{
					_channel->disableReading();
					handleClose();
				}
			}
			else
			{
				_channel->disableReading();
				handleClose();
			}
		}

		void TcpConnection::connectDestroyed()
		{
			_loop->assertInLoopThread();
			if (_state == kConnected)
			{
				setState(kDisconnected);
				_channel->disableAll();

				_connectionCallback(shared_from_this());
			}
			_channel->remove();
		}

		void TcpConnection::sendInLoop(const void* message, int len)
		{
			_loop->assertInLoopThread();
			if (_state == kDisconnected)
			{
				LOG_TRACE << "disconnected, give up writing";
				return;
			}

			if (!_channel->isWriting() && _outputBuffer.readableBytes() == 0)
			{
				_outputBuffer.append(message, len);
				if (_socket->writeRequest((char*)_outputBuffer.peek(), _outputBuffer.readableBytes()))
				{
					_channel->enableWriting();
				}
				else
				{
					if (_state == kConnected || _state == kDisconnecting)
						handleClose();
				}
			}
			else
			{
				_outputBuffer.append(message, len);
			}
		}

		void TcpConnection::handleRead(Timestamp receiveTime)
		{
			_loop->assertInLoopThread();
			_channel->disableReading();
			int savedErrno = 0;
			int len = _socket->readSize();
			if (len > 0)
			{
				_inputBuffer.hasWritten(len);
				_messageCallback(shared_from_this(), &_inputBuffer, receiveTime);
				_channel->enableReading();
				_inputBuffer.ensureWritableBytes(_rdlen);
				if (!_socket->readRequest((char*)_inputBuffer.beginWrite(), _inputBuffer.writableBytes()))
				{
					_channel->disableReading();
					if (_state == kConnected || _state == kDisconnecting)
						handleClose();
				}
			}
			else
			{
				if (_state == kConnected || _state == kDisconnecting)
					handleClose();
			}
		}

		void TcpConnection::handleWrite()
		{
			_loop->assertInLoopThread();
			_channel->disableWriting();
			int len = _socket->writeSize();
			if (len > 0)
			{
				_outputBuffer.retrieve(len);
				if (_outputBuffer.readableBytes() == 0)
				{
					if (_writeCompleteCallback)
					{
						_loop->queueInLoop(std::bind(_writeCompleteCallback, shared_from_this()));
					}
					if (_state == kDisconnecting)
					{
						shutdownInLoop();
					}
				}
				else
				{
					if (_socket->writeRequest((char *)_outputBuffer.peek(), _outputBuffer.readableBytes()))
					{
						_channel->enableWriting();
					}
					else
					{
						if (_state == kConnected || _state == kDisconnecting)
							handleClose();
					}
				}
			}
			else
			{
				if (_state == kConnected || _state == kDisconnecting)
					handleClose();
			}

		}


		void TcpConnection::handleClose()
		{
			_loop->assertInLoopThread();
			LOG_TRACE << "fd = " << _channel->fd() << " state = " << stateToString();
			assert(_state == kConnected || _state == kDisconnecting);
			setState(kDisconnected);
			_channel->disableAll();

			TcpConnectionPtr guardThis(shared_from_this());
			_connectionCallback(guardThis);
			_closeCallback(guardThis);
		}

		void TcpConnection::shutdownInLoop()
		{
			_loop->assertInLoopThread();
			if (!_channel->isWriting())
			{
				_socket->shutdownWrite();
			}
		}

		void TcpConnection::forceCloseInLoop()
		{
			_loop->assertInLoopThread();
			if (_state == kConnected || _state == kDisconnecting)
			{
				handleClose();
			}
		}

		void TcpConnection::send(const void* message, int len)
		{
			send(std::string(static_cast<const char*>(message), len));
		}

		void TcpConnection::send(const std::string& message)
		{
			if (_state == kConnected)
			{
				if (_loop->isInLoopThread())
				{
					sendInLoop(message);
				}
				else
				{
					_loop->runInLoop(
						std::bind((void(TcpConnection::*)(const std::string&)) &TcpConnection::sendInLoop,
							this,
							message));
				}
			}
		}

		void TcpConnection::send(Buffer* message)
		{
			if (_state == kConnected)
			{
				if (_loop->isInLoopThread())
				{
					sendInLoop(message->peek(), message->readableBytes());
					message->retrieveAll();
				}
				else
				{
					_loop->runInLoop(
						std::bind((void(TcpConnection::*)(const std::string&))&TcpConnection::sendInLoop,
							this,     // FIXME
							message->retrieveAllAsString()));
				}
			}
		}

		void TcpConnection::sendInLoop(const std::string& message)
		{
			sendInLoop(message.data(), message.size());
		}

		void TcpConnection::shutdown()
		{
			if (_state == kConnected)
			{
				setState(kDisconnecting);
				_loop->runInLoop(std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
			}
		}

		void TcpConnection::forceClose()
		{
			if (_state == kConnected || _state == kDisconnecting)
			{
				setState(kDisconnecting);
				_loop->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
			}
		}

		void TcpConnection::forceCloseWithDelay(double seconds)
		{
			if (_state == kConnected || _state == kDisconnecting)
			{
				//setState(kDisconnecting);
				//_loop->runAfter(
				//	seconds,
				//	makeWeakCallback(shared_from_this(),
				//		&TcpConnection::forceClose));
			}
		}

		void TcpConnection::setTcpNoDelay(bool on)
		{
			_socket->setTcpNoDelay(on);
		}

		void TcpConnection::handleError()
		{
			if (_state == kConnected || _state == kDisconnecting)
				handleClose();
		}

		const char* TcpConnection::stateToString() const
		{
			switch (_state)
			{
			case kDisconnected:
				return "kDisconnected";
			case kConnecting:
				return "kConnecting";
			case kConnected:
				return "kConnected";
			case kDisconnecting:
				return "kDisconnecting";
			default:
				return "unknown state";
			}
		}
	}
}


#endif // !INWINDOWS




