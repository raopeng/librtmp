#include "JobProducer.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "Poco/DateTime.h"
#include "Poco/DateTimeFormatter.h"

std::string srs_get_local_ip_port(srs_rtmp_t rtmp)
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

PullJobProducer::PullJobProducer(JobQueue<OneStream*>& redirect, JobQueue<OneStream*>& out, std::string prefix, \
                    std::vector<std::string> &ipportVector, int begin, int end, int totalJob)
    : m_redirectJobQueue(redirect)
    , m_outJobQueue(out)
    , m_prefix(prefix)
    , m_ipPortVector(ipportVector)
    , m_suffixBegin(begin)
    , m_suffixEnd(end)
    , m_totalJob(totalJob)
    , m_theadPool(20, 20, 20)
    , m_logger(Poco::Logger::get("PullJobProducer")){
}

void PullJobProducer::run(){
    for(int i = 0; i < m_totalJob; i++)
    {
        OneStream *node = new OneStream;
        m_redirectJobQueue.put(node);
    }
    
    Poco::RunnableAdapter<PullJobProducer> ra(*this, &PullJobProducer::startup);
    for(int i = 0; i < 20; i++)
    {
        m_theadPool.start(ra);
    }
    m_theadPool.joinAll();
}
void PullJobProducer::startup(){
    while(1){
        OneStream *node = m_redirectJobQueue.get();
        if(node == NULL){
            usleep(100 * 1000);
            continue;
        }
        
        char fileName[512];
        if(node->isRedirect != true){
            node->prefixName = m_prefix;
            node->ipPort = m_ipPortVector[rand() % m_ipPortVector.size()];
            node->randNum = rand() % (m_suffixEnd - m_suffixBegin + 1) + m_suffixBegin;
        }
        node->survivalTime = ((rand() % (30 - 20))+ 20);
        
        sprintf(fileName,"rtmp://%s/live/%s%d", node->ipPort.c_str(), node->prefixName.c_str(), node->randNum);
        node->rtmp = srs_rtmp_create(fileName);
        
        node->beginTime = srs_utils_time_ms();
        if (srs_rtmp_handshake(node->rtmp) != 0) {
            m_logger.error("simple handshake to [%s] failed.", std::string(fileName));
            srs_rtmp_destroy(node->rtmp);
            m_redirectJobQueue.put(node->reset());
            continue;
        }
        
        node->localIPPort = srs_get_local_ip_port(node->rtmp);
        m_logger.information("begin connect to [%s]->[rtmp://%s/live/%s%d].", node->localIPPort, \
                            node->ipPort, node->prefixName, node->randNum);
        
        if (srs_rtmp_connect_app(node->rtmp) != 0) {
            m_logger.error("connect [%s] vhost/app failed.", std::string(fileName));
            srs_rtmp_destroy(node->rtmp);
            m_redirectJobQueue.put(node->reset());
            continue;
        }
            
        if (srs_rtmp_play_stream(node->rtmp) != 0) {
            m_logger.error("play stream [%s] failed.", std::string(fileName));
            srs_rtmp_destroy(node->rtmp);
            m_redirectJobQueue.put(node->reset());
            continue;
        }
        node->afterPlayTime = srs_utils_time_ms();
        m_outJobQueue.put(node);
    }
}

