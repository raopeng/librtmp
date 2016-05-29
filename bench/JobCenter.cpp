#include "JobCenter.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "Poco/DateTime.h"
#include "Poco/DateTimeFormatter.h"


JobCenter::JobCenter()
    : m_pushEngine(m_pushJobQueue, m_pushEventQueue)
    , m_pullEngine(m_pullJobQueue, m_pullEventQueue)
    , m_logger(Poco::Logger::get("JobCenter")){
    m_thread.start(*this);
}

JobCenter::~JobCenter(){    
    m_thread.join();
}

bool JobCenter::AddJob(OneStreamSharePtr one){
    if(one.get() != NULL){        
        if(one->getType() == OneStream::PUSH){ // 添加推流任务
            m_pushJobQueue.put(one);
        }
        else{                       // 添加拉流任务
            m_pullJobQueue.put(one);
        }
        return true;
    }
    return false;
}

void JobCenter::run(){
    while(1){
        while(m_pushEventQueue.size() > 0){        // 推流事件
            StreamEventSharePtr e = m_pushEventQueue.get();
            if(e->m_oneStream->m_task){
                e->m_oneStream->m_task->onEvent(e);
            }
            //m_logger.information("push event: time[%ld] id[%d] event [%s].", e->m_eventTime, e->m_oneStream->id, eventSring[e->m_eventType]);
        }
        
        while(m_pullEventQueue.size() > 0){        // 拉事件
            StreamEventSharePtr e = m_pullEventQueue.get();
            if(e->m_oneStream->m_task){
                e->m_oneStream->m_task->onEvent(e);
            }
            //m_logger.information("pull event: time[%ld] id[%d] event [%s].", e->m_eventTime, e->m_oneStream->id, eventSring[e->m_eventType]);
        }
        
        usleep(100 * 1000);
    }
}

