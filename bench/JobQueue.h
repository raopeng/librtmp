#ifndef JOBQUEUE_H
#define JOBQUEUE_H

#include <list>
#include <string>
#include <iostream>
#include "srs_librtmp.h"
#include "Poco/SharedPtr.h"
#include "Poco/Mutex.h"

class BaseTask;
class OneStream
{
public:
    enum STREAM_TYPE{PUSH = 0, PULL};
    
    OneStream(STREAM_TYPE type, int duration, std::string ipPort, std::string prefixName, int suffixName, BaseTask* task);  
    ~OneStream();
    
    int getID(void);
    STREAM_TYPE getType(void);
    void addReceiveSize(int size);
    int getReceiveSize(void);
    int64_t getExpiredTime(void);
    void setExpiredTime(int64_t currentTime);
    void setRemoteIPPort(char* ip, int port);
    void setLocalIPPort(std::string ipPort);
    std::string getLocalIPPort(void);
    std::string getURL(void);
    
    BaseTask    *m_task;            // 这个流的任务对象指针，为了回调时指定对象
private:
    int         m_id;               // 这个流的唯一ID号
    int         m_receiveSize;      // 产生流量
    STREAM_TYPE m_type;             // 推流、或者拉流
    int         m_duration;         // 持续时间
    std::string m_remoteIPPort;     // 服务器IP和端口
    std::string m_localIPPort;      // 本地使用的IP和端口
    std::string m_prefixName;       // 流名字前缀
    int         m_suffixName;       // 流名字后缀

    int64_t     m_expiredTime;      // 流失效时间
    //bool        isRedirect;
private:
    friend class PushEngine;
    friend class PullEngine;
    srs_rtmp_t  rtmp;
    int         m_sendIndex;        // 推流用到
    int64_t     m_baseTimestamp;    // 推流用到
    static  int m_count;            // 类的静态变量，指定流ID号唯一
};

typedef Poco::SharedPtr<OneStream>  OneStreamSharePtr;

enum EVENT_TYPE{HANDSHAKE_FAIL = 0, CONNECT_FAIL, PUBLIC_FAIL, PUSH_STREAM_FAIL, PLAY_STREAM_FAIL, EPOLL_FAIL, CLOSE_BY_PEER, START_TCP, START_STREAM, END_STREAM};
const std::string eventSring[] = {"handshake_fail","connect_fail", "public_fail", "push_stream_fail", "play_stream_fail","epoll_fail", "close_by_peer", "start_tcp", "start_stream", "end_stream"};
class StreamEvent{
public:
    StreamEvent(OneStreamSharePtr id, int64_t time, EVENT_TYPE type){m_oneStream = id; m_eventTime = time; m_eventType = type; /*std::cout << "StreamEvent ctor." << std::endl;*/}
    ~StreamEvent(){/*std::cout << "StreamEvent dtor." << std::endl;*/}
    OneStreamSharePtr   m_oneStream;
    int64_t             m_eventTime;
    EVENT_TYPE          m_eventType;
};

typedef Poco::SharedPtr<StreamEvent>  StreamEventSharePtr;

class BaseTask{
public:
    virtual void onEvent(StreamEventSharePtr e) = 0;
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
