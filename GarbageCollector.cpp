#include "GarbageCollector.h"

// Garbage Collecter 구현을 위한 클래스 정의

GarbageCollector::GarbageCollector() 
{
	this->invalid_ratio_threshold = 1.0;
	this->RDY_v_flash_info_for_set_invalid_ratio_threshold = false; //가변적 플래시 메모리 정보 갱신 완료 후 true로 set
	this->gc_lazy_mode = true; //기록 공간 부족 시 false로 set
}

GarbageCollector::~GarbageCollector() {}

void GarbageCollector::print_invalid_ratio_threshold()
{
	printf("Current Invalid Ratio Threshold : %f\n", this->invalid_ratio_threshold);
}

int GarbageCollector::scheduler(FlashMem** flashmem, int mapping_method) //main scheduling function for GC
{
	if ((*flashmem) == NULL || mapping_method == 0) //플래시 메모리가 할당되어있고, 매핑 방식을 사용할 경우에만 수행
		return FAIL;

	if (this->RDY_v_flash_info_for_set_invalid_ratio_threshold == false) //Reorganization에 따른 가변적 플래시 메모리 정보가 준비되지 않았으면
	{
		//완전 무효화된 블록에 대해서 Victim Block 큐에 삽입만 수행, 이에 따라, 큐가 가득 찰 경우 전체 처리
		this->enqueue_job(flashmem, mapping_method);
		
		if((*flashmem)->victim_block_queue->is_full() == true)
			this->all_dequeue_job(flashmem, mapping_method); //VIctim Block 큐 내의 모든 Victim Block들에 대해 처라

		return SUCCESS;
	}

	printf("\n※ Starting GC Scheduling Task...");
	/***
		현재 플래시 메모리의 가용 가능 공간에 따른 Victim Block 선정 위한 무효율 임계값 계산
		Victim Block 선정 위한 정보 존재 시 무효율 임계값에 따라 Victim Block 큐에 삽입
	***/
	this->set_invalid_ratio_threshold(flashmem);
	this->enqueue_job(flashmem, mapping_method);

	/***
		현재 플래시 메모리의 무효 데이터 비율 및 기록 가능 공간 확인
		현재 Victim Block 큐 확인(is_full, is_empty)
	***/
	unsigned int written_sector_count = (*flashmem)->v_flash_info.written_sector_count; //현재 플래시 메모리의 기록된 섹터 수
	F_FLASH_INFO f_flash_info = (*flashmem)->get_f_flash_info(); //플래시 메모리 생성 시 결정되는 고정된 정보

	/***
		물리적으로 남아있는 기록 가능 공간 = 전체 byte단위 값 - (기록된 섹터 수 * 섹터 당 바이트 값)
		=> 사용자에게 보여지지 않는 용량이므로, Spare Block을 포함시킨다.

		논리적으로 남아있는 기록 공간은 실제 직접적 데이터 기록이 불가능한 여분의 Spare Block이 차지하는 총 byte값을 제외한다
	***/

	unsigned int physical_using_space = (*flashmem)->v_flash_info.written_sector_count * SECTOR_INC_SPARE_BYTE; //물리적으로 사용 중인 공간
	unsigned int physical_free_space = f_flash_info.storage_byte - physical_using_space; //물리적으로 남아있는 기록 가능 공간

	//논리적으로 남아있는 기록 가능 공간 = 전체 byte단위 값 - (기록된 섹터들 중 무효 섹터 제외 * 섹터 당 바이트 값) - Spare Block이 차지하는 총 byte값
	unsigned int logical_using_space = ((*flashmem)->v_flash_info.written_sector_count - (*flashmem)->v_flash_info.invalid_sector_count) * SECTOR_INC_SPARE_BYTE;
	unsigned int logical_free_space = (f_flash_info.storage_byte - logical_using_space) - f_flash_info.spare_block_byte;

	bool flag_vq_is_full = (*flashmem)->victim_block_queue->is_full();
	bool flag_vq_is_empty = (*flashmem)->victim_block_queue->is_empty();

	//실제 물리적으로 남아있는 기록 가능 공간이 없을 경우
	if (physical_free_space < (BLOCK_PER_SECTOR * SECTOR_INC_SPARE_BYTE))
	{
		//기록 공간 부족 시 기록 공간 확보를 위해 Lazy Mode 비활성화
		switch (this->gc_lazy_mode)
		{
		case true:
			this->gc_lazy_mode = false;
			break;

		case false:
			break;
		}

		if (flag_vq_is_full == true || flag_vq_is_empty == false) //Victim Block 큐가 가득 차 있거나 비어있지 않은 경우
			this->all_dequeue_job(flashmem, mapping_method); //VIctim Block 큐 내의 모든 Victim Block들에 대해 처라
		else if (flag_vq_is_empty == true && mapping_method == 3) //Victim Block 큐가 빈 경우
			full_merge(flashmem, mapping_method); //테이블 내의 전체 블록에 대해 가능 할 경우 Merge 수행 (Log Algorithm을 적용한 하이브리드 매핑의 경우에만 수행)
		else
		{
			/***
				블록 매핑의 경우 Merge 불가능, Erase만 수행
				Overwrite 발생 시 해당 블록은 항상 무효화되므로, 섹터(페이지)단위의 무효화된 데이터가 존재하지 않다.
				따라서, 이 경우 실제 논리적 저장공간을 모두 사용하여서 더 이상 기록이 불가능한 경우이다.
			***/
			printf("End with no Operation\n(All physical spaces are used)\n");
			return FAIL;
		}
	}
	else //실제 물리적으로 남아있는 기록 가능 공간에 여유가 있을 경우
	{
		//Lazy Mode 활성화
		switch (this->gc_lazy_mode)
		{
		case true:
			break;

		case false:
			this->gc_lazy_mode = true;
			break;
		}

		if (flag_vq_is_empty == true) //Victim Block 큐가 빈 경우
		{
			//아무런 작업도 하지 않음
			printf("End with no Operation\n");
			return COMPLETE;
		}
		else if (flag_vq_is_full == false && flag_vq_is_empty == false) //Victim Block 큐가 가득 차 있지 않고, 비어있지 않은 경우
		{
			switch (this->gc_lazy_mode)
			{
			case true:
				//연속된 쓰기 작업에 대한 Write Performance 향상을 위하여 Victim Block 큐가 가득 찰 때까지 아무런 작업을 수행하지 않는다.
				printf("End with no Operation (Lazy Mode)\n");
				return COMPLETE;

			case false:
				this->one_dequeue_job(flashmem, mapping_method); //하나를 빼 와서 처리
				break;
			}
		}
		else //Victim Block 큐가 가득 찬 경우
			this->all_dequeue_job(flashmem, mapping_method); //모든 Victim Block을 빼와서 처리
	}

	printf("Success\n");
	return SUCCESS;
}


