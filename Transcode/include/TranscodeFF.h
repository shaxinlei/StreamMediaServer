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
	class BinaryReader;

	class TranscodeFF :public Startable
	{
	public:
		TranscodeFF(int dst_width, int dst_height, int bitRate, char * rtmp_in_url, char * rtmp_out_url);

		void run(Exception &ex);

	private:
		int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);

		int _dst_width;
		int _dst_height;
		int _bitRate;
		int _my_pts;


		char * _rtmp_in_url;
		char * _rtmp_out_url;

	};


} //namespace FFMPEG