
#include "Decode.h"     
#include <iostream>
#include <winerror.h>
#define BUF_SIZE 32768
using namespace std;

namespace Transcode
{
	VideoQueue videoQueue;

	Decode::Decode()
	{

		avio_in = NULL;
		inbuffer = NULL;
		packet = NULL;
		frame = NULL;
		dec_ctx = NULL;
		piFmt = NULL;
		av_register_all();											//注册所有编解码器，复用器和解复用器
		ifmt_ctx = avformat_alloc_context();					   //初始化AVFormatContext结构体，主要给结构体分配内存、设置字段默认值
		
	}

	int read_buffer(void *opaque, uint8_t *buf, int buf_size)
	{
		//int ret = -1;
		if (opaque != NULL)
		{
			av_log(NULL,AV_LOG_INFO,"buf_size:%i\n",buf_size);
			int size = buf_size - 10;
			memcpy(buf, opaque, size);
			return size;
		}
		return -1;
	}


	int Decode::decode(int buf_size,const uint8_t *packet)
	{
		int ret = 0;
		int i = 0;
		inbuffer = (unsigned char*)av_malloc(2*buf_size);            //为输入缓冲区间分配内存
		uint8_t *decodeBuffer = (uint8_t*)av_malloc(buf_size);
		memcpy(decodeBuffer, packet, buf_size);
		
		/*open input file*/
		av_log(NULL, AV_LOG_INFO, " The buf_size received:%i\n", buf_size);
		avio_in = avio_alloc_context(inbuffer, buf_size, 0, decodeBuffer, read_buffer, NULL, NULL);
		if (avio_in == NULL)
			return 0;

		/*if(av_probe_input_buffer(avio_in, &piFmt, "", NULL, 0, 0) < 0)			//探测流格式
		{
			av_log(NULL, AV_LOG_ERROR, "probe filed!");
		}*/
		/*原本的输入AVFormatContext的指针pb（AVIOContext类型）
		*指向这个自行初始化的输入AVIOContext结构体。
		*AVIOContext *pb：输入数据的缓存
		*/
		
		ifmt_ctx->pb = avio_in;     //important

		ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;                                          //通过标志位说明采用自定义AVIOContext
		if ((ret = avformat_open_input(&ifmt_ctx, "whatever", NULL, NULL)) < 0) {      //打开多媒体数据并且获得一些相关的信息，函数执行成功的话，其返回值大于等于0
			av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			return ret;
		}
		if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {					//该函数可以读取一部分视音频数据并且获得一些相关的信息
			av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
			return ret;
		}
		printf("***nb_stream%d \n", ifmt_ctx->nb_streams);
		for (i = 0; i < ifmt_ctx->nb_streams; i++) {       //nb_streams为流的数目
			AVStream *stream;
			AVCodecContext *codec_ctx;
			stream = ifmt_ctx->streams[i];
			codec_ctx = stream->codec;                    //codec为指向该视频/音频流的AVCodecContext
			INFO("nb_stream:",i)
			/* Reencode video & audio and remux subtitles etc. */
			if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){       //编解码器的类型（视频，音频）  
				printf("video stream\n");
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//该函数用于初始化一个视音频编解码器的AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //根据解码器的ID查找解码器，找到就返回查找到的解码器
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
					return ret;
				}
			}
			if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
				INFO("audio stream:")
		}
		return 1;
	}

}  //namespace Transcode
