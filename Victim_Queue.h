#ifndef _VICTIM_QUEUE_H_
#define _VICTIM_QUEUE_H_

#include "FlashMem.h"

// Victim Block Queue 구현을 위한 원형 큐 선언

typedef struct VICTIM_BLOCK_INFO victim_element;

class Victim_Queue
{
public:
	Victim_Queue();
	Victim_Queue(unsigned int block_size);
	~Victim_Queue();
	
	victim_element* queue_array; //Victim Block에 대한 원형 배열
	unsigned int front, rear;

	bool is_empty(); //공백 상태 검출
	bool is_full(); //포화 상태 검출
	void print(); //출력(debug)
	unsigned int get_count(); //큐의 요소 개수 반환

	int enqueue(victim_element src_element); //삽입
	int dequeue(victim_element& dst_element); //삭제

private:
	void init(unsigned int block_size); //Victim Block 개수에 따른 큐 초기화
	unsigned int queue_size; //큐의 할당 크기
};
#endif