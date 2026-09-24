//////////////////////////////////////////////////////////////////////////////////
//�ļ���Mutex_POSIX.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-03-02
//������POSIX�������ӿ�
///////////////////////////////////////////////////////////////////////////////////

#include "Mutex_POSIX.h"

#ifndef INWINDOWS

#include "Exception.h"
#include "Timestamp.h"
#include <time.h>
#include <sys/time.h>

namespace EtherDB
{
	MutexImpl::MutexImpl()
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		if (pthread_mutex_init(&_mutex, &attr))
		{
			pthread_mutexattr_destroy(&attr);
			throw SystemException("cannot create mutex");
		}
		pthread_mutexattr_destroy(&attr);
	}

	MutexImpl::~MutexImpl()
	{
		pthread_mutex_destroy(&_mutex);
	}

	void MutexImpl::lockImpl()
	{
		if (pthread_mutex_lock(&_mutex))
			throw SystemException("cannot lock mutex");
	}

	bool MutexImpl::tryLockImpl()
	{
		int rc = pthread_mutex_trylock(&_mutex);
		if (rc == 0)
			return true;
		else if (rc == EBUSY)
			return false;
		else
			throw SystemException("cannot lock mutex");
	}

	bool MutexImpl::tryLockImpl(long milliseconds)
	{
		struct timespec abstime;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		abstime.tv_sec = tv.tv_sec + milliseconds / 1000;
		abstime.tv_nsec = tv.tv_usec * 1000 + (milliseconds % 1000) * 1000000;
		if (abstime.tv_nsec >= 1000000000)
		{
			abstime.tv_nsec -= 1000000000;
			abstime.tv_sec++;
		}
		int rc = pthread_mutex_timedlock(&_mutex, &abstime);
		if (rc == 0)
			return true;
		else if (rc == ETIMEDOUT)
			return false;
		else
			throw SystemException("cannot lock mutex");
	}

	void MutexImpl::unlockImpl()
	{
		if (pthread_mutex_unlock(&_mutex))
			throw SystemException("cannot unlock mutex");
	}



}


#endif // !INWINDOWS



