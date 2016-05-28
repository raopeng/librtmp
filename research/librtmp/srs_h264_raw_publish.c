/*
The MIT License (MIT)

Copyright (c) 2013-2015 SRS(simple-rtmp-server)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/**
gcc srs_h264_raw_publish.c ../../objs/lib/srs_librtmp.a -g -O0 -lstdc++ -o srs_h264_raw_publish
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// for open h264 raw file.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
       
#include "../../objs/include/srs_librtmp.h"

#define PORT 8888
#define BUFFER_SIZE  10 * 1024
#define FRAME_HEADER_SIZE 9

int main(int argc, char** argv)
{
    printf("publish raw h.264 as rtmp stream to server like FMLE/FFMPEG/Encoder\n");
    printf("SRS(simple-rtmp-server) client librtmp library.\n");
    printf("version: %d.%d.%d\n", srs_version_major(), srs_version_minor(), srs_version_revision());
    
    if (argc <= 1) {
        printf("Usage: %s <h264_raw_file> <rtmp_publish_url>\n", argv[0]);
        printf("     h264_raw_file: the h264 raw steam file.\n");
        printf("     rtmp_publish_url: the rtmp publish url.\n");
        printf("For example:\n");
        printf("     %s ./720p.h264.raw rtmp://127.0.0.1:1935/live/livestream\n", argv[0]);
        printf("Where the file: http://winlinvip.github.io/srs.release/3rdparty/720p.h264.raw\n");
        printf("See: https://github.com/simple-rtmp-server/srs/issues/66\n");
        exit(-1);
    }
    
    const char* rtmp_url = argv[1];
    srs_human_trace("rtmp_url=%s", rtmp_url);
       
    struct sockaddr_in server;
    struct sockaddr_in dest;
    int socket_fd, client_fd,num;
    socklen_t size;

    char buffer[BUFFER_SIZE];
    memset(buffer,0,sizeof(buffer));
    int yes =1;

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))== -1) 
    {
        printf("Socket failure.\n");
        return 1;
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
    {
        printf("Setsockopt.\n");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    memset(&dest,0,sizeof(dest));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY; 
    if ((bind(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr )))== -1)    
    {
        printf("Binding Failure\n");
        return 1;
    }

    if ((listen(socket_fd, 10))== -1)
    {
        printf("Listening Failure.\n");
        return 1;
    }

    char frame[10 * BUFFER_SIZE];
    int frame_size = 0;
    int receive_size = 0;
    int is_connected = 0;
    int timestamp = 0;
    unsigned char frame_type;
    srs_rtmp_t rtmp;

    while(1) 
    {
        size = sizeof(struct sockaddr_in);  
        if ((client_fd = accept(socket_fd, (struct sockaddr *)&dest, &size)) == -1)
        {
            printf("Accept Failure.\n");
            return 1;
        }
        printf("Server got connection from client : %s\n" ,inet_ntoa(dest.sin_addr) );

        while(1)
        {
            if ((num = recv(client_fd, buffer, BUFFER_SIZE, 0))== -1) 
            {
                printf("Recv Failure.\n");
                is_connected = 0;
                close(client_fd);
                break;
            }   
            else if (num == 0) 
            {
                printf("Connection closed.\n");
                is_connected = 0;
                close(client_fd);
                break;
            }
            
            //c.send(buffer, num);

            if(!is_connected)
            {
                // connect rtmp context
                rtmp = srs_rtmp_create(rtmp_url);
                
                if (srs_rtmp_handshake(rtmp) != 0) {
                    srs_human_trace("simple handshake failed.");
                    goto rtmp_destroy;
                }
                srs_human_trace("simple handshake success");
                
                if (srs_rtmp_connect_app(rtmp) != 0) {
                    srs_human_trace("connect vhost/app failed.");
                    goto rtmp_destroy;
                }
                srs_human_trace("connect vhost/app success");
                
                if (srs_rtmp_publish_stream(rtmp) != 0) {
                    srs_human_trace("publish stream failed.");
                    goto rtmp_destroy;
                }
                srs_human_trace("publish stream success");
                is_connected = 1;
            }

            memcpy(frame + receive_size, buffer, num);
            receive_size += num;
            srs_human_trace("Message received: %d, total: %d" , num ,receive_size);       
            while(receive_size - FRAME_HEADER_SIZE >= frame_size)
            {
                if(frame_size == 0)
                {
                    char* p = frame;
                    frame_type = p[0];
                    p = frame + 1;
                    timestamp = ((unsigned char)p[3] | (unsigned char)p[2] << 8 | (unsigned char)p[1] << 16 | (unsigned char)p[0] << 24);
                    p = frame + 1 + 4;
                    frame_size = ((unsigned char)p[3] | (unsigned char)p[2] << 8 | (unsigned char)p[1] << 16 | (unsigned char)p[0] << 24);
                    srs_human_trace("frame type=%d timestamp= %d, size = %d",frame_type, timestamp, frame_size);
                }
                if(frame_size <= receive_size - FRAME_HEADER_SIZE)
                {
                    if(frame_type == 0x1) // audio
                    {
                        int ret = 0;
                        if(ret = srs_aac_write_raw_frame(rtmp, frame + FRAME_HEADER_SIZE, frame_size, timestamp) != 0)
                        {
                            srs_human_trace("send audio raw data failed. ret=%d", ret);
                        }
                    }
                    else if(frame_type == 0x2) // video
                    {                        
                        memcpy(frame + 1 + 4, "\x00\x00\x00\x01", 4);
                        int ret = srs_h264_write_raw_frames(rtmp, frame + 1 + 4, frame_size + 4, timestamp, timestamp);
                        if (ret != 0) {
                            if (srs_h264_is_dvbsp_error(ret)) {
                                srs_human_trace("ignore drop video error, code=%d", ret);
                            } else if (srs_h264_is_duplicated_sps_error(ret)) {
                                srs_human_trace("ignore duplicated sps, code=%d", ret);
                            } else if (srs_h264_is_duplicated_pps_error(ret)) {
                                srs_human_trace("ignore duplicated pps, code=%d", ret);
                            } else {
                                srs_human_trace("send h264 raw data failed. ret=%d", ret);
                                goto rtmp_destroy;
                            }
                        }
                    }
                    else
                    {
                        assert(0);
                    }

                    receive_size = receive_size - frame_size - FRAME_HEADER_SIZE;
                    assert(receive_size >= 0);
                    if(receive_size > 0)
                        memcpy(frame, frame + frame_size + FRAME_HEADER_SIZE, receive_size);
                    frame_size = 0;
                }
                else
                {
                    //printf("else 1... %d\n" , receive_size);
                }
            }
        }

        srs_human_trace("close socket fd=%d", client_fd);
        close(client_fd);
    }
    
rtmp_destroy:
    srs_rtmp_destroy(rtmp);
    close(socket_fd);
    
    return 0;
}

