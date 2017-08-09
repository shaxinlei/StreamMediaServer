

#include <iostream>

#include "TranscodeFF.h"
#include "Mona/Logs.h"

#define BUF_SIZE 32768
#define SAVE_AS_FILE_FLAG 0

using namespace std;

namespace Mona
{
	TranscodeFF::TranscodeFF(int dst_width, int dst_height, int bitRate, char * rtmp_in_url, char * rtmp_out_url) :Startable("TranscodeFF")
	{
		_dst_width = dst_width  ;
		_dst_height= dst_height ;
		_bitRate=bitRate ;
		_rtmp_in_url=rtmp_in_url  ;
		_rtmp_out_url=rtmp_out_url ;

	}

	int TranscodeFF::flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index)
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

	
	void TranscodeFF::run(Exception& ex)
	{
		av_register_all();											//注册所有编解码器，复用器和解复用器
		int  dst_width = _dst_width;
		int dst_height = _dst_height;
		int  bitRate = _bitRate;
		char *  rtmp_in_url = _rtmp_in_url;
		char * rtmp_out_url = _rtmp_out_url;



		AVFormatContext* ifmt_ctx=NULL;		//AVFormatContext:统领全局的基本结构体。主要用于处理封装格式（FLV/MK/RMVB）
		AVInputFormat *piFmt=NULL;

		AVFormatContext* ofmt_ctx=NULL;
		AVPacket packet, enc_pkt;          //存储压缩数据（视频对应H.264等码流数据，音频对应PCM采样数据）
		AVFrame *frame=NULL;            //AVPacket存储非压缩的数据（视频对应RGB/YUV像素数据，音频对应PCM采样数据）
		enum AVMediaType type;				//指明了类型，是视频，音频，还是字幕

		AVStream *out_stream=NULL;             //AVStream，AVCodecContext：视音频流对应的结构体，用于视音频编解码。
		AVStream *in_stream=NULL;
		AVCodecContext *dec_ctx=NULL, *enc_ctx=NULL;
		AVCodec *encoder=NULL;                 //AVCodec是存储编解码器信息的结构体，enconder存储编码信息的结构体

		SwsContext *pSwsCtx=NULL;

		int flag=0;

		AVFrame* newFrame=NULL;

		
		int ret = 0;
		int i = 0;
		unsigned int stream_index;
		int got_frame, enc_got_frame;

		avformat_network_init();            //需要播放网络视频流

		ifmt_ctx = avformat_alloc_context();															//初始化AVFormatContext结构体
		avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", rtmp_out_url);



		if ((ret = avformat_open_input(&ifmt_ctx, rtmp_in_url, NULL, NULL)) < 0) {							//打开多媒体数据并且获得一些相关的信息，函数执行成功的话，其返回值大于等于0
			av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			goto end;
		}
	
		if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {										//该函数可以读取一部分视音频数据并且获得一些相关的信息
			av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
			goto end;
		}
		av_dump_format(ifmt_ctx, 0, NULL, false);
		//printf("***nb_stream%d \n", ifmt_ctx->nb_streams);
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
					goto end;
				}
			}
			if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
				INFO("audio stream:")
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//该函数用于初始化一个视音频编解码器的AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //根据解码器的ID查找解码器，找到就返回查找到的解码器
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
					goto end;
				}
		}

			//打开输出地址（推流地址）
		if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&ofmt_ctx->pb, rtmp_out_url, AVIO_FLAG_WRITE);
			if (ret < 0) {
				printf("无法打开地址 '%s'", rtmp_out_url);
				goto end;
			}
		}	
		
		for (i = 0; i < 1; i++) {
			out_stream = avformat_new_stream(ofmt_ctx, NULL);              //初始化AVStream结构体（分配内存，设置默认值），并返回这个这个结构体
			if (!out_stream) {
				av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
				goto end;
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

		/* init muxer, write output file header */
		ret = avformat_write_header(ofmt_ctx, NULL);      //写视频文件头
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
			goto end;
		}
		pSwsCtx = sws_getContext(dec_ctx->width, dec_ctx->height, AV_PIX_FMT_YUV420P, dst_width, dst_height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
		i = 0;


	



		frame = av_frame_alloc();
		if (!frame) {
			ret = AVERROR(ENOMEM);
		}


		newFrame = av_frame_alloc();
		int retNext = av_image_alloc(newFrame->data, newFrame->linesize, dst_width, dst_height, AV_PIX_FMT_YUV420P, 1);
		if (retNext< 0) {
			printf("Could not allocate destination image\n");
			return;
		}

		/* read all packets */
		while (1) {
			i++;
			//读取码流中的音频若干帧或者视频一帧
			if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
				break;

			stream_index = packet.stream_index;        //Packet所在stream的index
			if (stream_index != 0)
				continue;
			type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
			av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
				stream_index);

			av_log(NULL, AV_LOG_DEBUG, "Going to reencode the frame\n");
			
			packet.dts = av_rescale_q_rnd(packet.dts,                      //解码时间戳        av_rescale_q_rn用来转换时间戳
				ifmt_ctx->streams[stream_index]->time_base,
				ifmt_ctx->streams[stream_index]->codec->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

			packet.pts = av_rescale_q_rnd(packet.pts,                      //显示时间戳
				ifmt_ctx->streams[stream_index]->time_base,
				ifmt_ctx->streams[stream_index]->codec->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

			//解码一帧视频数据。输入一个压缩编码的结构体AVPacket，输出一个解码后的结构体AVFrame
			ret = avcodec_decode_video2(ifmt_ctx->streams[stream_index]->codec, frame,
				&got_frame, &packet);       //如果没有帧可以解压缩，got_frame为零，否则，非零

			//printf("Decode 1 Packet\tsize:%d\tpts:%lld\tdts:%lld\n", packet.size, packet.pts,packet.dts);

			//INFO("frame type:", frame->pict_type)
			if (ret < 0) {     //解码一帧视频失败
			/*	av_frame_free(&frame);*/
				av_free_packet(&packet);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				continue;
			}
			if (got_frame) {    //帧被解压缩
				frame->pts = av_frame_get_best_effort_timestamp(frame);   //设置显示时间戳
				frame->pict_type = AV_PICTURE_TYPE_NONE;      //图片帧类型

			

				sws_scale(pSwsCtx, frame->data, frame->linesize, 0, dec_ctx->height, newFrame->data, newFrame->linesize);

				//av_frame_free(&frame);     //释放结构体

				newFrame->pts = flag++;    //设置显示时间戳
				//newFrame->pict_type = AV_PICTURE_TYPE_NONE;      //图片帧类型

				enc_pkt.data = NULL;     //指向保存压缩数据的指针，这就是AVPacket实际的数据。
				enc_pkt.size = 0;
				av_init_packet(&enc_pkt);   //使用默认值初始化

				//编码一帧视频数据,成功编码一个AVPacket时enc_got_frame设置为1
				ret = avcodec_encode_video2(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
					newFrame, &enc_got_frame);


				//printf("Encode 1 Packet\tsize:%d\tpts:%lld\tdts:%lld\n", enc_pkt.size, enc_pkt.pts,packet.dts);


				//av_frame_free(&frame);     //释放结构体
				
			
				
				if (ret < 0)
				{
					av_free_packet(&enc_pkt);
					goto end;
				}
					
				/*如果未成功编码一个AVPacket,退出本次循环，
				* 如果成功编码成一个AVPacket,则继续执行
				*/
				if (!enc_got_frame)
				{
					printf("enc_got_frame=0\n");
					continue;
				}

				/* prepare packet for muxing */
				enc_pkt.stream_index = stream_index;
				enc_pkt.dts = av_rescale_q_rnd(enc_pkt.dts,
					ofmt_ctx->streams[stream_index]->codec->time_base,
					ofmt_ctx->streams[stream_index]->time_base,
					(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				enc_pkt.pts = av_rescale_q_rnd(enc_pkt.pts,
					ofmt_ctx->streams[stream_index]->codec->time_base,
					ofmt_ctx->streams[stream_index]->time_base,
					(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				//enc_pkt.pts = enc_pkt.dts + 1;
				enc_pkt.duration = av_rescale_q(enc_pkt.duration,           //数据的时长，以所属媒体流的时间基准为单位
					ofmt_ctx->streams[stream_index]->codec->time_base,
					ofmt_ctx->streams[stream_index]->time_base);
				//av_log(NULL, AV_LOG_INFO, "Muxing frame %d\n", i);
				/* mux encoded frame */
				av_write_frame(ofmt_ctx, &enc_pkt);                          //输出一帧视音频数据
				av_free_packet(&enc_pkt);
				if (ret < 0)
					goto end;
			}
			else {
			/*	av_frame_free(&frame);*/
			}
			av_free_packet(&packet);
		}
		/* flush encoders */
		for (i = 0; i < 1; i++) {
			/* flush encoder */
			ret = flush_encoder(ofmt_ctx, i);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
				goto end;
			}
		}
		
		av_frame_free(&frame);
		av_frame_free(&newFrame);


		avformat_close_input(&ifmt_ctx);
		sws_freeContext(pSwsCtx);
		av_write_trailer(ofmt_ctx);   //写文件尾
	end:
	/*	if (packet.data)
		av_free_packet(&packet);*/
		if (frame)
		av_frame_free(&frame);
		if (newFrame)
		av_frame_free(&newFrame);
		if (ifmt_ctx)
		avformat_close_input(&ifmt_ctx);
		if (ofmt_ctx)
		avformat_free_context(ofmt_ctx);

		if (pSwsCtx)
		sws_freeContext(pSwsCtx);

		if (ret < 0)
			av_log(NULL, AV_LOG_ERROR, "Error occurred\n");

	}
}  //namespace FFMPEG
