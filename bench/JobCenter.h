#ifndef JOBCENTER_H
#define JOBCENTER_H

#include "JobQueue.h"
#include "PushEngine.h"
#include "PullEngine.h"
#include "Poco/Runnable.h"
#include "Poco/Logger.h"
#include "Poco/ThreadPool.h"
#include "Poco/RunnableAdapter.h"

class JobCenter : public Poco::Runnable
{
public:
    JobCenter();
    ~JobCenter();
    
    bool AddJob(OneStreamSharePtr one);
    
    void run();
private:
    JobQueue<OneStreamSharePtr>    m_pushJobQueue;          // 推流任务队列
    JobQueue<OneStreamSharePtr>    m_pullJobQueue;          // 拉流任务队列
    JobQueue<StreamEventSharePtr>   m_pushEventQueue;       // 推流事件队列
    JobQueue<StreamEventSharePtr>   m_pullEventQueue;       // 拉流事件队列
    
    PushEngine              m_pushEngine;                   // 推流引擎
    PullEngine              m_pullEngine;                   // 拉流引擎
    
    Poco::Thread            m_thread;                       // 任务处理中心线程
    Poco::Logger&           m_logger;
};

#endif
