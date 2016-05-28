#ifndef JOBQUEUE_H
#define JOBQUEUE_H

#include <list>
#include <string>
#include "srs_librtmp.h"
#include "Poco/Mutex.h"

class OneStream
{
public:
    OneStream();
    OneStream* clone(OneStream* stream);
    OneStream* reset();
    int         id;
    int         receiveSize;
    int64_t     survivalTime;
    int64_t     beginTime;
    int64_t     afterPlayTime;
    int64_t     lastDataTime;
    int64_t     endTime;
    srs_rtmp_t  rtmp;
    std::string ipPort;
    std::string prefixName;
    std::string localIPPort;
    int         randNum;
    int64_t     baseTimestamp;
    int         sendIndex;
    bool        isRedirect;
    static  int Count;
};

template <class T> class JobQueue
{
public:
    T get(void);
    
    void put(T node);

    int size(void);
private:
    std::list<T>            m_queue;
    Poco::FastMutex         m_mutex;
};

template <class T> T JobQueue<T>::get(void){
    T node = NULL;
    Poco::FastMutex::ScopedLock lock(m_mutex);
    if(!m_queue.empty()){
        node = m_queue.front();
        m_queue.pop_front();
    }        
    return node;
}
    
template <class T> void JobQueue<T>::put(T node){
    Poco::FastMutex::ScopedLock lock(m_mutex);
    m_queue.push_back(node);
}

template <class T> int JobQueue<T>::size(void){
    Poco::FastMutex::ScopedLock lock(m_mutex);
    return m_queue.size();
}

#endif
