#include <stdio.h>
#include "../Thread.h"
#include <base/AtomicInt.h>
#include "base/LogFile.h"
#include "base/Logging.h"

void threadtest()
{
	printf("tid=%d\n", Am::Thread::currentTid());
	Am::Thread::sleep(5000);
}

std::shared_ptr<Am::LogFile> g_logFile;

void outputFunc(const char* msg, int len)
{
  g_logFile->append(msg, len);
}

void flushFunc()
{
  g_logFile->flush();
}

int main(int argc, char **argv)
{
	//Am::Thread th(threadtest);
	//th.start();
	//th.join();

 // g_logFile.reset(new Am::LogFile("basetest", 200*1000));
 // Am::Logger::setOutput(outputFunc);
 // Am::Logger::setFlush(flushFunc);

 // std::string line = "1234567890 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

 // for (int i = 0; i < 1000; ++i)
 // {
 //   LOG_INFO << line << i;

	////Am::Thread::sleep(1);
 // }

 // 

	Am::AtomicInt32 count;
	count.getAndSet(0x11223381);
	count.add(7);
	printf("%lx\n", count.get());

	return 0;
}