#ifndef PULLENGINE_H
#define PULLENGINE_H

#include "JobQueue.h"
#include "Poco/Thread.h"
#include "Poco/Runnable.h"
#include "Poco/ThreadPool.h"
#include "Poco/RunnableAdapter.h"
#include "Poco/Mutex.h"
#include "Poco/Logger.h"

class PullEngine : public Poco::Runnable
{
private:
    std::string srs_get_local_ip_port(srs_rtmp_t rtmp);
    int addEpollIn(int nEpollFD, OneStreamSharePtr node);
    int delEpollIn(int nEpollFD, OneStreamSharePtr* node);
    void startup();
public:
    PullEngine(JobQueue<OneStreamSharePtr> &jobQueue, JobQueue<StreamEventSharePtr> &streamEventQueue);
    ~PullEngine();
    void run();

private:
    JobQueue<OneStreamSharePtr>     &m_jobQueue;
    JobQueue<StreamEventSharePtr>   &m_streamEventQueue;
    std::list<OneStreamSharePtr>    m_streamList;
    
    int                     m_epollFD;
    Poco::Thread            m_thread;
    Poco::Logger&           m_logger;
};

#endif