int GarbageCollector::one_dequeue_job(class FlashMem** flashmem, int mapping_method) //Victim Block 큐로부터 하나의 Victim Block을 빼와서 처리
{
	//블록 매핑 : 해당 Victim Block은 항상 무효화되어 있으므로 단순 Erase 수행
	//하이브리드 매핑 : 무효화된 블록일 경우 단순 Erase, 아닐 경우 Merge 수행

	victim_element victim_block; //큐에서 요소를 빼와서 저장
	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보

	/***
		블록 매핑은 Overwrite발생 시 해당 PBN은 무조건 무효화, Victim Block으로 선정 => 해당 PBN에 대한 Erase수행
	
		Log Algorithm을 적용한 하이브리드 매핑은 LBN에 대응된 PBN1 또는 PBN2의 모든 데이터가 무효화될시에 Victim Block으로 선정 => 해당 PBN에 대한 Erase 수행
		LBN에 PBN1, PBN2 모두 대응되어 있고(즉, 한쪽이라도 블록이 무효화되지않음), 기록공간 확보를 위해 LBN을 Victim Block으로 선정 => 해당 LBN에 대한 Merge 수행
		---
		=> 이에 따라, Victim Block 큐에 PBN(물리 블록 번호)에 대해서는 항상 무효화된 블록이므로 Erase 수행
		LBN일경우, LBN에 대응된 PBN1과 PBN2에 대한 Merge 수행
	***/
	if ((*flashmem)->victim_block_queue->dequeue(victim_block) == SUCCESS)
	{
		switch (mapping_method)
		{
		case 2: //블록 매핑
			if (victim_block.victim_block_invalid_ratio != 1.0) //Overwrite 발생 시 항상 해당 블록은 완전 무효화되므로 무효율이 1.0이 아니면 오류
				goto WRONG_INVALID_RATIO_ERR;
			
			Flash_erase(flashmem, victim_block.victim_block_num);

			break;

		case 3: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
			/***
					Victim Block으로 선정된 블록이 물리 블록일 경우, 해당 물리 블록(PBN)은 완전 무효화되었기에 Victim Block으로 선정되었다.
					이와 비교하여, 무효율 임계값에 따라 선정된 논리 블록(LBN)은 일부 유효 및 무효 데이터를 포함하고 있고, 해당
					LBN에 대응된 PBN1, PBN2에 대하여 Merge되어야 한다.
					---
					만약, Log Algorithm을 적용한 하이브리드 매핑에서 일부 유효 및 무효 데이터를 포함하고 있는 단일 물리 블록(PBN1 또는 PBN2)에 대해서만
					기록 공간 확보를 위하여, 무효율 임계값에 따라 선정 후 여분의 빈 Spare 블록을 사용하여 유효 데이터 copy 및 Erase 후 블록 교체 작업을
					수행한다면, 해당 LBN의 PBN1과 PBN2에 대해 Merge를 수행하는 것과 비교하여 더 적은 기록 공간을 확보하였지만, 유효 데이터 copy 및 
					해당 물리 블록 Erase로 인한 셀 마모 유발로 인해 비효율적이다.
			***/

			if (victim_block.is_logical == true) //Victim Block 번호가 LBN일 경우 : Merge 수행
				full_merge(flashmem, victim_block.victim_block_num, mapping_method);
			else //Victim Block 번호가 PBN일 경우 : Erase 수행
			{		
				if (victim_block.victim_block_invalid_ratio != 1.0)
					goto WRONG_INVALID_RATIO_ERR;

				Flash_erase(flashmem, victim_block.victim_block_num);
			}
			break;
		}
	}
	else
		return FAIL;

	return SUCCESS;

WRONG_INVALID_RATIO_ERR:
	fprintf(stderr, "오류 : Wrong Invalid Ratio (%f)\n", victim_block.victim_block_invalid_ratio);
	system("pause");
	exit(1);
}

