#include "JobQueue.h"

int OneStream::Count = 1;
OneStream::OneStream(){
    id = Count++;
    reset();
}

OneStream* OneStream::clone(OneStream* stream)
{
    OneStream* newOne = new OneStream();
    newOne->receiveSize = stream->receiveSize;
    newOne->survivalTime = stream->survivalTime;
    newOne->beginTime = stream->beginTime;
    newOne->afterPlayTime = stream->afterPlayTime;
    newOne->lastDataTime = stream->lastDataTime;
    newOne->endTime = stream->endTime;
    newOne->rtmp = stream->rtmp;
    newOne->ipPort = stream->ipPort;
    newOne->prefixName = stream->prefixName;
    newOne->localIPPort = stream->localIPPort;
    newOne->randNum = stream->randNum;
    newOne->baseTimestamp = stream->baseTimestamp;
    newOne->sendIndex = stream->sendIndex;
    newOne->isRedirect = stream->isRedirect;
    return newOne;
}

OneStream* OneStream::reset(){
    receiveSize = 0;
    survivalTime = 0;
    beginTime = 0;
    afterPlayTime = 0;
    lastDataTime = 0;
    endTime = 0;
    rtmp = NULL;
    ipPort.clear();
    prefixName.clear();
    localIPPort.clear();
    randNum = 0;
    baseTimestamp = 0;
    sendIndex = 0;
    isRedirect = false;
    return this;
}
