//////////////////////////////////////////////////////////////////////////////////
//�ļ���Ascii.h  
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Ascii
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Ascii_H_
#define __EtherDB_Ascii_H_

#include "EtherDBConfig.h"

namespace EtherDB
{
	class BASE_API Ascii
	{
	public:
		enum CharacterProperties
		{
			ACP_CONTROL = 0x0001,
			ACP_SPACE = 0x0002,
			ACP_PUNCT = 0x0004,
			ACP_DIGIT = 0x0008,
			ACP_HEXDIGIT = 0x0010,
			ACP_ALPHA = 0x0020,
			ACP_LOWER = 0x0040,
			ACP_UPPER = 0x0080,
			ACP_GRAPH = 0x0100,
			ACP_PRINT = 0x0200
		};

		static int properties(int ch);
		static bool hasSomeProperties(int ch, int properties);
		static bool hasProperties(int ch, int properties);
		static bool isAscii(int ch);
		static bool isSpace(int ch);
		static bool isDigit(int ch);
		static bool isHexDigit(int ch);
		static bool isPunct(int ch);
		static bool isAlpha(int ch);
		static bool isAlphaNumeric(int ch);
		static bool isLower(int ch);
		static bool isUpper(int ch);
		static int toLower(int ch);
		static int toUpper(int ch);

	private:
		static const int CHARACTER_PROPERTIES[128];
	};

}

#endif // !__EtherDB_Ascii_H_
