//////////////////////////////////////////////////////////////////////////////////
//���GException.cpp 
//�@�̡GLSPZ
//??�G2018-03-02
//�y�z�G�ݱ`
///////////////////////////////////////////////////////////////////////////////////

#include "Exception.h"
#include <stdio.h>
#include <typeinfo>

#ifndef INWINDOWS
#include <execinfo.h>
#else
#include "AmWindows.h"
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

namespace EtherDB
{
	Exception::Exception()
	{

	}

	Exception::Exception(const char* what)
		:_message(what)
	{
		fillStackTrace();
	}

	Exception::Exception(const std::string& what)
		: _message(what)
	{
		fillStackTrace();
	}

	Exception::Exception(const Exception& exc)
		: _message(exc._message),
		_stack(exc._stack)
	{

	}

	Exception::~Exception() throw()
	{

	}

	Exception& Exception::operator = (const Exception& exc)
	{
		_message = exc._message;
		_stack = exc._stack;
		return *this;
	}

	const char* Exception::name() const throw()
	{
		return "Exception";
	}

	const std::string Exception::message() const
	{
		return std::string(typeid(*this).name()) + ":" + _message;
	}

	const char* Exception::what() const throw()
	{
		return _message.c_str();
	}

	const char* Exception::stackTrace() const throw()
	{
		return _stack.c_str();
	}

	void Exception::fillStackTrace()
	{
#ifdef INWINDOWS
		void* stack[32];
		USHORT frames = CaptureStackBackTrace(0, 32, stack, NULL);

		char buf[512];
		_stack = "=== Stack Trace ===\n";

		for (USHORT i = 0; i < frames; ++i)
		{
			// ??���L�a�}�A�ݭn??��??�Υ~���u��ѪR
			snprintf(buf, sizeof(buf), "#%-2d 0x%p\n", i, stack[i]);
			_stack.append(buf);
		}

		_stack.append("(Use addr2line or WinDbg to resolve symbols)\n");
#else
		_stack = "Stack trace not supported\n";

#endif // INWINDOWS



	}

	AM_IMPLEMENT_EXCEPTION(LogicException, Exception, "Logic exception")
	AM_IMPLEMENT_EXCEPTION(AssertionViolationException, LogicException, "Assertion violation")
	AM_IMPLEMENT_EXCEPTION(NullPointerException, LogicException, "Null pointer")
	AM_IMPLEMENT_EXCEPTION(NullValueException, LogicException, "Null value")
	AM_IMPLEMENT_EXCEPTION(BugcheckException, LogicException, "Bugcheck")
	AM_IMPLEMENT_EXCEPTION(InvalidArgumentException, LogicException, "Invalid argument")
	AM_IMPLEMENT_EXCEPTION(NotImplementedException, LogicException, "Not implemented")
	AM_IMPLEMENT_EXCEPTION(RangeException, LogicException, "Out of range")
	AM_IMPLEMENT_EXCEPTION(IllegalStateException, LogicException, "Illegal state")
	AM_IMPLEMENT_EXCEPTION(InvalidAccessException, LogicException, "Invalid access")
	AM_IMPLEMENT_EXCEPTION(SignalException, LogicException, "Signal received")
	AM_IMPLEMENT_EXCEPTION(UnhandledException, LogicException, "Unhandled exception")

	AM_IMPLEMENT_EXCEPTION(RuntimeException, Exception, "Runtime exception")
	AM_IMPLEMENT_EXCEPTION(NotFoundException, RuntimeException, "Not found")
	AM_IMPLEMENT_EXCEPTION(ExistsException, RuntimeException, "Exists")
	AM_IMPLEMENT_EXCEPTION(TimeoutException, RuntimeException, "Timeout")
	AM_IMPLEMENT_EXCEPTION(SystemException, RuntimeException, "System exception")
	AM_IMPLEMENT_EXCEPTION(RegularExpressionException, RuntimeException, "Error in regular expression")
	AM_IMPLEMENT_EXCEPTION(LibraryLoadException, RuntimeException, "Cannot load library")
	AM_IMPLEMENT_EXCEPTION(LibraryAlreadyLoadedException, RuntimeException, "Library already loaded")
	AM_IMPLEMENT_EXCEPTION(NoThreadAvailableException, RuntimeException, "No thread available")
	AM_IMPLEMENT_EXCEPTION(PropertyNotSupportedException, RuntimeException, "Property not supported")
	AM_IMPLEMENT_EXCEPTION(PoolOverflowException, RuntimeException, "Pool overflow")
	AM_IMPLEMENT_EXCEPTION(NoPermissionException, RuntimeException, "No permission")
	AM_IMPLEMENT_EXCEPTION(OutOfMemoryException, RuntimeException, "Out of memory")
	AM_IMPLEMENT_EXCEPTION(DataException, RuntimeException, "Data error")

	AM_IMPLEMENT_EXCEPTION(DataFormatException, DataException, "Bad data format")
	AM_IMPLEMENT_EXCEPTION(SyntaxException, DataException, "Syntax error")
	AM_IMPLEMENT_EXCEPTION(CircularReferenceException, DataException, "Circular reference")
	AM_IMPLEMENT_EXCEPTION(PathSyntaxException, SyntaxException, "Bad path syntax")
	AM_IMPLEMENT_EXCEPTION(IOException, RuntimeException, "I/O error")
	AM_IMPLEMENT_EXCEPTION(ProtocolException, IOException, "Protocol error")
	AM_IMPLEMENT_EXCEPTION(FileException, IOException, "File access error")
	AM_IMPLEMENT_EXCEPTION(FileExistsException, FileException, "File exists")
	AM_IMPLEMENT_EXCEPTION(FileNotFoundException, FileException, "File not found")
	AM_IMPLEMENT_EXCEPTION(PathNotFoundException, FileException, "Path not found")
	AM_IMPLEMENT_EXCEPTION(FileReadOnlyException, FileException, "File is read-only")
	AM_IMPLEMENT_EXCEPTION(FileAccessDeniedException, FileException, "Access to file denied")
	AM_IMPLEMENT_EXCEPTION(CreateFileException, FileException, "Cannot create file")
	AM_IMPLEMENT_EXCEPTION(OpenFileException, FileException, "Cannot open file")
	AM_IMPLEMENT_EXCEPTION(WriteFileException, FileException, "Cannot write file")
	AM_IMPLEMENT_EXCEPTION(ReadFileException, FileException, "Cannot read file")
	AM_IMPLEMENT_EXCEPTION(UnknownURISchemeException, RuntimeException, "Unknown URI scheme")


	AM_IMPLEMENT_EXCEPTION(ApplicationException, Exception, "Application exception")
	AM_IMPLEMENT_EXCEPTION(BadCastException, RuntimeException, "Bad cast exception")

}

