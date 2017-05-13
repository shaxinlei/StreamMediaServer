#include "VideoBuffer.h"
namespace Mona
{

	VideoBuffer::VideoBuffer()
	{
		bufferSize = 0;
	}


	BinaryReader& VideoBuffer::front()
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [&]()
		{
			return !videoQueue.empty();
		});
		return videoQueue.front();
		lk.unlock();
	}

	void VideoBuffer::push(Mona::BinaryReader& videoPacket)
	{
		std::lock_guard<std::mutex> lk(mut);
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

	int VideoBuffer::getBufferSize()
	{
		return bufferSize;
	}
}
