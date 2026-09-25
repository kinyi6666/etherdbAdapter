//////////////////////////////////////////////////////////////////////////////////
//�ļ���Cookie.cpp
//���ߣ�LSPZ
//ʱ�䣺2018-01-02
//������Cookie
////////////////////////////////////////////////////////////////////////////////////

#include "Cookie.h"

namespace EtherDB
{
	namespace Net
	{
		Cookie::Cookie()
			:_expiry(0),
			_flag(false)
		{

		}

		Cookie::Cookie(std::string key, std::string value)
			:_key(key),
			_value(value)
		{

		}

		void Cookie::setDomain(std::string pattern)
		{

		}

		std::string Cookie::getDomain()
		{
			return "";
		}

		void Cookie::setMaxAge(int expiry)
		{

		}

		int Cookie::getMaxAge()
		{
			return 0;
		}

		std::string Cookie::getName()
		{
			return _key;
		}

		void Cookie::setValue(std::string newValue)
		{
			_value = newValue;
		}

		std::string Cookie::getValue()
		{
			return _value;
		}

		void Cookie::setPath(std::string uri)
		{

		}

		std::string Cookie::getPath()
		{
			return "";
		}

		void Cookie::setSecure(bool flag)
		{

		}

		void Cookie::setComment(std::string purpose)
		{

		}

		std::string Cookie::getComment()
		{
			return "";
		}

		std::string Cookie::toString()
		{
			std::string ret = "";
			do
			{
				if (_key.empty() || _value.empty())
					break;

				ret += _key + "=" + _value +";";


			} while (0);
			

			return ret;
		}
	}
}



