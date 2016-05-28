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
gcc srs_ingest_flv.c ../../objs/lib/srs_librtmp.a -g -O0 -lstdc++ -o srs_ingest_flv
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../../objs/include/srs_librtmp.h"

//typedef int Item;

typedef struct mediaFrame
{
    u_int32_t timestamp;
    char* data;
    int32_t size;
}Item;

typedef struct node * PNode;
typedef struct node
{
	Item data;
	PNode next;
}Node;

typedef struct
{
	PNode front;
	PNode rear;
	int size;
}Queue;

/*构造一个空队列*/
Queue *InitQueue();

/*销毁一个队列*/
void DestroyQueue(Queue *pqueue);

/*清空一个队列*/
void ClearQueue(Queue *pqueue);

/*判断队列是否为空*/
int IsEmpty(Queue *pqueue);

/*返回队列大小*/
int GetSize(Queue *pqueue);

/*返回队头元素*/
PNode GetFront(Queue *pqueue,Item *pitem);

/*返回队尾元素*/
PNode GetRear(Queue *pqueue,Item *pitem);

/*将新元素入队*/
PNode EnQueue(Queue *pqueue,Item item);

/*队头元素出队*/
PNode DeQueue(Queue *pqueue,Item *pitem);

/*遍历队列并对各数据项调用visit函数*/
void QueueTraverse(Queue *pqueue,void (*visit)());

/*构造一个空队列*/
Queue *InitQueue()
{
	Queue *pqueue = (Queue *)malloc(sizeof(Queue));
	if(pqueue!=NULL)
	{
		pqueue->front = NULL;
		pqueue->rear = NULL;
		pqueue->size = 0;
	}
	return pqueue;
}

/*销毁一个队列*/
void DestroyQueue(Queue *pqueue)
{
	if(IsEmpty(pqueue)!=1)
		ClearQueue(pqueue);
	free(pqueue);
}

/*清空一个队列*/
void ClearQueue(Queue *pqueue)
{
	while(IsEmpty(pqueue)!=1)
	{
		DeQueue(pqueue,NULL);
	}

}

/*判断队列是否为空*/
int IsEmpty(Queue *pqueue)
{
	if(pqueue->front==NULL&&pqueue->rear==NULL&&pqueue->size==0)
		return 1;
	else
		return 0;
}

/*返回队列大小*/
int GetSize(Queue *pqueue)
{
	return pqueue->size;
}

/*返回队头元素*/
PNode GetFront(Queue *pqueue,Item *pitem)
{
	if(IsEmpty(pqueue)!=1&&pitem!=NULL)
	{
		*pitem = pqueue->front->data;
	}
	return pqueue->front;
}

/*返回队尾元素*/

PNode GetRear(Queue *pqueue,Item *pitem)
{
	if(IsEmpty(pqueue)!=1&&pitem!=NULL)
	{
		*pitem = pqueue->rear->data;
	}
	return pqueue->rear;
}

/*将新元素入队*/
PNode EnQueue(Queue *pqueue,Item item)
{
	PNode pnode = (PNode)malloc(sizeof(Node));
	if(pnode != NULL)
	{
		pnode->data = item;
		pnode->next = NULL;
		
		if(IsEmpty(pqueue))
		{
			pqueue->front = pnode;
		}
		else
		{
			pqueue->rear->next = pnode;
		}
		pqueue->rear = pnode;
		pqueue->size++;
	}
	return pnode;
}

/*队头元素出队*/
PNode DeQueue(Queue *pqueue,Item *pitem)
{
	PNode pnode = pqueue->front;
	if(IsEmpty(pqueue)!=1&&pnode!=NULL)
	{
		if(pitem!=NULL)
			*pitem = pnode->data;
		pqueue->size--;
		pqueue->front = pnode->next;
		free(pnode);
		if(pqueue->size==0)
			pqueue->rear = NULL;
	}
	return pqueue->front;
}

/*遍历队列并对各数据项调用visit函数*/
void QueueTraverse(Queue *pqueue,void (*visit)())
{
	PNode pnode = pqueue->front;
	int i = pqueue->size;
	while(i--)
	{
		visit(pnode->data);
		pnode = pnode->next;
	}
		
}



