//////////////////////////////////////////////////////////////////////////////////
//�ļ���Mutex_WIN.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-03-02
//������WINDOWS�������ӿ�
///////////////////////////////////////////////////////////////////////////////////

#include "Mutex_WIN.h"
#include "Exception.h"
#include "Timestamp.h"

#ifdef INWINDOWS

namespace EtherDB
{
	MutexImpl::MutexImpl()
	{
		::InitializeCriticalSectionAndSpinCount(&_cs, 4000);
	}

	MutexImpl::~MutexImpl()
	{
		::DeleteCriticalSection(&_cs);
	}

	void MutexImpl::lockImpl()
	{
		try
		{
			::EnterCriticalSection(&_cs);
		}
		catch (...)
		{
			throw SystemException("cannot lock mutex");
		}
	}

	bool MutexImpl::tryLockImpl()
	{
		try
		{
			return ::TryEnterCriticalSection(&_cs) != 0;
		}
		catch (...)
		{

		}
		throw SystemException("cannot lock mutex");
	}

	bool MutexImpl::tryLockImpl(long milliseconds)
	{
		const int sleepMillis = 5;
		Timestamp now;
		int64_t diff(int64_t(milliseconds) * 1000);
		do
		{
			try
			{
				if (TryEnterCriticalSection(&_cs) == TRUE)
					return true;
			}
			catch (...)
			{
				throw SystemException("cannot lock mutex");
			}
			::Sleep(sleepMillis);
		} while (!now.isElapsed(diff));
		return false;
	}

	void MutexImpl::unlockImpl()
	{
		::LeaveCriticalSection(&_cs);
	}
}

#endif // INWINDOWS


