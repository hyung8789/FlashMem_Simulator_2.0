#ifndef _SPARE_AREA_H_
#define _SPARE_AREA_H_

// Spare Area에 대한 비트 단위 처리, Meta-data 판독을 위한 함수 SPARE_init, SPARE_read, SPARE_write 선언
// 물리적 가용 가능 공간 관리와 Garbage Collection을 위한 SPARE_reads, update_victim_block_info, update_v_flash_info_for_reorganization, update_v_flash_info_for_erase, calc_block_invalid_ratio 선언
// Meta-data를 통한 빈 일반 물리 블록 탐색 및 특정 물리 블록 내의 빈 물리 오프셋 탐색 위한 search_empty_block, search_empty_offset_in_block 선언

//8비트 크기의 Spare Area 초기값 지정
#define SPARE_INIT_VALUE (0xff) //0xff(16) = 11111111(2) = 255(10)

//비트 단위 boolean값 지정
#define TRUE_BIT (0x1)
#define FALSE_BIT (0x0)

static struct META_DATA* DO_NOT_READ_META_DATA = NULL; //Non-FTL 혹은 계층적 처리 과정에서 이미 meta 정보를 읽었을 경우 다시 읽지 않기 위해 사용

/***
	초기값 모두 0x1로 초기화
	블록 단위 Erase 연산 수행 시 모두 0x1로 초기화
	---------
	
	각 섹터(페이지)의 Spare Area의 전체 16byte에 대해 첫 1byte를 블록 및 섹터(페이지)의 상태 정보로 사용
	BLOCK_TYPE(Normal or Spare, 1bit) || IS_VALID (BLOCK, 1bit) || IS_EMPTY (BLOCK, 1bit) || IS_VALID (SECTOR, 1bit) || IS_EMPTY(SECTOR, 1bit) || DUMMY (3bit)
	기타, Block Address(Logical) Area, ECC Area 등 추가적으로 사용 시, META_DATA 구조체, SPARE_read, SPARE_write 수정
	---------
	
	< meta 정보 관리 방법 >

	- FTL_write (상위 계층) :
	1) 해당 섹터(페이지)의 meta 정보 또는 블록의 meta 정보 판별을 위하여 정의된 Spare Area 처리 함수(Spare_area.h, Spare_area.cpp)로부터 meta정보를 받아와 처리 수행
	2) 실제 섹터(페이지)에 직접적인 데이터 기록을 위하여 meta정보를 변경 및 기록 할 데이터를 Flash_write에 전달하여 수행
	3) 어떤 위치(기존 데이터)에 대하여 Overwrite를 수행할 경우 해당 위치(기존 데이터)를 무효화시키기 위하여 정의된 Spare Area 처리 함수를 통하여 수행

	- Flash_write , Spare Area 처리 함수들 (하위 계층) :
	1) 직접적인 섹터(페이지)에 대한 데이터 기록 및 해당 섹터(페이지)의 Spare Area에 대한 meta 정보 기록 수행
	2) 실제 섹터(페이지)에 대한 데이터 기록을 위해서는 meta 정보 (섹터(페이지) 상태 혹은 블록 상태)도 반드시 변경 시켜야 함
	=> 이에 따라, 호출 시점에 해당 섹터(페이지)에 대해 미리 캐시된 meta정보가 없을 경우 먼저 Spare Area를 통한 meta정보를 읽어들이고,
	해당 섹터(페이지)가 비어있으면, 기록 수행. 비어있지 않으면, 플래시 메모리의 특성에 따른 Overwrite 오류
	3) 이를 위하여, 상위 계층의 FTL_write에서 빈 섹터(페이지) 여부를 미리 변경시킬 경우, Flash_write에서 meta 정보 판별을 수행하지 않은 단순 기록만 수행할 수 있지만,
	매핑 방식을 사용하지 않을 경우, 쓰기 작업에 따른 Overwrite 오류를 검출하기 위해서는 직접 데이터 영역을 읽거나, 매핑 방식별로 별도의 처리 로직을 만들어야 한다.
	이에 따라, 공용 로직의 단순화를 위하여, 직접적인 섹터(페이지)단위의 기록이 발생하는 Flash_write상에서만 빈 섹터(페이지) 여부를 변경한다.

	< trace를 위한 Global 플래시 메모리 작업 카운트와 블록, 섹터(페이지) 당 마모도 카운트 관리 방법 >
	
	Global 플래시 메모리 작업 횟수 추적은 FlashMem.h의 VARIABLE_FLASH_INFO의 내용에 따른다.
	Spare Area에 대한 단일 read, write, 초기화(erase)작업도 한 번의 플래시 메모리의 read, write, erase 작업으로 취급
	단, 데이터 영역을 포함한 Spare Area와 동시 처리 발생 시 이는 한 번의 작업으로 처리한다.
	---
		1) Spare Area를 제외한 데이터 영역만 읽을 시(즉, 상위 계층에서 먼저 meta 정보 판독을 수행하였을 경우)에 Flash_read에서도 read 카운트 증가 
		(읽기가 해당 섹터(페이지)에 다시 발생하였으므로)

		2) erase 카운트는 블록 당 한 번이므로 Flash_erase에서 증가
		3) 어떠한 위치에 대하여 쓰기 작업 시 반드시 meta 정보(섹터 상태 혹은 블록 상태)를 함께 변경해야하므로, 데이터 영역만 처리할 수 없다
			=> 이에 따라 write 카운트는 Spare Area의 처리 함수에서만 증가
***/

