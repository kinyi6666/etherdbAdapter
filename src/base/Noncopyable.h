//////////////////////////////////////////////////////////////////////////////////
//�ļ���Noncopyable.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//���������ɿ�������
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Noncopyable_H_
#define __EtherDB_Noncopyable_H_

#include "EtherDBConfig.h"

namespace EtherDB
{
	class BASE_API Noncopyable
	{
	protected:
		Noncopyable() {}
		virtual ~Noncopyable() {}
	private:
		Noncopyable(const Noncopyable&);
		Noncopyable &operator=(const Noncopyable &);
	};
}


#endif // !__EtherDB_Noncopyable_H_