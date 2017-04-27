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

		void init_queue();        //初始化缓冲池

		void free_queue();		  //释放缓冲池中的videoBuffer指向的内存部分，并且重新进行初始化

		void put_queue(const uint8_t* buf, int size);            //想缓冲池中添加数据 

	    int get_queue(uint8_t* buf, int size);

		int getBufSize(){ return bufsize; }				  //返回缓冲池中的数据大小

		int get_overCapcityFlag(){ return overCapcityFlag; }	

		uint8_t* get_videoBuf(){ return videoBuf; };       //存储视频信息的地址

	private:
		uint8_t* videoBuf;
		int bufsize;       //缓冲大小
		int write_ptr;    //写位置
		int read_ptr;	  //读位置
		int capacity;     //缓冲容量
		int overCapcityFlag;
	};

}  //namespace Transcode
