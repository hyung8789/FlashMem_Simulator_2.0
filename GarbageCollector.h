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
	bool RDY_v_flash_info_for_set_invalid_ratio_threshold; //무효율 임계값 설정을 위한 가변적 플래시 메모리 정보 준비 상태
	
	/***
		< GC_lazy_mode >
		연속된 쓰기 작업에 대한 Performance 향상을 위하여
		스케줄러 호출 시 Victim Block 큐가 가득 찰 때까지 아무런 작업을 수행하지 않는다.
		이는 기록 공간이 부족할 시에 항상 기록 공간 확보를 위해 Victim Block이 선정되는 즉시 처리하도록 비활성시킨다.
		---
		true : 사용 (Default)
		false : 사용하지 않음
	***/
	bool GC_lazy_mode;

	void print_invalid_ratio_threshold();
	int scheduler(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //main scheduling function for GC

private: //Execution by scheduling function
	float invalid_ratio_threshold; //Victim Block 선정 위한 블록의 무효율 임계값

	int one_dequeue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //Victim Block 큐로부터 하나의 Victim Block을 빼와서 처리
	int all_dequeue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //Victim Block 큐의 모든 Victim Block을 빼와서 처리
	int enqueue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //Victim Block 큐에 삽입
	void set_invalid_ratio_threshold(class FlashMem*& flashmem); //현재 기록 가능한 스토리지 용량에 따른 가변적 무효율 임계값 설정
};
#endif