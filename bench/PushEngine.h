#ifndef PUSHENGINE_H
#define PUSHENGINE_H

#include "JobQueue.h"
#include "Poco/Stopwatch.h"
#include "Poco/Thread.h"
#include "Poco/Runnable.h"
#include "Poco/Mutex.h"
#include "Poco/Logger.h"

class PushEngine : public Poco::Runnable
{
private:
typedef struct Frame
{
    char type;
    int size;
    char* data;
    u_int32_t timestamp;
}Frame;
public:
    PushEngine(JobQueue<OneStreamSharePtr> &jobQueue, JobQueue<StreamEventSharePtr> &streamEventQueue);
    ~PushEngine();
    void run();

private:
    JobQueue<OneStreamSharePtr>     &m_jobQueue;
    JobQueue<StreamEventSharePtr>   &m_streamEventQueue;
    std::list<OneStreamSharePtr>    m_streamList;
    int64_t m_timeTick;
    
    Poco::Thread            m_thread;
    
    srs_flv_t m_flv;
    std::vector<Frame> m_flvFrame;
    Poco::Logger&    m_logger;
};

#endif
