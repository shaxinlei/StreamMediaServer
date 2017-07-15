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

		static void build_flv_message(char *tagHeader, char *tagEnd, int size, UInt32 timeStamp);

		int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

		void run(Exception &ex);     //重载startable中的run函数，执行线程操作

		int pushVideoPacket(BinaryReader &videoPacket);    //将视频包压入缓存队列

		int getVideoPacket(int& flag, uint8_t *buf, int& buf_size);    //取出视频包

		int decode();
		
		int encode();

	private:
		int init();       //初始化函数

		int unit();       //卸载函数，释放内存

		int prepare();



		

	};
}   //namespace Mona