int parse_flv(srs_flv_t flv, srs_flv_t out_flv);
int main(int argc, char** argv)
{
    int ret = 0;
    
    // user options.
    char* in_flv_file;
    char* out_flv_file;
    // flv handler
    srs_flv_t flv;
    srs_flv_t out_flv;
    
    printf("parse and show flv file detail.\n");
    printf("srs(simple-rtmp-server) client librtmp library.\n");
    printf("version: %d.%d.%d\n", srs_version_major(), srs_version_minor(), srs_version_revision());
    
    if (argc <= 2) {
        printf("inject flv file keyframes to metadata\n"
            "Usage: %s in_flv_file out_flv_file\n"
            "   in_flv_file         input flv file to inject.\n"
            "   out_flv_file        the inject output file, can be in_flv_file.\n"
            "For example:\n"
            "   %s doc/source.200kbps.768x320.flv injected.flv\n"
            "   %s ../../doc/source.200kbps.768x320.flv injected.flv\n",
            argv[0], argv[0], argv[0]);
        exit(-1);
    }
    
    in_flv_file = argv[1];
    out_flv_file = argv[2];
    srs_human_trace("input:  %s", in_flv_file);
    srs_human_trace("output:  %s", out_flv_file);

    if ((flv = srs_flv_open_read(in_flv_file)) == NULL) {
        ret = 2;
        srs_human_trace("open flv file failed. ret=%d", ret);
        return ret;
    }
    
    if ((out_flv = srs_flv_open_write(out_flv_file)) == NULL) {
        ret = 2;
        srs_human_trace("open flv file failed. ret=%d", ret);
        return ret;
    }    
    
    
    ret = parse_flv(flv, out_flv);
    srs_flv_close(flv);
    srs_flv_close(out_flv);    
    
    return ret;
}

void digit_to_char(char* src, int ssize, char* dst, int dsize)
{
    int i, j;
    char v;
    
    for (i = 0, j = 0; i < ssize && j < dsize; i++) {
        if (j >= dsize) {
            break;
        }
        v = (src[i] >> 4) & 0x0F;
        if (v < 10) {
            dst[j++] = '0' + v;
        } else {
            dst[j++] = 'A' + (v - 10);
        }
        
        if (j >= dsize) {
            break;
        }
        v = src[i] & 0x0F;
        if (v < 10) {
            dst[j++] = '0' + v;
        } else {
            dst[j++] = 'A' + (v - 10);
        }
        
        if (j >= dsize) {
            break;
        }
        if (i < ssize - 1) {
            dst[j++] = ' ';
        }
    }
}

int parse_bytes(char* data, int size, char* hbuf, int hsize, char* tbuf, int tsize, int print_size)
{
    memset(hbuf, 0, hsize);
    memset(tbuf, 0, tsize);

    if (size > 0) {
        digit_to_char(data, size, hbuf, hsize - 1);
    }
    
    if (size > print_size * 2) {
        digit_to_char(data + size - print_size, size, tbuf, tsize - 1);
    }

    return 0;
}

