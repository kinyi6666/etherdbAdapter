//////////////////////////////////////////////////////////////////////////////////
//�ļ���ByteOrder.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//�������ֽ�����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_ByteOrder_H_
#define __EtherDB_ByteOrder_H_

#include "EtherDBConfig.h"
#include <stdint.h>
#include <stdlib.h>


namespace EtherDB
{
	class BASE_API ByteOrder
	{
	public:
		//�ֽ��� ��ת
		static int16_t flipBytes(int16_t value);
		static uint16_t flipBytes(uint16_t value);
		static int32_t flipBytes(int32_t value);
		static uint32_t flipBytes(uint32_t value);
		static int64_t flipBytes(int64_t value);
		static uint64_t flipBytes(uint64_t value);

		//����--�����
		static int16_t toBigEndian(int16_t value);
		static uint16_t toBigEndian(uint16_t value);
		static int32_t toBigEndian(int32_t value);
		static uint32_t toBigEndian(uint32_t value);
		static int64_t toBigEndian(int64_t value);
		static uint64_t toBigEndian(uint64_t value);

		//���--������
		static int16_t fromBigEndian(int16_t value);
		static uint16_t fromBigEndian(uint16_t value);
		static int32_t fromBigEndian(int32_t value);
		static uint32_t fromBigEndian(uint32_t value);
		static int64_t fromBigEndian(int64_t value);
		static uint64_t fromBigEndian(uint64_t value);

		//����-��С��
		static int16_t toLittleEndian(int16_t value);
		static uint16_t toLittleEndian(uint16_t value);
		static int32_t toLittleEndian(int32_t value);
		static uint32_t toLittleEndian(uint32_t value);
		static int64_t toLittleEndian(int64_t value);
		static uint64_t toLittleEndian(uint64_t value);

		//С��-������
		static int16_t fromLittleEndian(int16_t value);
		static uint16_t fromLittleEndian(uint16_t value);
		static int32_t fromLittleEndian(int32_t value);
		static uint32_t fromLittleEndian(uint32_t value);
		static int64_t fromLittleEndian(int64_t value);
		static uint64_t fromLittleEndian(uint64_t value);

		//����-�������ֽ���
		static int16_t toNetwork(int16_t value);
		static uint16_t toNetwork(uint16_t value);
		static int32_t toNetwork(int32_t value);
		static uint32_t toNetwork(uint32_t value);
		static int64_t toNetwork(int64_t value);
		static uint64_t toNetwork(uint64_t value);

		//�����ֽ���-������
		static int16_t fromNetwork(int16_t value);
		static uint16_t fromNetwork(uint16_t value);
		static int32_t fromNetwork(int32_t value);
		static uint32_t fromNetwork(uint32_t value);
		static int64_t fromNetwork(int64_t value);
		static uint64_t fromNetwork(uint64_t value);

	};


#define AM_IMPLEMENT_BYTEORDER_NOOP_(op, type) \
	type ByteOrder::op(type value)		\
	{											\
		return value;							\
	}
#define AM_IMPLEMENT_BYTEORDER_FLIP_(op, type) \
	type ByteOrder::op(type value)		\
	{											\
		return flipBytes(value);				\
	}


#define AM_IMPLEMENT_BYTEORDER_NOOP(op) \
		AM_IMPLEMENT_BYTEORDER_NOOP_(op, int16_t)	\
		AM_IMPLEMENT_BYTEORDER_NOOP_(op, uint16_t)	\
		AM_IMPLEMENT_BYTEORDER_NOOP_(op, int32_t)	\
		AM_IMPLEMENT_BYTEORDER_NOOP_(op, uint32_t)	\
		AM_IMPLEMENT_BYTEORDER_NOOP_(op, int64_t)	\
		AM_IMPLEMENT_BYTEORDER_NOOP_(op, uint64_t)
#define AM_IMPLEMENT_BYTEORDER_FLIP(op) \
		AM_IMPLEMENT_BYTEORDER_FLIP_(op, int16_t)	\
		AM_IMPLEMENT_BYTEORDER_FLIP_(op, uint16_t)	\
		AM_IMPLEMENT_BYTEORDER_FLIP_(op, int32_t)	\
		AM_IMPLEMENT_BYTEORDER_FLIP_(op, uint32_t)	\
		AM_IMPLEMENT_BYTEORDER_FLIP_(op, int64_t)	\
		AM_IMPLEMENT_BYTEORDER_FLIP_(op, uint64_t)

#define AM_IMPLEMENT_BYTEORDER_BIG AM_IMPLEMENT_BYTEORDER_FLIP
#define AM_IMPLEMENT_BYTEORDER_LIT AM_IMPLEMENT_BYTEORDER_NOOP
}




#endif // !__Pz_ByteOrder_H_



