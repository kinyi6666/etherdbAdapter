#include "Condition_WIN.h"

#ifdef INWINDOWS

namespace EtherDB
{
	ConditionImpl::ConditionImpl(MutexImpl &impl)
		:_mutex(impl)
	{
		::InitializeConditionVariable(&_pcond);
	}

	ConditionImpl::~ConditionImpl()
	{

	}

	void ConditionImpl::waitImpl()
	{
		::SleepConditionVariableCS(&_pcond, &_mutex._cs, INFINITE);
	}

	bool ConditionImpl::tryWaitImpl(long milliseconds)
	{
		return ::SleepConditionVariableCS(&_pcond, &_mutex._cs, milliseconds);
	}

	void ConditionImpl::notifyImpl()
	{
		::WakeConditionVariable(&_pcond);
	}

	void ConditionImpl::notifyAllImpl()
	{
		::WakeAllConditionVariable(&_pcond);
	}
}


#endif // INWINDOWS


