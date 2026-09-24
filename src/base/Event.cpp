//////////////////////////////////////////////////////////////////////////////////
//�ļ���Event.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���
////////////////////////////////////////////////////////////////////////////////////

#include "Event.h"
#include "Exception.h"

namespace EtherDB
{
	Event::Event(bool autoReset)
		:_condition(_mutex),
		_count(1),
		_auto(autoReset)
	{	}

	Event::~Event()
	{	}

	void Event::set()
	{
		MutexLock lock(_mutex);
		_count = 0;
		if (_auto)
		{
			_condition.notify();
		}
		else
		{
			_condition.notifyAll();
		}
	}

	void Event::wait()
	{
		MutexLock lock(_mutex);
		while (_count > 0)
		{
			_condition.wait();
		}
		if (_auto)
			_count = 1;
	}

	void Event::wait(long milliseconds)
	{
		if (!tryWait(milliseconds))
			throw TimeoutException();
	}

	bool Event::tryWait(long milliseconds)
	{
		MutexLock lock(_mutex);

		if (_count <= 0)
		{
			if (_auto)
			{
				_count = 1;
			}
			return true;
		}

		_condition.wait(milliseconds);

		if (_count <= 0)
		{
			if (_auto)
			{
				_count = 1;
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	void Event::reset()
	{
		MutexLock lock(_mutex);
		_count = 1;
	}
}





