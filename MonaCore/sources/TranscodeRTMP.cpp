#include "Mona/TranscodeRTMP.h"
#include "Mona/Logs.h"


namespace  Mona
{
	TranscodeRTMP::TranscodeRTMP() :Startable("TranscodeRTMP")
	{
		av_register_all();
		avformat_network_init();
		in_fmt_ctx = NULL;
		out_fmt_ctx = NULL;
		width = 800;
		height = 600;
		video_index = -1;
		pts = -3600;
		dts = -3600;
		i = 0;
		frame_index = 0;
	}


	void TranscodeRTMP::run(Exception &ex)
	{
		int ret = 0;
		if (transStart() < 0)
		{
			DEBUG(" Init Failed")
		}
		else
		{
			while (1)
			{
				if (decode() >= 0)
					ret = encode();
				if (ret < 0)
					unit();
			}
			transEnd();
			
		}
		
	}

	/*�����ȡ�ļ��ĵ�ַ�����Ƕ�ȡ����rtsp����ַ����֮��Ľ���һ���޸ľͿ���ת����ͨ��ʽ*/
	int TranscodeRTMP::inputFile(char *file)
	{
		rtmp_url = file;
		return 0;
	}

	/*����ļ���*/
	int TranscodeRTMP::outputFile(char *file)
	{
		name_path = file;
		return 0;
	}

	void TranscodeRTMP::setEncodeParamter(int _dst_width, int _dst_height, int _bitRate)
	{
		width = _dst_width;
		height = _dst_height;
		bitRate = _bitRate;
	}

	/*FFmpeg�����ע����һЩ��ʼ������*/
	int TranscodeRTMP::init()
	{
		av_register_all();
		avformat_network_init();
		in_fmt_ctx = NULL;
		out_fmt_ctx = NULL;
		width = 800;
		height = 600;
		video_index = -1;
		pts = -3600;
		dts = -3600;
		i = 0;
		frame_index = 0;
		return 0;
	}