int GarbageCollector::all_dequeue_job(class FlashMem** flashmem, int mapping_method) //Victim Block 큐의 모든 Victim Block을 빼와서 처리
{
	while (1) //큐가 빌 때까지 작업
	{
		if((this->one_dequeue_job(flashmem, mapping_method) != SUCCESS))
			break;
	}

	return SUCCESS;
}


int GarbageCollector::enqueue_job(class FlashMem** flashmem, int mapping_method) //Victim Block 큐에 삽입
{
	/***
		Victim Block 정보 구조체 초기값
		---
		victim_block_num = DYNAMIC_MAPPING_INIT_VALUE;
		victim_block_invalid_ratio = -1;
	***/
	
	//Victim Block 정보 구조체가 초기값이 아니면, 요청이 들어왔으므로 임계값에 따라 처리
	if (((*flashmem)->victim_block_info.victim_block_num != DYNAMIC_MAPPING_INIT_VALUE) && (*flashmem)->victim_block_info.victim_block_invalid_ratio != -1)
	{
		if((*flashmem)->victim_block_info.victim_block_invalid_ratio >= this->invalid_ratio_threshold) //임계값보다 같거나 크면 삽입
			((*flashmem)->victim_block_queue->enqueue((*flashmem)->victim_block_info));

		//사용 후 다음 Victim Block 선정 위한 정보 초기화
		(*flashmem)->victim_block_info.clear_all();
		return SUCCESS;
	}
	else
		return FAIL;
}

void GarbageCollector::set_invalid_ratio_threshold(class FlashMem** flashmem) //현재 기록 가능한 스토리지 용량에 따른 가변적 무효율 임계값 설정
{
	unsigned int written_sector_count = (*flashmem)->v_flash_info.written_sector_count; //현재 플래시 메모리의 기록된 섹터 수
	F_FLASH_INFO f_flash_info = (*flashmem)->get_f_flash_info(); //플래시 메모리 생성 시 결정되는 고정된 정보

	/***
		물리적으로 남아있는 기록 가능 공간 = 전체 byte단위 값 - (기록된 섹터 수 * 섹터 당 바이트 값)
		=> 사용자에게 보여지지 않는 용량이므로, Spare Block을 포함시킨다.
	***/
	unsigned int physical_free_space = f_flash_info.storage_byte - (written_sector_count * SECTOR_INC_SPARE_BYTE);

	/***
		- 최소 임계값 0.03125 (블록 하나에 대해 빈 공간은 신경쓰지 않고 하나의 페이지가 무효화되었을 때의 무효율)
		- 최대 임계값 1.0 (블록 하나에 대해 블록 당 페이지 수(32)만큼 무효화되었을 때의 무효율)
	***/

	//전체 용량에 대한 물리적으로 남아있는 기록 가능 공간 비율로 임계값 설정
	try
	{
		float result_invalid_ratio_threshold = (float)physical_free_space / (float)f_flash_info.storage_byte;

		if (result_invalid_ratio_threshold == 0) //전체 용량에 대한 물리적으로 남아있는 기록 가능이 공간 없을 때
			this->invalid_ratio_threshold = 0.03125; //최소 임계값 설정 (1페이지 무효화된 무효율)
		else if (result_invalid_ratio_threshold > 0 && result_invalid_ratio_threshold <= 1)
			this->invalid_ratio_threshold = result_invalid_ratio_threshold;
		else //잘못된 임계값
			throw result_invalid_ratio_threshold;
	}
	catch (float result_invalid_ratio_threshold)
	{
		fprintf(stderr, "오류 : 잘못된 임계값(%f)", &result_invalid_ratio_threshold);
		system("pause");
		exit(1);
	}
}