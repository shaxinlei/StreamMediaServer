
#include "Decode.h"     
#include <iostream>
#include <winerror.h>
#define BUF_SIZE 32768
using namespace std;

namespace Transcode
{
	Decode::Decode()
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
		ifmt_ctx = avformat_alloc_context();					   //初始化AVFormatContext结构体，主要给结构体分配内存、设置字段默认值
		avformat_alloc_output_context2(&ofmt_ctx, NULL, "h264", NULL);
	}

	int read_buffer(void *opaque, uint8_t *buf, int buf_size)
	{
		//int ret = -1;
		if (opaque != NULL)
		{
			Mona::PacketReader *videoPacket = (Mona::PacketReader *) opaque;
			buf_size = FFMIN(buf_size, videoPacket->available());

			av_log(NULL,AV_LOG_INFO,"buf_size:%i\n",buf_size);
			memcpy(buf, videoPacket->current(), buf_size);
			videoPacket->copyEnd(buf_size);
			return buf_size;
		}
		return -1;
	}

	int write_buffer(void *opaque, uint8_t *buf, int buf_size){
		if (opaque != NULL)
		{
			Mona::Buffer *outBuffer = (Mona::Buffer *) opaque;
			outBuffer->append(buf, buf_size);			
			return buf_size;
		}
		return -1;
	}

	int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index)
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

	Mona::Buffer * Decode::decode(Mona::PacketReader &videoPacket)
	{
		int ret = 0;
		int i = 0;
		unsigned int stream_index;
		int got_frame, enc_got_frame;
		inbuffer = (unsigned char*)av_malloc(BUF_SIZE);            //为输入缓冲区间分配内存
		outbuffer = (unsigned char*)av_malloc(BUF_SIZE);

		avio_in = avio_alloc_context(inbuffer, BUF_SIZE, 0, &videoPacket, read_buffer, NULL, NULL);
		if (avio_in == NULL)
			return 0;

		avio_out = avio_alloc_context(outbuffer, BUF_SIZE, 0, &outVideoBuffer, NULL, write_buffer, NULL);  //初始化输出AVIOContext结构体
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
		
		if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {										//该函数可以读取一部分视音频数据并且获得一些相关的信息
			av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
			goto end;
		}
		av_dump_format(ifmt_ctx, 0, NULL, false);
		printf("***nb_stream%d \n", ifmt_ctx->nb_streams);
		for (i = 0; i < ifmt_ctx->nb_streams; i++) {       //nb_streams为流的数目
			AVStream *stream;
			AVCodecContext *codec_ctx;
			stream = ifmt_ctx->streams[i];
			codec_ctx = stream->codec;                    //codec为指向该视频/音频流的AVCodecContext
			INFO("nb_stream:",i)
			/* Reencode video & audio and remux subtitles etc. */
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
		}
		
		//av_dump_format(ifmt_ctx, 0, "whatever", 0);
		//avio_out->write_packet=write_packet;

		//原本的输出AVFormatContext的指针pb（AVIOContext类型）指向这个自行初始化的输出AVIOContext结构体
		ofmt_ctx->pb = avio_out;
		ofmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
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
				enc_ctx->height = dec_ctx->height;        //如果是视频的话，代表宽和高
				enc_ctx->width = dec_ctx->width;
				enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;     //宽高比
				enc_ctx->pix_fmt = encoder->pix_fmts[0];      //像素格式
				enc_ctx->time_base = dec_ctx->time_base;      //帧时间戳的基本时间单位（以秒为单位）
				//enc_ctx->time_base.num = 1;
				//enc_ctx->time_base.den = 25;
				//H264的必备选项，没有就会错
				enc_ctx->me_range = 16;						  //子单位最大运动估计搜索范围
				enc_ctx->max_qdiff = 4;						//帧之间的最大量化器差异
				enc_ctx->qmin = 10;							//最小量化器
				enc_ctx->qmax = 51;							//最大量化器
				enc_ctx->qcompress = 0.6;
				enc_ctx->refs = 3;					//运动估计参考帧的个数（H.264的话会有多帧，MPEG2这类的一般就没有了）
				enc_ctx->bit_rate = 500000;        //平均比特率

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
		//av_dump_format(ofmt_ctx, 0, "whatever", 1);
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
			printf("Decode 1 Packet\tsize:%d\tpts:%lld\n", packet.size, packet.pts);

			if (ret < 0) {     //解码一帧视频失败
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}
			if (got_frame) {    //帧被解压缩
				frame->pts = av_frame_get_best_effort_timestamp(frame);    //设置显示时间戳
				frame->pict_type = AV_PICTURE_TYPE_NONE;      //图片帧类型

				enc_pkt.data = NULL;     //指向保存压缩数据的指针，这就是AVPacket实际的数据。
				enc_pkt.size = 0;
				av_init_packet(&enc_pkt);   //使用默认值初始化

				//该函数用于编码一帧视频数据,成功编码一个AVPacket时enc_got_frame设置为1
				ret = avcodec_encode_video2(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
					frame, &enc_got_frame);


				printf("Encode 1 Packet\tsize:%d\tpts:%lld\n", enc_pkt.size, enc_pkt.pts);

				av_frame_free(&frame);     //释放结构体
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
				enc_pkt.duration = av_rescale_q(enc_pkt.duration,           //数据的时长，以所属媒体流的时间基准为单位
					ofmt_ctx->streams[stream_index]->codec->time_base,
					ofmt_ctx->streams[stream_index]->time_base);
				av_log(NULL, AV_LOG_INFO, "Muxing frame %d\n", i);
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

		
		av_write_trailer(ofmt_ctx);
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
		

		return &outVideoBuffer;
	}

	void Decode::build_flv_message(char* tagHeader, char* tagEnd, int size)
	{
		int dataSize = size;			 //视频数据长度
		int endSize = dataSize + 11;		//前一个Tag的长度

		unsigned char * p_dataSize = (unsigned char*)&dataSize;

		for (int i = 0; i < 3; i++)
		{
			tagHeader[3 - i] = *(p_dataSize + i);
		}

		unsigned char * p_endSize = (unsigned char*)&endSize;

		for (int i = 0; i < 4; i++)
		{
			tagEnd[3 - i] = *(p_endSize + i);
		}
	}


}  //namespace Transcode
