#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include "FlashMem.h"

// Garbage Collector에 의한 Victim Block 관리를 위한 원형 버퍼(원형 큐) 정의
/*
* 일단 쓰기가 발생한 블록에 대하여 vq에 무조건 추가하고 gc에 의해 검사해서 
현재 임계값에 따라 처리하도록 하는게 대기열 큐, Victim Block 큐로 이중 관리하는 것보다 좋을것 같다
--
ftl write
일단 작업 발생한 블록에 대해서 무조건 vq에 추갸
이후 end success 단계에서 gc에 의해 추가된 블록들에 대해 무효율 계산 및 현재 임계값에 따라 victim으로 선정 될 필요가 없는 경우 vq에서 제거
*/

typedef struct VICTIM_BLOCK_INFO victim_element;

class Ring_Buffer
{
public:
	Ring_Buffer();
	Ring_Buffer(unsigned int block_size);
	~Ring_Buffer();

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