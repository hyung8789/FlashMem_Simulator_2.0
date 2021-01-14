#include "Build_Options.h"

// Garbage Collecter 구현을 위한 클래스 정의

GarbageCollector::GarbageCollector() 
{
	this->RDY_terminate = false;
}

GarbageCollector::~GarbageCollector() {}

int GarbageCollector::scheduler(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type) //main scheduling function for GC
{
	F_FLASH_INFO f_flash_info;

	if (flashmem == NULL || mapping_method == MAPPING_METHOD::NONE) //플래시 메모리가 할당되어있고, 매핑 방식을 사용할 경우에만 수행
		goto END_EXE_COND_EXCEPTION;

	switch (this->RDY_terminate)
	{
	case true: //시스템 종료 대기 상태
		goto TERMINATE_PROC;

	case false:
		break;
	}

	switch (flashmem->v_flash_info.flash_state)
	{
	case FLASH_STATE::INIT: //가변적 플래시 메모리 정보를 재구성 과정에서 완전 무효화된 블록에 대해서 Victim Block 대기열에 삽입만 수행, 이에 따라, 대기열이 가득 찰 경우 전체 처리
		this->enqueue_job(flashmem, mapping_method, table_type);

		if (flashmem->victim_block_queue->is_full())
			this->all_dequeue_job(flashmem, mapping_method, table_type); //VIctim Block 큐 내의 모든 Victim Block들에 대해 처라

		return SUCCESS;
	
	default:
		break;
	}

	f_flash_info = flashmem->get_f_flash_info(); //플래시 메모리 생성 시 결정되는 고정된 정보

	printf("\n-----------------------------------\n");
	printf("※ Starting GC Scheduling Task...\n");

	if (this->enqueue_job(flashmem, mapping_method, table_type) == SUCCESS)
		printf("- enqueue job performed\n");

	switch (flashmem->v_flash_info.flash_state) //현재 플래시 메모리의 작업 상태에 따라
	{
	case FLASH_STATE::IDLE: //When the write operation ends
		if (flashmem->victim_block_queue->is_empty()) //Victim Block 대기열이 빈 경우
		{
			if(mapping_method == MAPPING_METHOD::HYBRID_LOG)
				if (flashmem->empty_block_queue->is_empty()) //사용 할 수 있는 빈 물리 블록이 없으면
				{
					full_merge(flashmem, mapping_method, table_type); //테이블 참조하여 모든 블록에 대하여 가능 할 경우 Merge 수행
					goto END_SUCCESS;
				}

			goto END_COMPLETE; //do nothing
		}
		else //비어있지 않거나, 가득 찬 경우
		{
			this->all_dequeue_job(flashmem, mapping_method, table_type); //Victim Block 대기열 내의 모든 Victim Block들에 대해 처리
			printf("- all dequeue job performed\n");

			goto END_SUCCESS;
		}
	case FLASH_STATE::WRITES: //When write operation occurs
	case FLASH_STATE::WRITE: //When write operation occurs
		if (flashmem->victim_block_queue->is_full()) //Victim Block 대기열이 가득 찬 경우
		{
			this->all_dequeue_job(flashmem, mapping_method, table_type); //Victim Block 대기열 내의 모든 Victim Block들에 대해 처리
			printf("- all dequeue job performed\n");

			goto END_SUCCESS;
		}

		goto END_COMPLETE; //do nothing

	case FLASH_STATE::FORCE_GC: //강제 가비지 컬렉션 실시
		if (mapping_method == MAPPING_METHOD::HYBRID_LOG)
			full_merge(flashmem, mapping_method, table_type);

		this->all_dequeue_job(flashmem, mapping_method, table_type); //Victim Block 대기열 내의 모든 Victim Block들에 대해 처리
		printf("- all dequeue job performed (Force GC)\n");

		goto END_SUCCESS;
	}

END_EXE_COND_EXCEPTION:
	return FAIL;

END_COMPLETE:
	printf("End with no Operation\n");
	printf("-----------------------------------\n");
	return COMPLETE;

END_SUCCESS:
	printf("Success\n");
	printf("-----------------------------------\n");
	return SUCCESS;

NO_PHYSICAL_SPACE_EXCEPTION_ERR:
	printf("All physical spaces are used\n");
	printf("-----------------------------------\n");
	system("pause");
	exit(1);

TERMINATE_PROC:
	this->all_dequeue_job(flashmem, mapping_method, table_type); //모든 Victim Block을 빼와서 처리
	flashmem->save_table(mapping_method);
	delete flashmem;

	exit(1);
}


