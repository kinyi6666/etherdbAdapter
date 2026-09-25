//////////////////////////////////////////////////////////////////////////////////
//�ļ���WeakCallback.h  
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#include "AsyncLogging.h"
#include "LogFile.h"
#include "Condition.h"
#include "Timestamp.h"

namespace EtherDB
{
	AsyncLogging::AsyncLogging(const std::string& basename,
		off_t rollSize,
		int flushIntervalMs)
		:_flushIntervalMs(flushIntervalMs),
		_running(false),
		_basename(basename),
		_rollSize(rollSize),
		_thread(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
		_latch(1),
		_mutex(),
		_cond(_mutex),
		_currentBuffer(new Buffer),
		_buffers()
	{
		_currentBuffer->bzero();
		_buffers.reserve(16);
	}

	AsyncLogging::~AsyncLogging()
	{
		if (_running)
		{
			stop();
		}
	}

	void AsyncLogging::append(const char* logline, int len)
	{
		MutexLock lock(_mutex);
		if (_currentBuffer->avail() > len)
		{
			_currentBuffer->append(logline, len);
		}
		else
		{
			_buffers.push_back(_currentBuffer);
			_currentBuffer.reset(new Buffer); // Rarely happens
			_currentBuffer->append(logline, len);
			_cond.notify();
		}

	}

	void AsyncLogging::start()
	{
		_running = true;
		_thread.start();
		_latch.wait();
	}

	void AsyncLogging::stop()
	{
		_running = false;
		_cond.notify();
		_thread.join();
	}

	void AsyncLogging::threadFunc()
	{
		assert(_running == true);
		_latch.countDown();
		LogFile output(_basename, _rollSize, false);
		BufferVector buffersToWrite;
		buffersToWrite.reserve(16);

		while (_running)
		{
			assert(buffersToWrite.empty());
			{
				MutexLock lock(_mutex);
				if (_buffers.empty())  // unusual usage!
				{
					// _flushIntervalMs is in milliseconds (Condition::tryWait
					// takes milliseconds) — flush pending lines at least this
					// often so the log file does not lag behind events.
					_cond.tryWait(_flushIntervalMs);
				}
				_buffers.push_back(_currentBuffer);
				_currentBuffer.reset(new Buffer); // Rarely happens
				buffersToWrite.swap(_buffers);
			}

			assert(!buffersToWrite.empty());

			for (size_t i = 0; i < buffersToWrite.size(); ++i)
			{
				output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());
			}

			buffersToWrite.clear();
			output.flush();
		}
		output.flush();
	}

}


