//////////////////////////////////////////////////////////////////////////////////
//�ļ���Thread_POSIX.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������߳�
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Thread_POSIX_H_
#define __EtherDB_Thread_POSIX_H_

#include "EtherDBConfig.h"

#ifndef INWINDOWS

#include "Noncopyable.h"
#include "Event.h"
#include <string>
#include <pthread.h>
#include <errno.h>
#include <memory>
#include <functional>

namespace EtherDB
{
	class Thread;
	class BASE_API ThreadImpl : Noncopyable
	{
	public:
		typedef std::function<void()> ThreadFun;

		ThreadImpl(const ThreadFun& fun, const std::string name);
		~ThreadImpl();

		int tidImpl() const;
		std::string nameImpl() const;
		void setStackSizeImpl(int size);
		int getStackSizeImpl() const;

		bool isRunningImpl() const;

		void startImpl();
		void joinImpl();
		void joinImpl(long milliseconds);
		bool tryJoinImpl(long milliseconds);


		static void sleepImpl(long milliseconds);
		//
		static void yieldImpl();
		static int currentTidImpl();
		static std::string currentNameImpl();
		static bool isMainThreadImpl();
	private:
		ThreadFun _fun;
		std::string _name;
		int _stackSize;
		std::shared_ptr<Event>  _begin;
		std::shared_ptr<Event>  _end;
		std::shared_ptr<int> _tid;
		std::shared_ptr<int> _state;
		pthread_t _ptid;

		friend Thread;
	};
}



#endif // !INWINDOWS



#endif // !__EtherDB_Thread_POSIX_H_

