#include "VideoBuffer.h"
#include "Mona/Logs.h"

namespace Mona
{

	VideoBuffer::VideoBuffer()
	{
		bufferSize = 0;
		MAX_SIZE = 10;
	}


	BinaryReader& VideoBuffer::front()
	{
		return videoQueue.front();
	}

	void VideoBuffer::push(BinaryReader& videoPacket)
	{
		std::lock_guard<std::mutex> lk(mut);
		videoQueue.push(videoPacket);
		bufferSize += videoPacket.size();
		if (size() == 1)
		{
			data_cond.notify_one();
		}
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

	void VideoBuffer::wait_and_pop(BinaryReader& value)
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this]{return !videoQueue.empty(); });
		value = videoQueue.front();
		videoQueue.pop();
	}

	int VideoBuffer::size()
	{
		return videoQueue.size();
	}


	bool VideoBuffer::empty()
	{
		std::lock_guard<std::mutex> lk(mut);
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
