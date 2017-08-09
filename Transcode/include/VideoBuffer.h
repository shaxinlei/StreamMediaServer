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

		TranscodeRequest& front();             //���ض�����Ԫ��

		void push(TranscodeRequest& request);     //�ڶ���β�����Ԫ��

		void pop();									//ɾ������Ԫ��

		int size();                                  //���ض�����Ԫ�ظ���

		bool empty();						//�ж϶����Ƿ�Ϊ��

		bool full();

		void wait_and_pop(TranscodeRequest& value);

	private:
		std::queue<TranscodeRequest> requestQueue;           //���flv video tag�Ķ���

		std::mutex mut;                                

		std::condition_variable data_cond;

		int MAX_SIZE;

	};
}
