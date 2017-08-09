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

		bool full();

		int getBufferSize();				//��ȡ��������Ƶ���ݵĴ�С

		void wait_and_pop(BinaryReader& value);

	private:
		std::queue<BinaryReader> videoQueue;           //���flv video tag�Ķ���

		std::mutex mut;                                

		std::condition_variable data_cond;

		int bufferSize;                     //��������Ƶ���ݵĴ�С

		int MAX_SIZE;

	};
}
