//////////////////////////////////////////////////////////////////////////////////
//�ļ���BlockingQueue.h   
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_BlockingQueue_H_
#define __EtherDB_BlockingQueue_H_

#include "Condition.h"
#include <deque>
#include <assert.h>

namespace EtherDB
{
	template<typename T>
	class BlockingQueue : Noncopyable
	{
	public:
		BlockingQueue()
			: mutex_(),
			notEmpty_(mutex_),
			queue_()
		{
		}

		void put(const T& x)
		{
			Mutex::MutexLock lock(mutex_);
			queue_.push_back(x);
			notEmpty_.notify();
		}

		T take()
		{
			Mutex::MutexLock lock(mutex_);
			while (queue_.empty())
			{
				notEmpty_.wait();
			}
			assert(!queue_.empty());
			T front(queue_.front());
			queue_.pop_front();
			return front;
		}

		size_t size() const
		{
			Mutex::MutexLock lock(mutex_);
			return queue_.size();
		}

	private:
		mutable Mutex mutex_;
		Condition         notEmpty_;
		std::deque<T>     queue_;
	};
}

#endif // !__EtherDB_BlockingQueue_H_


