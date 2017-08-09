

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
		av_register_all();											//ע�����б���������������ͽ⸴����
		int  dst_width = _dst_width;
		int dst_height = _dst_height;
		int  bitRate = _bitRate;
		char *  rtmp_in_url = _rtmp_in_url;
		char * rtmp_out_url = _rtmp_out_url;



		AVFormatContext* ifmt_ctx=NULL;		//AVFormatContext:ͳ��ȫ�ֵĻ����ṹ�塣��Ҫ���ڴ����װ��ʽ��FLV/MK/RMVB��
		AVInputFormat *piFmt=NULL;

		AVFormatContext* ofmt_ctx=NULL;
		AVPacket packet, enc_pkt;          //�洢ѹ�����ݣ���Ƶ��ӦH.264���������ݣ���Ƶ��ӦPCM�������ݣ�
		AVFrame *frame=NULL;            //AVPacket�洢��ѹ�������ݣ���Ƶ��ӦRGB/YUV�������ݣ���Ƶ��ӦPCM�������ݣ�
		enum AVMediaType type;				//ָ�������ͣ�����Ƶ����Ƶ��������Ļ

		AVStream *out_stream=NULL;             //AVStream��AVCodecContext������Ƶ����Ӧ�Ľṹ�壬��������Ƶ����롣
		AVStream *in_stream=NULL;
		AVCodecContext *dec_ctx=NULL, *enc_ctx=NULL;
		AVCodec *encoder=NULL;                 //AVCodec�Ǵ洢���������Ϣ�Ľṹ�壬enconder�洢������Ϣ�Ľṹ��

		SwsContext *pSwsCtx=NULL;

		int flag=0;

		AVFrame* newFrame=NULL;

		
		int ret = 0;
		int i = 0;
		unsigned int stream_index;
		int got_frame, enc_got_frame;

		avformat_network_init();            //��Ҫ����������Ƶ��

		ifmt_ctx = avformat_alloc_context();															//��ʼ��AVFormatContext�ṹ��
		avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", rtmp_out_url);



		if ((ret = avformat_open_input(&ifmt_ctx, rtmp_in_url, NULL, NULL)) < 0) {							//�򿪶�ý�����ݲ��һ��һЩ��ص���Ϣ������ִ�гɹ��Ļ����䷵��ֵ���ڵ���0
			av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			goto end;
		}
	
		if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {										//�ú������Զ�ȡһ��������Ƶ���ݲ��һ��һЩ��ص���Ϣ
			av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
			goto end;
		}
		av_dump_format(ifmt_ctx, 0, NULL, false);
		//printf("***nb_stream%d \n", ifmt_ctx->nb_streams);
		for (i = 0; i < ifmt_ctx->nb_streams; i++) {       //nb_streamsΪ������Ŀ
			AVStream *stream;
			AVCodecContext *codec_ctx;
			stream = ifmt_ctx->streams[i];
			codec_ctx = stream->codec;                    //codecΪָ�����Ƶ/��Ƶ����AVCodecContext
			//Reencode video & audio and remux subtitles etc. */
			if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){       //������������ͣ���Ƶ����Ƶ��  
				printf("video stream\n");
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//�ú������ڳ�ʼ��һ������Ƶ���������AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //���ݽ�������ID���ҽ��������ҵ��ͷ��ز��ҵ��Ľ�����
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
					goto end;
				}
			}
			if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
				INFO("audio stream:")
				/* Open decoder */
				ret = avcodec_open2(codec_ctx,						//�ú������ڳ�ʼ��һ������Ƶ���������AVCodecContext
					avcodec_find_decoder(codec_ctx->codec_id), NULL);    //���ݽ�������ID���ҽ��������ҵ��ͷ��ز��ҵ��Ľ�����
				if (ret < 0) {
					av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
					goto end;
				}
		}

			//�������ַ��������ַ��
		if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
			ret = avio_open(&ofmt_ctx->pb, rtmp_out_url, AVIO_FLAG_WRITE);
			if (ret < 0) {
				printf("�޷��򿪵�ַ '%s'", rtmp_out_url);
				goto end;
			}
		}	
		
		for (i = 0; i < 1; i++) {
			out_stream = avformat_new_stream(ofmt_ctx, NULL);              //��ʼ��AVStream�ṹ�壨�����ڴ棬����Ĭ��ֵ�����������������ṹ��
			if (!out_stream) {
				av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
				goto end;
			}
			in_stream = ifmt_ctx->streams[i];
			dec_ctx = in_stream->codec;
			enc_ctx = out_stream->codec;
			if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				encoder = avcodec_find_encoder(AV_CODEC_ID_H264);    //����AV_CODEC_ID_H264������
				enc_ctx->height = dst_height;        //�������Ƶ�Ļ��������͸�
				enc_ctx->width = dst_width;
				enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;     //��߱�
				//enc_ctx->sample_aspect_ratio.num = dst_width;
				//enc_ctx->sample_aspect_ratio.num = dst_height;
				enc_ctx->pix_fmt = encoder->pix_fmts[0];      //���ظ�ʽ
				enc_ctx->time_base = dec_ctx->time_base;      //֡ʱ����Ļ���ʱ�䵥λ������Ϊ��λ��
				//enc_ctx->time_base.num = 1;
				//enc_ctx->time_base.den = 25;
				//H264�ıر�ѡ�û�оͻ��
				enc_ctx->me_range = 16;						  //�ӵ�λ����˶�����������Χ
				enc_ctx->max_qdiff = 4;						//֮֡����������������
				enc_ctx->qmin = 10;							//��С������
				enc_ctx->qmax = 51;							//���������
				enc_ctx->gop_size = 15;
				enc_ctx->qcompress = 0.6;
				enc_ctx->refs = 3;					//�˶����Ʋο�֡�ĸ�����H.264�Ļ����ж�֡��MPEG2�����һ���û���ˣ�
				enc_ctx->bit_rate = bitRate;        //ƽ��������

				ret = avcodec_open2(enc_ctx, encoder, NULL);			//��ʼ�����򿪱�����
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
		av_opt_set(enc_ctx->priv_data, "tune", "zerolatency", 0);    //����ʵʱ����

		/* init muxer, write output file header */
		ret = avformat_write_header(ofmt_ctx, NULL);      //д��Ƶ�ļ�ͷ
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
			//��ȡ�����е���Ƶ����֡������Ƶһ֡
			if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
				break;

			stream_index = packet.stream_index;        //Packet����stream��index
			if (stream_index != 0)
				continue;
			type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
			av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
				stream_index);

			av_log(NULL, AV_LOG_DEBUG, "Going to reencode the frame\n");
			
			packet.dts = av_rescale_q_rnd(packet.dts,                      //����ʱ���        av_rescale_q_rn����ת��ʱ���
				ifmt_ctx->streams[stream_index]->time_base,
				ifmt_ctx->streams[stream_index]->codec->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

			packet.pts = av_rescale_q_rnd(packet.pts,                      //��ʾʱ���
				ifmt_ctx->streams[stream_index]->time_base,
				ifmt_ctx->streams[stream_index]->codec->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

			//����һ֡��Ƶ���ݡ�����һ��ѹ������Ľṹ��AVPacket�����һ�������Ľṹ��AVFrame
			ret = avcodec_decode_video2(ifmt_ctx->streams[stream_index]->codec, frame,
				&got_frame, &packet);       //���û��֡���Խ�ѹ����got_frameΪ�㣬���򣬷���

			//printf("Decode 1 Packet\tsize:%d\tpts:%lld\tdts:%lld\n", packet.size, packet.pts,packet.dts);

			//INFO("frame type:", frame->pict_type)
			if (ret < 0) {     //����һ֡��Ƶʧ��
			/*	av_frame_free(&frame);*/
				av_free_packet(&packet);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				continue;
			}
			if (got_frame) {    //֡����ѹ��
				frame->pts = av_frame_get_best_effort_timestamp(frame);   //������ʾʱ���
				frame->pict_type = AV_PICTURE_TYPE_NONE;      //ͼƬ֡����

			

				sws_scale(pSwsCtx, frame->data, frame->linesize, 0, dec_ctx->height, newFrame->data, newFrame->linesize);

				//av_frame_free(&frame);     //�ͷŽṹ��

				newFrame->pts = flag++;    //������ʾʱ���
				//newFrame->pict_type = AV_PICTURE_TYPE_NONE;      //ͼƬ֡����

				enc_pkt.data = NULL;     //ָ�򱣴�ѹ�����ݵ�ָ�룬�����AVPacketʵ�ʵ����ݡ�
				enc_pkt.size = 0;
				av_init_packet(&enc_pkt);   //ʹ��Ĭ��ֵ��ʼ��

				//����һ֡��Ƶ����,�ɹ�����һ��AVPacketʱenc_got_frame����Ϊ1
				ret = avcodec_encode_video2(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
					newFrame, &enc_got_frame);


				//printf("Encode 1 Packet\tsize:%d\tpts:%lld\tdts:%lld\n", enc_pkt.size, enc_pkt.pts,packet.dts);


				//av_frame_free(&frame);     //�ͷŽṹ��
				
			
				
				if (ret < 0)
				{
					av_free_packet(&enc_pkt);
					goto end;
				}
					
				/*���δ�ɹ�����һ��AVPacket,�˳�����ѭ����
				* ����ɹ������һ��AVPacket,�����ִ��
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
				enc_pkt.duration = av_rescale_q(enc_pkt.duration,           //���ݵ�ʱ����������ý������ʱ���׼Ϊ��λ
					ofmt_ctx->streams[stream_index]->codec->time_base,
					ofmt_ctx->streams[stream_index]->time_base);
				//av_log(NULL, AV_LOG_INFO, "Muxing frame %d\n", i);
				/* mux encoded frame */
				av_write_frame(ofmt_ctx, &enc_pkt);                          //���һ֡����Ƶ����
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
		av_write_trailer(ofmt_ctx);   //д�ļ�β
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
