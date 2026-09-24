//////////////////////////////////////////////////////////////////////////////////
//�ļ���Condition_POSIX.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//��������ɱ���POSIX�ӿ�
//////////////////////////////////////////////////////////////////////////////////

#include "Condition_POSIX.h"

#ifndef INWINDOWS

#include "Timestamp.h"
#include "Exception.h"

namespace EtherDB
{
	ConditionImpl::ConditionImpl(MutexImpl &impl)
		:_mutex(impl)
	{
		::pthread_cond_init(&_pcond, NULL);
	}

	ConditionImpl::~ConditionImpl()
	{
		::pthread_cond_destroy(&_pcond);
	}

	void ConditionImpl::waitImpl()
	{
		::pthread_cond_wait(&_pcond, &(_mutex._mutex));
	}

	bool ConditionImpl::tryWaitImpl(long milliseconds)
	{
		struct timespec abstime;
		Timestamp stamp = Timestamp::now();
		abstime.tv_sec = stamp.microSecondsSinceEpoch() / Timestamp::kMicroSecondsPerSecond + milliseconds / 1000;
		abstime.tv_nsec = (stamp.microSecondsSinceEpoch() % Timestamp::kMicroSecondsPerSecond) * 1000 + (milliseconds % 1000) * 1000000;
		if (abstime.tv_nsec >= 1000000000)
		{
			abstime.tv_nsec -= 1000000000;
			abstime.tv_sec++;
		}
		int rc = pthread_cond_timedwait(&_pcond, &(_mutex._mutex), &abstime);
		if (rc == 0)
			return true;
		else if (rc == ETIMEDOUT)
			return false;
		else
			throw SystemException("cannot wait cond");
	}

	void ConditionImpl::notifyImpl()
	{
		::pthread_cond_signal(&_pcond);
	}

	void ConditionImpl::notifyAllImpl()
	{
		::pthread_cond_broadcast(&_pcond);
	}
}

#endif // INWINDOWS
