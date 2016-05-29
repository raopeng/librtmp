#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <map>
#include <string>
#include <iostream>
#include "srs_librtmp.h"
#include "JobCenter.h"
#include "Poco/Thread.h"
#include "Poco/Logger.h"
#include "Poco/FileChannel.h"
#include "Poco/PatternFormatter.h"
#include "Poco/FormattingChannel.h"

#include "Poco/JSON/JSON.h"
#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Query.h"
#include "Poco/JSON/JSONException.h"
#include "Poco/JSON/Stringifier.h"
#include "Poco/JSON/ParseHandler.h"
#include "Poco/JSON/PrintHandler.h"
#include "Poco/JSON/Template.h"
#include "Poco/FileStream.h"

#include <assert.h>

using namespace std;
using namespace Poco;

class Task1 : public BaseTask{
public:
    Task1(JobCenter &jc)
    : m_logger(Poco::Logger::get("Task1")){
        Poco::JSON::Parser parser;
        Poco::FileInputStream fis("json.txt");
        Poco::Dynamic::Var result = parser.parse(fis);
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();
        Poco::DynamicStruct ds = *object;
        
        for(int i = 0; i < ds["publish"]["ip_port"].size(); i++)
        {
            std::string ip_port = ds["publish"]["ip_port"][i];
            m_public_ip_port.push_back(ip_port);
            Logger::root().information("publish.ip_port=%s", ip_port);
        }
        
        m_public_amount = ds["publish"]["amount"];
        m_public_duration = ds["publish"]["duration"];
        Logger::root().information("publish.amount=%d", m_public_amount);
        
        for(int i = 0; i < ds["play"]["ip_port"].size(); i++)
        {
            std::string ip_port = ds["play"]["ip_port"][i];
            m_play_ip_port.push_back(ip_port);
            Logger::root().information("play.ip_port=%s", ip_port);
        }
        
        m_prefix = "task1_";
        
        m_play_amount = ds["play"]["amount"];
        m_play_duration = ds["play"]["duration"];
        Logger::root().information("play.amount=%d", m_play_amount);
        
        int public_index = 0;
        int baseNameCount = 100000;
        while(public_index++ < m_public_amount){
            OneStreamSharePtr one = new OneStream(OneStream::PUSH, m_public_duration, m_public_ip_port[0], m_prefix, baseNameCount + public_index, this);
            jc.AddJob(one);
        }
        
        sleep(2);
        
        int play_index = 1;
        while(play_index++ <= m_play_amount){
            OneStreamSharePtr one = new OneStream(OneStream::PULL, m_play_duration, m_play_ip_port[0], m_prefix, baseNameCount + 1, this);
            jc.AddJob(one);
        }
    }
    virtual void onEvent(StreamEventSharePtr e){
        m_logger.information("event: time[%ld] id[%d] event [%s].", e->m_eventTime, e->m_oneStream->getID(), eventSring[e->m_eventType]);
    }
private:
    std::string              m_prefix;
    std::vector<std::string> m_public_ip_port;
    int m_public_amount;
    int m_public_duration;
    
    std::vector<std::string> m_play_ip_port;
    int m_play_amount;
    int m_play_duration;
    Poco::Logger&           m_logger;
};

int main(int argc, char *argv[])
{
	try
	{                
        Poco::FormattingChannel* Fchannel = new Poco::FormattingChannel(
                            new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S.%i [%q] : %t"),
                            new Poco::FileChannel("log.txt"));
        Fchannel->getChannel()->setProperty("rotation", "100 M");
        Fchannel->getFormatter()->setProperty("times", "local");        
        Poco::Logger::root().setChannel(Fchannel);        
        
        Logger::root().information("Bench program start.");
        
        JobCenter center;
        
        Task1 t1(center);
        
        while(1){
            usleep(10 * 1000);
        }
        
        Logger::root().information("Bench program stop.");
	}
	catch(Poco::JSON::JSONException& jsone)
	{
		printf("parse error : %s\n", jsone.message().c_str());
		return 1;
	}
    
    return 0;
}


