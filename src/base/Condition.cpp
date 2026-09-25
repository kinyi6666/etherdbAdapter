//////////////////////////////////////////////////////////////////////////////////
//�ļ���Condition.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���
////////////////////////////////////////////////////////////////////////////////////


#include "Condition.h"
#include "Exception.h"

namespace EtherDB
{
	Condition::Condition(Mutex& mutex)
		:_impl(mutex._impl)
	{}

	Condition::~Condition() { }

	void Condition::wait()
	{
		_impl.waitImpl();
	}

	void Condition::wait(long milliseconds)
	{
		if (!tryWait(milliseconds))
			throw TimeoutException();
	}

	bool Condition::tryWait(long milliseconds)
	{
		return _impl.tryWaitImpl(milliseconds);
	}

	void Condition::notify()
	{
		_impl.notifyImpl();
	}

	void Condition::notifyAll()
	{
		_impl.notifyAllImpl();
	}

}