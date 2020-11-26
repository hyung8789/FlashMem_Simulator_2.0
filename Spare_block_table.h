#ifndef _SPARE_BLOCK_TABLE_H_
#define _SPARE_BLOCK_TABLE_H_

#include "FlashMem.h"

// Round-Robin 기반의 Wear-leveling을 위한 Spare Block Selection Algorithm을 적용
// 원형 형태의 Spare Block 테이블 선언

#define SWAP(x,y,temp) ((temp)=(x) ,(x)=(y), (y)=(temp)) //x,y를 교환하는 매크로 정의

/***
	< Spare Block Table 접근 및 사용 시기 >

	- 블록 매핑 (Static Table, Dynamic Table)
	1) FTL_write 상 Overwrite에 의한 유효 데이터 copy시 Spare Block을 사용
	2) 사용된 Spare Block을 일반 블록으로 설정 및 매핑 테이블 상에서 SWAP

	- 하이브리드 매핑 (Log Algorithm, Dunamic Table)
	1) FTL_write 상 PBN1과 PBN2에 대한 full_merge발생 시 merge과정에서 Spare Block을 사용한 유효 데이터 copy
	2) 사용된 Spare Block을 일반 블록으로 설정 및 매핑 테이블 상에서 SWAP

	- GC Scheduler
	1) Log Algorithm을 적용한 하이브리드 매핑에서 하나의 논리 블록 LBN에 대응된 PBN1과 PBN2에 대해 Merge과정에서
	Spare Block에 유효 데이터 copy 후 먼저 PBN1과 PBN2 모두 Erase 수행된다음, 하나를 Spare Block과 SWAP수행
	=> 이에 따라, Merge가 발생하면, GC에서 Erase 발생하지 않음

	2) 블록 매핑에서 FTL_write과정에 Overwrite에 의한 블록 무효화가 일어난다면, 쓰기 작업의 성능을 위하여
	무효화된 블록은 Victim Block으로 선정 및 Spare Block과 SWAP수행
	=> 이에 따라, GC가 해당 Victim Block을 Erase 수행하여야 함 
***/

/***
	< Round-Robin Based Spare Block Table을 위한 기존 read_index 처리 방법 >

	1) 기존 플래시 메모리 제거하고 새로운 플래시 메모리 할당 시 (*flashmem != NULL)
	2) Bootloader에 의한 Reorganization시 기존 생성 된 플래시 메모리를 무시하고, 새로운 플래시 메모리 할당 시 (*flashmem == NULL)
	---
	=> Physical_func의 init함수 실행 시 기존 read_index 제거
***/

typedef unsigned int spare_block_element;

class Spare_Block_Table
{
public:
	Spare_Block_Table();
	/*** 
		FIXED_FLASH_INFO(F_FLASH_INFO) : 플래시 메모리 생성 시 결정되는 고정된 정보의 spare_block_size 전달받아 초기화
		---
		=> 초기 생성 시 Round-Robin 기반의 Wear-leveling을 위해 기존 read_index 값 재 할당 (존재 할 경우)
	***/
	Spare_Block_Table(unsigned int spare_block_size);
	~Spare_Block_Table();

	spare_block_element* table_array; //Spare Block에 대한 원형 배열

	void print(); //Spare Block에 대한 원형 배열 출력 함수(debug)

	/***
		일반 블록과 Spare Block을 SWAP시 전달 한 read_index를 통하여 table_array에 접근하여 SWAP 수행
		이에 따라 항상 초기 할당 수 만큼의 Spare Block을 가진다
		---
		모든 Spare Block에 대해 돌아가면서 사용하므로 특정 블록에만 쓰기 작업이 수행되는 것을 방지
	***/
	int rr_read(class FlashMem*& flashmem, spare_block_element& dst_spare_block, unsigned int& dst_read_index); //현재 read_index에 따른 read_index 전달, Spare Block 번호 전달 후 다음 Spare Block 위치로 이동
	int seq_write(spare_block_element src_spare_block); //테이블 값 순차 할당

	int save_read_index(); //Reorganization을 위해 현재 read_index 값 저장
	int load_read_index(); //Reorganization을 위해 기존 read_index 값 불러옴
	
private:
	void init(); //Spare Block 테이블 초기화

	bool is_full; //Spare Block 테이블의 모든 데이터 기록 여부
	unsigned int write_index; //Spare Block 테이블내에서의 순차적 기록을 위한 현재 기록 위치 지정
	unsigned int read_index; //Wear-leveling을 위해 읽고자 할 현재 Spare Block 위치
	unsigned int table_size; //테이블의 할당 크기
};
#endif