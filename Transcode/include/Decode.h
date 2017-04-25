#pragma once

#include "Mona/PacketReader.h"
#include "Mona/Logs.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
};



namespace  Transcode
{
	class Decode
	{
	public:
		Decode();

		//回调函数 将收到包的buffer拷贝到buf中
		friend int read_buffer(void *opaque, uint8_t *buf, int buf_size);

		int decode(int buf_size, const uint8_t *packet);

	private:
		AVFormatContext* ifmt_ctx;		//AVFormatContext:统领全局的基本结构体。主要用于处理封装格式（FLV/MK/RMVB）
		AVPacket* packet;				//存储压缩数据（视频对应H.264等码流数据，音频对应PCM采样数据）
		AVFrame *frame;				//AVPacket存储非压缩的数据（视频对应RGB/YUV像素数据，音频对应PCM采样数据）
		//	enum AVMediaType type;			//指明了类型，是视频，音频，还是字幕
		//	unsigned int stream_index;
		//	int got_frame;

		//AVStream *in_stream;
		AVCodecContext *dec_ctx;

		unsigned char* inbuffer;       //输入缓冲区间
		AVIOContext *avio_in;          //输入对应的结构体，用于输入
		int buffer_size;
		AVInputFormat *piFmt;
	


	};
} //namespace Transcode