#include "Build_Options.h"

// init, read, write, erase 함수 정의
// 플래시 메모리에 대해 물리적으로 접근하여 작업

int init(FlashMem*& flashmem, unsigned short megabytes, MAPPING_METHOD mapping_method, TABLE_TYPE table_type) //megabytes 크기의 플래시 메모리를 생성
{
	FILE* storage = NULL, //플래시 메모리 스토리지 파일 포인터
		* volume = NULL;  //생성한 플래시 메모리의 정보 (MB단위의 크기, 매핑 방식, 테이블 타입)를 저장하기 위한 파일 포인터

	//for block mapping
	unsigned int* block_level_mapping_table = NULL; //블록 단위 매핑 테이블
	//for hybrid mapping
	unsigned int** log_block_level_mapping_table = NULL;  //1 : 2 블록 단위 매핑 테이블(index : LBN, row : 전체 PBN의 수, col : PBN1,PBN2)
	__int8* offset_level_mapping_table = NULL; //오프셋 단위(0~31) 매핑 테이블

	unsigned char* data_inc_spare_array = NULL; //데이터 영역 + Spare Area의 char 배열
	unsigned char* spare_block_array = NULL; //데이터 영역 + Spare Area의 Spare Block에 대한 char 배열(Spare Area에 대한 초기값 지정)

	unsigned int init_next_pos = 0; //기록을 위한 파일 포인터의 다음 위치
	unsigned int spare_block_num = DYNAMIC_MAPPING_INIT_VALUE; //Spare Block 대기열에 할당 위한 Spare Block 번호
	bool write_spare_block_proc = false;

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보

	//기존 플래시 메모리 제거 후 재 생성
	if (flashmem != NULL)
	{
		delete flashmem;
		flashmem = NULL;
	}
	remove("rr_read_index.txt"); //기존 Spare Block Queue의 read_index 제거

	flashmem = new FlashMem(megabytes); //새로 할당

	//Spare Block을 포함하던 안하던 생성해야 하는 전체 섹터(블록) 수는 같음
	//섹터마다 Spare Area를 포함(512+16byte)하여, Spare Area를 고려하지 않은(512byte) 섹터 수(sector_size) 만큼 만들어야 함
	f_flash_info = flashmem->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	if (((storage = fopen("storage.bin", "wb")) == NULL) || ((volume = fopen("volume.txt", "wt")) == NULL)) //스토리지 파일, 스토리지 정보 파일
		goto NULL_FILE_PTR_ERR;

	/*** 매핑 테이블, Empty Block Queue, Spare Block Queue 생성 및 초기화 ***/
	spare_block_num = f_flash_info.block_size - 1; //전체 블록의 맨 뒤에서부터 순차적으로 초기 Spare Block 할당을 위한 전체 블록 수-1 

	switch (mapping_method) 
	{
	default:
		break;

	case MAPPING_METHOD::BLOCK: //블록 매핑
		block_level_mapping_table = new unsigned int[f_flash_info.block_size - f_flash_info.spare_block_size]; //Spare 블록 수를 제외한 만큼의 매핑 테이블 생성
		flashmem->spare_block_queue = new Spare_Block_Queue(f_flash_info.spare_block_size + 1);
		
		if(table_type == TABLE_TYPE::DYNAMIC) //Static Table의 경우 쓰기 발생 시 빈 블록 할당이 필요 없으므로, Empty Block Queue를 사용하지 않는다.
			flashmem->empty_block_queue = new Empty_Block_Queue((f_flash_info.block_size - f_flash_info.spare_block_size) + 1);

		//Spare 블록은 전체 블록의 맨 뒤에서부터 순차적으로 할당
		for (unsigned int i = 0; i < f_flash_info.spare_block_size; i++)
		{
			if (flashmem->spare_block_queue->enqueue(spare_block_num--) == FAIL)
			{
				fprintf(stderr, "치명적 오류 : Spare Block Queue 초기 할당 오류\n");
				system("pause");
				exit(EXIT_FAILURE);
			}
		}

		//table type에 따른 테이블 초기화, Static Table의 경우 Empty Block Queue를 사용하지 않음
		if (table_type == TABLE_TYPE::STATIC)
		{
			for (unsigned int table_index = 0; table_index < f_flash_info.block_size - f_flash_info.spare_block_size; table_index++)
			{
				block_level_mapping_table[table_index] = table_index;
			}
		}
		else //Dynamic table
		{
			for (unsigned int table_index = 0; table_index < f_flash_info.block_size - f_flash_info.spare_block_size; table_index++)
			{
				block_level_mapping_table[table_index] = DYNAMIC_MAPPING_INIT_VALUE;
				flashmem->empty_block_queue->enqueue(table_index); //비어있는 PBN에 대해 Empty Block Queue로 할당
			}
		}
		break;

	case MAPPING_METHOD::HYBRID_LOG: //하이브리드 매핑 (log algorithm - 1:2 block level mapping)
		log_block_level_mapping_table = new unsigned int*[f_flash_info.block_size - f_flash_info.spare_block_size]; //row : 전체 PBN의 수
		offset_level_mapping_table = new __int8[f_flash_info.block_size * BLOCK_PER_SECTOR]; //오프셋 단위 테이블(Spare Block 포함)
		flashmem->spare_block_queue = new Spare_Block_Queue(f_flash_info.spare_block_size + 1);
		flashmem->empty_block_queue = new Empty_Block_Queue((f_flash_info.block_size - f_flash_info.spare_block_size) + 1);

		//Spare 블록은 전체 블록의 맨 뒤에서부터 순차적으로 할당
		for (unsigned int i = 0; i < f_flash_info.spare_block_size; i++)
		{
			if (flashmem->spare_block_queue->enqueue(spare_block_num--) == FAIL)
			{
				fprintf(stderr, "치명적 오류 : Spare Block Queue 초기 할당 오류\n");
				system("pause");
				exit(EXIT_FAILURE);
			}
		}

		for (unsigned int table_index = 0; table_index < f_flash_info.block_size - f_flash_info.spare_block_size; table_index++) //row1:PBN1
		{
			log_block_level_mapping_table[table_index] = new unsigned int[2]; //col : 두 공간은 각각 PBN1, PBN2를 나타냄
			log_block_level_mapping_table[table_index][0] = DYNAMIC_MAPPING_INIT_VALUE; //PBN1에 해당하는 위치
			log_block_level_mapping_table[table_index][1] = DYNAMIC_MAPPING_INIT_VALUE; //PBN2에 해당하는 위치
			flashmem->empty_block_queue->enqueue(table_index); //비어있는 PBN에 대해 Empty Block Queue로 할당
		}

		for (unsigned int table_index = 0; table_index < f_flash_info.block_size * BLOCK_PER_SECTOR; table_index++)
		{
			offset_level_mapping_table[table_index] = OFFSET_MAPPING_INIT_VALUE;
		}
	
		break;
	}

	/*** 테이블 메모리에 캐싱 및 저장 ***/
	switch (mapping_method)
	{
	default:
		break;

	case MAPPING_METHOD::BLOCK: //블록 매핑 방식
		//테이블 연결
		flashmem->block_level_mapping_table = block_level_mapping_table;
		break;

	case MAPPING_METHOD::HYBRID_LOG: //하이브리드 매핑 (log algorithm - 1:2 block level mapping)
		//테이블 연결
		flashmem->log_block_level_mapping_table = log_block_level_mapping_table;
		flashmem->offset_level_mapping_table = offset_level_mapping_table;
		break;
	}
	flashmem->save_table(mapping_method);

	/*** 매핑 방식을 사용할 경우 GC를 위한 Victim Block 큐 생성 ***/
	switch (mapping_method)
	{
	case MAPPING_METHOD::NONE:
		break;

	default:
		flashmem->victim_block_queue = new Victim_Block_Queue(round(f_flash_info.block_size * VICTIM_BLOCK_QUEUE_RATIO));
		break;
	}

	/*** Spare Area를 포함한 1섹터 크기의 data_inc_spare_array 생성 ***/
	data_inc_spare_array = new unsigned char[SECTOR_INC_SPARE_BYTE]; //Spare Area를 포함한 섹터(528바이트) 크기의 배열
	memset(data_inc_spare_array, NULL, SECTOR_INC_SPARE_BYTE); //NULL값으로 모두 초기화 (바이트 단위)
	for (int byte_unit = SECTOR_PER_BYTE; byte_unit < SECTOR_INC_SPARE_BYTE - 1; byte_unit++) //섹터 내(0~527)의 512 ~ 527 까지 Spare Area에 대해 할당
	{
		data_inc_spare_array[byte_unit] = SPARE_INIT_VALUE; //0xff(16) = 11111111(2) = 255(10) 로 초기화
	}

	/*** 
		Spare Block에 대한 1섹터 크기의 spare_block_array 생성 
		0 ~ 511 : Data area
		512 ~ 527 : Spare area
	***/
	if (mapping_method != MAPPING_METHOD::NONE) //매핑 방식 사용 시
	{
		spare_block_array = new unsigned char[SECTOR_INC_SPARE_BYTE];
		memset(spare_block_array, NULL, SECTOR_INC_SPARE_BYTE);
		spare_block_array[SECTOR_PER_BYTE] = (0x7f); //0x7f(16) = 01111111(2) = 127(10) 로 초기화

		for (int byte_unit = SECTOR_PER_BYTE + 1; byte_unit < SECTOR_INC_SPARE_BYTE; byte_unit++) //섹터 내(0~527)의 513 ~ 527 까지 Spare Area에 대해 할당
		{
			spare_block_array[byte_unit] = SPARE_INIT_VALUE; //0xff(16) = 11111111(2) = 255(10)로 초기화
		}
	}

	/*** 플래시 메모리 스토리지 파일 생성 ***/
	init_next_pos = ftell(storage);
	write_spare_block_proc = false;

	if (mapping_method == MAPPING_METHOD::NONE) //non-FTL
	{
		while (1) //입력받은 MB만큼 파일에 기록
		{

			fwrite(data_inc_spare_array, sizeof(unsigned char), SECTOR_INC_SPARE_BYTE, storage); //데이터 저장 공간 기록
			init_next_pos += SECTOR_INC_SPARE_BYTE;

			printf("%ubytes / %ubytes (%.1f%%)\r", init_next_pos, f_flash_info.storage_byte, ((float)init_next_pos / (float)(f_flash_info.storage_byte)) * 100);
			if (init_next_pos >= f_flash_info.storage_byte) break; //다음에 기록할 위치가 Spare Area를 포함한 저장공간의 용량을 넘을 경우 종료
		}
	}
	else //Spare Block 할당
	{
		while (1) //입력받은 MB만큼 파일에 기록
		{
			if (!(write_spare_block_proc))
			{
				fwrite(data_inc_spare_array, sizeof(unsigned char), SECTOR_INC_SPARE_BYTE, storage); //데이터 저장 공간 기록
				init_next_pos += SECTOR_INC_SPARE_BYTE;
			}
			else
			{
				//첫번째 오프셋에 대해서만 Spare Block 정보를 변경한 spare_block_array로 기록
				fwrite(spare_block_array, sizeof(unsigned char), SECTOR_INC_SPARE_BYTE, storage); //데이터 저장 공간 (Spare block) 기록
				for (__int8 offset_index = 1; offset_index < BLOCK_PER_SECTOR; offset_index++)
				{
					fwrite(data_inc_spare_array, sizeof(unsigned char), SECTOR_INC_SPARE_BYTE, storage); //데이터 저장 공간 (Spare block) 기록
				}
				init_next_pos += (SECTOR_INC_SPARE_BYTE * BLOCK_PER_SECTOR);
			}
			
			if (init_next_pos >= f_flash_info.storage_byte - f_flash_info.spare_block_byte) //다음에 기록할 위치가 초기 Spare Block 위치인 경우
				write_spare_block_proc = true; 

			printf("%ubytes / %ubytes (%.1f%%)\r", init_next_pos, f_flash_info.storage_byte, ((float)init_next_pos / (float)f_flash_info.storage_byte) * 100);
			if (init_next_pos >= f_flash_info.storage_byte) 
				break; //다음에 기록할 위치가 Spare Area를 포함한 저장공간의 용량을 넘을 경우 종료
		}
	}

	/*** 스토리지 파일 생성 위한 버퍼 제거 ***/
	delete[] data_inc_spare_array;
	delete[] spare_block_array;

	/*** 플래시 메모리 용량 및 매핑 방법 기록 ***/
	fprintf(volume, "%hd\n", f_flash_info.flashmem_size); //생성한 플래시 메모리의 MB 크기
	fprintf(volume, "%d\n", mapping_method); //매핑 방식
	fprintf(volume, "%d", table_type); //테이블 타입

	printf("\n%u megabytes flash memory\n", f_flash_info.flashmem_size);

	fclose(storage);
	fclose(volume);

	flashmem->v_flash_info.flash_state = FLASH_STATE::IDLE;

	return SUCCESS;

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 혹은 volume.txt 파일을 쓰기모드로 열 수 없습니다. (init)\n");
	system("pause");
	exit(EXIT_FAILURE);
}

