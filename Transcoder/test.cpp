#include <stdio.h>

#define __STDC_CONSTANT_MACROS
#define BUFFER_SIZE	32768
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
};


int main(int argc, char* argv[])
{
	int ret;
	AVFormatContext* ifmt_ctx = NULL;    //AVFormatContext:统领全局的基本结构体。主要用于处理封装格式（FLV/MK/RMVB）

	int             i, audioStream;
	AVCodecContext  *pCodecCtx;
	AVCodec         *pCodec;

	char url[] = "rtmp://live.hkstv.hk.lxdns.com/live/hks";

	av_register_all();
	avformat_network_init();
	ifmt_ctx = avformat_alloc_context();
	//Open  
	if (avformat_open_input(&ifmt_ctx, url, NULL, NULL) != 0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	// Retrieve stream information  
	if (av_find_stream_info(ifmt_ctx) < 0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	// Dump valid information onto standard error  
	av_dump_format(ifmt_ctx, 0, url, false);

	// Find the first audio stream  
	audioStream = -1;
	for (i = 0; i < ifmt_ctx->nb_streams; i++)
		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			audioStream = i;
			break;
		}

	if (audioStream == -1){
		printf("Didn't find a audio stream.\n");
		return -1;
	}

	// Get a pointer to the codec context for the audio stream  
	pCodecCtx = ifmt_ctx->streams[audioStream]->codec;

	// Find the decoder for the audio stream  
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL){
		printf("Codec not found.\n");
		return -1;
	}

	// Open codec  
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0){
		printf("Could not open codec.\n");
		return -1;
	}
}
	