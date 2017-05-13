#include "VideoBuffer.h"
namespace Mona
{

	VideoBuffer::VideoBuffer()
	{
		bufferSize = 0;
		MAX_SIZE = 10;
	}


	BinaryReader& VideoBuffer::front()
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [&]()
		{
			return !videoQueue.empty();
		});
		return videoQueue.front();
	}

	void VideoBuffer::push(Mona::BinaryReader& videoPacket)
	{
		//std::lock_guard<std::mutex> lk(mut);
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk,[&]{
			return !full();
		});
		videoQueue.push(videoPacket);
		bufferSize += videoPacket.size();
		data_cond.notify_one();
	}

	void VideoBuffer::pop()
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk,[&]()
		{
			return !videoQueue.empty();
		});
		bufferSize -= videoQueue.back().size();
		videoQueue.pop();
		data_cond.notify_one();
		lk.unlock();
	}

	int VideoBuffer::size()
	{
		return videoQueue.size();
	}


	bool VideoBuffer::empty()
	{
		return videoQueue.empty();
	}

	bool VideoBuffer::full()
	{
		return (size() >= MAX_SIZE) ? true : false;
	}


	int VideoBuffer::getBufferSize()
	{
		return bufferSize;
	}
}
