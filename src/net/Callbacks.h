//////////////////////////////////////////////////////////////////////////////////
//�ļ���Callbacks.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//�������ص���������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_Callbacks_H_
#define __EtherDB_Net_Callbacks_H_

#include "AmNetConfig.h"
#include <base/Timestamp.h>
#include <memory>
#include <functional>


namespace EtherDB
{
	namespace Net
	{
		class Buffer;
		class TcpConnection;
		typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
		typedef std::weak_ptr<TcpConnection> TcpConnectionWtr;
		typedef std::function<void()> TimerCallback;
		typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
		typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;
		typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;
		typedef std::function<void(const TcpConnectionPtr&, size_t)> HighWaterMarkCallback;

		typedef std::function<void(const TcpConnectionPtr&,
			Buffer*,
			Timestamp)> MessageCallback;

		void defaultConnectionCallback(const TcpConnectionPtr& conn);
		void defaultMessageCallback(const TcpConnectionPtr& conn,
			Buffer* buffer,
			Timestamp receiveTime);
	}
}



#endif // !__EtherDB_Net_Callbacks_H_