int Flash_read(FlashMem*& flashmem, class META_DATA*& dst_meta_buffer, unsigned int PSN, char& dst_data) //물리 섹터에 데이터를 읽어옴
{
	FILE* storage = NULL;
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼, FTL 알고리즘을 위해 dst_buffer로 전달

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	unsigned int read_pos = 0; //읽고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점
	char read_buffer = NULL; //읽어들인 데이터

	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}
	f_flash_info = flashmem->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	if (PSN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - 1)) //범위 초과 오류
	{
		fprintf(stderr, "out of range error\n");
		return FAIL;
	}

	if ((storage = fopen("storage.bin", "rb")) == NULL) //읽기 + 이진파일 모드
		goto NULL_FILE_PTR_ERR;

	read_pos = SECTOR_INC_SPARE_BYTE * PSN; //읽고자 하는 물리 섹터(페이지)의 위치
	spare_pos = read_pos + SECTOR_PER_BYTE; //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)

	if (dst_meta_buffer == NULL) //상위 계층에서 meta정보 요청 시 meta정보 전달
	{
		fseek(storage, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동
		SPARE_read(flashmem, storage, meta_buffer);

		//현재 파일 포인터의 위치는 읽고자 하는 물리 섹터(페이지)의 다음 섹터(페이지)의 시작 위치
		fseek(storage, -SECTOR_INC_SPARE_BYTE, SEEK_CUR); //읽고자 하는 물리 섹터(페이지)의 위치로 다시 이동
		fread(&read_buffer, sizeof(char), 1, storage); //해당 물리 섹터(페이지)에 기록된 값 읽기

		dst_meta_buffer = meta_buffer;
	}
	else //상위 계층에서 이미 Spare 판독 함수를 통해 해당 섹터의 meta정보를 판독 하였을 경우, 혹은 Non-FTL일 경우
	{
		fseek(storage, read_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 위치로 이동
		fread(&read_buffer, sizeof(char), 1, storage); //해당 물리 섹터(페이지)에 기록된 값 읽기

		/*** trace위한 정보 기록 ***/
		flashmem->v_flash_info.flash_read_count++; //Global 플래시 메모리 읽기 카운트 증가

#ifdef PAGE_TRACE_MODE //Trace for Per Sector(Page) Wear-leveling
		flashmem->page_trace_info[PSN].read_count++; //해당 섹터(페이지)의 읽기 카운트 증가
#endif

#ifdef BLOCK_TRACE_MODE //Trace for Per Block Wear-leveling
		flashmem->block_trace_info[PSN / BLOCK_PER_SECTOR].read_count++; //해당 블록의 읽기 카운트 증가
#endif

	}

	//읽어들인 데이터 값이 있으면 전달, 없으면 NULL 전달
	dst_data = read_buffer;

	fclose(storage);

	if (read_buffer != NULL)
		return SUCCESS;
	else 
		return COMPLETE;

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 파일을 읽기모드로 열 수 없습니다. (Flash_read)\n");
	system("pause");
	exit(EXIT_FAILURE);
}

int Flash_write(FlashMem*& flashmem, class META_DATA*& src_meta_buffer, unsigned int PSN, const char src_data) //물리 섹터에 데이터를 기록
{
	FILE* storage = NULL;
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	unsigned int write_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점
	
	/*** 이미 입력된 위치에 데이터 입력 시도시 overwrite 오류 발생 ***/

	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}
	f_flash_info = flashmem->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	if (PSN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - 1)) //범위 초과 오류
	{
		fprintf(stderr, "out of range error\n");
		return FAIL;
	}

	if ((storage = fopen("storage.bin", "rb+")) == NULL) //읽고 쓰기 모드 + 이진파일 모드
		goto NULL_FILE_PTR_ERR;

	write_pos = SECTOR_INC_SPARE_BYTE * PSN; //쓰고자 하는 위치
	spare_pos = write_pos + SECTOR_PER_BYTE; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)

	if (src_meta_buffer != NULL) //상위 계층에 기존에 읽어들인 meta정보가 존재할 시에 meta 정보 변경을 위하여 다시 읽지 않는다
	{
		if (src_meta_buffer->get_sector_state() == SECTOR_STATE::EMPTY) //해당 섹터(페이지)가 비어있다면 meta 정보 변경 후 기록
		{
			fseek(storage, write_pos, SEEK_SET); //쓰고자 하는 물리 섹터(페이지)의 위치로 이동
			fwrite(&src_data, sizeof(char), 1, storage); //데이터 기록(1바이트 크기)

			src_meta_buffer->set_sector_state(SECTOR_STATE::VALID);

			//1바이트만큼 기록하였으므로 511바이트만큼 뒤의 Spare area로 이동 
			fseek(storage, (SECTOR_PER_BYTE - 1), SEEK_CUR);
			SPARE_write(flashmem, storage, src_meta_buffer); //새로운 meta정보 기록

			std::cout << "done" << std::endl;
		}
		else //해당 섹터(페이지)가 VALID 혹은 INVALID 
		{
			std::cout << "overwrite error" << std::endl;
			fclose(storage);

			return COMPLETE;
		}
	}
	else //상위 계층에서 기존에 읽어들인 meta정보가 존재하지 않을 시에 meta정보 판별 및 변경 위해 먼저 Spare Area 읽는다
	{
		fseek(storage, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동
		SPARE_read(flashmem, storage, meta_buffer); //해당 섹터(페이지)의 meta정보를 읽는다
		
		if (meta_buffer->get_sector_state() == SECTOR_STATE::EMPTY) //해당 섹터(페이지)가 비어있다면 meta 정보 변경 후 기록
		{
			//현재 파일 포인터의 위치는 읽고자 하는 물리 섹터(페이지)의 다음 섹터(페이지)의 시작 위치
			fseek(storage, -SECTOR_INC_SPARE_BYTE, SEEK_CUR); //쓰고자 하는 물리 섹터(페이지)의 위치로 다시 이동
			fwrite(&src_data, sizeof(char), 1, storage); //데이터 기록(1바이트 크기)

			meta_buffer->set_sector_state(SECTOR_STATE::VALID);

			//1바이트만큼 기록하였으므로 511바이트만큼 뒤의 Spare area로 이동 
			fseek(storage, (SECTOR_PER_BYTE - 1), SEEK_CUR);
			SPARE_write(flashmem, storage, meta_buffer); //새로운 meta정보 기록

			std::cout << "done" << std::endl;
		}
		else
		{
			std::cout << "overwrite error" << std::endl;

			delete meta_buffer;
			fclose(storage);

			return COMPLETE;
		}
	}

	/***
		상위 계층에서 이미 읽어들인 meta 정보를 포함하여 기록을 요구하였을 경우, 사용된 meta 정보의 메모리 해제는 해당 계층에서 수행
		현재 계층에서 meta 정보를 읽어서 기록을 수행하였을 경우, 사용된 meta 정보의 메모리 해제는 현재 계층에서 수행
	***/
	if (src_meta_buffer == NULL) //현재 계층에서 meta 정보 읽기 및 기록 수행 하였을 경우
		if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
			goto MEM_LEAK_ERR;
		
	/*** for Remaining Space Management ***/
	flashmem->v_flash_info.written_sector_count++; //기록된 섹터 수 증가

	if(storage != NULL)
		fclose(storage);

	return SUCCESS;

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 파일을 읽고 쓰기 모드로 열 수 없습니다. (Flash_write)\n");
	system("pause");
	exit(EXIT_FAILURE);

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (Flash_write)\n");
	system("pause");
	exit(EXIT_FAILURE);
}

