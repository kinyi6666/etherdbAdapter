//////////////////////////////////////////////////////////////////////////////////
//�ļ���CountDownLatch.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_CountDownLatch_H_
#define __EtherDB_CountDownLatch_H_

#include "EtherDBConfig.h"
#include "Condition.h"

namespace EtherDB
{
	class BASE_API CountDownLatch : Noncopyable
	{
	public:
		explicit CountDownLatch(int count);
		void wait();
		void wait(long milliseconds);
		bool tryWait(long milliseconds);
		void countDown();
		int getCount() const;

	private:
		mutable Mutex _mutex;
		Condition _condition;
		int _count;
	};
}

#endif // !__EtherDB_CountDownLatch_H_


