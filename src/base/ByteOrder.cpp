//////////////////////////////////////////////////////////////////////////////////
//�ļ���ByteOrder.cpp 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//�������ֽ�����
////////////////////////////////////////////////////////////////////////////////////

#include "ByteOrder.h"

namespace EtherDB
{
	int16_t ByteOrder::flipBytes(int16_t value)
	{
		return int16_t(flipBytes(uint16_t(value)));
	}


	uint16_t ByteOrder::flipBytes(uint16_t value)
	{
		return ((value >> 8) & 0x00FF) | ((value << 8) & 0xFF00);
	}


	int32_t ByteOrder::flipBytes(int32_t value)
	{
		return int32_t(flipBytes(uint32_t(value)));
	}


	uint32_t ByteOrder::flipBytes(uint32_t value)
	{
		return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00)
			| ((value << 8) & 0x00FF0000) | ((value << 24) & 0xFF000000);
	}


	int64_t ByteOrder::flipBytes(int64_t value)
	{
		return int64_t(flipBytes(uint64_t(value)));
	}


	uint64_t ByteOrder::flipBytes(uint64_t value)
	{
		uint32_t hi = uint32_t(value >> 32);
		uint32_t lo = uint32_t(value & 0xFFFFFFFF);
		return uint64_t(flipBytes(hi)) | (uint64_t(flipBytes(lo)) << 32);
	}




	AM_IMPLEMENT_BYTEORDER_BIG(toBigEndian)
		AM_IMPLEMENT_BYTEORDER_BIG(fromBigEndian)
		AM_IMPLEMENT_BYTEORDER_BIG(toNetwork)
		AM_IMPLEMENT_BYTEORDER_BIG(fromNetwork)
		AM_IMPLEMENT_BYTEORDER_LIT(toLittleEndian)
		AM_IMPLEMENT_BYTEORDER_LIT(fromLittleEndian)

}


