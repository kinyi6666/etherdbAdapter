//////////////////////////////////////////////////////////////////////////////////
//�ļ���ThreadLocal.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������߳�
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_ThreadLocal_H_
#define __EtherDB_ThreadLocal_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"
#include <map>


namespace EtherDB
{
	class ThreadLocalStorage;
	extern __thread ThreadLocalStorage *t_localStorage;

	class BASE_API TLSAbstractSlot
	{
	public:
		TLSAbstractSlot();
		virtual ~TLSAbstractSlot();
	};

	template <class C>
	class TLSSlot : public TLSAbstractSlot, Noncopyable
	{
	public:
		TLSSlot() :_value() {}
		~TLSSlot() {}
		C &value(){	return _value;}

	private:
		C _value;
	};

	class BASE_API ThreadLocalStorage
	{
	public:
		ThreadLocalStorage();
		~ThreadLocalStorage();

		TLSAbstractSlot*& get(const void* key);
		static ThreadLocalStorage& current();

		static void clear();
	private:
		typedef std::map<const void*, TLSAbstractSlot*> TLSMap;
		TLSMap _map;
	};

	template <class C>
	class ThreadLocal : Noncopyable
	{
		typedef TLSSlot<C> Slot;
	public:
		ThreadLocal(){}

		~ThreadLocal(){}

		C* operator -> (){	return &get();	}
		C& operator * (){	return get();}

		C& get()
		{
			TLSAbstractSlot*& p = ThreadLocalStorage::current().get(this);
			if (!p)
			{
				p = new Slot;
			}
			return static_cast<Slot*>(p)->value();
		}
	};
}



#endif // !__EtherDB_ThreadLocal_H_



