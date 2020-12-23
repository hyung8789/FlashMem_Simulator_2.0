#ifndef _CIRCULAR_QUEUE_H_
#define _CIRCULAR_QUEUE_H_

// Circular_Queue 클래스 템플릿 및 하위 클래스 선언
// Round-Robin 기반의 Wear-leveling을 위한 Empty Block 대기열, Spare Block 대기열 선언
// Victim Block 처리에 효율성을 위한 Victim Block 대기열 선언

struct VICTIM_BLOCK_INFO //FlashMem.cpp
{
	/***
		무효화된 블록의 경우 Dynamic Table 타입에서 매핑 테이블에서 대응되지 않기에 PBN을 저장
		Static Table 타입에서는 대응되어 있지만, 사용하지 않는다. 따라서, 마찬가지로 PBN을 저장
		---
		< is_logical >
		true : victim_block_num이 논리 블록 번호일 경우
		false : victim_block_num이 물리 블록 번호일 경우
	***/

	bool is_logical;
	unsigned int victim_block_num;
	float victim_block_invalid_ratio;

	void clear_all(); //Victim Block 선정을 위한 정보 초기화
}; //Victim Block 선정을 위한 블록 정보 구조체

typedef struct VICTIM_BLOCK_INFO victim_block_element;
typedef unsigned int empty_block_num;
typedef unsigned int spare_block_num;

template <typename data_type, typename element_type> // <큐에 할당 할 자료형, 큐의 요소의 타입>
class Circular_Queue //Circular_queue.hpp
{
public:
	Circular_Queue();
	Circular_Queue(data_type queue_size);
	~Circular_Queue();

	element_type* queue_array; //원형 배열
	data_type front, rear;

	bool is_empty(); //공백 상태 검출
	bool is_full(); //포화 상태 검출
	data_type get_count(); //큐의 요소 개수 반환

protected:
	data_type queue_size; //큐의 할당 크기
};


/*
init에서 Spare 블록 제외한 전체 블록 크기로 할당
bootloader에 의해 블록 스캔 시 빈 블록에 대하여 추가
Static Table의 경우, 쓰기 발생 시 빈 블록 할당 과정이 필요 없으므로 Empty Block Queue를 사용하지 않는다
*/

class Empty_Block_Queue : public Circular_Queue<unsigned int, empty_block_num> //Circular_queue.hpp
{
public:
	inline Empty_Block_Queue(unsigned int queue_size) : Circular_Queue<unsigned int, empty_block_num>(queue_size) {};

	inline void print(); //출력
	inline int enqueue(empty_block_num src_block_num); //Empty Block 순차 할당
	inline int dequeue(empty_block_num& dst_block_num); //Empty Block 가져오기
};

/***
	< Spare Block Table 접근 및 사용 시기 >

	- 블록 매핑 (Static Table, Dynamic Table)
	1) FTL_write 상 Overwrite에 의한 유효 데이터 copy시 Spare Block을 사용
	2) 사용된 Spare Block을 일반 블록으로 설정 및 매핑 테이블 상에서 SWAP

	- 하이브리드 매핑 (Log Algorithm, Dunamic Table)
	1) FTL_write 상 PBN1과 PBN2에 대한 full_merge발생 시 merge과정에서 Spare Block을 사용한 유효 데이터 copy
	2) 사용된 Spare Block을 일반 블록으로 설정 및 매핑 테이블 상에서 SWAP

	- 공통
	모든 물리적 공간이 유효 데이터로 가득 찼을 경우 기존의 유효 데이터에 대해 Overwrite가 발생하면 Overwrite가 발생한
	해당 블록의 기존 데이터들에 대해 Spare Block으로 copy후 새로운 데이터 기록 및 일반 블록으로 설정, 기존 블록은 Erase 후 Spare Block으로 설정 (SWAP)

	- GC Scheduler
	1) Log Algorithm을 적용한 하이브리드 매핑에서 하나의 논리 블록 LBN에 대응된 PBN1과 PBN2에 대해 Merge과정에서
	Spare Block에 유효 데이터 copy 후 먼저 PBN1과 PBN2 모두 Erase 수행된다음, 하나를 Spare Block과 SWAP수행
	=> 이에 따라, Merge가 발생하면, GC에서 Erase 발생하지 않음

	2) 블록 매핑에서 FTL_write과정에 Overwrite에 의한 블록 무효화가 일어난다면, 쓰기 작업의 성능을 위하여
	무효화된 블록은 Victim Block으로 선정 및 Spare Block과 SWAP수행
	=> 이에 따라, GC가 해당 Victim Block을 Erase 수행하여야 함
***/

class Spare_Block_Queue : public Circular_Queue<unsigned int, spare_block_num> //Circular_queue.hpp
{
public:
	inline Spare_Block_Queue(unsigned int queue_size); //초기 생성 시 Round-Robin 기반의 Wear-leveling을 위해 기존 read_index 값 존재 할 경우 재 할당

	inline void print(); //출력
	inline int enqueue(spare_block_num src_block_num); //Spare Block 순차 할당

	/***
		일반 블록과 Spare Block을 SWAP시 전달 한 read_index를 통하여 queue_array에 접근하여 SWAP 수행
		이에 따라 항상 초기 할당 수 만큼의 Spare Block을 가진다.
		---
		모든 Spare Block에 대해 돌아가면서 사용하므로 특정 블록에만 쓰기 작업이 수행되는 것을 방지
	***/
	inline int dequeue(class FlashMem*& flashmem, spare_block_num& dst_block_num, unsigned int& dst_read_index); //빈 Spare Block, 해당 블록의 index 가져오기

private:
	int save_read_index(); //현재 read_index 값 저장
	int load_read_index(); //기존 read_index 값 불러오기
};

class Victim_Block_Queue : public Circular_Queue<unsigned int, victim_block_element> //Circular_queue.hpp
{
public:
	inline Victim_Block_Queue(unsigned int queue_size) : Circular_Queue<unsigned int, victim_block_element>(queue_size) {};

	inline void print(); //출력
	inline int enqueue(victim_block_element src_block_element); //삽입
	inline int dequeue(victim_block_element& dst_block_element); //삭제
};

#include "Circular_Queue.hpp" //Circular_Queue 클래스 템플릿 및 하위 클래스 기능 정의부
#endif