enum class BLOCK_STATE : const unsigned //블록 상태 정보
{
	/***
		각 블록의 첫 번째 섹터(페이지)의 Spare 영역에 해당 블록 정보를 관리
		일반 데이터 블록 혹은 직접적 데이터 기록이 불가능한 Spare 블록에 대하여,
		- EMPTY : 해당 블록은 블록 내의 모든 오프셋에 대해 비어있고, 유효하다. (초기 상태)
		- VALID : 해당 블록은 유효하고, 일부 유효한 데이터가 기록되어 있다.
		- INVALID : 해당 블록 내의 모든 오프셋에 대해 무효하거나, 일부 유효 데이터를 포함하고 있지만 더 이상 사용 불가능. 재사용 위해서 블록 단위 Erase 수행하여야 함
	***/

	/***
		BLOCK_TYPE || IS_VALID || IS_EMPTY
		블록에 대한 6가지 상태, 2^3 = 8, 3비트 필요
	***/

	NORMAL_BLOCK_EMPTY = (0x7), //초기 상태 (Default), 0x7(16) = 7(10) = 111(2)
	NORMAL_BLOCK_VALID = (0x6), //0x6(16) = 6(10) = 110(2)
	NORMAL_BLOCK_INVALID = (0x4), //0x4(16) = 4(10) = 100(2)
	SPARE_BLOCK_EMPTY = (0x3), //0x3(16) = 3(10) = 011(2)
	SPARE_BLOCK_VALID = (0x2), //0x2(16) = 2(10) = 010(2)
	SPARE_BLOCK_INVALID = (0x0) //0x0(16) = 0(10) = 000(2)
};

enum class SECTOR_STATE : const unsigned //섹터(페이지) 상태 정보
{
	/***
		모든 섹터(페이지)에서 관리
		- EMPTY : 해당 섹터(페이지)는 비어있고, 유효하다. (초기 상태)
		- VALID : 해당 섹터(페이지)는 유효하고, 유효한 데이터가 기록되어 있다.
		- INVALID : 해당 섹터(페이지)는 무효하고, 무효한 데이터가 기록되어 있다. 재사용 위해서 블록 단위 Erase 수행하여야 함
	***/

	/***
		IS_VALID || IS_EMPTY
		섹터에 대한 3가지 상태, 2^2 = 4, 2비트 필요
	***/

	EMPTY = (0x3), //초기 상태 (Default), 0x3(16) = 3(10) = 11(2)
	VALID = (0x2), //0x2(16) = 2(10) = 10(2)
	INVALID = (0x0) //0x0(16) = 0(10) = 00(2)
};

struct META_DATA //Flash_read,write와 FTL_read,write간의 계층적 처리를 위해 외부적으로 생성 및 접근
{
	BLOCK_STATE block_state; //블록 상태
	SECTOR_STATE sector_state; //섹터 상태
};

//Spare_area.cpp
//Spare Area 데이터에 대한 처리 함수
int SPARE_init(class FlashMem*& flashmem, FILE*& storage_spare_pos); //물리 섹터(페이지)의 Spare Area에 대한 초기화

int SPARE_read(class FlashMem*& flashmem, FILE*& storage_spare_pos, META_DATA*& dst_meta_buffer); //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 구조체 형태로 반환
int SPARE_read(class FlashMem*& flashmem, unsigned int PSN, META_DATA*& dst_meta_buffer); //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 구조체 형태로 반환

