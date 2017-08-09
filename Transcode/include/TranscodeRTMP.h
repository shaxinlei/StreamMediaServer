#pragma once

#include "Mona/Startable.h"

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

namespace  Mona
{
	class TranscodeRTMP :public Startable
	{
	public:
		TranscodeRTMP();

		int inputFile(char *file);
		int outputFile(char *file);
		void setEncodeParamter(int _dst_width, int _dst_height, int _bitRate);
		int transStart();
		int decode();
		int encode();
		int transEnd();
		void run(Exception &ex);
	private:
		int init();
		int unit();
		int prepare();
		int TranscodeRTMP::flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);
		void ResolutionChange(AVCodecContext *pCodecCtx, AVFrame *pFrame, AVFrame *pNewFrame, int pNewWidth, int pNewHeight);

		AVFormatContext *in_fmt_ctx;
		AVStream *in_video_stream;
		AVFrame *frame;
		AVFrame *old_frame;
		AVPacket *pkt_in;


		AVFormatContext *out_fmt_ctx;
		AVStream *out_video_stream;
		AVPacket pkt_out;

		SwsContext *pSwsCtx;

		char *rtmp_url;
		char *name_path;

		int width;
		int height;
		int bitRate;

		int64_t pts, dts;
		int last_pts;
		int last_dts;

		int video_index;
		int got_frame;
		int got_picture;
		int frame_index;
		int i;
	};
}
