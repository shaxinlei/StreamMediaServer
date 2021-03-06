
  
#include <iostream>

#include "Mona/Transcode.h"
#include "Mona/Logs.h"
#define READ_BUF_SIZE 32768*4
#define BUF_SIZE 32768
#define SAVE_AS_FILE_FLAG 0

using namespace std;

namespace Mona
{
	Transcode::Transcode() :Startable("Transcode"), _publication(NULL), flag(0), fp_write(NULL), needNetwork(0), newFrame(NULL)
	{
		avio_in = NULL;
		avio_out = NULL;
		inbuffer = NULL;
		outbuffer = NULL;
		frame = NULL;
		dec_ctx = NULL;
		piFmt = NULL;


		ofmt_ctx = NULL;
		frame = NULL;
		out_stream = NULL;
		in_stream = NULL;
		dec_ctx = NULL;
		enc_ctx = NULL;
		encoder = NULL;

		av_register_all();											//注册所有编解码器，复用器和解复用器
		ifmt_ctx = avformat_alloc_context();					    //初始化AVFormatContext结构体，主要给结构体分配内存、设置字段默认值
		
	}
	Transcode::~Transcode(){}

	int read_buffer(void *opaque, uint8_t *buf, int buf_size)
	{
		int flag = ((Transcode *)opaque)->flag;
		int tureSize = 0;
		tureSize = ((Transcode *)opaque)->getVideoPacket(flag, buf, buf_size);
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

	void Transcode::build_flv_message(char* tagHeader, char* tagEnd, int size, Mona::UInt32 timeStamp)
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

	int Transcode::pushVideoPacket(BinaryReader& videoPacket)
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

	int Transcode::getVideoPacket(int& flag, uint8_t* buf, int& buf_size)
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

	int Transcode::flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index)
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

	void Transcode::resolutionChange(AVCodecContext *pCodecCtx, AVFrame *pFrame, AVFrame *pNewFrame, int pNewWidth, int pNewHeight)
	{
		int ret = av_image_alloc(pNewFrame->data, pNewFrame->linesize, pNewWidth, pNewHeight, AV_PIX_FMT_YUV420P, 1);
		if (ret< 0) {
			printf("Could not allocate destination image\n");
			return ;
		}
		//用 sws_getContext函数 得到 sws_scale函数 运行的上下文，之后用 sws_scale函数 将图形缩放
		SwsContext *pSwsCtx = NULL;
		pSwsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, pNewWidth, pNewHeight, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
		if (pSwsCtx == NULL)
			return;
		sws_scale(pSwsCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pNewFrame->data, pNewFrame->linesize);
		sws_freeContext(pSwsCtx);
	}

	void Transcode::setEncodeParamter(int _dst_width, int _dst_height, int _bitRate)
	{
		dst_width = _dst_width;
		dst_height = _dst_height;
		bitRate = _bitRate;
	}

	void Transcode::run(Exception& ex)
	{
		int ret = 0;
		int i = 0;
		unsigned int stream_index;
		int got_frame, enc_got_frame;
		char out_filename[500] = "rtmp://127.0.0.1:1937/live/livestream";
		/*如果需要将直播流保存为文件*/
		if (SAVE_AS_FILE_FLAG)    
		{
			fopen_s(&fp_write, "test.h264", "wb+");
			avformat_alloc_output_context2(&ofmt_ctx, NULL, "h264", NULL);
		}
		else
		{
			avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);
		}
		
		inbuffer = (unsigned char*)av_malloc(READ_BUF_SIZE);            //为输入缓冲区间分配内存
		outbuffer = (unsigned char*)av_malloc(BUF_SIZE);
		avio_in = avio_alloc_context(inbuffer, READ_BUF_SIZE, 0, this, read_buffer, NULL, NULL);
		if (avio_in == NULL)
			return;

		avio_out = avio_alloc_context(outbuffer, BUF_SIZE, 0, fp_write, NULL, write_buffer, NULL);  //初始化输出AVIOContext结构体
		if (avio_out == NULL)
			goto end;

		/*
		*原本的输入AVFormatContext的指针pb（AVIOContext类型）
		*指向这个自行初始化的输入AVIOContext结构体。
		*/

