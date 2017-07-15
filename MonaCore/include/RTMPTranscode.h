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
		friend int write_buffer_to_file(void *opaque, uint8_t *buf, int buf_size);    //�����������ݻ�д���ļ�
		friend int write_buffer(void *opaque, uint8_t *buf, int buf_size);

		void outDestintion(char* desPath, int _needNetwork){ des_path = desPath; needNetwork = _needNetwork; }
		void setTransParamters(int _width, int _height, int _bitRate){ width = _width; height = _height; bitRate = _bitRate; }

		int tranStart();    //transcode start
		int tranEnd();      //transcode end
		
		void run(Exception &ex);     //����startable�е�run������ִ���̲߳���

		int decode();         //����
		
		int encode();         //����

	private:
		int init();       //��ʼ������

		int unit();       //ж�غ������ͷ��ڴ�

		int prepare();
		int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

		void build_flv_message(char *tagHeader, char *tagEnd, int size, UInt32 timeStamp);

		int pushVideoPacket(BinaryReader &videoPacket);    //����Ƶ��ѹ�뻺�����

		int getVideoPacket(uint8_t *buf, int& buf_size);    //ȡ����Ƶ��

		void resolutionChange(AVCodecContext *pCodecCtx, AVFrame *pFrame, AVFrame *pNewFrame, int pNewWidth, int pNewHeight);     //ͼ������

		AVFormatContext* ifmt_ctx;          //ý���ļ�����ṹ��
		unsigned char* inbuffer;           //���뻺������
		AVIOContext *avio_in;              //�����Ӧ�Ľṹ�壬��������
		AVInputFormat *piFmt;
		AVStream *in_stream;
		AVFrame *frame;


		AVFormatContext* ofmt_ctx;			//����ṹ��
		unsigned char* outbuffer;
		AVIOContext *avio_out;
		AVStream *out_stream;

		char *des_path;

		int width;           //Ŀ��ͼ���ȣ����ص�λ��
		int height;			 //Ŀ��ͼ��߶ȣ����ص�λ��
		int bitRate;		 //Ŀ����Ƶ������
		int needNetwork;     //����Ƿ�Ϊ���������ǵĻ���1��


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