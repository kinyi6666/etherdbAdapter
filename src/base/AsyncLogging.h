//////////////////////////////////////////////////////////////////////////////////
//�ļ���AsyncLogging.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_AsyncLogging_H_
#define __EtherDB_AsyncLogging_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"
#include "LogStream.h"
#include "Mutex.h"
#include "Thread.h"

#include <memory>
#include <vector>

namespace EtherDB
{
	class BASE_API AsyncLogging
		:Noncopyable
	{
	public:
		// flushIntervalMs: how long the background thread may hold pending log
		// lines in memory before writing+flushing them to the log file, in
		// MILLISECONDS (Condition::tryWait takes milliseconds). Keep this small
		// (order of 1s): with a large value the log file lags behind the events
		// by that amount, which makes `tail -f` useless for debugging.
		AsyncLogging(const std::string& basename,
			off_t rollSize,
			int flushIntervalMs = 3000);

		~AsyncLogging();

		void append(const char* logline, int len);
		void start();
		void stop();

	private:
		void threadFunc();

		typedef detail::FixedBuffer<detail::kLargeBuffer> Buffer;
		typedef std::shared_ptr<Buffer> BufferPtr;
		typedef std::vector<BufferPtr> BufferVector;

		const int _flushIntervalMs;
		bool _running;
		std::string _basename;
		off_t _rollSize;
		Thread _thread;
		CountDownLatch _latch;
		Mutex _mutex;
		Condition _cond;
		BufferPtr _currentBuffer;
		BufferVector _buffers;
	};
}


#endif // !__EtherDB_AsyncLogging_H_


