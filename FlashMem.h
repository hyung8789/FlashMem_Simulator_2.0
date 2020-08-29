/***

	Simulates for Block level Mapping Method with 2 Types of Mapping Table, 
	Hybrid Mapping Method (Log algorithm - 1:2 Block level mapping with Dynamic Table)

	1) Static Table : Mapping table initially all correspond 1 : 1 (Logical Sector or Block Num -> Physical Sector or Block Num)
	2) Dynamic Table : Mapping table initially all empty (non-assigned)

===========================================================================================================

※ Additional implementation

	- Garbage Collecter
		=> Because the cost of erasing operations is greater than reading and writing, 
		freeing up space for recording during idle time or through garbage collection at an appropriate time.

	- Trace function
		=> Performance test through write operation by specific pattern.
		
===========================================================================================================

by https://github.com/hyung8789
***/

// 플래시 메모리 생성을 위한 FlashMem 클래스 선언
// 플래시 메모리 스토리지 파일에 직접 접근하는 함수 init, Flash_read, Flash_write, Flash_erase 선언
// FTL 알고리즘에 따른 논리적 연산 함수 Print_Table, FTL_read, FTL_write 및 특정 패턴에 의한 쓰기 성능을 측정하기 위한 trace 선언

#ifndef _FLASHMEM_H_
#define _FLASHMEM_H_

#define _CRT_SECURE_NO_WARNINGS

/*** Build Option ***/
#define DEBUG_MODE 1 //디버그 모드 - 발생 가능한 모든 오류 상황 추적 (0 : 사용 안함, 1 : 사용)
#define BLOCK_TRACE_MODE 1 //모든 물리 블록 당 마모도 추적 모드 - 텍스트 파일로 출력 (0 : 사용 안함, 1 : 사용)
//Round-Robin Based Wear-levling을 위헤 초기 빈 블록 할당 시 Spare Block Table을 이용한다면, 동적 테이블 타입에서는 적용 가능 하지만, 정적 테이블 타입에서는 초기에 이미 할당되어있기 때문에 적용 불가
//#define INIT_EMPTY_BLOCK_WITH_RR_SPARE_BLOCK_TABLE 0 //빈 블록 할당 시 Spare Block Table을 통한 할당 (0 : 사용 안함(빈 일반 블록으로 할당), 1 : 사용)
#define SPARE_BLOCK_RATIO 0.08 //전체 블록 개수에 대한 시스템에서 관리할 Spare Block 비율 (8%)

#include <stdio.h> //fscanf,fprinf,fwrite,fread
#include <stdint.h> //정수 자료형
#include <iostream> //C++ 입출력
#include <sstream> //stringstream
#include <windows.h> //시스템 명령어
#include <math.h> //ceil,floor,round
#include <stdbool.h> //boolean
#include <chrono> //trace 시간 측정

#include "Spare_area.h"
#include "Victim_Queue.h"
#include "Spare_block_table.h"
#include "GarbageCollector.h"

#define MAX_CAPACITY_MB 65472 //생성가능 한 플래시 메모리의 MB단위 최대 크기

#define MB_PER_BLOCK 64 //1MB당 64 블록
#define MB_PER_SECTOR 2048 //1MB당 2048섹터
#define BLOCK_PER_SECTOR 32 //한 블록에 해당하는 섹터의 개수
#define SECTOR_PER_BYTE 512 //한 섹터의 데이터가 기록되는 영역의 크기 (byte)

#define SPARE_AREA_BYTE 16 //한 섹터의 Spare 영역의 크기 (byte)
#define SPARE_AREA_BIT 128 //한 섹터의 Spare 영역의 크기 (bit)
#define SECTOR_INC_SPARE_BYTE 528 //Spare 영역을 포함한 한 섹터의 크기 (byte)

#define DYNAMIC_MAPPING_INIT_VALUE UINT32_MAX //동적 매핑 테이블의 초기값 (생성 가능한 플래시 메모리의 용량 내에서 나올 수가 없는 물리적 주소 값(PBN,PSN))
#define OFFSET_MAPPING_INIT_VALUE INT8_MAX //오프셋 단위 매핑 테이블의 초기값 (한 블록에 대한 오프셋 테이블의 최대 크기는 블록 당 섹터(페이지)수이므로 이 범위 내에서 나올 수 없는 값)

