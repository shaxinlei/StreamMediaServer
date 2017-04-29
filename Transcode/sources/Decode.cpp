
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
		av_register_all();											//ע�����б���������������ͽ⸴����
		ifmt_ctx = avformat_alloc_context();					   //��ʼ��AVFormatContext�ṹ�壬��Ҫ���ṹ������ڴ桢�����ֶ�Ĭ��ֵ
		
	}

	int read_buffer(void *opaque, uint8_t *buf, int buf_size)
	{
		//int ret = -1;
		if (opaque != NULL)
		{
			av_log(NULL,AV_LOG_INFO,"buf_size:%i\n",buf_size);
			memcpy(buf, opaque, buf_size);
			return buf_size;
		}
		return -1;
	}


	int Decode::decode(int buf_size,const uint8_t *packet)
	{
		int ret = 0;
		int i = 0;
		inbuffer = (unsigned char*)av_malloc(2*buf_size);            //Ϊ���뻺����������ڴ�
		uint8_t *decodeBuffer = (uint8_t*)av_malloc(buf_size);
		memcpy(decodeBuffer, packet, buf_size);
		
		/*open input file*/
		av_log(NULL, AV_LOG_INFO, " The buf_size received:%i\n", buf_size);
		avio_in = avio_alloc_context(inbuffer, buf_size, 0, decodeBuffer, read_buffer, NULL, NULL);
		if (avio_in == NULL)
			return 0;

		/*if(av_probe_input_buffer(avio_in, &piFmt, "", NULL, 0, 0) < 0)			//̽������ʽ
		{
			av_log(NULL, AV_LOG_ERROR, "probe filed!");
		}*/
		/*ԭ��������AVFormatContext��ָ��pb��AVIOContext���ͣ�
		*ָ��������г�ʼ��������AVIOContext�ṹ�塣
		*AVIOContext *pb���������ݵĻ���
		*/
		
		ifmt_ctx->pb = avio_in;     //important

		ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;                                          //ͨ����־λ˵�������Զ���AVIOContext
		if ((ret = avformat_open_input(&ifmt_ctx, "whatever", NULL, NULL)) < 0) {      //�򿪶�ý�����ݲ��һ��һЩ��ص���Ϣ������ִ�гɹ��Ļ����䷵��ֵ���ڵ���0
			av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			return ret;
		}
		if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {					//�ú������Զ�ȡһ��������Ƶ���ݲ��һ��һЩ��ص���Ϣ
			av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
			return ret;
		}
		printf("***nb_stream%d \n", ifmt_ctx->nb_streams);
		for (i = 0; i < ifmt_ctx->nb_streams; i++) {       //nb_streamsΪ������Ŀ
			AVStream *stream;
			AVCodecContext *codec_ctx;
			stream = ifmt_ctx->streams[i];
			codec_ctx = stream->codec;                    //codecΪָ�����Ƶ/��Ƶ����AVCodecContext
			INFO("nb_stream:",i)
			/* Reencode video & audio and remux subtitles etc. */
			if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){       //������������ͣ���Ƶ����Ƶ��  
				printf("video stream\n");
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//�ú������ڳ�ʼ��һ������Ƶ���������AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //���ݽ�������ID���ҽ��������ҵ��ͷ��ز��ҵ��Ľ�����
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
