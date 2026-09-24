

#ifndef __EtherDB_Net_Buffer_H_
#define __EtherDB_Net_Buffer_H_

#include "AmNetConfig.h"
#include <base/ByteOrder.h>
#include <algorithm>
#include <vector>
#include <string.h>
#include <assert.h>

namespace EtherDB
{
	namespace Net
	{
		class AMNET_API Buffer
		{
		public:
			static const size_t kCheapPrepend;
			static const size_t kInitialSize;

			explicit  Buffer(size_t initialSize = kInitialSize)
				: _buffer(kCheapPrepend + initialSize),
				_readerIndex(kCheapPrepend),
				_writerIndex(kCheapPrepend)
			{
				assert(readableBytes() == 0);
				assert(writableBytes() == initialSize);
				assert(prependableBytes() == kCheapPrepend);
			}

			void swap(Buffer& rhs)
			{
				_buffer.swap(rhs._buffer);
				std::swap(_readerIndex, rhs._readerIndex);
				std::swap(_writerIndex, rhs._writerIndex);
			}

			size_t readableBytes() const
			{
				return _writerIndex - _readerIndex;
			}

			size_t writableBytes() const
			{
				return _buffer.size() - _writerIndex;
			}

			size_t prependableBytes() const
			{
				return _readerIndex;
			}

			const char* peek() const
			{
				return begin() + _readerIndex;
			}

			const char* findCRLF() const
			{
				const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
				return crlf == beginWrite() ? NULL : crlf;
			}

			const char* findCRLF(const char* start) const
			{
				assert(peek() <= start);
				assert(start <= beginWrite());
				const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
				return crlf == beginWrite() ? NULL : crlf;
			}

			const char* findEOL() const
			{
				const void* eol = memchr(peek(), '\n', readableBytes());
				return static_cast<const char*>(eol);
			}

			const char* findEOL(const char* start) const
			{
				assert(peek() <= start);
				assert(start <= beginWrite());
				const void* eol = memchr(start, '\n', beginWrite() - start);
				return static_cast<const char*>(eol);
			}

			void retrieve(size_t len)
			{
				assert(len <= readableBytes());
				if (len < readableBytes())
				{
					_readerIndex += len;
				}
				else
				{
					retrieveAll();
				}
			}

			void retrieveUntil(const char* end)
			{
				assert(peek() <= end);
				assert(end <= beginWrite());
				retrieve(end - peek());
			}

			void retrieveInt64()
			{
				retrieve(sizeof(int64_t));
			}

			void retrieveInt32()
			{
				retrieve(sizeof(int32_t));
			}

			void retrieveInt16()
			{
				retrieve(sizeof(int16_t));
			}

			void retrieveInt8()
			{
				retrieve(sizeof(int8_t));
			}

			void retrieveAll()
			{
				_readerIndex = kCheapPrepend;
				_writerIndex = kCheapPrepend;
			}

			std::string retrieveAllAsString()
			{
				return retrieveAsString(readableBytes());
			}

			std::string retrieveAsString(size_t len)
			{
				assert(len <= readableBytes());
				std::string result(peek(), len);
				retrieve(len);
				return result;
			}

			std::string toStringPiece() const
			{
				return std::string(peek(), static_cast<int>(readableBytes()));
			}

			void append(const std::string& str)
			{
				append(str.data(), str.size());
			}

			void append(const char* /*restrict*/ data, size_t len)
			{
				ensureWritableBytes(len);
				std::copy(data, data + len, beginWrite());
				hasWritten(len);
			}

			void append(const void* data, size_t len)
			{
				append(static_cast<const char*>(data), len);
			}

			void ensureWritableBytes(size_t len)
			{
				if (writableBytes() < len)
				{
					makeSpace(len);
				}
				assert(writableBytes() >= len);
			}

			char* beginWrite()
			{
				return begin() + _writerIndex;
			}

			const char* beginWrite() const
			{
				return begin() + _writerIndex;
			}

			void hasWritten(size_t len)
			{
				assert(len <= writableBytes());
				_writerIndex += len;
			}
			void unwrite(size_t len)
			{
				assert(len <= readableBytes());
				_writerIndex -= len;
			}

