//////////////////////////////////////////////////////////////////////////////////
//�ļ���Exception.h 
//���ߣ�LSPZ
//ʱ�䣺2018-03-02
//�������쳣
///////////////////////////////////////////////////////////////////////////////////

#ifndef __EtherDB_Exception_H_
#define __EtherDB_Exception_H_

#include "EtherDBConfig.h"
#include <exception>

namespace EtherDB
{
	class BASE_API Exception : public std::exception
	{
	public:
		Exception();
		explicit Exception(const char* what);
		explicit Exception(const std::string& what);
		Exception(const Exception& exc);
		virtual ~Exception() throw();
		Exception& operator = (const Exception& exc);

		virtual const char* name() const throw();
		virtual const std::string message() const;
		virtual const char* what() const throw();
		virtual const char* stackTrace() const throw();

	private:
		void fillStackTrace();

		std::string _message;
		std::string _stack;
	};



#define AM_DECLARE_EXCEPTION(API, CLS, BASE)    \
		class API CLS : public BASE					 \
		{											 \
		public:										 \
			CLS();									 \
			explicit CLS(const char* what);          \
			explicit CLS(const std::string& what);   \
			CLS(const CLS& exc);                     \
			CLS& operator = (const CLS& exc);		 \
			virtual const char* name() const throw();\
		};

#define AM_IMPLEMENT_EXCEPTION(CLS, BASE, NAME)		 \
	CLS::CLS(){}									 \
	CLS::CLS(const char* what):BASE(what){}			 \
	CLS::CLS(const std::string& what):BASE(what){}   \
	CLS::CLS(const CLS& exc) : BASE(exc) {}			 \
	CLS& CLS::operator = (const CLS& exc)            \
	{												 \
		BASE::operator = (exc);						 \
		return *this;								 \
	}												 \
	const char* CLS::name() const throw(){return NAME;}

	AM_DECLARE_EXCEPTION(BASE_API, LogicException, Exception)
	AM_DECLARE_EXCEPTION(BASE_API, AssertionViolationException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, NullPointerException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, NullValueException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, BugcheckException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, InvalidArgumentException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, NotImplementedException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, RangeException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, IllegalStateException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, InvalidAccessException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, SignalException, LogicException)
	AM_DECLARE_EXCEPTION(BASE_API, UnhandledException, LogicException)

	AM_DECLARE_EXCEPTION(BASE_API, RuntimeException, Exception)
	AM_DECLARE_EXCEPTION(BASE_API, NotFoundException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, ExistsException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, TimeoutException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, SystemException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, RegularExpressionException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, LibraryLoadException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, LibraryAlreadyLoadedException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, NoThreadAvailableException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, PropertyNotSupportedException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, PoolOverflowException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, NoPermissionException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, OutOfMemoryException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, DataException, RuntimeException)

	AM_DECLARE_EXCEPTION(BASE_API, DataFormatException, DataException)
	AM_DECLARE_EXCEPTION(BASE_API, SyntaxException, DataException)
	AM_DECLARE_EXCEPTION(BASE_API, CircularReferenceException, DataException)
	AM_DECLARE_EXCEPTION(BASE_API, PathSyntaxException, SyntaxException)
	AM_DECLARE_EXCEPTION(BASE_API, IOException, RuntimeException)
	AM_DECLARE_EXCEPTION(BASE_API, ProtocolException, IOException)
	AM_DECLARE_EXCEPTION(BASE_API, FileException, IOException)
	AM_DECLARE_EXCEPTION(BASE_API, FileExistsException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, FileNotFoundException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, PathNotFoundException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, FileReadOnlyException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, FileAccessDeniedException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, CreateFileException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, OpenFileException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, WriteFileException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, ReadFileException, FileException)
	AM_DECLARE_EXCEPTION(BASE_API, UnknownURISchemeException, RuntimeException)

	AM_DECLARE_EXCEPTION(BASE_API, ApplicationException, Exception)
	AM_DECLARE_EXCEPTION(BASE_API, BadCastException, RuntimeException)
}



#endif // !__EtherDB_Exception_H_

