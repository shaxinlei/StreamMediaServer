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

		//�ص����� ���յ�����buffer������buf��
		friend int read_buffer(void *opaque, uint8_t *buf, int buf_size);

		int decode(int buf_size, Mona::PacketReader& packet);

	private:
		AVFormatContext* ifmt_ctx;		//AVFormatContext:ͳ��ȫ�ֵĻ����ṹ�塣��Ҫ���ڴ����װ��ʽ��FLV/MK/RMVB��
		AVPacket* packet;				//�洢ѹ�����ݣ���Ƶ��ӦH.264���������ݣ���Ƶ��ӦPCM�������ݣ�
		AVFrame *frame;				//AVPacket�洢��ѹ�������ݣ���Ƶ��ӦRGB/YUV�������ݣ���Ƶ��ӦPCM�������ݣ�
		//	enum AVMediaType type;			//ָ�������ͣ�����Ƶ����Ƶ��������Ļ
		//	unsigned int stream_index;
		//	int got_frame;

		//AVStream *in_stream;
		AVCodecContext *dec_ctx;

		unsigned char* inbuffer;       //���뻺������
		AVIOContext *avio_in;          //�����Ӧ�Ľṹ�壬��������
		int buffer_size;
	


	};
} //namespace Transcode