int SPARE_write(class FlashMem*& flashmem, FILE*& storage_spare_pos, META_DATA*& src_meta_buffer); //META_DATA에 대한 구조체 전달받아, 물리 섹터의 Spare Area에 기록
int SPARE_write(class FlashMem*& flashmem, unsigned int PSN, META_DATA*& src_meta_buffer); //META_DATA에 대한 구조체 전달받아, 물리 섹터의 Spare Area에 기록

/*** Depending on Spare area processing function ***/
//for Remaining Space Management and Garbage Collection
int SPARE_reads(class FlashMem*& flashmem, unsigned int PBN, META_DATA**& dst_block_meta_buffer_array); //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 구조체 배열 형태로 반환
int SPARE_writes(class FlashMem*& flashmem, unsigned int PBN, META_DATA**& src_block_meta_buffer_array); //한 물리 블록 내의 모든 섹터(페이지)에 대해 meta정보 기록

int update_victim_block_info(class FlashMem*& flashmem, bool is_logical, unsigned int src_block_num, enum MAPPING_METHOD mapping_method); //Victim Block 선정을 위한 블록 정보 구조체 갱신 및 GC 스케줄러 실행
/*** 이미 읽어들인 meta 정보를 이용하여 수행 ***/
int update_victim_block_info(class FlashMem*& flashmem, bool is_logical, unsigned int src_block_num, META_DATA**& src_block_meta_buffer_array, enum MAPPING_METHOD mapping_method); //Victim Block 선정을 위한 블록 정보 구조체 갱신 및 GC 스케줄러 실행 (블록 매핑)
int update_victim_block_info(class FlashMem*& flashmem, bool is_logical, unsigned int src_block_num, META_DATA**& src_PBN1_block_meta_buffer_array, META_DATA**& src_PBN2_block_meta_buffer_array, enum MAPPING_METHOD mapping_method);  //Victim Block 선정을 위한 블록 정보 구조체 갱신 및 GC 스케줄러 실행 (하이브리드 매핑)

int update_v_flash_info_for_reorganization(class FlashMem*& flashmem, META_DATA**& src_block_meta_buffer_array); //특정 물리 블록 하나에 대한 META_DATA 구조체 배열을 통한 판별을 수행하여 물리적 가용 가능 공간 계산 위한 가변적 플래시 메모리 정보 갱신
int update_v_flash_info_for_erase(class FlashMem*& flashmem, META_DATA**& src_block_meta_buffer_array); //Erase하고자 하는 특정 물리 블록 하나에 대해 META_DATA 구조체 배열을 통한 판별을 수행하여 플래시 메모리의 가변적 정보 갱신

int calc_block_invalid_ratio(META_DATA**& src_block_meta_buffer_array, float& dst_block_invalid_ratio); //특정 물리 블록 하나에 대한 META_DATA 구조체 배열을 통한 판별을 수행하여 무효율 계산 및 전달

//int search_empty_normal_block_for_dynamic_table(class FlashMem*& flashmem, unsigned int& dst_block_num, META_DATA*& dst_meta_buffer, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type); //빈 일반 물리 블록(PBN)을 순차적으로 탐색하여 PBN또는 테이블 상 LBN 값, 해당 PBN의 meta정보 전달
int search_empty_offset_in_block(class FlashMem*& flashmem, unsigned int src_PBN, __int8& dst_Poffset, META_DATA*& dst_meta_buffer, enum MAPPING_METHOD mapping_method); //일반 물리 블록(PBN) 내부를 순차적인 비어있는 위치 탐색, Poffset 값, 해당 위치의 meta정보 전달
int search_empty_offset_in_block(META_DATA**& src_block_meta_buffer_array, __int8& dst_Poffset, enum MAPPING_METHOD mapping_method); //일반 물리 블록(PBN)의 블록 단위 meta 정보를 순차적인 비어있는 위치 탐색, Poffset 값 전달
int print_block_meta_info(class FlashMem*& flashmem, bool is_logical, unsigned int src_block_num, enum MAPPING_METHOD mapping_method); //블록 내의 모든 섹터(페이지)의 meta 정보 출력

//메모리 해제
int deallocate_single_meta_buffer(META_DATA*& src_meta_buffer);
int deallocate_block_meta_buffer_array(META_DATA**& src_block_meta_buffer_array);
#endif