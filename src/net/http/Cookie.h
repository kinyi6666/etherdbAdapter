//////////////////////////////////////////////////////////////////////////////////
//�ļ���Cookie.h 
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Cookie
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Net_Cookie_H_
#define __EtherDB_Net_Cookie_H_

#include "../AmNetConfig.h"
#include <vector>

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API Cookie
		{
		public:
			Cookie();
			Cookie(std::string key, std::string value);

			void setDomain(std::string pattern);
			std::string getDomain();

			void setMaxAge(int expiry);
			int getMaxAge();

			std::string getName();
			void setValue(std::string newValue);
			std::string getValue();

			void setPath(std::string uri);
			std::string getPath();

			void setSecure(bool flag);

			void setComment(std::string purpose);
			std::string getComment();

			std::string toString();

		private:
			std::string _key;
			std::string _value;
			std::string _pattern;
			int _expiry;
			std::string _uri;
			bool _flag;
			std::string _purpose;
		};

		typedef std::vector<Cookie> Cookis;
	}
}


#endif // !__EtherDB_Net_Cookie_H_