	/*׼����һЩ�������趨*/
	int TranscodeRTMP::prepare()
	{
		int ret = 0;
		avformat_network_init();            //��Ҫ����������Ƶ��
		if (ret = avformat_open_input(&in_fmt_ctx, rtmp_url, NULL, NULL) < 0) {
			printf("�޷����ļ�\n");
			return ret;
		}
		if (ret = avformat_find_stream_info(in_fmt_ctx, 0) < 0) {
			printf("�޷��ҵ�����Ϣ\n");
			return ret;
		}
		av_dump_format(in_fmt_ctx, 0, rtmp_url, 0);
		for (unsigned i = 0; i < in_fmt_ctx->nb_streams; i++) {
			if (in_fmt_ctx->streams[i]->codec->coder_type == AVMEDIA_TYPE_VIDEO) {
				in_video_stream = in_fmt_ctx->streams[i];
				video_index = i;
				break;
			}
		}
		/*Ѱ����򿪽�����*/
		if (0 > avcodec_open2(in_video_stream->codec, avcodec_find_decoder(in_video_stream->codec->codec_id), NULL)) {
			printf("�޷��ҵ�������\n");
		}
		/*��������ļ�*/
		avformat_alloc_output_context2(&out_fmt_ctx, NULL, "flv", name_path);
		if (!out_fmt_ctx) {
			printf("���ܴ�������ļ�\n");
			ret = AVERROR_UNKNOWN;
			return ret;
		}
		/*���������*/
		out_video_stream = avformat_new_stream(out_fmt_ctx, NULL);
		if (!out_video_stream) {
			printf("�޷���������\n");
			ret = AVERROR_UNKNOWN;
			return ret;
		}
		/*�������á���ʼ*/
		out_video_stream->codec->codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		out_video_stream->codec->height = height;
		out_video_stream->codec->width = width;
		out_video_stream->codec->time_base.num = in_fmt_ctx->streams[i]->avg_frame_rate.den;
		out_video_stream->codec->time_base.den = in_fmt_ctx->streams[i]->avg_frame_rate.num;
		out_video_stream->codec->sample_aspect_ratio = in_video_stream->codec->sample_aspect_ratio;
		out_video_stream->codec->pix_fmt = in_video_stream->codec->pix_fmt;
		out_video_stream->codec->pix_fmt = out_video_stream->codec->codec->pix_fmts[0];
		out_video_stream->codec->bit_rate = bitRate;
		out_video_stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
		/*��������������ļ��޸�*/
		out_video_stream->codec->thread_count = 6;
		out_video_stream->codec->max_b_frames = 3;
		out_video_stream->codec->b_frame_strategy = 1;
		out_video_stream->codec->gop_size = 10;
		out_video_stream->codec->keyint_min = 25;
		out_video_stream->codec->trellis = 1;
		out_video_stream->codec->me_subpel_quality = 7;
		out_video_stream->codec->refs = 3;
		out_video_stream->codec->me_method = ME_HEX;
		out_video_stream->codec->coder_type = FF_CODER_TYPE_AC;
		out_video_stream->codec->me_range = 16;
		out_video_stream->codec->max_qdiff = 4;
		out_video_stream->codec->qmin = 0;
		out_video_stream->codec->qmax = 69;
		out_video_stream->codec->qcompress = 0.6;
		out_video_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		/*�������á�����*/
		/*Ѱ�ұ�����*/
		if (!out_video_stream->codec->codec) {
			printf("�Ҳ���������\n");
			ret = AVERROR_UNKNOWN;
			return ret;
		}

		/*�򿪱�����*/
		if ((avcodec_open2(out_video_stream->codec, out_video_stream->codec->codec, NULL)) < 0) {
			printf("�޷��򿪱�����\n");
			ret = AVERROR_UNKNOWN;
			return ret;
		}
		if (video_index == -1) {
			printf("û�ҵ���Ƶ��\n");
			ret = AVERROR_UNKNOWN;
			return ret;
		}

		old_frame = av_frame_alloc();

		frame = av_frame_alloc();
		int size = avpicture_get_size(out_video_stream->codec->pix_fmt, width, height);
		uint8_t* frame_buf = (uint8_t *)av_malloc(size);
		avpicture_fill((AVPicture *)frame, frame_buf, out_video_stream->codec->pix_fmt, width, height);
		frame->format = AV_PIX_FMT_YUV420P;
		frame->width = width;
		frame->height = height;

		frame->linesize[0] = width;
		frame->linesize[1] = width / 2;
		frame->linesize[2] = width / 2;

		int y_size = out_video_stream->codec->width * out_video_stream->codec->height;
		pkt_in = (AVPacket *)av_malloc(sizeof(AVPacket));
		av_new_packet(pkt_in, y_size);

		return 0;
	}

	/*ת��֮ǰ��׼����дͷ�ļ���*/
	int TranscodeRTMP::transStart()
	{
		
		prepare();
		int ret = avio_open(&out_fmt_ctx->pb, name_path, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf("�޷��򿪵�ַ '%s'", name_path);
		}
		avformat_write_header(out_fmt_ctx, NULL);
		pSwsCtx = sws_getContext(in_fmt_ctx->streams[video_index]->codec->width, in_fmt_ctx->streams[video_index]->codec->height, AV_PIX_FMT_YUV420P, width, height, AV_PIX_FMT_YUV420P, SWS_SINC, NULL, NULL, NULL);
		return ret;
	}

