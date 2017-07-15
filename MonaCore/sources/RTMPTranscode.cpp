#include  "RTMPTranscode.h"
#include "Mona/Logs.h"
#define READ_BUF_SIZE 32768*4
#define BUF_SIZE 32768

namespace Mona
{
	int read_buffer(void *opaque, uint8_t *buf, int buf_size)
	{
		int tureSize = 0;
		tureSize = ((RTMPTranscode *)opaque)->getVideoPacket(buf, buf_size);
		//av_log(NULL, AV_LOG_INFO, " read buf_size:%i\n", tureSize);
		return tureSize;
	}

	//Write File  回调函数
	int write_buffer(void *opaque, uint8_t *buf, int buf_size){
		FILE* fp_write = (FILE*)opaque;
		if (!feof(fp_write)){
			int true__size = fwrite(buf, 1, buf_size, fp_write);
			INFO("write data to buf:",true__size)
			return true__size;
		}
		else{
			return -1;
		}
	}


	void RTMPTranscode::build_flv_message(char* tagHeader, char* tagEnd, int size, Mona::UInt32 timeStamp)
	{
		int dataSize = size;			 //视频数据长度
		int endSize = dataSize + 11;		//前一个Tag的长度
		//INFO("timestamp:", timeStamp)
		unsigned char * p_dataSize = (unsigned char*)&dataSize;

		for (int i = 0; i < 3; i++)
		{
			tagHeader[3 - i] = *(p_dataSize + i);
		}

		unsigned char * p_timeStamp = (unsigned char *)&timeStamp;
		for (int i = 0; i < 3; i++)
		{
			tagHeader[6 - i] = *(p_timeStamp + i);
		}

		unsigned char * p_endSize = (unsigned char*)&endSize;

		for (int i = 0; i < 4; i++)
		{
			tagEnd[3 - i] = *(p_endSize + i);
		}
	}

	int RTMPTranscode::pushVideoPacket(BinaryReader& videoPacket)
	{
		if ((&videoPacket) != NULL)
		{
			videoQueue.push(videoPacket);
			return 1;
		}
		else
		{
			ERROR("videoPacket is point to NULL")
				return 0;
		}

	}

	int RTMPTranscode::getVideoPacket(uint8_t* buf, int& buf_size)
	{
		BinaryReader videoPacket;
		videoQueue.wait_and_pop(videoPacket);
		//printf("buffer_size:%d\tpackageSize:%d\n", buf_size, videoPacket.available());
		buf_size = FFMIN(buf_size, videoPacket.available());
		memcpy(buf, videoPacket.current(), buf_size);
		//av_log(NULL, AV_LOG_INFO, " getVideoPacket:%i\n", buf_size);
		videoPacket.moveCurrent(buf_size);
		delete videoPacket.data();
		return buf_size;
	}

