#ifndef _BUILD_OPTIONS_H_
#define _BUILD_OPTIONS_H_

#include "FlashMem.h"

// 빌드 옵션 정의

/***
	< 예외 처리 >
	하위 계층에 의존적인 상위 계층에서 추가적인 예외 처리를 하지 않고, 단순 호출만 하도록, 
	발생 가능한 모든 치명적인 예외에 대해서는 하위 계층에서 처리한다.
***/

#define DEBUG_MODE //디버그 모드 - 발생 가능한 모든 오류 상황 추적 (주석 처리 : 사용 안함, 기본 값 : 사용)
#define BLOCK_TRACE_MODE //모든 물리 블록 당 마모도 추적 모드 - 텍스트 파일(trace_per_block_result.txt)로 출력 (주석 처리 : 사용 안함, 기본 값 : 사용)
/***
	Victim Block Queue의 크기와 Spare Block 개수를 다르게 할 경우(전체 블록 개수에 대한 비율이 다를 경우)
	Round-Robin Based Wear-leveling을 위한 선정된 Victim Block들과 Spare Block 개수 크기의 Spare Block Table을 통한 SWAP 발생 시에
	Spare Block Table의 모든 Spare Block들이 GC에 의해 처리되지 않았을 경우, GC에 의해 강제로 처리하도록 해야 함
	=> 예외 처리를 추가해야 하지만 구현생략, 양 측 비율을 같게 설정해야 한다.
	---
	ex)
	1) SPARE_BLOCK_RATIO가 10%이고, VICTIM_BLOCK_QUEUE_RATIO가 10%인 경우 가능
	2) SPARE_BLOCK_RATIO가 8%이고, VICTIM_BLOCK_QUEUE_RATIO가 10%인 경우 불가능
	3) SPARE_BLOCK_RATIO가 10%이고, VICTIM_BLOCK_QUEUE_RATIO가 8%인 경우 불가능
***/
#define SPARE_BLOCK_RATIO 0.08 //전체 블록 개수에 대한 시스템에서 관리할 Spare Block 비율 (8%)
#define VICTIM_BLOCK_QUEUE_RATIO 0.08 //Victim Block 큐의 크기를 생성된 플래시 메모리의 전체 블록 개수에 대한 비율 크기로 설정
#endif