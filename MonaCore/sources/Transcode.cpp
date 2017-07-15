
  
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

		av_register_all();											//ע�����б���������������ͽ⸴����
		ifmt_ctx = avformat_alloc_context();					    //��ʼ��AVFormatContext�ṹ�壬��Ҫ���ṹ������ڴ桢�����ֶ�Ĭ��ֵ
		
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

	//Write File  �ص�����
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
		int dataSize = size;			 //��Ƶ���ݳ���
		int endSize = dataSize + 11;		//ǰһ��Tag�ĳ���
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
		//�� sws_getContext���� �õ� sws_scale���� ���е������ģ�֮���� sws_scale���� ��ͼ������
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
		char out_filename[500] = "rtmp://60.205.186.144:1937/live/livestream";
		/*�����Ҫ��ֱ��������Ϊ�ļ�*/
		if (SAVE_AS_FILE_FLAG)    
		{
			fopen_s(&fp_write, "test.h264", "wb+");
			avformat_alloc_output_context2(&ofmt_ctx, NULL, "h264", NULL);
		}
		else
		{
			avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", out_filename);
		}
		
		inbuffer = (unsigned char*)av_malloc(READ_BUF_SIZE);            //Ϊ���뻺����������ڴ�
		outbuffer = (unsigned char*)av_malloc(BUF_SIZE);
		avio_in = avio_alloc_context(inbuffer, READ_BUF_SIZE, 0, this, read_buffer, NULL, NULL);
		if (avio_in == NULL)
			return;

		avio_out = avio_alloc_context(outbuffer, BUF_SIZE, 0, fp_write, NULL, write_buffer, NULL);  //��ʼ�����AVIOContext�ṹ��
		if (avio_out == NULL)
			goto end;

		/*
		*ԭ��������AVFormatContext��ָ��pb��AVIOContext���ͣ�
		*ָ��������г�ʼ��������AVIOContext�ṹ�塣
		*/

		ifmt_ctx->pb = avio_in;     //important

		ifmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;															 //ͨ����־λ˵�������Զ���AVIOContext
		if ((ret = avformat_open_input(&ifmt_ctx, "what", NULL, NULL)) < 0) {							//�򿪶�ý�����ݲ��һ��һЩ��ص���Ϣ������ִ�гɹ��Ļ����䷵��ֵ���ڵ���0
			av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
			goto end;
		}
		AVDictionary* pOptions = NULL;
		ifmt_ctx->probesize = 64* 1024;
		ifmt_ctx->max_analyze_duration = 5 * AV_TIME_BASE;
		if ((ret = avformat_find_stream_info(ifmt_ctx, &pOptions)) < 0) {										//�ú������Զ�ȡһ��������Ƶ���ݲ��һ��һЩ��ص���Ϣ
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
		}

		//av_dump_format(ifmt_ctx, 0, "whatever", 0);
		//avio_out->write_packet=write_packet;

		//ԭ�������AVFormatContext��ָ��pb��AVIOContext���ͣ�ָ��������г�ʼ�������AVIOContext�ṹ��
		if (SAVE_AS_FILE_FLAG)
		{
			ofmt_ctx->pb = avio_out;
			ofmt_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
		}
		else
		{
			needNetwork = 1;
			if (needNetwork) {
				//��Ҫ����������Ƶ
				avformat_network_init();
			}
			//�������ַ��������ַ��
			if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
				ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
				if (ret < 0) {
					printf("�޷��򿪵�ַ '%s'", out_filename);
					goto end;
				}
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
		i = 0;
		/* read all packets */
		while (1) {
			i++;
			/*��ȡ�����е���Ƶ����֡������Ƶһ֡,���磬������Ƶ��ʱ��
			*ÿ����һ����Ƶ֡����Ҫ�ȵ��� av_read_frame()���һ֡��Ƶ
			*��ѹ�����ݣ�Ȼ����ܶԸ����ݽ��н��루����H.264��һ֡ѹ������ͨ����Ӧһ��NAL��*/
			if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
				break;
			if (packet.size > 32768)
			{
				WARN("abandon the packet which size over 32768")
				continue;
			}
				
			stream_index = packet.stream_index;        //Packet����stream��index
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

			INFO("frame type:", frame->pict_type)
			if (ret < 0) {     //����һ֡��Ƶʧ��
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}
			if (got_frame) {    //֡����ѹ��
				frame->pts = flag++;   //������ʾʱ���
				frame->pict_type = AV_PICTURE_TYPE_NONE;      //ͼƬ֡����

				newFrame = av_frame_alloc();
				if (!newFrame){
					return;
				}
				
				resolutionChange(dec_ctx, frame, newFrame, dst_width, dst_height);
				

				newFrame->pts = flag;    //������ʾʱ���
				newFrame->pict_type = AV_PICTURE_TYPE_NONE;      //ͼƬ֡����

				enc_pkt.data = NULL;     //ָ�򱣴�ѹ�����ݵ�ָ�룬�����AVPacketʵ�ʵ����ݡ�
				enc_pkt.size = 0;
				av_init_packet(&enc_pkt);   //ʹ��Ĭ��ֵ��ʼ��

				//�ú������ڱ���һ֡��Ƶ����,�ɹ�����һ��AVPacketʱenc_got_frame����Ϊ1
				ret = avcodec_encode_video2(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
					newFrame, &enc_got_frame);


				//printf("Encode 1 Packet\tsize:%d\tpts:%lld\tdts:%lld\n", enc_pkt.size, enc_pkt.pts,packet.dts);

				av_frame_free(&frame);     //�ͷŽṹ��
				av_frame_free(&newFrame);
				if (ret < 0)
					goto end;
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
				av_write_frame(ofmt_ctx, &enc_pkt);                          //av_write_frame()�������һ֡����Ƶ����
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

		av_write_trailer(ofmt_ctx);   //д�ļ�β
	end:
		av_freep(avio_in);			//�ͷŽṹ��
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
