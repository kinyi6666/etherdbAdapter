//////////////////////////////////////////////////////////////////////////////////
//�ļ���CountDownLatch.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//������
////////////////////////////////////////////////////////////////////////////////////

#include "CountDownLatch.h"
#include "Exception.h"

namespace EtherDB
{
	CountDownLatch::CountDownLatch(int count)
		:_condition(_mutex),
		_count(count)
	{	}

	void CountDownLatch::wait()
	{
		MutexLock lock(_mutex);
		while (_count > 0)
		{
			_condition.wait();
		}
	}

	void CountDownLatch::wait(long milliseconds)
	{
		if (!tryWait(milliseconds))
			throw TimeoutException();
	}

	bool CountDownLatch::tryWait(long milliseconds)
	{
		MutexLock lock(_mutex);
		if (_count <= 0)
			return true;

		_condition.wait(milliseconds);
		if (_count <= 0)
			return true;
		else
			return false;
	}

	void CountDownLatch::countDown()
	{
		MutexLock lock(_mutex);
		--_count;
		if (_count <= 0)
		{
			_condition.notifyAll();
		}
	}


	int CountDownLatch::getCount() const
	{
		MutexLock lock(_mutex);
		return _count;
	}
}

