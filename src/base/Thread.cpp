//////////////////////////////////////////////////////////////////////////////////
//�ļ���Thread.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������߳�
////////////////////////////////////////////////////////////////////////////////////

#include "Thread.h"

namespace EtherDB
{
	AtomicInt32 Thread::_numCreated;

	Thread::Thread(const ThreadFun& fun)
		:_id(_numCreated.incrementAndGet()),
		_impl(fun, "Thread")
	{
		char buf[24] = { 0 };
		snprintf(buf, sizeof(buf), "Thread%d", _id);
		_impl._name = buf;
	}

	Thread::Thread(const ThreadFun& fun, const std::string& name)
		:_id(_numCreated.incrementAndGet()),
		_impl(fun, name)
	{

	}

	Thread::~Thread()
	{

	}


	//�߳�ID
	int Thread::id() const
	{
		return _id;
	}

	//�߳�TID
	int Thread::tid() const
	{
		return _impl.tidImpl();
	}

	//�߳�����
	std::string Thread::name() const
	{
		return _impl.nameImpl();
	}

	//�����̶߳�ջ��С
	void Thread::setStackSize(int size)
	{
		_impl.setStackSizeImpl(size);
	}

	//��ȡ�̶߳�ջ��С
	int Thread::getStackSize() const
	{
		return _impl.getStackSizeImpl();
	}


	void Thread::start()
	{
		_impl.startImpl();
	}

	void Thread::join()
	{
		_impl.joinImpl();
	}

	void Thread::join(long milliseconds)
	{
		_impl.joinImpl(milliseconds);
	}

	bool Thread::tryJoin(long milliseconds)
	{
		return _impl.tryJoinImpl(milliseconds);
	}

	bool Thread::isRunning() const
	{
		return _impl.isRunningImpl();
	}



	void Thread::sleep(long milliseconds)
	{
		ThreadImpl::sleepImpl(milliseconds);
	}

	void Thread::yield()
	{
		ThreadImpl::yieldImpl();
	}

	int Thread::currentTid()
	{
		return ThreadImpl::currentTidImpl();
	}

	std::string Thread::currentName()
	{
		return ThreadImpl::currentNameImpl();
	}

	bool Thread::isMainThread()
	{
		return ThreadImpl::isMainThreadImpl();
	}

}












