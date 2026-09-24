//////////////////////////////////////////////////////////////////////////////////
//�ļ���ThreadLocal.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-07
//�������߳�
////////////////////////////////////////////////////////////////////////////////////

#include "ThreadLocal.h"

namespace EtherDB
{
	TLSAbstractSlot::TLSAbstractSlot()
	{

	}

	TLSAbstractSlot::~TLSAbstractSlot()
	{

	}


	ThreadLocalStorage::ThreadLocalStorage()
	{

	}

	ThreadLocalStorage::~ThreadLocalStorage()
	{
		for (TLSMap::iterator it = _map.begin(); it != _map.end(); ++it)
		{
			delete it->second;
		}
	}

	TLSAbstractSlot*& ThreadLocalStorage::get(const void* key)
	{
		TLSMap::iterator it = _map.find(key);
		if (it == _map.end())
			return _map.insert(TLSMap::value_type(key, reinterpret_cast<EtherDB::TLSAbstractSlot*>(0))).first->second;
		else
			return it->second;
	}

	ThreadLocalStorage& ThreadLocalStorage::current()
	{
		return *t_localStorage;
	}

	void ThreadLocalStorage::clear()
	{
		if (t_localStorage)
		{
			//TODO
		}
	}

}


