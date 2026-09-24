//////////////////////////////////////////////////////////////////////////////////
//�ļ���AtomicInt.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//������ԭ����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_AtomicInt_H_
#define __EtherDB_AtomicInt_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"

namespace EtherDB
{
	namespace detail
	{
		template <typename T>
		class AtomicIntegerT : Noncopyable
		{
		public:
			AtomicIntegerT():_value(0) {}

			T get();
			T getAndAdd(T x);
			T addAndGet(T x);
			T getAndSet(T newValue);
			T incrementAndGet() { return addAndGet(1); }
			T decrementAndGet() { return addAndGet(-1); }
			void add(T x) { getAndAdd(x); }
			void increment() { incrementAndGet(); }
			void decrement() { decrementAndGet(); }

		private:
			volatile T _value;
		};


	}

	typedef detail::AtomicIntegerT<uint32_t> AtomicInt32;
	typedef detail::AtomicIntegerT<uint64_t> AtomicInt64;
}

#ifdef INWINDOWS

#include "AmWindows.h"

namespace EtherDB
{
	namespace detail
	{
		template <typename T>
		T AtomicIntegerT<T>::get()
		{
			return ::InterlockedCompareExchange(&_value, 0, 0);
		}

		template <typename T>
		T AtomicIntegerT<T>::getAndAdd(T x)
		{
			return ::InterlockedExchangeAdd(&_value, x);
		}

		template <typename T>
		T AtomicIntegerT<T>::addAndGet(T x)
		{
			return ::InterlockedExchangeAdd(&_value, x) + x;
		}

		template <typename T>
		T AtomicIntegerT<T>::getAndSet(T newValue)
		{
			return ::InterlockedExchange(&_value, newValue);
		}
	}
}


#else

namespace EtherDB
{
	namespace detail
	{
		template <typename T>
		T AtomicIntegerT<T>::get()
		{
			return __sync_val_compare_and_swap(&_value, 0, 0);
		}

		template <typename T>
		T AtomicIntegerT<T>::getAndAdd(T x)
		{
			return __sync_fetch_and_add(&_value, x);
		}

		template <typename T>
		T AtomicIntegerT<T>::addAndGet(T x)
		{
			return getAndAdd(x) + x;
		}

		template <typename T>
		T AtomicIntegerT<T>::getAndSet(T newValue)
		{
			//type __sync_lock_test_and_set (type *ptr, type value, ...)
			//��*ptr��Ϊvalue������*ptr����֮ǰ��ֵ
			//void __sync_lock_release(type *ptr, ...)
			//��*ptr��0
			return __sync_lock_test_and_set(&_value, newValue);
		}
	}
}


#endif // INWINDOWS



#endif //!__EtherDB_AtomicInt_H_


