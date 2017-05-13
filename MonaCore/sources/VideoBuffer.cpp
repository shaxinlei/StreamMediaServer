#include "VideoBuffer.h"
namespace Mona
{
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
		data_cond.notify_one();
	}

	void VideoBuffer::pop()
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk,[&]()
		{
			return !videoQueue.empty();
		});
		videoQueue.pop();
		lk.unlock();
	}

	int VideoBuffer::size()
	{
		videoQueue.size();
	}


	bool VideoBuffer::empty()
	{
		return videoQueue.empty();
	}
}