int parse_flv(srs_flv_t flv, srs_flv_t out_flv)
{
    int ret = 0;
    
    // flv header
    char header[13];
    // packet data
    char type;
    u_int32_t timestamp = 0;
    u_int32_t videoBaseTime = 0;
    u_int32_t audioBaseTime = 0;
    char* data = NULL;
    int32_t size;
    int64_t offset = 0;
    int isVideoBegin = 0;
    int isAudioBegin = 0;
    Queue *audioQueue = InitQueue();
    Queue *videoQueue = InitQueue();
    
    if ((ret = srs_flv_read_header(flv, header)) != 0) {
        return ret;
    }
    
    if ((ret = srs_flv_write_header(out_flv, header)) != 0) 
    {
        return ret;
    }
    
    srs_human_trace("start parse flv");
    for (;;) {
        offset = srs_flv_tellg(flv);
        
        // tag header
        if ((ret = srs_flv_read_tag_header(flv, &type, &size, &timestamp)) != 0) {
            if (srs_flv_is_eof(ret)) {
                srs_human_trace("parse completed.");
                break;
            }
            srs_human_trace("flv get packet failed. ret=%d", ret);
            return ret;
        }
        
        if (size <= 0) {
            srs_human_trace("invalid size=%d", size);
            char sizeData[4];
            srs_flv_read_previous_tag_size(flv, sizeData);
            continue;
        }
        
        data = (char*)malloc(size);
        
        if ((ret = srs_flv_read_tag_data(flv, data, size)) == 0) {
            if ((ret = srs_human_print_rtmp_packet(type, timestamp, data, size)) == 0) {
                char hbuf[48]; char tbuf[48];
                parse_bytes(data, size, hbuf, sizeof(hbuf), tbuf, sizeof(tbuf), 16);
                //srs_human_raw("offset=%d, first and last 16 bytes:\n"
                //    "[+00, +15] %s\n[-15, EOF] %s\n", (int)offset, hbuf, tbuf);
            } else {
                srs_human_trace("print packet failed. ret=%d", ret);
            }
        } else {
            srs_human_trace("read flv failed. ret=%d", ret);
        }
        
        if((isVideoBegin < 2) && type == SRS_RTMP_TYPE_VIDEO)
        {
            if(srs_utils_flv_video_avc_packet_type(data, size) == 0) //sps_pps
            {
                srs_flv_write_tag(out_flv, type, timestamp, data, size);
            }
            else if((srs_utils_flv_video_frame_type(data, size) == 1)) // keyframe
            {
                if(isAudioBegin == 1)
                {
                    isVideoBegin++;
                }                    
            }
            
            if(isVideoBegin < 2)
            {
                free(data);
                continue;
            }
            
            videoBaseTime = timestamp;
        }
        
        if(isAudioBegin == 0 && type == SRS_RTMP_TYPE_AUDIO)
        {
            if(srs_utils_flv_audio_aac_packet_type(data, size) == 0) //aac metadata
            {
                srs_flv_write_tag(out_flv, type, timestamp, data, size);
                isAudioBegin = 1;
            }
            free(data);
            continue;
        }
        
        Item item;
        item.timestamp = timestamp;
        item.data = data;
        item.size = size;
        if(type == SRS_RTMP_TYPE_AUDIO)
        {
            EnQueue(audioQueue,item);
        }
        else if(type == SRS_RTMP_TYPE_VIDEO)
        {
            EnQueue(videoQueue,item);            
        }
    }
    
    // find firtst audio frame which near from the first I frame
    while(!IsEmpty(audioQueue))
    {
        Item item;
        GetFront(audioQueue, &item);
        if(item.timestamp < videoBaseTime)
        {
            DeQueue(audioQueue, &item);
            free(item.data);
        }
        else
        {
            audioBaseTime = item.timestamp;
            break;
        }
    }
    
    // re-order audio and video frame base on their timestamp
    while(!IsEmpty(videoQueue))
    {
        Item videoItem;
        GetFront(videoQueue, &videoItem);
        
        while(!IsEmpty(audioQueue))
        {
            Item audioItem;
            GetFront(audioQueue, &audioItem);
            if(audioItem.timestamp - audioBaseTime < videoItem.timestamp - videoBaseTime)
            {
                srs_flv_write_tag(out_flv, SRS_RTMP_TYPE_AUDIO, audioItem.timestamp - audioBaseTime, audioItem.data, audioItem.size);
                DeQueue(audioQueue, &audioItem);
                free(audioItem.data);
            }
            else
            {
                break;
            }
        }
        
        if ((ret = srs_flv_write_tag(out_flv, SRS_RTMP_TYPE_VIDEO, videoItem.timestamp - videoBaseTime, videoItem.data, videoItem.size)) != 0) 
        {
            srs_human_trace("write flv failed. ret=%d", ret);
        }
        
        DeQueue(videoQueue, &videoItem);
        free(videoItem.data);
        
        if (ret != 0) {
            srs_human_trace("parse failed, ret=%d", ret);
            return ret;
        }
    }
    
    return ret;
}