	int RTMPTranscode::flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index)
	{
		int ret;
		int got_frame;
		AVPacket enc_pkt;
		if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
			CODEC_CAP_DELAY))
			return 0;
		while (1) {
			av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
			//ret = encode_write_frame(NULL, stream_index, &got_frame);
			enc_pkt.data = NULL;
			enc_pkt.size = 0;
			av_init_packet(&enc_pkt);
			ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, &enc_pkt,
				NULL, &got_frame);
			av_frame_free(NULL);
			if (ret < 0)
				break;
			if (!got_frame)
			{
				ret = 0; break;
			}

			enc_pkt.stream_index = stream_index;
			enc_pkt.dts = av_rescale_q_rnd(enc_pkt.dts,
				fmt_ctx->streams[stream_index]->codec->time_base,
				fmt_ctx->streams[stream_index]->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			enc_pkt.pts = av_rescale_q_rnd(enc_pkt.pts,
				fmt_ctx->streams[stream_index]->codec->time_base,
				fmt_ctx->streams[stream_index]->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			enc_pkt.duration = av_rescale_q(enc_pkt.duration,
				fmt_ctx->streams[stream_index]->codec->time_base,
				fmt_ctx->streams[stream_index]->time_base);
			av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
			/* mux encoded frame */
			ret = av_write_frame(fmt_ctx, &enc_pkt);
			if (ret < 0)
				break;
		}
		return ret;
	}

	void RTMPTranscode::resolutionChange(AVCodecContext *pCodecCtx, AVFrame *pFrame, AVFrame *pNewFrame, int pNewWidth, int pNewHeight)
	{
		int ret = av_image_alloc(pNewFrame->data, pNewFrame->linesize, pNewWidth, pNewHeight, AV_PIX_FMT_YUV420P, 1);
		if (ret< 0) {
			printf("Could not allocate destination image\n");
			return;
		}
		//用 sws_getContext函数 得到 sws_scale函数 运行的上下文，之后用 sws_scale函数 将图形缩放
		SwsContext *pSwsCtx = NULL;
		pSwsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, pNewWidth, pNewHeight, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
		if (pSwsCtx == NULL)
			return;
		sws_scale(pSwsCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pNewFrame->data, pNewFrame->linesize);
		sws_freeContext(pSwsCtx);
	}

	/*卸载函数，释放内存*/
	int RTMPTranscode::unit()
	{
		if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
			avio_close(ofmt_ctx->pb);
		}
		avcodec_close(ofmt_ctx->streams[0]->codec);
		av_freep(&ofmt_ctx->streams[0]->codec);
		av_freep(&ofmt_ctx->streams[0]);
		avformat_free_context(ifmt_ctx);
		avformat_free_context(ofmt_ctx);
		return 0;
	}

	/*FFmpeg组件注册与初始化操作*/
	int RTMPTranscode::init()
	{
		av_register_all();
		if (needNetwork)
		{
			avformat_network_init();
		}

		ifmt_ctx = NULL;
		inbuffer = NULL;
		avio_in = NULL;
		piFmt = NULL;
		in_stream = NULL;
		frame = NULL;
		dec_ctx = NULL;

		ofmt_ctx = NULL;
		outbuffer = NULL;
		avio_out = NULL;
		out_stream = NULL;
		enc_ctx = NULL;

		video_index = -1;
		pts = -3600;
		dts = -3600;
		frame_index = 0;
		ifmt_ctx = avformat_alloc_context();	//初始化AVFormatContext结构体
		
		return 1;
	}

	/*准备和参数设定*/
	int RTMPTranscode::prepare()
	{
		int ret = 0;
		int i = 0;
		unsigned int stream_index;
		int got_frame, enc_got_frame;
		/*如果需要将转码后的*/
		if (!needNetwork)
		{
			fopen_s(&fp_write, "test.h264", "wb+");
			avformat_alloc_output_context2(&ofmt_ctx, NULL, "h264", NULL);
		}
		else
		{
			avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", des_path);
		}
		inbuffer = (unsigned char*)av_malloc(READ_BUF_SIZE);            //为输入缓冲区间分配内存
		outbuffer = (unsigned char*)av_malloc(BUF_SIZE);
		avio_in = avio_alloc_context(inbuffer, READ_BUF_SIZE, 0, this, read_buffer, NULL, NULL);
		if (avio_in == NULL)
			unit();
		avio_out = avio_alloc_context(outbuffer, BUF_SIZE, 0, fp_write, NULL, write_buffer, NULL);  //初始化输出AVIOContext结构体
		if (avio_out == NULL)
			unit();
		/*
		*原本的输入AVFormatContext的指针pb（AVIOContext类型）
		*指向这个自行初始化的输入AVIOContext结构体。
		*/
		ifmt_ctx->pb = avio_in;     //important
		ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
		if ((ret = avformat_open_input(&ifmt_ctx, "what", NULL, NULL)) < 0) {							//打开多媒体数据并且获得一些相关的信息，函数执行成功的话，其返回值大于等于0
			av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			unit();
		}
		AVDictionary* pOptions = NULL;
		ifmt_ctx->probesize = 64 * 1024;
		ifmt_ctx->max_analyze_duration = 5 * AV_TIME_BASE;
		if ((ret = avformat_find_stream_info(ifmt_ctx, &pOptions)) < 0) {										//该函数可以读取一部分视音频数据并且获得一些相关的信息
			av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
			unit();
		}
		av_dump_format(ifmt_ctx, 0, NULL, false);
		for (i = 0; i < ifmt_ctx->nb_streams; i++) {       //nb_streams为流的数目
			AVStream *stream;
			AVCodecContext *codec_ctx;
			stream = ifmt_ctx->streams[i];
			codec_ctx = stream->codec;                    //codec为指向该视频/音频流的AVCodecContext
			//Reencode video & audio and remux subtitles etc. */
			if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){       //编解码器的类型（视频，音频）  
				printf("video stream\n");
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//该函数用于初始化一个视音频编解码器的AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //根据解码器的ID查找解码器，找到就返回查找到的解码器
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
					unit();
				}
			}
			if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
				INFO("audio stream:")
		}
		if (needNetwork)
		{
			if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
				ret = avio_open(&ofmt_ctx->pb, des_path, AVIO_FLAG_WRITE);
				if (ret < 0) {
					printf("无法打开地址 '%s'", des_path);
					unit();
				}
			}
		}
		for (i = 0; i < 1; i++) {
			out_stream = avformat_new_stream(ofmt_ctx, NULL);              //初始化AVStream结构体（分配内存，设置默认值），并返回这个这个结构体
			if (!out_stream) {
				av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
				unit();
			}
			in_stream = ifmt_ctx->streams[i];
			dec_ctx = in_stream->codec;
			enc_ctx = out_stream->codec;
			if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				encoder = avcodec_find_encoder(AV_CODEC_ID_H264);    //返回AV_CODEC_ID_H264编码器
				enc_ctx->height = dst_height;        //如果是视频的话，代表宽和高
				enc_ctx->width = dst_width;
				enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;     //宽高比
				//enc_ctx->sample_aspect_ratio.num = dst_width;
				//enc_ctx->sample_aspect_ratio.num = dst_height;
				enc_ctx->pix_fmt = encoder->pix_fmts[0];      //像素格式
				enc_ctx->time_base = dec_ctx->time_base;      //帧时间戳的基本时间单位（以秒为单位）
				//enc_ctx->time_base.num = 1;
				//enc_ctx->time_base.den = 25;
				//H264的必备选项，没有就会错
				enc_ctx->me_range = 16;						  //子单位最大运动估计搜索范围
				enc_ctx->max_qdiff = 4;						//帧之间的最大量化器差异
				enc_ctx->qmin = 10;							//最小量化器
				enc_ctx->qmax = 51;							//最大量化器
				enc_ctx->gop_size = 15;
				enc_ctx->qcompress = 0.6;
				enc_ctx->refs = 3;					//运动估计参考帧的个数（H.264的话会有多帧，MPEG2这类的一般就没有了）
				enc_ctx->bit_rate = bitRate;        //平均比特率

				ret = avcodec_open2(enc_ctx, encoder, NULL);			//初始化并打开编码器
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
					goto end;
				}
			}
			else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
				av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
				goto end;
			}
			else {
				/* if this stream must be remuxed */
				ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
					ifmt_ctx->streams[i]->codec);
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
					goto end;
				}
			}
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
		av_opt_set(enc_ctx->priv_data, "tune", "zerolatency", 0);    //进行实时编码
	}





}
