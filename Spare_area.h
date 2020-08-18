#ifndef _SPARE_AREA_H_
#define _SPARE_AREA_H_

#include "FlashMem.h"

// Spare Area에 대한 비트 단위 처리, Meta-data 판독을 위한 함수 SPARE_init, SPARE_read, SPARE_write 선언
// 물리적 가용 가능 공간 관리와 Garbage Collection을 위한 SPARE_reads, update_v_flash_info_for_reorganization, update_v_flash_info_for_erase, calc_block_invalid_ratio 선언
// Meta-data를 통한 빈 일반 물리 블록 탐색 및 특정 물리 블록 내의 빈 물리 오프셋 탐색 위한 search_empty_block, search_empty_offset_in_block 선언

//8비트 크기의 Spare Area 초기값 지정
#define SPARE_INIT_VALUE (0xff) //0xff(16) = 11111111(2) = 255(10)

//비트 단위 boolean값 지정
#define TRUE_bit (0x1)
#define FALSE_bit (0x0)

/***
	1byte == 8bit
	Soare Area의 크기는 16바이트이므로, 총 128비트의 크기
	각 섹터마다 Spare 영역에 (Spare 블록 여부 || 블록 무효화 여부 || 빈 블록 여부 || 섹터 무효화 여부 || 빈 섹터 여부 || 사용하지 않는 공간) 순으로 기록

	- 초기값 모두 0x1 (true)로 초기화
	- 블록 단위 Erase 연산 수행 시 모두 0x1 (true)로 초기화
	
	-----------------------------------------------------------

	< meta 정보 관리 방법 >

	- FTL_write :
	1) 해당 섹터(페이지)의 meta 정보 또는 블록의 meta 정보 판별을 위하여 정의된 Spare Area 처리 함수(Spare_area.h, Spare_area.cpp)로부터 meta정보를 받아와 처리 수행
	2) 실제 섹터(페이지)에 직접적인 데이터 기록을 위하여 meta정보를 변경 및 기록 할 데이터를 Flash_write에 전달하여 수행
	3) 어떤 위치에 대하여 Overwrite를 수행할 경우 해당 위치(페이지 또는 블록)를 무효화시키기 위하여 정의된 Spare Area처리 함수를 통하여 수행

	- Flash_write : 
	1) 직접적인 섹터(페이지)에 대한 데이터 기록 및 해당 섹터(페이지)의 Spare Area에 대한 meta 정보 기록 수행
	2) 실제 섹터(페이지)에 대한 데이터 기록을 위해서는 meta 정보 (빈 섹터(페이지) 여부)도 반드시 변경 시켜야 함
	=> 이에 따라, 호출 시점에 해당 섹터(페이지)에 대해 미리 캐시된 meta정보가 없을 경우 먼저 Spare Area를 통한 meta정보를 읽어들이고,
	해당 섹터(페이지)가 비어있으면, 기록 수행. 비어있지 않으면, 플래시 메모리의 특성에 따른 Overwrite 오류
	3) 이를 위하여, 상위 계층의 FTL_write에서 빈 섹터(페이지) 여부를 미리 변경시킬 경우, Flash_write에서 meta 정보 판별을 수행하지 않은 단순 기록만 수행할 수 있지만,
	매핑 방식을 사용하지 않을 경우, 쓰기 작업에 따른 Overwrite 오류를 검출하기 위해서는 직접 데이터 영역을 읽거나, 매핑 방식별로 별도의 처리 로직을 만들어야 한다.
	이에 따라, Common 로직의 단순화를 위하여, 직접적인 섹터(페이지)단위의 기록이 발생하는 Flash_write상에서만 빈 섹터(페이지) 여부를 변경한다.

***/

enum class META_DATA_BIT_POS : const __int8 //Spare Area의 meta-data에 대한 비트 위치 열거형 정의
{
	/***
		128bit크기의 Spare Area에 대해 각 meta-data의 Spare Area상에서 비트 위치 지정
		2^127 ~ 2^0범위를 기준으로 위치 지정
	***/
	//==========================================================================================================================
	//각 블록의 첫 번째 섹터(페이지)의 Spare 영역에 해당 블록 정보를 관리
	not_spare_block = 127, //해당 블록이 시스템에서 관리하는 Spare Block 여부 (1bit) - 현재 비트 단위 처리에 대해 판별을 시작하는 위치
	valid_block = 126, //해당 블록의 무효화 여부 (1bit)
	empty_block = 125, //해당 블록에 데이터 기록 여부 (1bit)

