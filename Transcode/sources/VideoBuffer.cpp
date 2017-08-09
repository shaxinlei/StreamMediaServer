#include "VideoBuffer.h"

namespace Mona
{

	TranscodeRequestQueue::TranscodeRequestQueue()
	{
		MAX_SIZE = 10;
	}


	TranscodeRequest& TranscodeRequestQueue::front()
	{
		return requestQueue.front();
	}

	void TranscodeRequestQueue::push(TranscodeRequest& request)
	{
		std::lock_guard<std::mutex> lk(mut);
		requestQueue.push(request);
		if (size() == 1)
		{
			data_cond.notify_one();
		}
	}

	void TranscodeRequestQueue::pop()
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk,[&]()
		{
			return !requestQueue.empty();
		});
		requestQueue.pop();
		data_cond.notify_one();
		lk.unlock();
	}

	void TranscodeRequestQueue::wait_and_pop(TranscodeRequest& value)
	{
		std::unique_lock<std::mutex> lk(mut);
		data_cond.wait(lk, [this]{return !requestQueue.empty(); });
		value = requestQueue.front();
		requestQueue.pop();
	}

	int TranscodeRequestQueue::size()
	{
		return requestQueue.size();
	}


	bool TranscodeRequestQueue::empty()
	{
		std::lock_guard<std::mutex> lk(mut);
		return requestQueue.empty();
	}

	bool TranscodeRequestQueue::full()
	{
		return (size() >= MAX_SIZE) ? true : false;
	}
}
