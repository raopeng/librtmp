#include "JobConsumer.h"
#include <sys/epoll.h>
#include <iostream>

int PullJobConsumer::addEpollIn(int nEpollFD, OneStream *node){
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = node;
    int fd = srs_get_fd(node->rtmp);
    if (epoll_ctl(nEpollFD, EPOLL_CTL_ADD, fd, &ev) < 0) {
        m_logger.error("add epoll_ctl failed.");
        return -1;
    }
    return 0;
}

int PullJobConsumer::delEpollIn(int nEpollFD, OneStream *node){
    struct epoll_event ev;
    int fd = srs_get_fd(node->rtmp);
    if (epoll_ctl(nEpollFD, EPOLL_CTL_DEL, fd, &ev) < 0) {
        m_logger.error("del epoll_ctl failed.");
        return -1;
    }
    return 0;
}

PullJobConsumer::PullJobConsumer(int total, JobQueue<OneStream*>& exceptionJob)
    : nEpollFD(-1)
    , tickTime(0)
    , totalNumber(total)
    , m_exceptionJobQueue(exceptionJob)
    , m_logger(Poco::Logger::get("PullJobConsumer")){        
        nEpollFD = epoll_create(total);
}
void PullJobConsumer::run(){
    while(1)
    {
        int64_t currentTime = srs_utils_time_ms();
        if(currentTime - tickTime > 1000){
            int mapSize = m_streamMap.size();
            m_logger.information("current size[%d] taskQueue size[%d] jobQueue size[%d]", \
                                    mapSize, redirectJobQueue.size(), jobQueue.size());
            tickTime = currentTime;
            
#if 0
            for(std::map<int, OneStream*>::iterator it = m_streamMap.begin(); it != m_streamMap.end(); it++){
                OneStream *node = it->second;
                m_logger.information("==> : [%s]->[rtmp://%s/live/%s%d] size[%d]K recv time[%ld]s", \
                             node->localIPPort, node->ipPort,node->prefixName, node->randNum, node->receiveSize / 1000,\
                             (node->lastDataTime / 1000)% 1000);
                node->receiveSize = 0;
            }
#endif
            m_streamPullIPMap.clear();
            for( std::map<int, OneStream*>::iterator it = m_streamMap.begin(); it != m_streamMap.end(); ){
                OneStream *node = it->second;
                
                // 把流ID和服务器IP检查一遍，持续拉了3秒才检查
                if(((currentTime - node->beginTime) / 1000) > 3){
                    std::map<int, std::string>::iterator findIt = m_streamPullIPMap.find(node->randNum);
                    if(findIt == m_streamPullIPMap.end()){
                        m_streamPullIPMap[node->randNum] = node->ipPort;
                    }
                    else{
                        // 看是否是同一个IP地址
                        if(node->ipPort.compare(m_streamPullIPMap[node->randNum]) != 0){
                            m_logger.error("error dup pull stream [rtmp://%s/live/%s%d]. before is pull from[%s]", \
                                node->ipPort, node->prefixName, node->randNum, m_streamPullIPMap[node->randNum]);
                        }
                    }   
                }
                
                if(currentTime - node->lastDataTime > 3000){
                    // 收流3秒超时关闭
                    node->endTime = currentTime;//node->lastDataTime;
                    m_exceptionJobQueue.put(node->clone(node));
                    
                    m_logger.information("time out close stream : [%s]->[rtmp://%s/live/%s%d] size[%d]K dur[%ld]s", \
                             node->localIPPort, node->ipPort,node->prefixName, node->randNum, node->receiveSize / 1000,\
                             (node->endTime - node->beginTime) / 1000);
                    
                    delEpollIn(nEpollFD, node);
                    srs_rtmp_destroy(node->rtmp);
                    redirectJobQueue.put(node->reset());                    
                    m_streamMap.erase(it++);
                }
                else{
                    it++;
                }
            }
        }
        
        while(jobQueue.size() > 0)
        {
            OneStream *node = jobQueue.get();
            if(node != NULL){
                node->isRedirect = false;
                if(addEpollIn(nEpollFD, node) != 0){
                    srs_rtmp_destroy(node->rtmp);
                    redirectJobQueue.put(node->reset());
                }
                else{
                    node->survivalTime = node->survivalTime * 1000 + currentTime;
                    node->lastDataTime = currentTime;
                    m_streamMap[node->id] = node;
                }
            }
        }
            
        struct epoll_event events[totalNumber];
        int nFds;
        nFds = epoll_wait(nEpollFD, events, totalNumber, 100);
        for (int i = 0; i < nFds; i++){
            if (events[i].events & EPOLLIN){
                int size = 0;
                char type;
                char* data = NULL;
                u_int32_t timestamp;
                int  redirectPort = 0;
                char redirectIP[64];
                bool endThisStream = false;
                OneStream *node = (OneStream*)events[i].data.ptr;
                
                if(srs_rtmp_read_packet2(node->rtmp, &type, &timestamp, &data, &size, redirectIP, &redirectPort) != 0){
                    endThisStream = true;
                    node->endTime = currentTime;
                    m_exceptionJobQueue.put(node->clone(node));
                    // 收流异常被动关闭
                    m_logger.information("exception close stream : [%s]->[rtmp://%s/live/%s%d] size[%d]K dur[%ld]s", \
                             node->localIPPort, node->ipPort,node->prefixName, node->randNum, node->receiveSize / 1000,\
                             (node->endTime - node->beginTime) / 1000);
                }
                else{
                    if(size > 0 && data != NULL){
                        node->lastDataTime = currentTime;
                        node->receiveSize += size;
                        free(data);
                    }
                
                    if(node->survivalTime < currentTime){
                        // case 时间到主动关闭
                        endThisStream = true;
                        
                        m_logger.information("close stream : [%s]->[rtmp://%s/live/%s%d] size[%d]K dur[%ld]s",\
                                node->localIPPort, node->ipPort , node->prefixName, \
                                node->randNum, node->receiveSize / 1000, (currentTime - node->beginTime) / 1000);
                    }
                    else if(redirectPort != 0){
                        m_logger.information("redirect [%s]->[%s%d]%s->%s:%d. handshake--%ld->play--%ld->redirect=[%ld]ms",\
                                node->localIPPort, node->prefixName, node->randNum, node->ipPort, std::string(redirectIP),\
                                redirectPort, node->afterPlayTime - node->beginTime, \
                                currentTime - node->afterPlayTime, currentTime - node->beginTime);

                        char ipPortBuf[64];
                        sprintf(ipPortBuf,"%s:%d", redirectIP, redirectPort);
                        node->ipPort = ipPortBuf;
                        node->isRedirect = true;
                        endThisStream = true;
                    }
                }
                
                if(endThisStream){
                    delEpollIn(nEpollFD, node);
                    srs_rtmp_destroy(node->rtmp);
                    redirectJobQueue.put(node->isRedirect ? node : node->reset());
                    m_streamMap.erase(node->id);
                }
            }
        }
    }
}

