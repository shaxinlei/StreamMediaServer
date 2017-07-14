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
		friend int write_buffer_to_file(void *opaque, uint8_t *buf, int buf_size);    //�����������ݻ�д���ļ�
		friend int write_buffer(void *opaque, uint8_t *buf, int buf_size);

		static void build_flv_message(char *tagHeader, char *tagEnd, int size, UInt32 timeStamp);

		int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

		void run(Exception &ex);     //����startable�е�run������ִ���̲߳���

		int pushVideoPacket(BinaryReader &videoPacket);    //����Ƶ��ѹ�뻺�����

		int getVideoPacket(int& flag, uint8_t *buf, int& buf_size);    //ȡ����Ƶ��

		int decode();
		
		int encode();

	private:
		int init();       //��ʼ������

		int unit();       //ж�غ������ͷ��ڴ�

		int prepare();



		

	};
}   //namespace Mona