//연산 결과 값 정의
#define SUCCESS 1 //성공
#define	COMPLETE 0 //단순 연산 완료
#define FAIL -1 //실패

extern int mapping_method; //현재 플래시 메모리의 매핑 방식 (0 : 매핑 안함, 1 : 섹터 매핑, 2 : 블록 매핑, 3 : 하이브리드 매핑)
extern int table_type; //매핑 테이블의 타입 (0 : Static Table, 1 : Dynamic Table)

struct VICTIM_BLOCK_INFO
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

typedef struct VARIABLE_FLASH_INFO 
{
	/***
		기록 된 섹터 수 카운트는 Flash_write에서 관리
		기록 시에 항상 meta정보를 같이 기록하므로 무효화된 섹터 수 카운트는 Spare Area의 처리 함수에서 관리
	***/
	unsigned int written_sector_count; //현재 플래시 메모리에 실제로 데이터가 기록 된 섹터 수 (유효 데이터, 무효 데이터 모두 포함)
	unsigned int invalid_sector_count; //현재 플래시 메모리에 무효화된 섹터 수

	/***
		Spare Area에 대한 단일 read, write 작업도 한 번의 플래시 메모리의 read, write 작업으로 취급
		---
		1) Spare Area를 제외한 데이터 영역만 읽을 시에는 예외 처리를 주어 Flash_read에서 read 카운트 관리
		2) erase 카운트는 블록 당 한 번이므로 Flash_erase에서 관리
		3) 쓰기 작업 시에는 meta정보를 변경해야하므로, 데이터 영역만 처리할 수 없다
			=> 이에 따라 write 카운트는 Spare Area의 처리 함수에서만 관리
	***/

	//플래시 메모리의 전체 작업 횟수 추적
	unsigned int flash_write_count;
	unsigned int flash_erase_count;
	unsigned int flash_read_count;

	//초기화
	void clear_all(); //모두 초기화
	void clear_trace_info(); //trace를 위한 정보 초기화

}V_FLASH_INFO; //플래시 메모리의 가변적 정보를 관리하기 위한 구조체

typedef struct FIXED_FLASH_INFO
{
	unsigned short flashmem_size; //플래시 메모리의 MB 크기
	unsigned int block_size; //할당된 메모리 크기에 해당하는 전체 블록의 개수 (Spare Block 포함)
	unsigned int sector_size; //할당된 메모리 크기에 해당하는 전체 섹터의 개수 (Spare Block 포함)
	//for ftl algorithm
	unsigned int data_only_storage_byte; //할당된 메모리 크기에 해당하는 총 byte 크기 반환 (Spare Area 제외)
	unsigned int storage_byte; //할당된 메모리 크기에 해당하는 총 byte 크기 (Spare Area 포함)
	unsigned int spare_area_byte; //할당된 메모리 크기에 해당하는 전체 Spare Area의 byte 크기
	unsigned int spare_block_size; //할당된 메모리 크기에 해당하는 시스템에서 관리하는 Spare Block 개수
	unsigned int spare_block_byte; //할당된 메모리 크기에 해당하는 시스템에서 관리하는 Spare Block의 총 byte 크기

}F_FLASH_INFO; //플래시 메모리 생성 시 결정되는 고정된 정보

struct BLOCK_TRACE_INFO
{
	//해당 블록에 발생한 모든 쓰기, 지우기, 읽기 횟수 추적
	unsigned int block_write_count;
	unsigned int block_erase_count;
	unsigned int block_read_count;

	void clear_all(); //모두 초기화
}; //블록 당 마모도 trace용

class FlashMem //FlashMem.cpp
{
public:
	/***
		Max Capacity : 65,472‬‬MB (63GB)
		(0 ~ 65,535)
		---
		65,472MB(68,652,367,872bytes) Flash Memory Storage :
		134,086,656‬‬ Sectors
		4,190,208 Blocks
		---

		※ using unsigned int (4byte, 0 ~ 4,294,967,295) for :
		
		1) to access all physical sectors of storage file created in bytes
		2) be able to express for all blocks in Block level Mapping Table
	***/