	/*����һ֡*/
	int TranscodeRTMP::decode()
	{
		int ret = 0;
		got_frame = -1;
		if (av_read_frame(in_fmt_ctx, pkt_in) < 0) {
			av_frame_free(&frame);
			av_frame_free(&old_frame);
			ret = AVERROR_UNKNOWN;
			av_free_packet(pkt_in);
			return ret;
		}

		if (pkt_in->stream_index == video_index) {
			if (avcodec_decode_video2(in_fmt_ctx->streams[video_index]->codec, old_frame, &got_frame, pkt_in) < 0) {
				av_frame_free(&frame);
				av_frame_free(&old_frame);
				av_free_packet(pkt_in);
				printf("�޷�����\n");
				ret = AVERROR_UNKNOWN;
				return ret;
			}
		}
		printf("got_frame:%d\n", got_frame);
		av_free_packet(pkt_in);
		if (got_frame > 0) {
			ResolutionChange(in_fmt_ctx->streams[video_index]->codec, old_frame, frame, width, height);
		}
		av_frame_free(&old_frame);
		return ret;
	}

	/*����һ֡*/
	int TranscodeRTMP::encode()
	{
		av_init_packet(&pkt_out);
		int ret = 0;
		got_picture = -1;
		if (got_frame > 0) {
			frame->pts = i++;
			pkt_out.data = NULL;
			pkt_out.size = 0;
			if (avcodec_encode_video2(out_video_stream->codec, &pkt_out, frame, &got_picture) < 0) {
				av_free_packet(&pkt_out);
				av_frame_free(&frame);
				printf("�޷�����\n");
				ret = AVERROR_UNKNOWN;
				return ret;
			}
			if (got_picture > 0) {
				printf("�ɹ����� %5d\t��С:%5d\n", frame_index, pkt_out.size);          
				printf("dts:%10d\tpts:%6d\n", pkt_out.dts, pkt_out.pts);
				frame_index++;
				av_write_frame(out_fmt_ctx, &pkt_out);
				av_free_packet(&pkt_out);
			}
		}
		av_free_packet(&pkt_out);
		av_frame_free(&frame);
		return ret;
	}

	/*ж�غ������ͷ��ڴ�*/
	int TranscodeRTMP::unit()
	{
		if (out_fmt_ctx && !(out_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
			avio_close(out_fmt_ctx->pb);
		}
		avcodec_close(out_fmt_ctx->streams[0]->codec);
		av_freep(&out_fmt_ctx->streams[0]->codec);
		av_freep(&out_fmt_ctx->streams[0]);
		avformat_free_context(in_fmt_ctx);
		avformat_free_context(out_fmt_ctx);
		sws_freeContext(pSwsCtx);
		return 0;
	}

	/*�ͷ��ڴ��ڵ�֡����дβ�ļ�*/
	int TranscodeRTMP::transEnd()
	{
		int ret = flush_encoder(out_fmt_ctx, video_index);
		if (ret < 0) {
			printf("�ͷ�ʧ��\n");
			return -1;
		}
		avformat_close_input(&in_fmt_ctx);
		av_write_trailer(out_fmt_ctx);
		unit();
		return 0;
	}

	int TranscodeRTMP::flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index)
	{
		int ret;
		int got_frame;
		AVPacket enc_pkt;
		if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &CODEC_CAP_DELAY)) {
			return 0;
		}
		while (1) {
			enc_pkt.data = NULL;
			enc_pkt.size = 0;
			av_init_packet(&enc_pkt);
			ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);
			av_frame_free(NULL);
			if (ret < 0) {
				break;
			}
			if (!got_frame) {
				ret = 0;
				break;
			}
			printf("�ͷű�������: �ɹ����� 1 ֡!\tsize:%5d\n", enc_pkt.size);
			ret = av_write_frame(fmt_ctx, &enc_pkt);
			if (ret < 0)
				break;
		}
		return ret;
	}

	void TranscodeRTMP::ResolutionChange(AVCodecContext *pCodecCtx, AVFrame *pFrame, AVFrame *pNewFrame, int pNewWidth, int pNewHeight)
	{
		if (pSwsCtx == NULL)
			return;
		sws_scale(pSwsCtx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pNewFrame->data, pNewFrame->linesize);
	}
}