PushJobConsumer::PushJobConsumer(int total)
    : m_totalNumber(total)
    , m_timeTick(0)
    , m_logger(Poco::Logger::get("PushJobConsumer")){       
    int ret = 0;    
        
    //if ((m_flv = srs_flv_open_read("/home/zhiguangq/movie/nba.flv")) == NULL) {
    if ((m_flv = srs_flv_open_read("nba.flv")) == NULL) {
        m_logger.error("open flv file failed.");
        m_flv = NULL;
    }
    
    char header[13];
    if ((ret = srs_flv_read_header(m_flv, header)) != 0) {
        m_logger.error("read header error.");
    }
    
    for (;;) {
        // tag header
        Frame f;
        if ((ret = srs_flv_read_tag_header(m_flv, &f.type, &f.size, &f.timestamp)) != 0) {
            if (srs_flv_is_eof(ret)) {
                m_logger.information("parse completed.");
                break;
            }
            m_logger.error("flv get packet failed. ret=%d");
            break;
        }
        if (f.size <= 0) {
            m_logger.error("invalid size.");
            break;
        }
        
        f.data = (char*)malloc(f.size);
        if ((ret = srs_flv_read_tag_data(m_flv, f.data, f.size)) != 0) {
            m_logger.error("read data error.");
            break;
        }
        m_flvFrame.push_back(f);
    }
    m_logger.information("frame size = %d", (int)m_flvFrame.size());
}
    
void PushJobConsumer::run(){
    while(1){
        int64_t currentTime = srs_utils_time_ms();
        
        if(currentTime - m_timeTick > 1000){
            m_timeTick = currentTime;
#if 0
            for(std::list<OneStream*>::iterator it = m_streamList.begin(); it != m_streamList.end(); it++){
                m_logger.information("==> current publish [rtmp://%s/live/%s%d] size[%d]", (*it)->ipPort, \
                            (*it)->prefixName, (*it)->randNum, (*it)->receiveSize/1000);
                (*it)->receiveSize = 0;
            }
#endif
        }
        
        if(m_jobQueue.size() > 0){
            OneStream *node = m_jobQueue.get();
            if(node != NULL){
                node->survivalTime = (node->survivalTime * 1000) + currentTime;
                node->beginTime = currentTime;
                node->sendIndex = 0;
                m_streamList.push_back(node);
            }
        }
            
        for(std::list<OneStream*>::iterator it = m_streamList.begin(); it != m_streamList.end(); ){
            bool endThisStream = false;
            if((*it)->sendIndex == 0 || (*it)->sendIndex + 1 > m_flvFrame.size())
            {
                (*it)->baseTimestamp = currentTime;
                (*it)->sendIndex = 0;
            }
            
            if(((*it)->baseTimestamp + m_flvFrame[(*it)->sendIndex].timestamp) <= currentTime)
            {
                Frame f = m_flvFrame[((*it)->sendIndex)++];
                char* data = (char*)malloc(f.size);
                memcpy(data, f.data, f.size);
                if (srs_rtmp_write_packet((*it)->rtmp, f.type, ((*it)->baseTimestamp + f.timestamp) % 10000000, data, f.size) != 0) {
                    endThisStream = true;
                    m_logger.error("error to publish [rtmp://%s/live/%s%d].", \
                          (*it)->ipPort, (*it)->prefixName, (*it)->randNum);
                    
                }
                (*it)->receiveSize += f.size;
            }
            
            // 推流时间到
            if(currentTime > (*it)->survivalTime)
            {
                m_logger.information("stop to publish [rtmp://%s/live/%s%d].", \
                          (*it)->ipPort, (*it)->prefixName, (*it)->randNum);
                endThisStream = true;
            }
            
            if(endThisStream){
                (*it)->endTime = currentTime;
                srs_rtmp_destroy((*it)->rtmp);
                m_pendingJobQueue.put(*it);
                it = m_streamList.erase(it);
                continue;
            }
            else{
                it++;
            }
        }

        usleep(10 * 1000);
    }
}