	FlashMem();
	FlashMem(unsigned short megabytes); //megabytes 크기의 플래시 메모리 생성
	~FlashMem();
	
	BLOCK_TRACE_INFO* block_trace_info; //전체 블록에 대한 각 블록 당 마모도 trace 위한 배열 (index : PBN)
	//==========================================================================================================================
	//Information for Remaining Space Management and Garbage Collection
	V_FLASH_INFO v_flash_info; //플래시 메모리의 가변적 정보를 관리하기 위한 구조체
	VICTIM_BLOCK_INFO victim_block_info; //Victim Block 선정을 위한 블록 정보 구조체
	class Victim_Queue* victim_block_queue; //Victim Block 큐
	class GarbageCollector* gc; //Garage Collector
	//==========================================================================================================================
	//Mappping Table
	//for block mapping
	unsigned int* block_level_mapping_table; //블록 단위 매핑 테이블
	//for hybrid mapping
	unsigned int** log_block_level_mapping_table; //1 : 2 블록 단위 매핑 테이블(index : LBN, row : 전체 PBN의 수, col : PBN1,PBN2)
	__int8* offset_level_mapping_table; //오프셋 단위(0~31) 매핑 테이블
	//for block mapping, hybrid mapping
	class Spare_Block_Table* spare_block_table; //Spare Block 테이블
	//==========================================================================================================================
	//Table Management, Reorganization process
	void bootloader(FlashMem** flashmem, int& mapping_method, int& table_type); //Reorganization process from initialized flash memory storage file
	void deallocate_table(); //현재 캐시된 모든 테이블 해제
	void load_table(int mapping_method); //매핑방식에 따른 매핑 테이블 로드
	void save_table(int mapping_method); //매핑방식에 따른 매핑 테이블 저장	
	//==========================================================================================================================
	//Screen I/O functions
	void switch_mapping_method(int& mapping_method, int& table_type); //현재 플래시 메모리의 매핑 방식 및 테이블 타입 변경
	void disp_command(int mapping_method, int table_type); //커맨드 명령어 출력
	void input_command(FlashMem** flashmem, int& mapping_method, int& table_type); //커맨드 명령어 입력
	void disp_flash_info(FlashMem** flashmem, int mapping_method, int table_type); //현재 생성된 플래시 메모리의 정보 출력	
	//==========================================================================================================================

	F_FLASH_INFO get_f_flash_info(); //플래시 메모리의 고정된 정보 반한

private: //Fixed data
	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
};

//Physical_func.cpp
int init(FlashMem** flashmem, unsigned short megabytes, int mapping_method, int table_type); //megabytes 크기의 플래시 메모리를 생성
int Flash_read(FlashMem** flashmem, class META_DATA** dst_buffer, unsigned int PSN, char& dst_data); //물리 섹터에 데이터를 읽어옴
int Flash_write(FlashMem** flashmem, class META_DATA** src_buffer, unsigned int PSN, const char src_data); //물리 섹터에 데이터를 기록
int Flash_erase(FlashMem** flashmem, unsigned int PBN); //물리 블록에 해당하는 데이터를 지움

//FTL_func.cpp
int Print_table(FlashMem** flashmem, int mapping_method, int table_type);  //매핑 테이블 출력
int FTL_read(FlashMem** flashmem, unsigned int LSN, int mapping_method, int table_type); //논리 섹터 또는 논리 블록에 해당되는 매핑테이블 상 물리 섹터 또는 물리 블록의 위치를 반환
int FTL_write(FlashMem** flashmem, unsigned int LSN, const char src_data, int mapping_method, int table_type); //논리 섹터 또는 논리 블록에 해당되는 매핑테이블 상 물리 섹터 또는 물리 블록 위치에 기록
int full_merge(FlashMem** flashmem, unsigned int LBN, int mapping_method); //특정 LBN에 대응된 PBN1과 PBN2에 대하여 Merge 수행
int full_merge(FlashMem** flashmem, int mapping_method); //테이블내의 대응되는 모든 블록에 대해 Merge 수행
int trace(FlashMem** flashmem, int mapping_method, int table_type); //특정 패턴에 의한 쓰기 성능을 측정하는 함수
#endif