#pragma once
#include <iostream>
#include <stdio.h>
#include <cstdint>

namespace  Transcode
{
	class VideoQueue
	{
	public:
		VideoQueue();

		void init_queue();        //��ʼ�������

		void free_queue();		  //�ͷŻ�����е�videoBufferָ����ڴ沿�֣��������½��г�ʼ��

		void put_queue(const uint8_t* buf, int size);            //�뻺������������ 

	    int get_queue(uint8_t* buf, int size);

		int getBufSize(){ return bufsize; }				  //���ػ�����е����ݴ�С

		int get_overCapcityFlag(){ return overCapcityFlag; }	

		uint8_t* get_videoBuf(){ return videoBuf; };       //�洢��Ƶ��Ϣ�ĵ�ַ

	private:
		uint8_t* videoBuf;
		int bufsize;       //�����С
		int write_ptr;    //дλ��
		int read_ptr;	  //��λ��
		int capacity;     //��������
		int overCapcityFlag;
	};

}  //namespace Transcode