int GarbageCollector::one_dequeue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type) //Victim Block 큐로부터 하나의 Victim Block을 빼와서 처리
{
	//블록 매핑 : 해당 Victim Block은 항상 무효화되어 있으므로 단순 Erase 수행
	//하이브리드 매핑 : 무효화된 블록일 경우 단순 Erase, 아닐 경우 Merge 수행

	victim_block_element victim_block;
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼

	spare_block_num empty_spare_block_num = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int empty_spare_block_index = DYNAMIC_MAPPING_INIT_VALUE;

	/***
		<블록 매핑>

		Overwrite발생 시 해당 PBN은 무조건 무효화, Wear-leveling을 위한 Spare Block과 교체하여 새로운 데이터 및 기존 유효 데이터들
		기록 및 기존 블록은 Victim Block으로 선정 => 해당 PBN에 대한 Erase 수행
		
		<하이브리드 매핑>

		1) Log Algorithm을 적용한 하이브리드 매핑은 LBN에 대응된 PBN1 또는 PBN2의 모든 데이터가 무효화될시에 블록 단위 매핑 테이블에서 해제 후 Victim Blok으로 선정
		2) Wear-leveling을 위한 Spare Block과 교체, Victim Block은 Erase 수행 후 Spare Block 대기열에 추가, 교체 된 Spare Block은 Empty Block 대기열에 추가

		LBN에 PBN1, PBN2 모두 대응되어 있고(즉, 한쪽이라도 블록이 무효화되지않음), 기록공간 확보를 위해 LBN을 Victim Block으로 선정 => 해당 LBN에 대한 Merge 수행
		
		만약, 모든 물리적 공간이 유효 데이터들로 기록 되어 있고 (Empty Block Queue에 빈 블록이 존재하지 않음),
		더 이상 새로운 빈 물리 블록을 할당 할 수 없는 상황에서, 기존 데이터에 대해 Overwrite가 발생한다면,
		블록 매핑의 방식처럼, Spare Block과 교체하여 새로운 데이터 및 기존 유효 데이터들 기록 및 기존 블록은 Victim Block으로 선정 => 해당 PBN에 대한 Erase 수행
		---
		=> 블록 매핑의 경우 Overwrite 시 여분의 빈 Spare Block을 활용하여 기록 수행 및 이전 블록은 무효화 처리되어 Wear-leveling을 위해 다른 빈 Spare Block과 교체를 수행한다.
		이와 비교하여, 하이브리드 매핑의 경우 Overwrite 시 PBN1 또는 PBN2의 모든 데이터가 무효화될 시에 PBN1의 경우 PBN2에 새로운 데이터가 기록 될 것이고, PBN2의 경우 PBN1에 기록될 것이다.
		무효화 처리된 블록은 Spare Block과 교체 작업을 수행 하고, 
		Victim Block으로 선정하되, 기록 공간 확보를 위해 (미리 교체하여 빈 블록을 대응 시킬 경우, 다른 LBN에서 사용 불가능)
		바로 여분의 Spare Block과 교체는 수행하지 않고 매핑 테이블에서 Unlink만 수행한다.
		GC에 의해 무효화 처리된 블록을 Erase하고, Wear-leveling을 위한 여분의 빈 Spare Block과 교체한다.
	***/
	if (flashmem->victim_block_queue->dequeue(victim_block) == SUCCESS)
	{
		switch (victim_block.proc_state) //해당 Victim Block의 현재 처리된 상태에 따라
		{
		case VICTIM_BLOCK_PROC_STATE::UNLINKED:
			/***
				해당 블록(PBN)은 어떠한 매핑 테이블에도 대응되어 있지 않음
				erase 수행 시 0xff 값으로 모두 초기화되므로,
				erase 수행 전(NORMAL_BLOCK_INVALID) => erase 수행 후(NORMAL_BLOCK_EMPTY) => SPARE_BLOCK_EMPTY로 변경 및 Wear-leveling을 위해 Spare Block과 교체, 해당 블록은 빈 블록 대기열에 추가
			***/
			Flash_erase(flashmem, victim_block.victim_block_num);

			SPARE_read(flashmem, (victim_block.victim_block_num * BLOCK_PER_SECTOR), meta_buffer);
			meta_buffer->set_block_state(BLOCK_STATE::SPARE_BLOCK_EMPTY);
			SPARE_write(flashmem, (victim_block.victim_block_num * BLOCK_PER_SECTOR), meta_buffer); //해당 블록의 첫 번째 페이지에 meta정보 기록 

			if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;

			/*** 해당 블록은 어떠한 매핑 테이블에도 대응되어 있지 않다. Wear-leveling을 위하여 Victim Block을 Spare Block과 교체 ***/
			if (flashmem->spare_block_queue->dequeue(flashmem, empty_spare_block_num, empty_spare_block_index) == FAIL)
				goto SPARE_BLOCK_EXCEPTION_ERR;

			flashmem->spare_block_queue->queue_array[empty_spare_block_index] = victim_block.victim_block_num;

			SPARE_read(flashmem, (empty_spare_block_num * BLOCK_PER_SECTOR), meta_buffer);
			meta_buffer->set_block_state(BLOCK_STATE::NORMAL_BLOCK_EMPTY);
			SPARE_write(flashmem, (empty_spare_block_num * BLOCK_PER_SECTOR), meta_buffer); //해당 블록의 첫 번째 페이지에 meta정보 기록 

			if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;

			if (table_type == TABLE_TYPE::DYNAMIC) //Victim Block이 어디에도 대응되지 않는 경우는 Dynamic Table 경우밖에 없음
				flashmem->empty_block_queue->enqueue(empty_spare_block_num); //교체 된 Spare Block을 Empty Block 대기열에 추가 (Dynamic Table)

			break;

		case VICTIM_BLOCK_PROC_STATE::SPARE_LINKED:
			/***
				해당 블록은 비어있는 Spare Block을 사용하여 기존의 유효 데이터 및 새로운 데이터 기록을 수행하기 위해 교체 작업이 수행되어, Spare Block 대기열에 대응되어 있음
				erase 수행 시 0xff 값으로 모두 초기화되므로,
				erase 수행 전(NORMAL_BLOCK_INVALID) => erase 수행 후(NORMAL_BLOCK_EMPTY) => SPARE_BLOCK_EMPTY로 변경
			***/

			Flash_erase(flashmem, victim_block.victim_block_num);

			SPARE_read(flashmem, (victim_block.victim_block_num * BLOCK_PER_SECTOR), meta_buffer);
			meta_buffer->set_block_state(BLOCK_STATE::SPARE_BLOCK_EMPTY);
			SPARE_write(flashmem, (victim_block.victim_block_num * BLOCK_PER_SECTOR), meta_buffer); //해당 블록의 첫 번째 페이지에 meta정보 기록 

			if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;

			break;

		case VICTIM_BLOCK_PROC_STATE::UNPROCESSED_FOR_MERGE: //현재 사용하지 않는 기능
			if (!victim_block.is_logical) //물리 블록 번호(PBN)일 경우 오류
				goto WRONG_VICTIM_BLOCK_PROC_STATE;

			full_merge(flashmem, victim_block.victim_block_num, mapping_method, table_type);

			break;

		default:
			goto WRONG_VICTIM_BLOCK_PROC_STATE;
		}
	}
	else
		return FAIL;

END_SUCCESS:
	return SUCCESS;

SPARE_BLOCK_EXCEPTION_ERR:
	if (VICTIM_BLOCK_QUEUE_RATIO != SPARE_BLOCK_RATIO)
		fprintf(stderr, "Spare Block Queue에 할당된 크기의 공간 모두 사용 : 미구현, GC에 의해 처리되도록 해야한다.\n");
	else
	{
		fprintf(stderr, "치명적 오류 : Spare Block Queue 및 GC Scheduler에 대한 예외 발생 (one_dequeue_job)\n");
		system("pause");
		exit(1);
	}
	return FAIL;

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (one_dequeue_job)\n");
	system("pause");
	exit(1);

WRONG_VICTIM_BLOCK_PROC_STATE:
	fprintf(stderr, "치명적 오류 : Wrong VICTIM_BLOCK_PROC_STATE (one_dequeue_job)\n");
	system("pause");
	exit(1);
}

int GarbageCollector::all_dequeue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type) //Victim Block 큐의 모든 Victim Block을 빼와서 처리
{
	while (1) //큐가 빌 때까지 작업
	{
		if((this->one_dequeue_job(flashmem, mapping_method, table_type) != SUCCESS))
			break;
	}

	return SUCCESS;
}


int GarbageCollector::enqueue_job(class FlashMem*& flashmem, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type) //Victim Block 큐에 삽입
{
	if (flashmem->victim_block_info.victim_block_num != DYNAMIC_MAPPING_INIT_VALUE)
	{
		(flashmem->victim_block_queue->enqueue(flashmem->victim_block_info));

		//사용 후 다음 Victim Block 선정 위한 정보 초기화
		flashmem->victim_block_info.clear_all();
		return SUCCESS;
	}

	return FAIL;
}