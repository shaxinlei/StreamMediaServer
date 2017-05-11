#pragma once

#include "Mona/PacketReader.h"
#include "Mona/Startable.h"
#include "Mona/Invoker.h"
#include <queue>


namespace Mona {
	class FlashStream;
}

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
};



namespace  Mona
{

	class Transcode :public Startable, public virtual Object
	{
	public:
		Transcode();
		~Transcode();

		//回调函数 将收到包的buffer拷贝到buf中
		friend int read_buffer(void *opaque, uint8_t *buf, int buf_size);
		friend int write_buffer(void *opaque, uint8_t *buf, int buf_size);

		//int decode(int size,const uint8_t *buf);
		Buffer * decode(PacketReader &videoPacket);     //解码

		static void build_flv_message(char *tagHeader, char *tagEnd, int size, UInt32 &timeStamp);

		int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

		void run(Exception& ex);

		void receiveVideoPacket(BinaryReader &videoPacket);
	private:
		AVFormatContext* ifmt_ctx;		//AVFormatContext:统领全局的基本结构体。主要用于处理封装格式（FLV/MK/RMVB）
		unsigned char* inbuffer;       //输入缓冲区间
		unsigned char* outbuffer;
		AVIOContext *avio_in;          //输入对应的结构体，用于输入
		AVIOContext *avio_out;
		AVInputFormat *piFmt;

		AVFormatContext* ofmt_ctx;
		AVPacket packet, enc_pkt;          //存储压缩数据（视频对应H.264等码流数据，音频对应PCM采样数据）
		AVFrame *frame;            //AVPacket存储非压缩的数据（视频对应RGB/YUV像素数据，音频对应PCM采样数据）
		enum AVMediaType type;				//指明了类型，是视频，音频，还是字幕

		AVStream *out_stream;             //AVStream，AVCodecContext：视音频流对应的结构体，用于视音频编解码。
		AVStream *in_stream;
		AVCodecContext *dec_ctx, *enc_ctx;
		AVCodec *encoder;                 //AVCodec是存储编解码器信息的结构体，enconder存储编码信息的结构体
		Buffer outVideoBuffer;

		std::queue<BinaryReader> video_bf_queue;
		CRITICAL_SECTION m_lock;
	};

	
} //namespace FFMPEG