#ifndef _BUILD_OPTIONS_H_
#define _BUILD_OPTIONS_H_
#define _CRT_SECURE_NO_WARNINGS

// 빌드 옵션 및 기타 전처리 매크로, 필요한 헤더 파일 정의

/***
	< 예외 처리 >
	하위 계층에 의존적인 상위 계층에서 추가적인 예외 처리를 하지 않고, 단순 호출만 하도록, 
	발생 가능한 모든 치명적인 예외에 대해서는 하위 계층에서 처리한다.
***/

//#define DEBUG_MODE //디버그 모드 - 발생 가능한 모든 오류 상황 추적 (주석 처리 : 사용 안함, 기본 값 : 사용)
#define PAGE_TRACE_MODE //모든 섹터(페이지) 당 마모도 추적 모드 - 텍스트 파일(trace_per_page_result.txt)로 출력 (주석 처리 : 사용 안함, 기본 값 : 사용)
#define BLOCK_TRACE_MODE //모든 물리 블록 당 마모도 추적 모드 - 텍스트 파일(trace_per_block_result.txt)로 출력 (주석 처리 : 사용 안함, 기본 값 : 사용)
#define SPARE_BLOCK_RATIO 0.08 //전체 블록 개수에 대한 시스템에서 관리할 Spare Block 비율 (8%)
#define VICTIM_BLOCK_QUEUE_RATIO 0.08 //Victim Block 큐의 크기를 생성된 플래시 메모리의 전체 블록 개수에 대한 비율 크기로 설정
//#define GC_LAZY_MODE_RATIO_THRESHOLD

/***
	Victim Block Queue의 크기와 Spare Block 개수를 다르게 할 경우(전체 블록 개수에 대한 비율이 다를 경우)
	Round-Robin Based Wear-leveling을 위한 선정된 Victim Block들과 Spare Block 개수 크기의 Spare Block Queue을 통한 SWAP 발생 시에
	Spare Block Queue의 모든 Spare Block들이 GC에 의해 처리되지 않았을 경우, GC에 의해 강제로 처리하도록 해야 함
	=> 예외 처리를 추가해야 하지만 구현생략, 양 측 비율을 같게 설정해야 한다.
	---
	ex)
	1) SPARE_BLOCK_RATIO가 10%이고, VICTIM_BLOCK_QUEUE_RATIO가 10%인 경우 가능
	2) SPARE_BLOCK_RATIO가 8%이고, VICTIM_BLOCK_QUEUE_RATIO가 10%인 경우 불가능
	3) SPARE_BLOCK_RATIO가 10%이고, VICTIM_BLOCK_QUEUE_RATIO가 8%인 경우 불가능
***/

#include <stdio.h> //fscanf,fprinf,fwrite,fread
#include <stdint.h> //정수 자료형
#include <iostream> //C++ 입출력
#include <sstream> //stringstream
#include <windows.h> //시스템 명령어
#include <math.h> //ceil,floor,round
#include <stdbool.h> //boolean
#include <chrono> //trace 시간 측정
#include <random> //random_device, mersenne twister

//연산 결과 값 정의
#define SUCCESS 1 //성공
#define	COMPLETE 0 //단순 연산 완료
#define FAIL -1 //실패

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

#include "Spare_area.h"
#include "Circular_Queue.h" //대기열을 위해 Meta 정보 관련 정의 필요
#include "FlashMem.h"
#include "utils.hpp"
#include "GarbageCollector.h"
#endif