			void appendInt64(int64_t x)
			{
				int64_t be64 = ByteOrder::toNetwork(x);
				append(&be64, sizeof be64);
			}

			void appendInt32(int32_t x)
			{
				int32_t be32 = ByteOrder::toNetwork(x);
				append(&be32, sizeof be32);
			}

			void appendInt16(int16_t x)
			{
				int16_t be16 = ByteOrder::toNetwork(x);
				append(&be16, sizeof be16);
			}

			void appendInt8(int8_t x)
			{
				append(&x, sizeof x);
			}

			int64_t readInt64()
			{
				int64_t result = peekInt64();
				retrieveInt64();
				return result;
			}

			int32_t readInt32()
			{
				int32_t result = peekInt32();
				retrieveInt32();
				return result;
			}

			int16_t readInt16()
			{
				int16_t result = peekInt16();
				retrieveInt16();
				return result;
			}

			int8_t readInt8()
			{
				int8_t result = peekInt8();
				retrieveInt8();
				return result;
			}

			int64_t peekInt64() const
			{
				assert(readableBytes() >= sizeof(int64_t));
				int64_t be64 = 0;
				::memcpy(&be64, peek(), sizeof be64);
				return ByteOrder::fromNetwork(be64);
			}

			int32_t peekInt32() const
			{
				assert(readableBytes() >= sizeof(int32_t));
				int32_t be32 = 0;
				::memcpy(&be32, peek(), sizeof be32);
				return ByteOrder::fromNetwork(be32);
			}

			int16_t peekInt16() const
			{
				assert(readableBytes() >= sizeof(int16_t));
				int16_t be16 = 0;
				::memcpy(&be16, peek(), sizeof be16);
				return ByteOrder::fromNetwork(be16);
			}

			int8_t peekInt8() const
			{
				assert(readableBytes() >= sizeof(int8_t));
				int8_t x = *peek();
				return x;
			}

			void prependInt64(int64_t x)
			{
				int64_t be64 = ByteOrder::toNetwork(x);
				prepend(&be64, sizeof be64);
			}

			void prependInt32(int32_t x)
			{
				int32_t be32 = ByteOrder::toNetwork(x);
				prepend(&be32, sizeof be32);
			}

			void prependInt16(int16_t x)
			{
				int16_t be16 = ByteOrder::toNetwork(x);
				prepend(&be16, sizeof be16);
			}

			void prependInt8(int8_t x)
			{
				prepend(&x, sizeof x);
			}

			void prepend(const void* data, size_t len)
			{
				assert(len <= prependableBytes());
				_readerIndex -= len;
				const char* d = static_cast<const char*>(data);
				std::copy(d, d + len, begin() + _readerIndex);
			}

			void shrink(size_t reserve)
			{
				Buffer other;
				other.ensureWritableBytes(readableBytes() + reserve);
				other.append(toStringPiece());
				swap(other);
			}

			size_t internalCapacity() const
			{
				return _buffer.capacity();
			}
		private:
			char* begin()
			{
				return &*_buffer.begin();
			}

			const char* begin() const
			{
				return &*_buffer.begin();
			}

			void makeSpace(size_t len)
			{
				if (writableBytes() + prependableBytes() < len + kCheapPrepend)
				{
					size_t readable = readableBytes();
					std::vector<char> buffer(len + readable + kCheapPrepend);
					std::copy(begin() + _readerIndex,
						begin() + _writerIndex,
						&*buffer.begin() + kCheapPrepend);
					_readerIndex = kCheapPrepend;
					_writerIndex = _readerIndex + readable;
					std::swap(_buffer, buffer);
				}
				else
				{
					assert(kCheapPrepend < _readerIndex);
					size_t readable = readableBytes();
					std::copy(begin() + _readerIndex,
						begin() + _writerIndex,
						begin() + kCheapPrepend);
					_readerIndex = kCheapPrepend;
					_writerIndex = _readerIndex + readable;
					assert(readable == readableBytes());
				}
			}

		private:
			std::vector<char> _buffer;
			size_t _readerIndex;
			size_t _writerIndex;

			static const char kCRLF[];
		};
	}
}



#endif // !__EtherDB_Net_Buffer_H_




