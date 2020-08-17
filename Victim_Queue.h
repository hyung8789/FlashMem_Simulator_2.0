#ifndef _VICTIM_QUEUE_H_
#define _VICTIM_QUEUE_H_

#include "FlashMem.h"

// Victim Block Queue 구현을 위한 원형 큐 선언

/*** Build Option ***/
#define VICTIM_BLOCK_QUEUE_RATIO 0.5 //Victim Block 큐의 크기를 생성된 플래시 메모리의 전체 블록 개수에 대한 비율 (50%) 크기로 설정

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
	void print(); //원형큐 출력 함수(debug)
	unsigned int get_count(); //큐에 존재하는 요소의 개수를 반환

	int enqueue(victim_element src_data); //삽입 함수
	int dequeue(victim_element& dst_data); //삭제 함수

private:
	void init(unsigned int block_size); //Victim Block 개수에 따른 큐 초기화
	unsigned int queue_size; //큐의 할당 크기
};
#endif