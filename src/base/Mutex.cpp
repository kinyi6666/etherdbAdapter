//////////////////////////////////////////////////////////////////////////////////
//�ļ���Mutex.cpp  
//���ߣ�LSPZ
//ʱ�䣺2018-03-02
//������������
///////////////////////////////////////////////////////////////////////////////////

#include "Mutex.h"
#include "Exception.h"

namespace EtherDB
{
	void Mutex::lock()
	{
		_impl.lockImpl();
	}

	void Mutex::lock(long milliseconds)
	{
		if (!_impl.tryLockImpl(milliseconds))
			throw TimeoutException();
	}

	bool Mutex::tryLock()
	{
		return _impl.tryLockImpl();
	}

	bool Mutex::tryLock(long milliseconds)
	{
		return _impl.tryLockImpl(milliseconds);
	}

	void Mutex::unlock()
	{
		_impl.unlockImpl();
	}
}
