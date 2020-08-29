#ifndef _VICTIM_QUEUE_H_
#define _VICTIM_QUEUE_H_

#include "FlashMem.h"

// Victim Block Queue 구현을 위한 원형 큐 선언

/*** Build Option ***/
/***
	Victim Block Queue의 크기와 Spare Block 개수를 다르게 할 경우 Round-Robin Based Wear-leveling을 위한
	선정된 Victim Block들과 Spare Block 개수 크기의 Spare Block Table을 통한 SWAP 발생 시에
	Spare Block Table의 모든 Spare Block들이 GC에 의해 처리되지 않았을 경우, GC에 의해 강제로 처리하도록 해야 함
	=> 구현생략
	---
	Victim Block Queue의 크기와 Spare Block 개수를 같게 함으로서, Victim Block Queue의 상태에 따라 처리를 수행
***/
#define VICTIM_BLOCK_QUEUE_RATIO 0.08 //Victim Block 큐의 크기를 생성된 플래시 메모리의 전체 블록 개수에 대한 비율 크기로 설정

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