#ifndef __EtherDBTHREAD_H__
#define __EtherDBTHREAD_H__
#include <pthread.h>

namespace EtherDB
{
    class EtherDBThread {
    public:
        EtherDBThread() :  m_Running(false),m_CPUID(0),m_tid(0){}

        virtual ~EtherDBThread() {
            stop();
        }
        
        void setCPUID (unsigned short cpuId) {
            m_CPUID = cpuId;
        }

        void start() {
            if (!m_Running) {
                m_Running = true;
                pthread_create(&m_tid, nullptr, &EtherDBThread::thread_func, this);

                // cpu schedule
                
            }
        }

        void stop() {
            if (m_Running) {
                m_Running = false;
                pthread_join(m_tid, nullptr);
            }
        }

    protected:
        virtual void* run() = 0;
        bool m_Running;
        unsigned short m_CPUID;
    private:
        pthread_t m_tid;
    
        static void* thread_func(void* arg) {
            EtherDBThread* self = static_cast<EtherDBThread*>(arg);
            return self->run();
        }
    };
}


#endif //__EtherDBTHREAD_H__