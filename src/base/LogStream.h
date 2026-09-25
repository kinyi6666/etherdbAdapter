////////////////////////////////////////////////////////////////////////////////
//�ļ���LogStream.h  
//���ߣ�LSPZ
//ʱ�䣺2018-01-03
//������Ԫ����
////////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_LogStream_H_
#define __EtherDB_LogStream_H_

#include "EtherDBConfig.h"
#include "Noncopyable.h"
#include <assert.h>
#include <string>
#include <string.h>

namespace EtherDB
{
	namespace detail
	{
		const int kSmallBuffer = 4000;
		const int kLargeBuffer = 4000 * 1000;

		template<int SIZE>
		class BASE_API FixedBuffer : Noncopyable
		{
		public:
			FixedBuffer()
				:_cur(_data)
			{
				setCookie(cookieStart);
			}

			~FixedBuffer()
			{
				setCookie(cookieEnd);
			}

			void append(const char* buf, size_t len)
			{
				if (static_cast<size_t>(avail()) > len)
				{
					memcpy(_cur, buf, len);
					_cur += len;
				}
			}

			const char* data() const { return _data; }
			int length() const { return static_cast<int>(_cur - _data); }
			char* current() { return _cur; }
			int avail() const { return static_cast<int>(end() - _cur); }
			void add(size_t len) { _cur += len; }

			void reset() { _cur = _data; }
			void bzero() { ::memset(_data, 0, sizeof _data); }

			const char* debugString() {
				*_cur = '\0';
				return _data;
			}
			void setCookie(void(*cookie)()) { _cookie = cookie; }
			std::string toString() const { return std::string(_data, length()); }

		private:
			const char* end() const { return _data + sizeof _data; }

			static void cookieStart() {}
			static void cookieEnd() {}

		private:
			void(*_cookie)();
			char _data[SIZE];
			char* _cur;
		};
	}

	class BASE_API LogStream
		:Noncopyable
	{
		typedef LogStream self;
	public:
		typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

		self& operator<<(bool v)
		{
			_buffer.append(v ? "1" : "0", 1);
			return *this;
		}

		self& operator<<(short);
		self& operator<<(unsigned short);
		self& operator<<(int);
		self& operator<<(unsigned int);
		self& operator<<(long);
		self& operator<<(unsigned long);
		self& operator<<(long long);
		self& operator<<(unsigned long long);

		self& operator<<(const void*);

		self& operator<<(float v)
		{
			*this << static_cast<double>(v);
			return *this;
		}
		self& operator<<(double);

		self& operator<<(char v)
		{
			_buffer.append(&v, 1);
			return *this;
		}

		self& operator<<(const char* str)
		{
			if (str)
			{
				_buffer.append(str, strlen(str));
			}
			else
			{
				_buffer.append("(null)", 6);
			}
			return *this;
		}

		self& operator<<(const unsigned char* str)
		{
			return operator<<(reinterpret_cast<const char*>(str));
		}

		self& operator<<(const std::string& v)
		{
			_buffer.append(v.c_str(), v.size());
			return *this;
		}

		self& operator<<(const Buffer& v)
		{
			*this << v.toString();
			return *this;
		}

		void append(const char* data, int len) { _buffer.append(data, len); }
		const Buffer& buffer() const { return _buffer; }
		void resetBuffer() { _buffer.reset(); }

	private:
		void staticCheck();

		template<typename T>
		void formatInteger(T);

	private:
		Buffer _buffer;

		static const int kMaxNumericSize = 32;
	};

	class Fmt // : boost::noncopyable
	{
	public:
		template<typename T>
		Fmt(const char* fmt, T val)
		{
			assert(std::is_arithmetic<T>::value == true);

			length_ = snprintf(buf_, sizeof buf_, fmt, val);
			assert(static_cast<size_t>(length_) < sizeof buf_);
		}

		const char* data() const { return buf_; }
		int length() const { return length_; }

	private:
		char buf_[32];
		int length_;
	};

	inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
	{
		s.append(fmt.data(), fmt.length());
		return s;
	}
}
#endif // !__EtherDB_LogStream_H_


