#include <queue>
#include <mutex>
#include <condition_variable>

#include  "Mona/BinaryReader.h"

#include "Mona/Logs.h"

namespace Mona
{

	struct TranscodeRequest
	{
		char* in_url;
		char* out_url;
		//int resolutionType; 
		int dstHeight;
		int dstWidth;
		int streamType;
	};

	class TranscodeRequestQueue
	{
	public:
		TranscodeRequestQueue();

		TranscodeRequest& front();             //返回队列首元素

		void push(TranscodeRequest& request);     //在队列尾部添加元素

		void pop();									//删除队首元素

		int size();                                  //返回队列中元素个数

		bool empty();						//判断队列是否为空

		bool full();

		void wait_and_pop(TranscodeRequest& value);

	private:
		std::queue<TranscodeRequest> requestQueue;           //存放flv video tag的队列

		std::mutex mut;                                

		std::condition_variable data_cond;

		int MAX_SIZE;

	};
}
