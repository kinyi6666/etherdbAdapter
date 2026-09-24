//////////////////////////////////////////////////////////////////////////////////
//�ļ���Singleton.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//����������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Singleton_H_
#define __EtherDB_Singleton_H_

#include "Mutex.h"
#include <assert.h>
#include <stdlib.h>

namespace EtherDB
{
	namespace detail
	{
		template<typename T>
		struct has_no_destroy {
			template<typename U, void (U::*)()> struct HELPS;
			template<typename U> static char Test(HELPS<U, &U::no_destroy>*);
			template<typename U> static int Test(...);
			const static bool value = sizeof(Test<T>(0)) == sizeof(char);
		};
	}

	template<typename T>
	class Singleton : Noncopyable
	{
	public:
		static T& instance()
		{
			if (value_ == NULL)
			{
				MutexLock lock(mutex_);
				if (value_ == NULL)
				{
					init();
				}
			}
			assert(value_ != NULL);
			return *value_;
		}

		static void init()
		{
			value_ = new T();
			if (!detail::has_no_destroy<T>::value)
			{
				::atexit(destroy);
			}
		}

		static void destroy()
		{
			typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
			T_must_be_complete_type dummy; (void)dummy;

			delete value_;
			value_ = NULL;
		}

	private:
		Singleton();
		~Singleton();

		static Mutex mutex_;
		static T* value_;
	};

	template<typename T>
	Mutex Singleton<T>::mutex_;

	template<typename T>
	T* Singleton<T>::value_ = NULL;
}

#endif // !__EtherDB_Singleton_H_







