cmake_minimum_required(VERSION 2.6)
PROJECT(demo)

INCLUDE_DIRECTORIES($ENV{PWD}/../../objs/include)
LINK_DIRECTORIES($ENV{PWD}/../../objs/lib)
INCLUDE_DIRECTORIES(/home/zhiguangq/workspace/librtp/sample/rtpCapture/3rd-party/include)
LINK_DIRECTORIES(/home/zhiguangq/workspace/librtp/sample/rtpCapture/3rd-party/lib)

ADD_EXECUTABLE(demo JobQueue.cpp JobProducer.cpp JobConsumer.cpp main.cpp)
TARGET_LINK_LIBRARIES(demo rt)
TARGET_LINK_LIBRARIES(demo pthread)
TARGET_LINK_LIBRARIES(demo PocoJSONd.a)
TARGET_LINK_LIBRARIES(demo PocoFoundationd.a)
TARGET_LINK_LIBRARIES(demo rtmp.a)