int Flash_erase(FlashMem*& flashmem, unsigned int PBN) //물리 블록에 해당하는 데이터를 지움
{
	FILE* storage = NULL;

	char erase_buffer = NULL; //섹터(페이지)단위의 데이터 영역에 지우고자 할 때 덮어씌우는 값

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	unsigned int erase_start_pos = (SECTOR_INC_SPARE_BYTE * BLOCK_PER_SECTOR) * PBN; //지우고자 하는 블록 위치의 시작 
	META_DATA** block_meta_buffer_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태

	/***
		해당 블록이 속한 섹터들에 대해서 모두 erase
		각 섹터들의 Spare area도 초기화
	***/

	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}
	f_flash_info = flashmem->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	if (PBN > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1)) //범위 초과 오류
	{
		fprintf(stderr, "out of range error\n");
		return FAIL;
	}

	if ((storage = fopen("storage.bin", "rb+")) == NULL) //읽고 쓰기모드 + 이진파일 모드 (쓰고 읽기 모드로 열 경우 파일내용이 모두 초기화)
		goto NULL_FILE_PTR_ERR;
	
	/*** 해당 블록에 대해 Erase 수행 전 플래시 메모리의 가변적 정보 갱신 ***/
	/*** for Remaining Space Management ***/
	SPARE_reads(flashmem, PBN, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	update_v_flash_info_for_erase(flashmem, block_meta_buffer_array); //지우기 전 해당 블록내의 모든 섹터(페이지)에 대하여 meta 정보를 통해 가변적 플래시 메모리 정보 갱신

	/*** Deallocate block_meta_buffer_array ***/
	if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
		goto MEM_LEAK_ERR;

	fseek(storage, erase_start_pos, SEEK_SET); //erase하고자 하는 물리 블록의 시작 위치로 이동

	for (__int8 offset_index =  0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		fwrite(&erase_buffer, sizeof(char), 1, storage); //데이터 영역 초기화
		fseek(storage, SECTOR_PER_BYTE - 1, SEEK_CUR); //데이터 기록 시 1byte만 기록하도록 하였으므로, 나머지 데이터 영역(511byte)에 대해 건너뜀

		SPARE_init(flashmem, storage); //Spare area 초기화

#ifdef PAGE_TRACE_MODE //Trace for Per Sector(Page) Wear-leveling
		flashmem->page_trace_info[(erase_start_pos / SECTOR_INC_SPARE_BYTE) + offset_index].erase_count++; //해당 섹터(페이지)의 지우기 카운트 증가
#endif
	}
	
	/*** trarce위한 정보 기록 ***/
	flashmem->v_flash_info.flash_erase_count++; //Global 플래시 메모리 지우기 카운트 증가

#ifdef BLOCK_TRACE_MODE //Trace for Per Block Wear-leveling
	flashmem->block_trace_info[PBN].erase_count++; //해당 블록의 지우기 카운트 증가
#endif

	fclose(storage);

	printf("%u-th block erased\n", PBN);
	return SUCCESS;

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 파일을 읽고 쓰기 모드로 열 수 없습니다. (Flash_erase)\n");
	system("pause");
	exit(EXIT_FAILURE);

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (Flash_erase)\n");
	system("pause");
	exit(EXIT_FAILURE);
}