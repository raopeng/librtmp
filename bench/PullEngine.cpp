#include "PullEngine.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>

std::string PullEngine::srs_get_local_ip_port(srs_rtmp_t rtmp)
{
    int s = srs_get_fd(rtmp);
    struct sockaddr addr;
    socklen_t len = sizeof(addr);
    getsockname(s, (struct sockaddr*)&addr, &len);
    
    char ipstr[32];
    struct sockaddr_in *si = (struct sockaddr_in *)&addr;
    int port = ntohs(si->sin_port);
    inet_ntop(AF_INET, &si->sin_addr, ipstr, sizeof ipstr);
    
    sprintf(ipstr, "%s:%d", ipstr, port);
    
    return std::string(ipstr);
}

int PullEngine::addEpollIn(int nEpollFD, OneStreamSharePtr node){
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = new OneStreamSharePtr(node);
    int fd = srs_get_fd(node->rtmp);
    if (epoll_ctl(nEpollFD, EPOLL_CTL_ADD, fd, &ev) < 0) {
        m_logger.error("add epoll_ctl failed.");
        return -1;
    }
    return 0;
}

int PullEngine::delEpollIn(int nEpollFD, OneStreamSharePtr* node){
    struct epoll_event ev;
    int fd = srs_get_fd((*node)->rtmp);
    delete node;
    if (epoll_ctl(nEpollFD, EPOLL_CTL_DEL, fd, &ev) < 0) {
        m_logger.error("del epoll_ctl failed.");
        return -1;
    }
    return 0;
}

PullEngine::PullEngine(JobQueue<OneStreamSharePtr> &jobQueue, JobQueue<StreamEventSharePtr> &streamEventQueue)
    : m_jobQueue(jobQueue)
    , m_streamEventQueue(streamEventQueue)
    , m_thread("PullEngine")
    , m_logger(Poco::Logger::get("PullEngine")){       
    
    m_epollFD = epoll_create(65535);

    m_thread.start(*this);
}

PullEngine::~PullEngine(){
    m_thread.join();
}
    
void PullEngine::run(){
    while(1){
        int64_t currentTime = srs_utils_time_ms();
        int64_t epollTime = 0;
        
        struct epoll_event events[1024];
        int nFds = epoll_wait(m_epollFD, events, 1024, 0);      // 如果没有整件，立即返回
        for (int i = 0; i < nFds; i++){
            if (events[i].events & EPOLLIN){
                int size = 0;
                char type;
                char* data = NULL;
                u_int32_t timestamp;
                int  redirectPort = 0;
                char redirectIP[64];
                bool endThisStream = false;
                OneStreamSharePtr node = *((OneStreamSharePtr*)events[i].data.ptr);
                
                if(srs_rtmp_read_packet2(node->rtmp, &type, &timestamp, &data, &size, redirectIP, &redirectPort) != 0){
                    // 收流异常被动关闭
                    endThisStream = true;                   
                    m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, srs_utils_time_ms(), CLOSE_BY_PEER)));      // 发送被动关闭事件
                    m_logger.information("exception close stream : [%s]->[%s], size[%d]K", node->getLocalIPPort(), node->getURL(), node->getReceiveSize());
                }
                else{
                    if(size > 0 && data != NULL){
                        node->addReceiveSize(size);
                        free(data);
                    }
                
                    if(node->getExpiredTime() < currentTime){
                        // case 时间到主动关闭
                        endThisStream = true;
                        m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, srs_utils_time_ms(), END_STREAM)));            // 发送拉流结束事件
                        m_logger.information("close stream : [%s]->[%s], size[%d]K", node->getLocalIPPort(), node->getURL(), node->getReceiveSize());
                    }
                    else if(redirectPort != 0){
                    //    m_logger.information("redirect [%s]->[%s%d]%s->%s:%d. handshake--%ld->play--%ld->redirect=[%ld]ms",\
                                node->localIPPort, node->prefixName, node->randNum, node->ipPort, std::string(redirectIP),\
                                redirectPort, node->afterPlayTime - node->beginTime, \
                                currentTime - node->afterPlayTime, currentTime - node->beginTime);

                        node->setRemoteIPPort(redirectIP, redirectPort);
                        //node->isRedirect = true;
                        endThisStream = true;
                    }
                }
                
                if(endThisStream){
                    delEpollIn(m_epollFD, (OneStreamSharePtr*)events[i].data.ptr);
                    srs_rtmp_destroy(node->rtmp);
                }
            }
        }
        epollTime = srs_utils_time_ms() - currentTime;
        
        if(epollTime < 100)  // 如果在20毫秒内处理完一次epoll，说明收流比较空闲，可以处理队列了
        {
            OneStreamSharePtr node = m_jobQueue.get();
            if(node.get() != NULL){
                std::string url = node->getURL();
                node->setExpiredTime(currentTime);
            
                m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, srs_utils_time_ms(), START_TCP)));    // 发送开始TCP链接事件
                node->rtmp = srs_rtmp_create(url.c_str());
                if (srs_rtmp_handshake(node->rtmp) != 0) {
                    srs_rtmp_destroy(node->rtmp);
                    m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, HANDSHAKE_FAIL)));    // 发送握手失败事件
                    m_logger.error("simple handshake to [%s] failed.", url);
                    continue;
                }
            
                node->setLocalIPPort(srs_get_local_ip_port(node->rtmp));
                m_logger.information("begin connect to [%s]->[%s].", node->getLocalIPPort(), node->getURL());
            
                if (srs_rtmp_connect_app(node->rtmp) != 0) {
                    srs_rtmp_destroy(node->rtmp);
                    m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, CONNECT_FAIL)));      // 发送连接失败事件
                    m_logger.error("connect [%s] vhost/app failed.", url);
                    continue;
                }
                
                if (srs_rtmp_play_stream(node->rtmp) != 0) {
                    srs_rtmp_destroy(node->rtmp);
                    m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, PLAY_STREAM_FAIL)));   // 发送play失败事件
                    m_logger.error("play stream [%s] failed.", url);
                    continue;
                }
            
                if(addEpollIn(m_epollFD, node) != 0){
                    srs_rtmp_destroy(node->rtmp);
                    m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, EPOLL_FAIL)));          // epoll监控失败事件
                    continue;
                }
            
                m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, srs_utils_time_ms(), START_STREAM)));    // 发送开始推流事件
            }
            else{
                usleep(10 * 1000);
            }
        }
    }
}
