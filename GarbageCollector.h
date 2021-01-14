#ifndef _GARBAGECOLLECTOR_H_
#define _GARBAGECOLLECTOR_H_

// Garbage Collecter 구현을 위한 클래스 선언

#define SWAP(x,y,temp) ((temp)=(x) ,(x)=(y), (y)=(temp)) //x,y를 교환하는 매크로 정의

class GarbageCollector //GarbageCollector.cpp
{
public:
	GarbageCollector();
	~GarbageCollector();

	bool RDY_terminate; //종료 대기 상태

	int scheduler(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //main scheduling function for GC

private: //Execution by scheduling function
	int one_dequeue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //Victim Block 큐로부터 하나의 Victim Block을 빼와서 처리
	int all_dequeue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //Victim Block 큐의 모든 Victim Block을 빼와서 처리
	int enqueue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //Victim Block 큐에 삽입
};
#endif