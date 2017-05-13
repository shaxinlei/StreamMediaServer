#include <queue>
#include <mutex>
#include <condition_variable>

#include  "Mona/BinaryReader.h"

namespace Mona
{
	class VideoBuffer
	{
	public:
		BinaryReader& front();             //���ض�����Ԫ��

		void push(BinaryReader& videoPacket);     //�ڶ���β�����Ԫ��

		void pop();									//ɾ������Ԫ��

		int size();                                  //���ض�����Ԫ�ظ���

		bool empty();						//�ж϶����Ƿ�Ϊ��

	private:
		std::queue<BinaryReader> videoQueue;

		std::mutex mut;

		std::condition_variable data_cond;


	};
}
