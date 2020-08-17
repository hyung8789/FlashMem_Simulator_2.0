#include "Victim_Queue.h"

// Victim Block Queue 구현을 위한 원형 큐 정의

Victim_Queue::Victim_Queue()
{
	this->queue_array = NULL;
	this->queue_size = 0;
	this->front = this->rear = 0;
}

Victim_Queue::Victim_Queue(unsigned int block_size)
{
	this->queue_array = NULL;
	this->queue_size = 0;
	this->init(block_size);
}

Victim_Queue::~Victim_Queue()
{
	if (this->queue_array != NULL)
	{
		delete[] this->queue_array;
		this->queue_array = NULL;
	}
}

void Victim_Queue::init(unsigned int block_size) //Victim Block 개수에 따른 큐 초기화
{
	if (this->queue_array == NULL)
	{
		this->queue_size = round(block_size * VICTIM_BLOCK_QUEUE_RATIO);
		this->queue_array = new victim_element[queue_size];
	}

	this->front = this->rear = 0;
}

//공백 상태 검출 함수
bool Victim_Queue::is_empty()
{
	return (this->front == this->rear); //front와 rear의 값이 같으면 공백상태
}

//포화 상태 검출 함수
bool Victim_Queue::is_full()
{
	/* front <- (front+1) % MAX_QUEUE_SIZE
	   rear <- (rear+1) % MAX_QUEUE_SIZE
	   MAX_QUEUE_SIZE가 5일경우 front와 rear의 값은 0,1,2,3,4,0과 같이 변화 
	*/

	return ((this->rear + 1) % this->queue_size == this->front); //증가된 위치에서 front를 만날경우 포화상태
}

//원형큐 출력
void Victim_Queue::print()
{
	printf("QUEUE(front = %u rear = %u)\n", this->front, this->rear);
	if (this->is_empty() != true) //큐가 비어있지 않으면
	{
		unsigned int i = this->front;

		do {
			i = (i + 1) % (this->queue_size); //0,1,2,3,4,0과 같이 변화
			
			switch (this->queue_array[i].is_logical)
			{
			case true:
				std::cout << "victim_LBN : ";
				break;

			case false:
				std::cout << "victim_PBN : ";
				break;
			}
			std::cout << this->queue_array[i].victim_block_num << std::endl;
			std::cout << "victim_block_invalid_ratio : " << this->queue_array[i].victim_block_invalid_ratio << std::endl;
			std::cout << "----------------------------------" << std::endl;

			if (i == this->rear) //rear위치까지 도달 후 종료
				break;
		} while (i != this->front); //큐를 한 바퀴 돌때까지
	}
	printf("\n");
	system("pause");
}

//삽입 함수
int Victim_Queue::enqueue(victim_element src_data)
{
	if (this->is_full() == true) //가득 찼으면
		return FAIL;

	//rear의 값을 증가시킨후 데이터를 입력
	this->rear = (this->rear + 1) % this->queue_size;
	this->queue_array[this->rear] = src_data; //copy value
	
	return SUCCESS;
}

//삭제 함수
int Victim_Queue::dequeue(victim_element& dst_data)
{
	if (this->is_empty() == true) //비어있으면
		return FAIL;

	//front의 값을 증가시킨후 front위치의 데이터를 리턴
	this->front = (this->front + 1) % this->queue_size;
	
	dst_data = this->queue_array[this->front]; //dst_PBN으로 값 전달

	return SUCCESS;
}

//큐에 존재하는 요소의 개수를 반환
unsigned int Victim_Queue::get_count()
{
	return (this->rear) - (this->front);
}