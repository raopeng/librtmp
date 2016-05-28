#ifndef JOBPRODUCER_H
#define JOBPRODUCER_H

#include "JobQueue.h"
#include "Poco/Runnable.h"
#include "Poco/Logger.h"
#include "Poco/ThreadPool.h"
#include "Poco/RunnableAdapter.h"

class PullJobProducer : public Poco::Runnable
{
public:
    PullJobProducer(JobQueue<OneStream*>& redirect, JobQueue<OneStream*>& out, std::string prefix, \
                    std::vector<std::string> &ipportVector, int begin, int end, int totalJob);
    void run();
    void startup();
private:
    JobQueue<OneStream*> &m_redirectJobQueue;
    JobQueue<OneStream*> &m_outJobQueue;
    std::string m_prefix;
    std::vector<std::string> &m_ipPortVector;
    int m_suffixBegin;
    int m_suffixEnd;
    int m_totalJob;
    
    Poco::ThreadPool m_theadPool;
    Poco::Logger&    m_logger;
};

class PushJobProducer : public Poco::Runnable
{
private:
   typedef struct Duration{
       int64_t begin;
       int64_t end;
       Duration(int64_t b, int64_t e){begin = b; end = e;}
   }Duration;
   typedef struct Job{
       bool isRuning;
       std::list<Duration> m_durationList;
       Job(){isRuning = false;}
   }Job;
public:
    PushJobProducer(JobQueue<OneStream*>& pending, JobQueue<OneStream*>& out, JobQueue<OneStream*>& exceptionJob, \
                        std::string prefix, std::vector<std::string> &ipportVector, \
                        int begin, int end, int totalJob);
    void run();
private:
    JobQueue<OneStream*> &m_pendingJobQueue;
    JobQueue<OneStream*> &m_outJobQueue;
    JobQueue<OneStream*> &m_exceptionJobQueue;
    std::string m_prefix;
    std::vector<std::string> &m_ipPortVector;
    std::map<int, Job*> m_jobMap;
    int m_suffixBegin;
    int m_suffixEnd;
    int m_totalJob;
    int m_currentJob;
    Poco::Logger&    m_logger;
};

#endif
