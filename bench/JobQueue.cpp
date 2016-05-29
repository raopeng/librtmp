#include "JobQueue.h"

int OneStream::m_count = 1;
OneStream::OneStream(STREAM_TYPE type, int duration, std::string ipPort, std::string prefixName, int suffixName, BaseTask* task)
    : rtmp(NULL)
    , m_sendIndex(0)
    , m_baseTimestamp(0)
    , m_task(task)
    , m_id(m_count++)
    , m_receiveSize(0)
    , m_type(type)
    , m_duration(duration)
    , m_remoteIPPort(ipPort)
    , m_prefixName(prefixName)
    , m_suffixName(suffixName)
    , m_expiredTime(0)
{
}

OneStream::~OneStream(){
}

int OneStream::getID(void){
    return m_id;
}

OneStream::STREAM_TYPE OneStream::getType(void){
    return m_type;
}

void OneStream::addReceiveSize(int size){
    m_receiveSize += size;
}

int OneStream::getReceiveSize(void){
    return m_receiveSize;
}

int64_t OneStream::getExpiredTime(void){
    return m_expiredTime;
}

void OneStream::setExpiredTime(int64_t currentTime){
    m_expiredTime = currentTime + m_duration * 1000;
}

void OneStream::setRemoteIPPort(char* ip, int port){
    char ipPortBuf[64];
    sprintf(ipPortBuf,"%s:%d", ip, port);
    m_remoteIPPort = std::string(ipPortBuf);
}

void OneStream::setLocalIPPort(std::string ipPort){
    m_localIPPort = ipPort;
}
std::string OneStream::getLocalIPPort(void){
    return m_localIPPort;
}

std::string OneStream::getURL(void){
    char url[512];
    sprintf(url,"rtmp://%s/live/%s%d", m_remoteIPPort.c_str(), m_prefixName.c_str(), m_suffixName);
    return std::string(url);
}