PushJobProducer::PushJobProducer(JobQueue<OneStream*>& pending, JobQueue<OneStream*>& out, \
                JobQueue<OneStream*>& exceptionJob, std::string prefix, \
                std::vector<std::string> &ipportVector, int begin, int end, int totalJob)
        : m_pendingJobQueue(pending)
        , m_outJobQueue(out)
        , m_exceptionJobQueue(exceptionJob)
        , m_prefix(prefix)
        , m_ipPortVector(ipportVector)
        , m_suffixBegin(begin)
        , m_suffixEnd(end)
        , m_totalJob(totalJob)
        , m_currentJob(0)
        , m_logger(Poco::Logger::get("PushJobProducer")){
}
void PushJobProducer::run(){
    // all Job init
    for(int i = m_suffixBegin; i <= m_suffixEnd; i++)
    {
        m_jobMap[i] = new Job;
    }
    
    for(int i = 1; i <= m_totalJob; i++){
        OneStream *node = new OneStream;
        if(node != NULL){
            node->randNum = -1;
            m_pendingJobQueue.put(node);
        }
    }
    
    while(1){
        OneStream *node = NULL;
        if(m_pendingJobQueue.size() > 0){
            node = m_pendingJobQueue.get();
            //
            if(node->randNum != -1 && m_jobMap[node->randNum]->isRuning){
                m_jobMap[node->randNum]->isRuning = false;
                m_jobMap[node->randNum]->m_durationList.pop_front();
                m_jobMap[node->randNum]->m_durationList.push_front(Duration(node->beginTime, node->endTime));
                if(m_jobMap[node->randNum]->m_durationList.size() > 5){
                    m_jobMap[node->randNum]->m_durationList.pop_back();
                }
                
                /*
                for(std::list<Duration>::iterator it = m_jobMap[node->randNum]->m_durationList.begin();
                        it != m_jobMap[node->randNum]->m_durationList.end(); it++)
                {
                    std::cout << it->begin << " : " << it->end << std::endl;
                }
                std::cout << "----------->" << node->randNum << std::endl;
                */
            }
            
            int randN = 0;
            node->reset();
            do{
                randN = rand() % (m_suffixEnd - m_suffixBegin + 1) + m_suffixBegin;
            }while(m_jobMap[randN]->isRuning);
            node->randNum = randN;
            node->survivalTime = (rand() % (30 - 20))+ 20;
            node->ipPort = m_ipPortVector[rand() % m_ipPortVector.size()];
            node->prefixName = m_prefix;
        }
        else{
            // 判断异常断开的拉流，是否是正常的
            while(m_exceptionJobQueue.size() > 0){
                OneStream* exceptionNode = m_exceptionJobQueue.get();
                if(m_jobMap.find(exceptionNode->randNum) != m_jobMap.end()){
                    for(std::list<Duration>::iterator it = m_jobMap[exceptionNode->randNum]->m_durationList.begin();
                        it != m_jobMap[exceptionNode->randNum]->m_durationList.end(); it++){
                        
                        if(it->end >= exceptionNode->beginTime &&
                            exceptionNode->endTime >= it->begin &&
                            exceptionNode->endTime <= it->end &&  // 拉流结束时间不能大于推流结束时间
                            exceptionNode->beginTime >= it->begin // 拉流开始时间不能少于推流开始时间
                            ){

                            m_logger.error("error close stream :[%s]->[rtmp://%s/live/%s%d] size[%d]K dur[%ld]s push[%s-%s] pull[%s-%s]", \
                            exceptionNode->localIPPort, exceptionNode->ipPort, exceptionNode->prefixName, exceptionNode->randNum, \
                            exceptionNode->receiveSize / 1000, \
                            (exceptionNode->endTime - exceptionNode->beginTime) / 1000, \
                            Poco::DateTimeFormatter::format(Poco::DateTime(Poco::Timestamp(it->begin * 1000 + 28800000000)), "%Y-%m-%d %H:%M:%S.%i"), \
                            Poco::DateTimeFormatter::format(Poco::DateTime(Poco::Timestamp(it->end * 1000 + 28800000000)), "%Y-%m-%d %H:%M:%S.%i"), \
                            Poco::DateTimeFormatter::format(Poco::DateTime(Poco::Timestamp(exceptionNode->beginTime * 1000 + 28800000000)), "%Y-%m-%d %H:%M:%S.%i"), \
                            Poco::DateTimeFormatter::format(Poco::DateTime(Poco::Timestamp(exceptionNode->endTime * 1000 + 28800000000)), "%Y-%m-%d %H:%M:%S.%i"));
                            break;
                        }
                    }
                }
                
                delete exceptionNode;
            }
            
            usleep(100 * 1000);
            continue;
        }
            
        char fileName[512];        
        sprintf(fileName,"rtmp://%s/live/%s%d", node->ipPort.c_str(), node->prefixName.c_str(), node->randNum);
        node->rtmp = srs_rtmp_create(fileName);
        m_logger.information("begin publish to [%s].", std::string(fileName));

        if (srs_rtmp_handshake(node->rtmp) != 0) {
            m_logger.error("simple handshake to [%s] failed.", std::string(fileName));
            srs_rtmp_destroy(node->rtmp);
            m_pendingJobQueue.put(node);
            continue;
        }
            
        if (srs_rtmp_connect_app(node->rtmp) != 0) {
            m_logger.error("connect [%s] vhost/app failed.", std::string(fileName));
            srs_rtmp_destroy(node->rtmp);
            m_pendingJobQueue.put(node);
            continue;
        }

        if (srs_rtmp_publish_stream(node->rtmp) != 0) {
            m_logger.error("publish stream [%s] failed.", std::string(fileName));
            srs_rtmp_destroy(node->rtmp);
            m_pendingJobQueue.put(node);
            continue;
        }

        node->beginTime = srs_utils_time_ms();
        m_jobMap[node->randNum]->isRuning = true;
        m_jobMap[node->randNum]->m_durationList.push_front(Duration(node->beginTime, 0x7fffffffffffffff));
        m_outJobQueue.put(node);
    }
}

