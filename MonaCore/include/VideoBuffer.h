#include <queue>
#include <mutex>
#include <condition_variable>

#include  "Mona/BinaryReader.h"

namespace Mona
{
	class VideoBuffer
	{
	public:
		VideoBuffer();

		BinaryReader& front();             //���ض�����Ԫ��

		void push(BinaryReader& videoPacket);     //�ڶ���β�����Ԫ��

		void pop();									//ɾ������Ԫ��

		int size();                                  //���ض�����Ԫ�ظ���

		bool empty();						//�ж϶����Ƿ�Ϊ��

		int getBufferSize();				//��������Ƶ���ݵĴ�С

	private:
		std::queue<BinaryReader> videoQueue;

		std::mutex mut;

		std::condition_variable data_cond;

		int bufferSize;

	};
}
