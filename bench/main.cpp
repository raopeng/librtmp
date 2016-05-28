#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <map>
#include <string>
#include <iostream>
#include "srs_librtmp.h"
#include "JobConsumer.h"
#include "JobProducer.h"
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

// TODO
// 3. 流名字不断步进变化

int main(int argc, char *argv[])
{
    JobQueue<OneStream*> exceptionJobQueue;
    //JobQueue<OneStream*> jobQueue;

    std::vector<std::string> public_ip_port;
    int public_amount, public_suffix_begin, public_suffix_end;
    
    std::vector<std::string> play_ip_port;
    int play_amount, play_suffix_begin, play_suffix_end;
	try
	{        
        // JSON configure 
        Poco::JSON::Parser parser;
        Poco::FileInputStream fis("json.txt");
        Poco::Dynamic::Var result = parser.parse(fis);
        Poco::JSON::Object::Ptr object = result.extract<Poco::JSON::Object::Ptr>();               
        Poco::DynamicStruct ds = *object;
        
        //
        Poco::FormattingChannel* Fchannel = new Poco::FormattingChannel(
                            new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S.%i [%q] : %t"),
                            new Poco::FileChannel("log.txt"));
        Fchannel->getChannel()->setProperty("rotation", "100 M");
        Fchannel->getFormatter()->setProperty("times", "local");
        
        Poco::Logger::root().setChannel(Fchannel);
        
        for(int i = 0; i < ds["publish"]["ip_port"].size(); i++)
        {
            std::string ip_port = ds["publish"]["ip_port"][i];
            public_ip_port.push_back(ip_port);
            Logger::root().information("publish.ip_port=%s", ip_port);
        }
        
        std::string public_prefix = ds["publish"]["prefix"];
        Logger::root().information("publish.prefix=%s", public_prefix);
        
        public_suffix_begin = ds["publish"]["suffix_begin"];
        Logger::root().information("publish.suffix_begin=%d", public_suffix_begin);
        
        public_suffix_end = ds["publish"]["suffix_end"];
        Logger::root().information("publish.suffix_end=%d", public_suffix_end);
        
        public_amount = ds["publish"]["amount"];
        Logger::root().information("publish.public_amount=%d", public_amount);
        
        for(int i = 0; i < ds["play"]["ip_port"].size(); i++)
        {
            std::string ip_port = ds["play"]["ip_port"][i];
            play_ip_port.push_back(ip_port);
            Logger::root().information("publish.ip_port=%s", ip_port);
        }
        
        std::string play_prefix = ds["play"]["prefix"];
        Logger::root().information("publish.prefix=%s", play_prefix);
        
        play_suffix_begin = ds["play"]["suffix_begin"];
        Logger::root().information("publish.suffix_begin=%d", play_suffix_begin);
        
        play_suffix_end = ds["play"]["suffix_end"];
        Logger::root().information("publish.suffix_end=%d", play_suffix_end);
        
        play_amount = ds["play"]["amount"];
        Logger::root().information("publish.amount=%d", play_amount);
        
        Logger::root().information("Bench program start.");
        
        PushJobConsumer pushConsumer(public_amount);
        PushJobProducer pushProducer(pushConsumer.m_pendingJobQueue , pushConsumer.m_jobQueue, \
                                     exceptionJobQueue, public_prefix , public_ip_port, \
                                     public_suffix_begin, public_suffix_end, public_amount);
        Poco::Thread t1("PushJobProducer");
        Poco::Thread t2("PushJobConsumer");
        t1.start(pushProducer);
        t2.start(pushConsumer);

        PullJobConsumer pullConsumer(play_amount, exceptionJobQueue);
        PullJobProducer pullProducer(pullConsumer.redirectJobQueue, pullConsumer.jobQueue, \
                                     play_prefix, play_ip_port, play_suffix_begin, play_suffix_end, play_amount);
        Poco::Thread t3("PullJobProducer");
        Poco::Thread t4("PullJobConsumer");    
        t3.start(pullProducer);
        t4.start(pullConsumer);
        
        t1.join();
        t2.join();
        t3.join();
        t4.join();
        
        Logger::root().information("Bench program stop.");
	}
	catch(Poco::JSON::JSONException& jsone)
	{
		printf("parse error : %s\n", jsone.message().c_str());
		return 1;
	}
    
    return 0;
}


