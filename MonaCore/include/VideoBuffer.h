#include <queue>
#include <mutex>
#include <condition_variable>

#include  "Mona/BinaryReader.h"

namespace Mona
{
	class VideoBuffer
	{
	public:
		BinaryReader& front();             //返回队列首元素

		void push(BinaryReader& videoPacket);     //在队列尾部添加元素

		void pop();									//删除队首元素

		int size();                                  //返回队列中元素个数

		bool empty();						//判断队列是否为空

	private:
		std::queue<BinaryReader> videoQueue;

		std::mutex mut;

		std::condition_variable data_cond;


	};
}