	//모든 섹터(페이지)에서 관리
	valid_sector = 124, //해당 섹터(페이지)의 무효화 여부 (1bit)
	empty_sector = 123 //해당 섹터(페이지)에 데이터 기록 여부 (1bit) - 현재 비트 단위 처리에 대해 판별이 끝나는 위치
	//==========================================================================================================================
	//이 후의 비트 자리들에 대해서는 사용 안함 (2^122 ~ 2^0)
};

class META_DATA //Spare Area에 기록된 meta-data에 대한 판독을 쉽게 하기 위한 클래스
{
public:
	META_DATA();
	~META_DATA();
	
	/***
		열거형으로 정의된 META_DATA_BIT_POS에 의해 각 meta정보의 비트자리(2^127 ~ 2^0)가 지정되었으므로
		배열의 index(0~127)를 비트자리로 간주하여, 읽거나(read) 기록 시(write)에 
		맨 뒤 (2^127에 해당하는 자리, index : 127)에서부터 index 0까지 순차적으로 수행
	***/

	bool* meta_data_array; //meta정보에 대한 배열(기록 또는 읽을 시에 뒤에서부터)

	int seq_write(unsigned flag_bit); //읽어들인 flag_bit로부터 뒤에서부터 순차적으로(sequential) 기록
	int seq_read(bool& dst_result); //뒤에서부터 순차적으로(sequential) 읽어서 값 전달

private:
	bool is_full; //META_DATA의 모든 데이터 기록 여부
	int write_index; //META_DATA내에서의 순차적 기록을 위한 현재 기록 위치 지정
	int read_index; //META_DATA내에서의 순차적 읽기를 위한 현재 읽기 위치 지정
};

//Spare_area.cpp
//Spare Area 데이터에 대한 처리 함수
int SPARE_init(class FlashMem** flashmem, FILE** storage_spare_pos); //물리 섹터(페이지)의 Spare Area에 대한 초기화
META_DATA* SPARE_read(class FlashMem** flashmem, FILE** storage_spare_pos); //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 클래스 형태로 반환
int SPARE_write(class FlashMem** flashmem, FILE** storage_spare_pos, META_DATA** src_data); //META_DATA에 대한 클래스 전달받아, 물리 섹터의 Spare Area에 기록

//Overloading for FTL function
META_DATA* SPARE_read(class FlashMem** flashmem, unsigned int PSN); //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 클래스 형태로 반환
int SPARE_write(class FlashMem** flashmem, unsigned int PSN, META_DATA** src_data); //META_DATA에 대한 클래스 전달받아, 물리 섹터의 Spare Area에 기록

/*** Depending on Spare area processing function ***/
//for Remaining Space Management and Garbage Collection
META_DATA** SPARE_reads(class FlashMem** flashmem, unsigned int PBN); //한 물리 블록내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태로 반환
int update_victim_block_info(class FlashMem** flashmem, bool is_logical, unsigned int src_Block_num, int mapping_method); //Victim Block 선정을 위한 블록 정보 구조체 갱신
int update_v_flash_info_for_reorganization(class FlashMem** flashmem, META_DATA** src_data); //특정 물리 블록 하나에 대한 META_DATA 클래스 배열을 통한 판별을 수행하여 물리적 가용 가능 공간 계산 위한 가변적 플래시 메모리 정보 갱신
int update_v_flash_info_for_erase(class FlashMem** flashmem, META_DATA** src_data); //Erase하고자 하는 특정 물리 블록 하나에 대해 META_DATA 클래스 배열을 통한 판별을 수행하여 플래시 메모리의 가변적 정보 갱신
int calc_block_invalid_ratio(META_DATA** src_data, float& dst_block_invalid_ratio); //특정 물리 블록 하나에 대한 META_DATA 클래스 배열을 통한 판별을 수행하여 무효율 계산 및 전달
//meta정보를 통한 빈 일반 물리 블록 탐색
int search_empty_block(class FlashMem** flashmem, unsigned int& dst_Block_num, META_DATA** dst_data, int mapping_method, int table_type); //빈 일반 물리 블록(PBN)을 순차적으로 탐색하여 PBN또는 테이블 상 LBN 값, 해당 PBN의 meta정보 전달
//meta정보를 통한 물리 블록 내의 빈 물리 오프셋 탐색
int search_empty_offset_in_block(class FlashMem** flashmem, unsigned int src_PBN, __int8& dst_Poffset, META_DATA** dst_data); //일반 물리 블록(PBN) 내부를 순차적으로 탐색하여 Poffset 값, 해당 위치의 meta정보 전달
#endif