		ifmt_ctx->pb = avio_in;     //important

		ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;															 //通过标志位说明采用自定义AVIOContext
		if ((ret = avformat_open_input(&ifmt_ctx, "what", NULL, NULL)) < 0) {							//打开多媒体数据并且获得一些相关的信息，函数执行成功的话，其返回值大于等于0
			av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			goto end;
		}
		AVDictionary* pOptions = NULL;
		ifmt_ctx->probesize = 64* 1024;
		ifmt_ctx->max_analyze_duration = 5 * AV_TIME_BASE;
		if ((ret = avformat_find_stream_info(ifmt_ctx, &pOptions)) < 0) {										//该函数可以读取一部分视音频数据并且获得一些相关的信息
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
				av_log(NULL, AV_LOG_INFO, "stream %d is video stream\n",i);
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//该函数用于初始化一个视音频编解码器的AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //根据解码器的ID查找解码器，找到就返回查找到的解码器
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
					goto end;
				}
			}
			if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				av_log(NULL, AV_LOG_INFO, "stream %d is audio stream\n", i);
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//该函数用于初始化一个视音频编解码器的AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //根据解码器的ID查找解码器，找到就返回查找到的解码器
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
					goto end;
				}
			}
				
		}

		//av_dump_format(ifmt_ctx, 0, "whatever", 0);
		//avio_out->write_packet=write_packet;

		//原本的输出AVFormatContext的指针pb（AVIOContext类型）指向这个自行初始化的输出AVIOContext结构体
		if (SAVE_AS_FILE_FLAG)
		{
			ofmt_ctx->pb = avio_out;
			ofmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
		}
		else
		{
			needNetwork = 1;
			if (needNetwork) {
				//需要播放网络视频
				avformat_network_init();
			}
			//打开输出地址（推流地址）
			if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
				ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
				if (ret < 0) {
					printf("无法打开地址 '%s'", out_filename);
					goto end;
				}
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
		i = 0;
		/* read all packets */
		while (1) {
			i++;
			/*读取码流中的音频若干帧或者视频一帧,例如，解码视频的时候，
			*每解码一个视频帧，需要先调用 av_read_frame()获得一帧视频
			*的压缩数据，然后才能对该数据进行解码（例如H.264中一帧压缩数据通常对应一个NAL）*/
			if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
				break;
			if (packet.size > 32768)
			{
				WARN("abandon the packet which size over 32768")
				continue;
			}
				
			stream_index = packet.stream_index;        //Packet所在stream的index
			if (stream_index != 0)
				continue;
			type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
			av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
				stream_index);

			av_log(NULL, AV_LOG_DEBUG, "Going to reencode the frame\n");
			frame = av_frame_alloc();
			if (!frame) {
				ret = AVERROR(ENOMEM);
				break;
			}
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

			INFO("frame type:", frame->pict_type)
			if (ret < 0) {     //解码一帧视频失败
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}
			if (got_frame) {    //帧被解压缩
				frame->pts = flag++;   //设置显示时间戳
				frame->pict_type = AV_PICTURE_TYPE_NONE;      //图片帧类型

				newFrame = av_frame_alloc();
				if (!newFrame){
					return;
				}
				
				resolutionChange(dec_ctx, frame, newFrame, dst_width, dst_height);
				

				newFrame->pts = flag;    //设置显示时间戳
				newFrame->pict_type = AV_PICTURE_TYPE_NONE;      //图片帧类型

				enc_pkt.data = NULL;     //指向保存压缩数据的指针，这就是AVPacket实际的数据。
				enc_pkt.size = 0;
				av_init_packet(&enc_pkt);   //使用默认值初始化

				//该函数用于编码一帧视频数据,成功编码一个AVPacket时enc_got_frame设置为1
				ret = avcodec_encode_video2(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
					newFrame, &enc_got_frame);


				//printf("Encode 1 Packet\tsize:%d\tpts:%lld\tdts:%lld\n", enc_pkt.size, enc_pkt.pts,packet.dts);

				av_frame_free(&frame);     //释放结构体
				av_frame_free(&newFrame);
				if (ret < 0)
					goto end;
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
				av_write_frame(ofmt_ctx, &enc_pkt);                          //av_write_frame()用于输出一帧视音频数据
				if (ret < 0)
					goto end;
			}
			else {
				av_frame_free(&frame);
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

		av_write_trailer(ofmt_ctx);   //写文件尾
	end:
		av_freep(avio_in);			//释放结构体
		av_freep(avio_out);
		av_free(inbuffer);
		av_free(outbuffer);
		av_free_packet(&packet);
		av_frame_free(&frame);
		avformat_close_input(&ifmt_ctx);
		avformat_free_context(ofmt_ctx);

		if (ret < 0)
			av_log(NULL, AV_LOG_ERROR, "Error occurred\n");

	}
}  //namespace FFMPEG
