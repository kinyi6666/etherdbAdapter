//////////////////////////////////////////////////////////////////////////////////
//�ļ���BoundedBlockingQueue.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������������������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_BoundedBlockingQueue_H_
#define __EtherDB_BoundedBlockingQueue_H_

#include "Condition.h"
#include <assert.h>
#include <queue>

namespace EtherDB
{
	template<typename T>
	class BoundedBlockingQueue : Noncopyable
	{
	public:
		explicit BoundedBlockingQueue(int maxSize)
			: mutex_(),
			notEmpty_(mutex_),
			notFull_(mutex_),
			maxSize_(maxSize),
			queue_()
		{
		}

		void put(const T& x)
		{
			Mutex::MutexLock lock(mutex_);
			while (isQueueFull())
			{
				notFull_.wait();
			}
			assert(!isQueueFull());
			queue_.push(x);
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
			queue_.pop();
			notFull_.notify();
			return front;
		}

		bool empty() const
		{
			Mutex::MutexLock lock(mutex_);
			return queue_.empty();
		}

		bool full() const
		{
			Mutex::MutexLock lock(mutex_);
			return isQueueFull();
		}

		size_t size() const
		{
			Mutex::MutexLock lock(mutex_);
			return queue_.size();
		}

		size_t capacity() const
		{
			Mutex::MutexLock lock(mutex_);
			return queue_.capacity();
		}

	private:
		bool isQueueFull()
		{
			return queue_.size() >= maxSize_;
		}

	private:
		mutable Mutex			   mutex_;
		Condition                  notEmpty_;
		Condition                  notFull_;
		int						   maxSize_;
		std::queue<T>			   queue_;
	};
}

#endif // !__EtherDB_BoundedBlockingQueue_H_


