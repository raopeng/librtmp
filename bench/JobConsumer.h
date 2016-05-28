#ifndef JOBCONSUMER_H
#define JOBCONSUMER_H

#include "JobQueue.h"
#include "Poco/Stopwatch.h"
#include "Poco/Runnable.h"
#include "Poco/Mutex.h"
#include "Poco/Logger.h"

class PullJobConsumer : public Poco::Runnable
{
private:
    int addEpollIn(int nEpollFD, OneStream *node);
    int delEpollIn(int nEpollFD, OneStream *node);

public:
    PullJobConsumer(int total, JobQueue<OneStream*>& exceptionJob);

    void run();

    JobQueue<OneStream*> redirectJobQueue;
    JobQueue<OneStream*> jobQueue;
private:
    Poco::Clock myclock;
    std::map<int, OneStream*> m_streamMap;
    std::map<int, std::string> m_streamPullIPMap;
    
    int nEpollFD;
    int64_t tickTime;
    int totalNumber;
    JobQueue<OneStream*>& m_exceptionJobQueue;
    Poco::Logger&    m_logger;
};

class PushJobConsumer : public Poco::Runnable
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
    PushJobConsumer(int total);
    void run();
    
    JobQueue<OneStream*> m_pendingJobQueue;
    JobQueue<OneStream*> m_jobQueue;
private:
    std::list<OneStream*> m_streamList;
    int  m_totalNumber;
    int64_t m_timeTick;
    
    srs_flv_t m_flv;
    std::vector<Frame> m_flvFrame;
    Poco::Logger&    m_logger;
};

#endif
