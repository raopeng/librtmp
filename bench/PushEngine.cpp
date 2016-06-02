#include "PushEngine.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

PushEngine::PushEngine(JobQueue<OneStreamSharePtr> &jobQueue, JobQueue<StreamEventSharePtr> &streamEventQueue)
    : m_jobQueue(jobQueue)
    , m_streamEventQueue(streamEventQueue)
    , m_thread("PushEngine")
    , m_timeTick(0)
    , m_logger(Poco::Logger::get("PushEngine")){       
    int ret = 0;    

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
    
    m_thread.start(*this);
}

PushEngine::~PushEngine(){
    m_thread.join();
}
    
void PushEngine::run(){
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
            OneStreamSharePtr node = m_jobQueue.get();
            if(node.get() != NULL){
                node->setExpiredTime(currentTime);
                node->m_sendIndex = 0;
                m_streamList.push_back(node);
            }
            
            std::string url = node->getURL();
            node->rtmp = srs_rtmp_create(url.c_str());
            m_logger.information("begin publish to [%s].", url);

            if (srs_rtmp_handshake(node->rtmp) != 0) {
                srs_rtmp_destroy(node->rtmp);       // 析构rtmp
                m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, HANDSHAKE_FAIL)));    // 发送握手失败事件
                m_logger.error("simple handshake to [%s] failed.", url);
            }
                
            if (srs_rtmp_connect_app(node->rtmp) != 0) {
                srs_rtmp_destroy(node->rtmp);       // 析构rtmp
                m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, CONNECT_FAIL)));      // 发送连接失败事件
                m_logger.error("connect [%s] vhost/app failed.", url);
                continue;
            }

            if (srs_rtmp_publish_stream(node->rtmp) != 0) {
                srs_rtmp_destroy(node->rtmp);       // 析构rtmp
                m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, PUBLIC_FAIL)));       // 发送public失败事件
                m_logger.error("publish stream [%s] failed.", url);
                continue;
            }

            m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent(node, currentTime, START_STREAM)));          // 发送开始推流事件
        }
        
        //  发送当前时间片的帧到所有链接
        for(std::list<OneStreamSharePtr>::iterator it = m_streamList.begin(); it != m_streamList.end(); ){
            bool endThisStream = false;
            if((*it)->m_sendIndex == 0 || (*it)->m_sendIndex + 1 > m_flvFrame.size()){
                (*it)->m_baseTimestamp = currentTime;
                (*it)->m_sendIndex = 0;
            }
            
            if(((*it)->m_baseTimestamp + m_flvFrame[(*it)->m_sendIndex].timestamp) <= currentTime){
                Frame f = m_flvFrame[((*it)->m_sendIndex)++];
                char* data = (char*)malloc(f.size);
                memcpy(data, f.data, f.size);
                if (srs_rtmp_write_packet((*it)->rtmp, f.type, ((*it)->m_baseTimestamp + f.timestamp) % 10000000, data, f.size) != 0) {
                    endThisStream = true;
                    m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent((*it), currentTime, PUSH_STREAM_FAIL)));       // 发送推流失败事件
                    m_logger.error("error to publish [%s].", (*it)->getURL());                    
                }
                (*it)->addReceiveSize(f.size);
            }
            
            // 推流时间到
            if(currentTime > (*it)->getExpiredTime()){
                endThisStream = true;
                m_streamEventQueue.put(StreamEventSharePtr(new StreamEvent((*it), currentTime, END_STREAM)));            // 发送推流结束事件
                m_logger.information("stop to publish [%s].", (*it)->getURL());
            }
            
            if(endThisStream){
                srs_rtmp_destroy((*it)->rtmp);      // 析构rtmp
                it = m_streamList.erase(it);        // 从链表中删除
                continue;
            }
            else{
                it++;
            }
        }

        usleep(10 * 1000);
    }
}
