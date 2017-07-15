#pragma once

#include "Mona/Startable.h"
#include "Mona/Invoker.h"
#include "VideoBuffer.h"
#include <thread>
#include <queue>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
};

namespace Mona
{
	class RTMPTranscode :public Startable
	{
	public:
		RTMPTranscode();
		~RTMPTranscode(){};
			
		friend int read_buffer(void *opaque, uint8_t *buf, int buf_size);
		friend int write_buffer_to_file(void *opaque, uint8_t *buf, int buf_size);    //将解码后的数据回写入文件
		friend int write_buffer(void *opaque, uint8_t *buf, int buf_size);

		void outDestintion(char* desPath, int _needNetwork){ des_path = desPath; needNetwork = _needNetwork; }
		void setTransParamters(int _width, int _height, int _bitRate){ width = _width; height = _height; bitRate = _bitRate; }

		int tranStart();    //transcode start
		int tranEnd();      //transcode end
		
		void run(Exception &ex);     //重载startable中的run函数，执行线程操作

		int decode();         //解码
		
		int encode();         //编码

	private:
		int init();       //初始化函数

		int unit();       //卸载函数，释放内存

		int prepare();
		int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

		void build_flv_message(char *tagHeader, char *tagEnd, int size, UInt32 timeStamp);

		int pushVideoPacket(BinaryReader &videoPacket);    //将视频包压入缓存队列

		int getVideoPacket(uint8_t *buf, int& buf_size);    //取出视频包

		void resolutionChange(AVCodecContext *pCodecCtx, AVFrame *pFrame, AVFrame *pNewFrame, int pNewWidth, int pNewHeight);     //图像收缩

		AVFormatContext* ifmt_ctx;          //媒体文件输入结构体
		unsigned char* inbuffer;           //输入缓冲区间
		AVIOContext *avio_in;              //输入对应的结构体，用于输入
		AVInputFormat *piFmt;
		AVStream *in_stream;
		AVFrame *frame;


		AVFormatContext* ofmt_ctx;			//输出结构体
		unsigned char* outbuffer;
		AVIOContext *avio_out;
		AVStream *out_stream;

		char *des_path;

		int width;           //目标图像宽度（像素单位）
		int height;			 //目标图像高度（像素单位）
		int bitRate;		 //目标视频比特率
		int needNetwork;     //输出是否为网络流（是的话置1）


		int64_t pts, dts;
		int last_pts;
		int last_dts;

		int video_index;
		int got_frame;
		int got_picture;
		int frame_index;

		VideoBuffer videoQueue;

		FILE *fp_write;
		
		

	};
}   //namespace Mona