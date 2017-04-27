#include "Queue.h"
#include "libavutil/avutil.h"
#include "libavutil/mem.h"
#include "libavutil/log.h"
#define VIDEO_BUF_SIZE_MAX 1024*32*10
#define CAPACITY 1024*32

Transcode::VideoQueue::VideoQueue()
{
	init_queue();
}

void Transcode::VideoQueue::init_queue()
{
	videoBuf = (uint8_t*)malloc(sizeof(uint8_t)*VIDEO_BUF_SIZE_MAX);
	read_ptr = write_ptr = 0;
	bufsize = 0;
	capacity = CAPACITY;
	overCapcityFlag = 0;
}


void Transcode::VideoQueue::free_queue() {
	free(videoBuf);
	init_queue();
}

void  Transcode::VideoQueue::put_queue(const uint8_t* buf, int size) {
	uint8_t* dst = videoBuf + write_ptr;

	memcpy(dst, buf, size*sizeof(uint8_t));
	bufsize += size;
	if (bufsize >= capacity)
	{
		overCapcityFlag = 1;
		//av_log(NULL, AV_LOG_INFO, "over capacity!");
	}
	write_ptr += size;
		
}




int  Transcode::VideoQueue::get_queue(uint8_t* buf, int size) {

	memcpy(buf, videoBuf, sizeof(uint8_t)*size);

	return 0;
}