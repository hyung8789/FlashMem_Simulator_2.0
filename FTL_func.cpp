#include "FlashMem.h"

// Print_table, FTL_read, FTL_write, trace 정의
// 논리 섹터 번호 또는 논리 블록 번호를 매핑테이블과 대조하여 physical_func상의 read, write, erase에 전달하여 작업을 수행

int Print_table(FlashMem** flashmem, int mapping_method, int table_type)  //매핑 테이블 출력
{
	FILE* table_output = NULL; //테이블을 파일로 출력하기 위한 파일 포인터

	unsigned int block_table_size = 0; //블록 매핑 테이블의 크기
	unsigned int spare_table_size = 0; //Spare block 테이블의 크기
	unsigned int offset_table_size = 0; //오프셋 매핑 테이블의 크기

	unsigned int table_index = 0; //매핑 테이블 인덱스

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보

	if (*flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}
	f_flash_info = (*flashmem)->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다


	if ((table_output = fopen("table.txt", "wt")) == NULL)
	{
		fprintf(stderr, "table.txt 파일을 쓰기모드로 열 수 없습니다. (Print_table)\n");

		return FAIL;
	}

	switch (mapping_method) //매핑 방식에 따라 매핑 테이블 출력 설정
	{
	case 1:
		break;

	case 2: //블록 매핑
		block_table_size = f_flash_info.block_size - f_flash_info.spare_block_size; //일반 블록 수
		spare_table_size = f_flash_info.spare_block_size; //Spare block 수

		std::cout << "< Block Mapping Table (LBN -> PBN) >" << std::endl;
		fprintf(table_output, "< Block Mapping Table (LBN -> PBN) >\n");

		while (table_index < block_table_size)
		{
			//할당된 값들만 출력
			if ((*flashmem)->block_level_mapping_table[table_index] == DYNAMIC_MAPPING_INIT_VALUE)
			{
				table_index++;
			}
			else
			{
				printf("%u -> %u\n", table_index, (*flashmem)->block_level_mapping_table[table_index]);
				fprintf(table_output, "%u -> %u\n", table_index, (*flashmem)->block_level_mapping_table[table_index]);

				table_index++;
			}
		}

		std::cout << "\n< Spare Block Table (Index -> PBN) >" << std::endl;
		fprintf(table_output, "\n< Spare Block Table (Index -> PBN) >\n");
		table_index = 0;
		while (table_index < spare_table_size)
		{
			printf("%u -> %u\n", table_index, (*flashmem)->spare_block_table->table_array[table_index]);
			fprintf(table_output, "%u -> %u\n", table_index, (*flashmem)->spare_block_table->table_array[table_index]);

			table_index++;
		}

		break;

	case 3: //하이브리드 매핑 (log algorithm - 1:2 block level mapping)
		block_table_size = f_flash_info.block_size - f_flash_info.spare_block_size; //일반 블록 수
		spare_table_size = f_flash_info.spare_block_size; //Spare block 수
		offset_table_size = f_flash_info.block_size * BLOCK_PER_SECTOR; //오프셋 테이블의 크기

		std::cout << "< Hybrid Mapping Block level Table (LBN -> PBN1, PBN2) >" << std::endl;
		fprintf(table_output, "< Hybrid Mapping Block level Table (LBN -> PBN1, PBN2) >\n");

		while (table_index < block_table_size)
		{
			//할당된 값들만 출력
			if ((*flashmem)->log_block_level_mapping_table[table_index][0] == DYNAMIC_MAPPING_INIT_VALUE &&
				(*flashmem)->log_block_level_mapping_table[table_index][1] == DYNAMIC_MAPPING_INIT_VALUE) //PBN1과 PBN2가 할당되지 않은 경우
			{
				table_index++;
			}
			else if ((*flashmem)->log_block_level_mapping_table[table_index][0] != DYNAMIC_MAPPING_INIT_VALUE &&
				(*flashmem)->log_block_level_mapping_table[table_index][1] == DYNAMIC_MAPPING_INIT_VALUE) //PBN1 할당 PBN2가 할당되지 않은 경우
			{
				printf("%u -> %u, non-assigned\n", table_index, (*flashmem)->log_block_level_mapping_table[table_index][0]);
				fprintf(table_output, "%u -> %u, non-assigned\n", table_index, (*flashmem)->log_block_level_mapping_table[table_index][0]);

				table_index++;
			}
			else if ((*flashmem)->log_block_level_mapping_table[table_index][0] == DYNAMIC_MAPPING_INIT_VALUE &&
				(*flashmem)->log_block_level_mapping_table[table_index][1] != DYNAMIC_MAPPING_INIT_VALUE) //PBN1이 할당되지 않고 PBN2가 할당 된 경우
			{
				printf("%u -> non-assigned, %u\n", table_index, (*flashmem)->log_block_level_mapping_table[table_index][1]);
				fprintf(table_output, "%u -> non-assigned, %u\n", table_index, (*flashmem)->log_block_level_mapping_table[table_index][1]);

				table_index++;
			}
			else //PBN1, PBN2 모두 할당 된 경우
			{
				printf("%u -> %u, %u\n", table_index, (*flashmem)->log_block_level_mapping_table[table_index][0], (*flashmem)->log_block_level_mapping_table[table_index][1]);
				fprintf(table_output, "%u -> %u, %u\n", table_index, (*flashmem)->log_block_level_mapping_table[table_index][0], (*flashmem)->log_block_level_mapping_table[table_index][1]);

				table_index++;
			}
		}

		std::cout << "\n< Spare Block Table (Index -> PBN) >" << std::endl;
		fprintf(table_output, "\n< Spare Block Table (Index -> PBN) >\n");
		table_index = 0;
		while (table_index < spare_table_size)
		{
			printf("%u -> %u\n", table_index, (*flashmem)->spare_block_table->table_array[table_index]);
			fprintf(table_output, "%u -> %u\n", table_index, (*flashmem)->spare_block_table->table_array[table_index]);

			table_index++;
		}

		std::cout << "\n< Hybrid Mapping Offset level Table (Index -> POffset) >" << std::endl;
		fprintf(table_output, "\n< Hybrid Mapping Offset level Table (Index -> POffset) >\n");
		table_index = 0;
		while (table_index < offset_table_size)
		{
			//할당된 값들만 출력
			if ((*flashmem)->offset_level_mapping_table[table_index] == OFFSET_MAPPING_INIT_VALUE)
			{
				table_index++;
			}
			else
			{
				printf("%u -> %d\n", table_index, (*flashmem)->offset_level_mapping_table[table_index]);
				fprintf(table_output, "%u -> %d\n", table_index, (*flashmem)->offset_level_mapping_table[table_index]);
 				table_index++;
			}
		}
		break;

	default:
		fclose(table_output);
		return FAIL;
	}

	fclose(table_output);
	std::cout << ">> table.txt" << std::endl;
	system("notepad table.txt");

	return SUCCESS;
}

int FTL_read(FlashMem** flashmem, unsigned int LSN, int mapping_method, int table_type) //////논리 섹터 또는 논리 블록에 해당되는 매핑테이블 상 물리 섹터 또는 물리 블록의 위치를 반환
{
	unsigned int LBN = DYNAMIC_MAPPING_INIT_VALUE; //해당 논리 섹터가 위치하고 있는 논리 블록
	unsigned int PBN = DYNAMIC_MAPPING_INIT_VALUE; //해당 물리 섹터가 위치하고 있는 물리 블록
	unsigned int PBN1 = DYNAMIC_MAPPING_INIT_VALUE; //데이터 블록
	unsigned int PBN2 = DYNAMIC_MAPPING_INIT_VALUE; //로그 블록
	unsigned int PSN = DYNAMIC_MAPPING_INIT_VALUE; //실제로 저장된 물리 섹터 번호
	unsigned int offset_level_table_index = DYNAMIC_MAPPING_INIT_VALUE; //오프셋 단위 테이블 인덱스

	__int8 Loffset = OFFSET_MAPPING_INIT_VALUE; //블록 내의 논리적 섹터 Offset = 0 ~ 31
	__int8 Poffset = OFFSET_MAPPING_INIT_VALUE; //블록 내의 물리적 섹터 Offset = 0 ~ 31

	char read_buffer = NULL; //물리 섹터로부터 읽어들인 데이터

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼

	if (*flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}
	f_flash_info = (*flashmem)->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	//시스템에서 사용하는 Spare Block의 섹터(페이지)수만큼 제외
	if (LSN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - (f_flash_info.spare_block_size * BLOCK_PER_SECTOR) - 1)) //범위 초과 오류
	{
		fprintf(stderr, "out of range error\n");
		return FAIL;
	}

	switch (mapping_method)
	{
	case 1:
		break;

	case 2: //블록 매핑
		LBN = LSN / BLOCK_PER_SECTOR; //해당 논리 섹터가 위치하고 있는 논리 블록
		PBN = (*flashmem)->block_level_mapping_table[LBN];
		Loffset = Poffset = LSN % BLOCK_PER_SECTOR; //블록 내의 섹터 offset = 0 ~ 31
		PSN = (PBN * BLOCK_PER_SECTOR) + Poffset; //실제로 저장된 물리 섹터 번호

		if (PBN == DYNAMIC_MAPPING_INIT_VALUE) //Dynamic Table 초기값일 경우
			goto NON_ASSIGNED_TABLE;
		else
		{
			meta_buffer = SPARE_read(flashmem, PSN); //Spare 영역을 읽음

#if DEBUG_MODE == 1
			/*** 무효화된 블록 판별 - GC에 의해 아직 처리되지 않았을 경우 예외 테스트 ***/
			if (PSN % BLOCK_PER_SECTOR == 0) //읽을 위치가 블록의 첫 번째 섹터일 경우
			{
				switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block])
				{
				case true: //유효한 블록일 경우
					break;

				case false: //유효하지 않은 블록 - GC에 의해 아직 처리가 되지않았을 경우
					goto INVALID_BLOCK;
					break;
				}
			}
			else //블록의 첫 번째 페이지 Spare 영역을 통한 해당 블록의 무효화 판별
			{
				delete meta_buffer;
				meta_buffer = NULL;

				meta_buffer = SPARE_read(flashmem, (PBN * BLOCK_PER_SECTOR)); //블록의 첫 번째 페이지 Spare 영역을 읽음
				switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block])
				{
				case true: //유효한 블록일 경우
					delete meta_buffer;
					meta_buffer = SPARE_read(flashmem, PSN); //읽을 위치의 Spare 영역을 읽음
					break;

				case false: //유효하지 않은 블록
					goto INVALID_BLOCK;
					break;
				}
			}
#endif

			switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector])
			{
			case true: //비어있는 위치
				goto EMPTY_PAGE;
			
			case false: //비어있지 않은 위치
				Flash_read(flashmem, NULL, PSN, read_buffer); //데이터를 읽어옴
				goto OUTPUT_DATA_SUCCESS;

			}
		}

		break;

	case 3: //하이브리드 매핑 (log algorithm - 1:2 block level mapping)
		/***
			PBN1 : 오프셋을 항상 일치시킴
			PBN2 : 오프셋 단위 테이블을 통하여 읽어들임
		***/
		LBN = LSN / BLOCK_PER_SECTOR; //해당 논리 섹터가 위치하고 있는 논리 블록
		PBN1 = (*flashmem)->log_block_level_mapping_table[LBN][0]; //PBN1
		PBN2 = (*flashmem)->log_block_level_mapping_table[LBN][1]; //PBN2
		Loffset = Poffset = LSN % BLOCK_PER_SECTOR; //블록 내의 섹터 offset = 0 ~ 31
		
		if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //PBN1, PBN2 모두 할당 되지 않은 경우
			goto NON_ASSIGNED_TABLE;

		else if(PBN1 != DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //PBN1만 할당된 경우
		{ 
			PSN = (PBN1 * BLOCK_PER_SECTOR) + Poffset;
			meta_buffer = SPARE_read(flashmem, PSN);
			
			switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector])
			{
			case true: //비어있는 위치

#if DEBUG_MODE == 1
				switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector])
				{
				case true:
					break;

				case false:
					goto WRONG_META_ERR;
				}
#endif
				delete meta_buffer;
				meta_buffer = NULL;

				goto EMPTY_PAGE;

			case false: //비어있지 않은 위치

#if DEBUG_MODE == 1
				//PBN1만 할당되어 있으므로, Overwrite가 한 번도 발생하지 않았다. 따라서, 무효한 데이터는 존재하지 않늗다
				switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector])
				{
				case true:
					break;

				case false:
					goto WRONG_META_ERR;
				}
#endif

				delete meta_buffer;
				meta_buffer = NULL;

				Flash_read(flashmem, NULL, PSN, read_buffer);
				goto OUTPUT_DATA_SUCCESS;
			}	
		}
		else if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 != DYNAMIC_MAPPING_INIT_VALUE) //PBN2만 할당된 경우
		{
			Loffset = LSN % BLOCK_PER_SECTOR; //오프셋 단위 테이블 내에서의 LSN의 논리 오프셋
			offset_level_table_index = (PBN2 * BLOCK_PER_SECTOR) + Loffset; //오프셋 단위 테이블 내에서의 해당 LSN의 index값
			Poffset = (*flashmem)->offset_level_mapping_table[offset_level_table_index]; //LSN에 해당하는 물리 블록 내의 물리적 오프셋 위치
			PSN = (PBN2 * BLOCK_PER_SECTOR) + Poffset;
			meta_buffer = SPARE_read(flashmem, PSN);

			switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector])
			{
			case true: //비어있는 위치

#if DEBUG_MODE == 1
				switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector])
				{
				case true:
					break;

				case false:
					goto WRONG_META_ERR;
				}
#endif
				delete meta_buffer;
				meta_buffer = NULL;

				goto EMPTY_PAGE;

			case false: //비어있지 않은 위치

#if DEBUG_MODE == 1
				/***
					PBN2만 할당되어 있다는 것은 PBN1의 모든 오프셋 위치에 대해 Overwrite가 발생했기 때문
					따라서, PBN2만 할당되어 있는 상황에서 PBN2의 모든 데이터는 유효하다.
				***/
				switch (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector])
				{
				case true:
					break;

				case false:
					goto WRONG_META_ERR;
				}
#endif
				delete meta_buffer;
				meta_buffer = NULL;

				Flash_read(flashmem, NULL, PSN, read_buffer);
				goto OUTPUT_DATA_SUCCESS;
			}
		}
		else //PBN1, PBN2 모두 할당된 경우
		{
			/*** 먼저 PBN2를 읽는다 ***/
			Loffset = LSN % BLOCK_PER_SECTOR; //오프셋 단위 테이블 내에서의 LSN의 논리 오프셋
			offset_level_table_index = (PBN2 * BLOCK_PER_SECTOR) + Loffset; //오프셋 단위 테이블 내에서의 해당 LSN의 index값
			Poffset = (*flashmem)->offset_level_mapping_table[offset_level_table_index]; //LSN에 해당하는 물리 블록 내의 물리적 오프셋 위치
			
			/***
				PBN2의 오프셋 테이블은 초기 값 OFFSET_MAPPING_INIT_VALUE로 할당되어있고,
				항상 유효하고 비어있지 않은 위치만 가리킨다.
				---
				=> 이에 따라, PBN2의 오프셋 테이블이 초기 값이 아니라면, 그 위치는 항상 유효하고 비어있지 않다.
			***/
			if (Poffset != OFFSET_MAPPING_INIT_VALUE) //할당되어 있을 경우 항상 유효한 데이터이다
			{
				PSN = (PBN2 * BLOCK_PER_SECTOR) + Poffset;

#if DEBUG_MODE == 1
				meta_buffer = SPARE_read(flashmem, PSN);
				if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] == true ||
					meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == false) //비어있거나, 무효하면
					goto WRONG_META_ERR;
				
					delete meta_buffer;
					meta_buffer = NULL;
#endif

				Flash_read(flashmem, NULL, PSN, read_buffer);
				goto OUTPUT_DATA_SUCCESS;

			}
			else //할당되어 있지 않을 경우 PBN1을 읽는다
			{
				Loffset = Poffset = LSN % BLOCK_PER_SECTOR; //블록 내의 섹터 offset = 0 ~ 31
				PSN = (PBN1 * BLOCK_PER_SECTOR) + Poffset;
				meta_buffer = SPARE_read(flashmem, PSN);
				
				if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
					meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true) //비어있지 않고, 유효하면
				{
					delete meta_buffer;
					meta_buffer = NULL;

					Flash_read(flashmem, NULL, PSN, read_buffer);
					goto OUTPUT_DATA_SUCCESS;
				}
				goto EMPTY_PAGE;
			}		
		}
		break;

	default:
		return FAIL;
	}

OUTPUT_DATA_SUCCESS:
	if (meta_buffer != NULL)
		delete meta_buffer;
	std::cout << "PSN : " << PSN << std::endl; //물리 섹터 번호 출력
	std::cout << read_buffer << std::endl;

	return SUCCESS;

NON_ASSIGNED_TABLE:
	if (meta_buffer != NULL)
		delete meta_buffer;
	std::cout << "no data (Non-Assigned Block in Table)" << std::endl;

	return COMPLETE;

EMPTY_PAGE:
	if (meta_buffer != NULL)
		delete meta_buffer;
	std::cout << "no data (Empty Page)" << std::endl;

	return COMPLETE;

INVALID_PAGE:
	if (meta_buffer != NULL)
		delete meta_buffer;
	std::cout << "no data (Invalid Page)" << std::endl;

	return COMPLETE;

INVALID_BLOCK:
	if (meta_buffer != NULL)
		delete meta_buffer;
	std::cout << "no data (Invalid Block)" << std::endl;

	return COMPLETE;

WRONG_META_ERR:
	fprintf(stderr, "오류 : 잘못된 meta정보 (FTL_read)\n");
	system("pause");
	exit(1);
}

int FTL_write(FlashMem** flashmem, unsigned int LSN, const char src_data, int mapping_method, int table_type) //논리 섹터 또는 논리 블록에 해당되는 매핑테이블 상 물리 섹터 또는 물리 블록 위치에 기록
{
	char block_read_buffer[BLOCK_PER_SECTOR] = { NULL }; //한 블록 내의 데이터 임시 저장 변수
	__int8 read_buffer_index = 0; //데이터를 읽어서 임시저장하기 위한 read_buffer 인덱스 변수
	
	unsigned int empty_spare_block = DYNAMIC_MAPPING_INIT_VALUE; //기록할 빈 Spare 블록
	unsigned int spare_block_table_index = DYNAMIC_MAPPING_INIT_VALUE; //기록할 빈 Spare 블록의  Spare 블록 테이블 상 인덱스
	unsigned int tmp = DYNAMIC_MAPPING_INIT_VALUE; //테이블 SWAP위한 임시 변수

	//unsigned int empty_spare_PBN_for_valid_data_copy = DYNAMIC_MAPPING_INIT_VALUE; //Overwrite 시 유효 데이터 copy를 위한 빈 물리 블록 (Spare Block 사용)

	unsigned int LBN = DYNAMIC_MAPPING_INIT_VALUE; //해당 논리 섹터가 위치하고 있는 논리 블록
	unsigned int PBN = DYNAMIC_MAPPING_INIT_VALUE; //해당 물리 섹터가 위치하고 있는 물리 블록
	unsigned int PBN1 = DYNAMIC_MAPPING_INIT_VALUE; //해당 물리 섹터가 위치하고 있는 물리 블록(PBN1)
	unsigned int PBN2 = DYNAMIC_MAPPING_INIT_VALUE; //해당 물리 섹터가 위치하고 있는 물리 블록(PBN2)
	unsigned int PSN = DYNAMIC_MAPPING_INIT_VALUE; //실제로 저장된 물리 섹터 번호
	unsigned int offset_level_table_index = DYNAMIC_MAPPING_INIT_VALUE; //오프셋 단위 테이블 인덱스

	__int8 Loffset = OFFSET_MAPPING_INIT_VALUE; //블록 내의 논리적 섹터 Offset = 0 ~ 31
	__int8 Poffset = OFFSET_MAPPING_INIT_VALUE; //블록 내의 물리적 섹터 Offset = 0 ~ 31

	char read_buffer = NULL; //물리 섹터로부터 읽어들인 데이터
	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보

	//Spare area에 기록된 meta-data에 대해 읽어들일 버퍼
	META_DATA* meta_buffer = NULL; 
	META_DATA* PBN1_meta_buffer = NULL; //PBN1에 속한 페이지의 meta 정보
	META_DATA* PBN2_meta_buffer = NULL; //PBN2에 속한 페이지의 meta 정보
	META_DATA** PBN1_block_meta_data_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태 (PBN1)
	META_DATA** PBN2_block_meta_data_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태 (PBN2)
	bool PBN1_write_proc = false;
	bool PBN2_write_proc = false;

	if (*flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}
	f_flash_info = (*flashmem)->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	//시스템에서 사용하는 Spare Block의 섹터(페이지)수만큼 제외
	if (LSN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - (f_flash_info.spare_block_size * BLOCK_PER_SECTOR) - 1)) //범위 초과 오류
	{
		fprintf(stderr, "out of range error\n");
		return FAIL;
	}

	switch (mapping_method) //매핑 방식에 따라 해당 처리 위치로 이동
	{
	case 2: //블록 매핑
		if (table_type == 0) //블록 매핑 Static Table
			goto BLOCK_MAPPING_STATIC;

		else if (table_type == 1) //블록 매핑 Dynamic Table
			goto BLOCK_MAPPING_DYNAMIC;

		else 
			goto WRONG_TABLE_TYPE_ERR;

	case 3: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
		goto HYBRID_LOG_DYNAMIC;

	default:
		goto WRONG_TABLE_TYPE_ERR;
	}

BLOCK_MAPPING_STATIC: //블록 매핑 Static Table
	//사용자가 입력한 LSN으로 LBN을 구하고 대응되는 PBN과 물리 섹터 번호를 구함
	LBN = LSN / BLOCK_PER_SECTOR; //해당 논리 섹터가 위치하고 있는 논리 블록
	Loffset = Poffset = LSN % BLOCK_PER_SECTOR; //블록 내의 섹터 offset = 0 ~ 31
	PBN = (*flashmem)->block_level_mapping_table[LBN]; //실제로 저장된 물리 블록 번호

#if DEBUG_MODE == 1
	//항상 대응되어 있어야 함
	if (PBN == DYNAMIC_MAPPING_INIT_VALUE)
		goto WRONG_STATIC_TABLE_ERR;
#endif

	PSN = (PBN * BLOCK_PER_SECTOR); //해당 블록의 첫 번째 페이지
	meta_buffer = SPARE_read(flashmem, PSN); //Spare 영역을 읽음

	/***
		해당 블록이 무효화된 경우
		---
		블록이 무효화된 시점(즉, 이미 기록된 위치에 대해 Overwrite가 발생)에 Spare Block을 이용한 유효 데이터 copy 및 SWAP 수행
		=> 따라서, 이 경우는 발생하지 않음
	***/
	if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] == false)
	{

#if DEBUG_MODE == 1
		//일반 블록 테이블에 대하여 Spare Block이 대응되어 있거나, 무효한 블록이면, 비어있어서는 안된다.
		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] == true ||
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] == false)
			goto WRONG_META_ERR;
#endif
		delete meta_buffer;
		meta_buffer = NULL;
	}
	/***
		해당 블록이 무효화되지 않은 경우
		---
		1) 해당 블록이 비어있는 경우
		2) 해당 블록이 비어있지 않은 경우
			2-1) 해당 오프셋 위치가 빈 경우
			2-2) 해당 오프셋 위치가 비어있지 않은 경우 (Overwrite)
	***/
	else
	{
		/*** 해당 블록이 비어있는 경우 ***/
		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] == true)
		{
			PSN = (PBN * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치

			if (PSN % BLOCK_PER_SECTOR == 0) //기록 할 위치가 블록의 첫 번째 섹터일 경우 빈 블록 정보 변경
			{
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
				
				//해당 오프셋 위치에 기록
				goto BLOCK_MAPPING_COMMON_WRITE_PROC;
			}
			else //기록 할 위치가 블록의 첫 번째 섹터가 아니면 빈 블록 정보를 먼저 변경
			{
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
				SPARE_write(flashmem, (PBN * BLOCK_PER_SECTOR), &meta_buffer); //해당 블록의 첫 번째 페이지에 meta정보 기록 
				
				delete meta_buffer;
				meta_buffer = NULL;

				//해당 오프셋 위치에 기록
				goto BLOCK_MAPPING_COMMON_WRITE_PROC;
			}
		}
		/*** 해당 블록이 비어있지 않은 경우 ***/
		else
		{
			delete meta_buffer;
			meta_buffer = NULL;

			PSN = (PBN * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치
			meta_buffer = SPARE_read(flashmem, PSN);

			/*** 해당 오프셋 위치가 빈 경우 ***/
			if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] == true)
			{

#if DEBUG_MODE == 1
				//비어있는데 유효하지 않으면
				if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] != true)
					goto WRONG_META_ERR; //잘못된 meta 정보 오류
#endif

				//해당 오프셋 위치에 기록
				goto BLOCK_MAPPING_COMMON_WRITE_PROC;
			}
			/*** 해당 오프셋 위치가 비어있지 않은 경우 ***/
			else
				//현재 오프셋 위치의 meta 정보를 사용하여 블록 매핑 공용 Overwrite 처리 루틴으로 이동
				goto BLOCK_MAPPING_COMMON_OVERWRITE_PROC;
		}
	}

BLOCK_MAPPING_DYNAMIC: //블록 매핑 Dynamic Table
	//사용자가 입력한 LSN으로 LBN을 구하고 대응되는 PBN과 물리 섹터 번호를 구함
	LBN = LSN / BLOCK_PER_SECTOR; //해당 논리 섹터가 위치하고 있는 논리 블록
	Loffset = Poffset = LSN % BLOCK_PER_SECTOR; //블록 내의 섹터 offset = 0 ~ 31
	PBN = (*flashmem)->block_level_mapping_table[LBN]; //실제로 저장된 물리 블록 번호

	/*** LBN에 PBN이 대응되지 않은 경우 ***/
	if (PBN == DYNAMIC_MAPPING_INIT_VALUE)
	{
		if (search_empty_normal_block(flashmem, PBN, &meta_buffer, mapping_method, table_type) != SUCCESS) //빈 일반 블록(PBN)을 순차적으로 탐색하여 PBN 값, 해당 PBN의 meta정보 받아온다
			goto END_NO_SPACE; //기록 가능 공간 부족

		(*flashmem)->block_level_mapping_table[LBN] = PBN;

		PSN = (PBN * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치

		if (PSN % BLOCK_PER_SECTOR == 0) //기록 할 위치가 블록의 첫 번째 섹터일 경우 빈 블록 정보 변경
		{
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;

			//해당 오프셋 위치에 기록
			goto BLOCK_MAPPING_COMMON_WRITE_PROC;
		}
		else //기록 할 위치가 블록의 첫 번째 섹터가 아니면 빈 블록 정보를 먼저 변경
		{
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
			SPARE_write(flashmem, (PBN * BLOCK_PER_SECTOR), &meta_buffer); //해당 블록의 첫 번째 페이지에 meta정보 기록 

			delete meta_buffer;
			meta_buffer = NULL;

			//해당 오프셋 위치에 기록
			goto BLOCK_MAPPING_COMMON_WRITE_PROC;
		}
	}

	/*** 블록의 첫 번째 페이지의 Spare영역을 통한 해당 블록 정보 판별 및 연산 수행 ***/
	PSN = (PBN * BLOCK_PER_SECTOR); //해당 블록의 첫 번째 페이지

#if DEBUG_MODE == 1
	meta_buffer = SPARE_read(flashmem, PSN); //Spare 영역을 읽음

	/*** 
		해당 블록이 무효화된 경우, 해당 블록이 비어있는 경우
		---
		Dynamic Table의 경우 일반 블록 매핑 테이블에 항상 무효화되지않은 블록만 대응시킨다. 
		이에 따라, 무효화되지 않은 블록이 대응되는 경우는 발생하지 않는다.
		블록이 대응되는 시점에, 해당 블록에 쓰기 작업이 발생하므로, 빈 블록이 미리 대응되어 있는 경우는 발생하지 않는다.
	***/
	if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] == false ||
		meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] == true)
		goto WRONG_META_ERR; //잘못된 meta 정보

	delete meta_buffer;
	meta_buffer = NULL;
#endif

	/***
		해당 블록이 무효화되지 않은 경우
		---
		1) 해당 블록이 비어있는 경우 => 발생하지 않음
		2) 해당 블록이 비어있지 않은 경우
			2-1) 해당 오프셋 위치가 빈 경우
			2-2) 해당 오프셋 위치가 비어있지 않은 경우 (Overwrite)
	***/

	/*** 해당 블록이 비어있지 않은 경우 ***/
	PSN = (PBN * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치
	meta_buffer = SPARE_read(flashmem, PSN);

	/*** 해당 오프셋 위치가 빈 경우 ***/
	if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] == true)
	{

#if DEBUG_MODE == 1
		//비어있는데 유효하지 않으면
		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] != true)
			goto WRONG_META_ERR; //잘못된 meta 정보 오류
#endif

		//해당 오프셋 위치에 기록
		goto BLOCK_MAPPING_COMMON_WRITE_PROC;
	}
	/*** 해당 오프셋 위치가 비어있지 않은 경우 ***/
	else
	{
		//현재 오프셋 위치의 meta 정보를 사용하여 블록 매핑 공용 Overwrite 처리 루틴으로 이동
		goto BLOCK_MAPPING_COMMON_OVERWRITE_PROC;
	}


BLOCK_MAPPING_COMMON_WRITE_PROC: //블록 매핑 공용 처리 루틴 1 : 사용되고 있는 블록의 비어 있는 오프셋에 대한 기록 연산
	PSN = (PBN * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치
	
	if(meta_buffer == NULL)
		meta_buffer = SPARE_read(flashmem, PSN);

#if DEBUG_MODE == 1
	if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true)
		goto WRONG_META_ERR; //잘못된 meta 정보 오류
#endif

	//해당 오프셋 위치에 기록
	if (Flash_write(flashmem, &meta_buffer, PSN, src_data) == COMPLETE)
		goto OVERWRITE_ERR;
	
	delete meta_buffer;
	meta_buffer = NULL;

	goto END_SUCCESS; //종료

BLOCK_MAPPING_COMMON_OVERWRITE_PROC: //블록 매핑 공용 처리 루틴 2 : 사용되고 있는 블록의 기존 위치에 대한 Overwrite 연산
	/***
		1) 해당 PBN의 유효한 데이터(새로운 데이터가 기록될 위치 제외) 및 새로운 데이터를 여분의 빈 Spare Block을 사용하여 
		유효 데이터(새로운 데이터가 기록될 위치 제외) copy 및 새로운 데이터 기록
		2) 기존 PBN 내의 모든 섹터 및 해당 블록 무효화
		3) 기존 PBN과 사용된 Spare Block 테이블 상 교체 및 기존 PBN은 Victim Block으로 선정
	***/

	delete meta_buffer;
	meta_buffer = NULL;

	//유효 데이터 복사 (Overwrite할 위치 및 빈 위치를 제외) 및 기존 블록 무효화, 블록 내의 모든 섹터 무효화
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		PSN = (PBN * BLOCK_PER_SECTOR) + offset_index; //해당 블록에 대해 인덱싱

		meta_buffer = SPARE_read(flashmem, PSN);
		
		if (PSN == (PBN * BLOCK_PER_SECTOR) + Poffset || meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] == true) //Overwrite할 기존 위치 또는 빈 위치
		{
			//오프셋을 맞추기 위하여 블록 단위의 버퍼에 빈 공간으로 기록
			block_read_buffer[offset_index] = NULL;
		}
		else
			Flash_read(flashmem, NULL, PSN, block_read_buffer[offset_index]);

		/*** meta 정보 변경 : 블록 및 해당 블록 내의 섹터 무효화 ***/
		if (PSN % BLOCK_PER_SECTOR == 0) //첫 번째 섹터라면 블록 정보 추가로 변경
		{
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] = false; //Spare Block과 SWAP 위해 meta 정보 미리 변경
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] = false;
			
			//비어있는 섹터가 아니면 해당 섹터 무효화
			if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true)
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] = false;

			SPARE_write(flashmem, PSN, &meta_buffer);
		}
		else
		{
			//비어있는 섹터가 아니면 해당 섹터 무효화
			if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true)
			{
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] = false;
				SPARE_write(flashmem, PSN, &meta_buffer);
			}
			//비어있으면 아무것도 하지 않는다.
		}

		delete meta_buffer;
		meta_buffer = NULL;
	}

	//무효화된 PBN을 Victim Block으로 선정 위한 정보 갱신 
	if (update_victim_block_info(flashmem, false, PBN, mapping_method) != SUCCESS)
		goto VICTIM_BLOCK_INFO_EXCEPTION_ERR;

	(*flashmem)->gc->scheduler(flashmem, mapping_method);

	/*** 빈 Spare 블록을 찾아서 기록 ***/
	if ((*flashmem)->spare_block_table->rr_read(flashmem, empty_spare_block, spare_block_table_index) == FAIL)
		goto SPARE_BLOCK_EXCEPTION_ERR;
	
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		PSN = (empty_spare_block * BLOCK_PER_SECTOR) + offset_index; //순차적으로 블록 내의 각 페이지에 대해 인덱싱
		
		if (block_read_buffer[offset_index] != NULL) //블록 단위로 읽어들인 버퍼에서 해당 위치가 비어있지 않으면, 즉 유효한 데이터이면
		{
			meta_buffer = SPARE_read(flashmem, PSN); //섹터(페이지)의 Spare 정보 읽기

			if (PSN % BLOCK_PER_SECTOR == 0) //만약 기록 할 위치가 첫 번째 섹터라면
			{
				//해당 블록은 일반 블록화 될 것, 또한 빈 블록, 빈 섹터 여부 변경
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] = true;
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
			
				if (Flash_write(flashmem, &meta_buffer, PSN, block_read_buffer[offset_index]) == COMPLETE)
					goto OVERWRITE_ERR;
			}
			else
			{
				if (Flash_write(flashmem, &meta_buffer, PSN, block_read_buffer[offset_index]) == COMPLETE)
					goto OVERWRITE_ERR;
			}

			delete meta_buffer;
			meta_buffer = NULL;
		}
		else if (offset_index == Poffset) //블록 단위 버퍼가 비어있고, 기록 할 위치가 Overwrite 할 위치면 새로운 데이터로 기록
		{
			meta_buffer = SPARE_read(flashmem, PSN); //섹터(페이지)의 Spare 정보 읽기
			
			if (PSN % BLOCK_PER_SECTOR == 0) //만약 기록 할 위치가 첫 번째 섹터라면
			{
				//해당 블록은 일반 블록화 될 것, 또한 빈 블록, 빈 섹터 여부 변경
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] = true;
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;

				if (Flash_write(flashmem, &meta_buffer, PSN, src_data) == COMPLETE)
					goto OVERWRITE_ERR;
			}
			else
			{
				if (Flash_write(flashmem, &meta_buffer, PSN, src_data) == COMPLETE)
					goto OVERWRITE_ERR;
			}

			delete meta_buffer;
			meta_buffer = NULL;
		}
		else //비어있는 위치
		{
			//do nothing
		}
	}
	if (block_read_buffer[0] == NULL) //만약 첫 번째 섹터(페이지)에 해당하는 블록 단위 버퍼의 index 0이 비어있어서 해당 블록 정보가 변경되지 않았을 경우 변경
	{
		//해당 블록은 일반 블록화 될 것, 또한 빈 블록 여부 변경
		PSN = empty_spare_block * BLOCK_PER_SECTOR; //해당 블록의 첫 번째 섹터(페이지)로 초기화
		meta_buffer = SPARE_read(flashmem, PSN);
		meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] = true;
		meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
		SPARE_write(flashmem, PSN, &meta_buffer);
		delete meta_buffer;
		meta_buffer = NULL;
	}

	//블록 단위 테이블과 Spare Block 테이블 상에서 SWAP
	SWAP((*flashmem)->block_level_mapping_table[LBN], (*flashmem)->spare_block_table->table_array[spare_block_table_index], tmp);

	goto END_SUCCESS;


HYBRID_LOG_DYNAMIC: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
	LBN = LSN / BLOCK_PER_SECTOR; //해당 논리 섹터가 위치하고 있는 논리 블록
	PBN1 = (*flashmem)->log_block_level_mapping_table[LBN][0]; //실제로 저장된 물리 블록 번호(PBN1)
	PBN2 = (*flashmem)->log_block_level_mapping_table[LBN][1]; //실제로 저장된 물리 블록 번호(PBN2)
	Loffset = Poffset = LSN % BLOCK_PER_SECTOR; //블록 내의 논리 섹터 offset = 0 ~ 31
	PBN1_write_proc = PBN2_write_proc = false;

	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
		goto HYBRID_LOG_DYNAMIC_PBN1_PROC;
	else if (PBN1 != DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
		goto HYBRID_LOG_DYNAMIC_PBN1_PROC;
	else if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 != DYNAMIC_MAPPING_INIT_VALUE)
		goto HYBRID_LOG_DYNAMIC_PBN1_PROC;
	else
		goto HYBRID_LOG_DYNAMIC_PBN1_PROC;

HYBRID_LOG_DYNAMIC_PBN1_PROC: //PBN1에 대한 처리 루틴
	Loffset = Poffset = LSN % BLOCK_PER_SECTOR; //블록 내의 논리 섹터 offset = 0 ~ 31

	/*** 
		PBN2에서의 논리 오프셋 위치가 이전에 PBN1에서 Overwrite되어 PBN2에서 사용되고 있는 위치일 경우 
		PBN1을 PBN2의 기존 데이터에 대하여 1회의 Overwrite가 가능한 Log Block처럼 사용
		Overwrite가 같은 위치에 2번 발생 시 PBN1과 PBN2에 대하여 Merge수행 후 PBN1로 재할당
	***/
	if (PBN2 != DYNAMIC_MAPPING_INIT_VALUE)
	{
		offset_level_table_index = (PBN2 * BLOCK_PER_SECTOR) + Loffset; //오프셋 단위 테이블 내에서의 해당 LSN의 index값

		//PBN2에서 해당 오프셋 위치가 할당되어 있을 경우
		if ((*flashmem)->offset_level_mapping_table[offset_level_table_index] != OFFSET_MAPPING_INIT_VALUE)
		{
			/***
				PBN1을 PBN2의 기존 데이터에 대하여 1회의 Overwrite가 가능한 Log Block처럼 사용
				Overwrite가 같은 위치에 2번 발생 시(즉, PBN2에서 해당 위치가 이미 무효화되었을 경우)
				PBN1과 PBN2에 대하여 Merge수행 후 PBN1로 재할당
			***/

			/***
				기존 페이지 무효화, 만약 해당 블록의 모든 페이지가 무효화되었으면 해당 블록 무효화
				해당 블록의 모든 페이지가 무효화되는 시점에 valid_sector를 false로 set한 후 Spare Area에 기록이 발생하고, 이에 따라 무효 페이지 count 증가
				해당 블록의 모든 페이지의 Spare Area를 통해 모든 페이지가 무효화되었으면, valid_block을 false로 set한 후 블록의 첫 번쩨
				Spare Area에 기록이 발생하므로, 무효 페이지 개수가 다시 count되어 Overflow 발생 (Spare Area의 처리함수에서 meta정보를 통해 무효 페이지 count관리)ㄴ
				---
				블록 단위로 meta정보를 모두 읽어들인 뒤 판별 후 한꺼번에 meta 정보 수정 및 기록
			***/

			PBN2_meta_buffer = SPARE_read(flashmem, ((PBN2 * BLOCK_PER_SECTOR) + (*flashmem)->offset_level_mapping_table[offset_level_table_index]));

			switch (PBN2_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector])
			{
			////////////수정 예정
			case true: //PBN2의 기존 위치 무효화
				/*
				PBN2_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] = false;

				block_meta_data_array = SPARE_reads(flashmem, PBN);
				block_meta_data_array[(*flashmem)->offset_level_mapping_table[offset_level_table_index]]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] = false;

				SPARE_write(flashmem, ((PBN2 * BLOCK_PER_SECTOR) + (*flashmem)->offset_level_mapping_table[offset_level_table_index]), &PBN2_meta_buffer);
				delete PBN2_meta_buffer;
				PBN2_meta_buffer = NULL;
				*/

				(*flashmem)->offset_level_mapping_table[offset_level_table_index] = OFFSET_MAPPING_INIT_VALUE; //오프셋 초기화
				/////////////////////////////
				/*** 이에 따라 만약, PBN2의 모든 데이터가 무효화되었으면, PBN2 무효화 ***/
				if (update_victim_block_info(flashmem, false, PBN2, mapping_method) != SUCCESS)
					goto VICTIM_BLOCK_INFO_EXCEPTION_ERR;

				if ((*flashmem)->victim_block_info.victim_block_invalid_ratio == 1.0)
				{
					PBN2_meta_buffer = SPARE_read(flashmem, (PBN2 * BLOCK_PER_SECTOR));
					PBN2_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] = false;
					SPARE_write(flashmem, (PBN2 * BLOCK_PER_SECTOR), &PBN2_meta_buffer);

					delete PBN2_meta_buffer;
					PBN2_meta_buffer = NULL;

					//PBN2를 블록 단위 매핑 테이블 상에서 Unlink(연결 해제)
					(*flashmem)->log_block_level_mapping_table[LBN][1] = DYNAMIC_MAPPING_INIT_VALUE;

					(*flashmem)->gc->scheduler(flashmem, mapping_method);
				}
				else //GC에 의해 Victim Block으로 선정되지 않도록 구조체 초기화
					(*flashmem)->victim_block_info.clear_all();

				break;

			case false: //Overwrite가 같은 위치에 2번 발생 시 (즉, PBN2에서 해당 위치가 이미 무효화되었을 경우)
					//Merge를 위해서 PBN1, PBN2 모두 대응되어 있어야 한다.
				if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE || PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
					goto MERGE_COND_EXCEPTION_ERR;

				if (full_merge(flashmem, LBN, mapping_method) != SUCCESS) //Merge 수행 후 PBN1로 재 할당
					goto WRONG_META_ERR;

				goto HYBRID_LOG_DYNAMIC; //재 연산

				break;
			}
		}
	}
	
	/*** 만약, PBN1이 할당되어 있지 않을 경우, Spare Block이 아닌 빈 블록을 할당 ***/
	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE)
	{
		if (search_empty_normal_block(flashmem, PBN1, &PBN1_meta_buffer, mapping_method, table_type) != SUCCESS) //빈 일반 블록(PBN)을 순차적으로 탐색하여 PBN 값, 해당 PBN의 meta정보 받아온다
		{
			//기록 공간 없을 경우 Spare Block을 사용
			//현재 쓰기가 발생한 블록의 유효 데이터, 쓸 위치를 제외하고 Spare block에 기록, 새로운 데이터 기록 LBN에 할당
			//수정예정
			goto END_NO_SPACE; //기록 가능 공간 부족
		}
		(*flashmem)->log_block_level_mapping_table[LBN][0] = PBN1;

		PSN = (PBN1 * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치

		if (PSN % BLOCK_PER_SECTOR == 0) //기록 할 위치가 블록의 첫 번째 섹터일 경우 빈 블록 정보 변경
		{
			PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;

			//PBN1의 해당 오프셋 위치에 기록
			PBN1_write_proc = true;
			goto HYBRID_LOG_DYNAMIC_COMMON_WRITE_PROC;
		}
		else //기록 할 위치가 블록의 첫 번째 섹터가 아니면 빈 블록 정보를 먼저 변경
		{
			PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
			SPARE_write(flashmem, (PBN1 * BLOCK_PER_SECTOR), &PBN1_meta_buffer); //해당 블록의 첫 번째 페이지에 meta정보 기록 

			delete PBN1_meta_buffer;
			PBN1_meta_buffer = NULL;

			//PBN1의 해당 오프셋 위치에 기록
			PBN1_write_proc = true;
			goto HYBRID_LOG_DYNAMIC_COMMON_WRITE_PROC;
		}
	}

	/***
		해당 블록이 무효화되지 않은 경우
		---
		1) 해당 블록이 비어있는 경우 => 발생하지 않음
		2) 해당 블록이 비어있지 않은 경우
			2-1) 해당 오프셋 위치가 빈 경우
			=> 해당 위치 기록
			2-2) 해당 오프셋 위치가 유효하고, 비어있지 않은 경우 (Overwrite)
			=> PBN1의 데이터 무효화, PBN2에 기록
			2-2) 해당 오프셋 위치가 무효하고, 비어있지 않은 경우 (Overwrite)
			=> PBN2의 데이터 무효화, PBN2에 기록
	***/

	/*** 해당 블록이 비어있지 않은 경우 ***/
	PSN = (PBN1 * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치
	PBN1_meta_buffer = SPARE_read(flashmem, PSN);

	/*** 해당 오프셋 위치가 빈 경우 ***/
	if (PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] == true)
	{

#if DEBUG_MODE == 1
		//비어있는데 유효하지 않으면
		if (PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] != true)
			goto WRONG_META_ERR; //잘못된 meta 정보 오류
#endif

		//PBN1의 해당 오프셋 위치에 기록
		PBN1_write_proc = true;
		goto HYBRID_LOG_DYNAMIC_COMMON_WRITE_PROC;
	}
	/*** 해당 오프셋 위치가 유효하고, 비어있지 않은 경우 ***/
	else if (PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
		PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true)
	{
		///
		///
		///
		/*** PBN1의 기존 데이터 무효화 ***/
		PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] = false;
		SPARE_write(flashmem, PSN, &PBN1_meta_buffer);
		delete PBN1_meta_buffer;
		PBN1_meta_buffer = NULL;

		/*** 이에 따라, 만약, PBN1의 모든 데이터가 무효화되었으면, PBN1 무효화 ***/
		if (update_victim_block_info(flashmem, false, PBN1, mapping_method) != SUCCESS)
			goto VICTIM_BLOCK_INFO_EXCEPTION_ERR;
		
		/// <summary>
		/// 

		/// </summary>
		/// <param name="flashmem"></param>
		/// <param name="LSN"></param>
		/// <param name="src_data"></param>
		/// <param name="mapping_method"></param>
		/// <param name="table_type"></param>
		/// <returns></returns>
		if ((*flashmem)->victim_block_info.victim_block_invalid_ratio == 1.0)
		{
			PBN1_meta_buffer = SPARE_read(flashmem, (PBN1 * BLOCK_PER_SECTOR));
			PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] = false;
			SPARE_write(flashmem, (PBN1 * BLOCK_PER_SECTOR), &PBN1_meta_buffer);

			delete PBN1_meta_buffer;
			PBN1_meta_buffer = NULL;

			//PBN1를 블록 단위 매핑 테이블 상에서 Unlink(연결 해제)
			(*flashmem)->log_block_level_mapping_table[LBN][0] = DYNAMIC_MAPPING_INIT_VALUE;

			(*flashmem)->gc->scheduler(flashmem, mapping_method);
		}
		else //GC에 의해 Victim Block으로 선정되지 않도록 구조체 초기화
			(*flashmem)->victim_block_info.clear_all();

		/*** PBN2에 새로운 데이터 기록 수행 ***/
		goto HYBRID_LOG_DYNAMIC_PBN2_PROC;
	}
	/*** 해당 오프셋 위치가 무효하고, 비어있지 않은 경우 ***/
	else if (PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
		PBN1_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] != true)
	{
		delete PBN1_meta_buffer;
		PBN1_meta_buffer = NULL;
		
		//PBN2의 데이터 무효화, PBN2에 새로운 데이터 기록 수행
		goto HYBRID_LOG_DYNAMIC_PBN2_PROC;
	}
	else
		goto WRONG_META_ERR;

HYBRID_LOG_DYNAMIC_PBN2_PROC: //PBN2에 대한 처리 루틴
	/*** 만약, PBN2이 할당되어 있지 않을 경우, Spare Block이 아닌 빈 블록을 할당 ***/
	if (PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
	{
		if (search_empty_normal_block(flashmem, PBN2, &PBN2_meta_buffer, mapping_method, table_type) != SUCCESS) //빈 일반 블록(PBN)을 순차적으로 탐색하여 PBN 값, 해당 PBN의 meta정보 받아온다
		{
			//기록 공간 없을 경우 Spare Block을 사용
			//현재 쓰기가 발생한 블록의 유효 데이터, 쓸 위치를 제외하고 Spare block에 기록, 새로운 데이터 기록 LBN에 할당
			//수정예정
			goto END_NO_SPACE; //기록 가능 공간 부족
		}
		(*flashmem)->log_block_level_mapping_table[LBN][1] = PBN2;

		/*** 순차적으로 비어있는 오프셋 위치에 기록 ***/
		if (search_empty_offset_in_block(flashmem, PBN2, Poffset, &PBN2_meta_buffer) == FAIL)
			goto WRONG_META_ERR;

		offset_level_table_index = (PBN2 * BLOCK_PER_SECTOR) + Loffset; //오프셋 단위 테이블 내에서의 해당 LSN의 index값
		(*flashmem)->offset_level_mapping_table[offset_level_table_index] = Poffset; //LSN에 해당하는 물리 블록 내의 물리적 오프셋 위치
		PSN = (PBN2 * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치

		if (PSN % BLOCK_PER_SECTOR == 0) //기록 할 위치가 블록의 첫 번째 섹터일 경우 빈 블록 정보 변경
		{
			PBN2_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;

			//PBN2의 해당 오프셋 위치에 기록
			PBN2_write_proc = true;
			goto HYBRID_LOG_DYNAMIC_COMMON_WRITE_PROC;
		}
		else //기록 할 위치가 블록의 첫 번째 섹터가 아니면 빈 블록 정보를 먼저 변경
		{
			PBN2_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
			SPARE_write(flashmem, (PBN2 * BLOCK_PER_SECTOR), &PBN2_meta_buffer); //해당 블록의 첫 번째 페이지에 meta정보 기록 

			delete PBN2_meta_buffer;
			PBN2_meta_buffer = NULL;

			//PBN2의 해당 오프셋 위치에 기록
			PBN2_write_proc = true;
			goto HYBRID_LOG_DYNAMIC_COMMON_WRITE_PROC;
		}
	}

	/***
		해당 블록이 무효화되지 않은 경우
		---
		1) 해당 블록이 비어있는 경우 => 발생하지 않음
		2) 해당 블록이 비어있지 않은 경우
			2-1) 기록 가능 한 빈 공간 존재 시
			=> 순차적으로 빈 공간에 기록
			2-2) 기록 가능 한 빈 공간 존재하지 않을 시
			=> PBN1과 PBN2를 여분의 빈 Spare Block을 사용하여 유효 데이터 Merge 수행, PBN1로 재 할당
	***/

	/*** PBN2에서의 논리 오프셋 위치가 PBN1에서 Overwrite되어 사용되고 있는 위치일 경우 ***/
	offset_level_table_index = (PBN2 * BLOCK_PER_SECTOR) + Loffset; //오프셋 단위 테이블 내에서의 해당 LSN의 index값
	Poffset = (*flashmem)->offset_level_mapping_table[offset_level_table_index]; //LSN에 해당하는 물리 블록 내의 물리적 오프셋 위치
	if (Poffset != DYNAMIC_MAPPING_INIT_VALUE)
	{
		//PBN2의 기존 위치 무효화
		PBN2_meta_buffer = SPARE_read(flashmem, PSN);
		PBN2_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] = false;
		SPARE_write(flashmem, PSN, &PBN2_meta_buffer);
		(*flashmem)->offset_level_mapping_table[offset_level_table_index] = OFFSET_MAPPING_INIT_VALUE; //오프셋 초기화

		delete PBN2_meta_buffer;
		PBN2_meta_buffer = NULL;

		/*** 이에 따라, 만약, PBN2의 모든 데이터가 무효화되었으면, PBN2 무효화 ***/
		if (update_victim_block_info(flashmem, false, PBN2, mapping_method) != SUCCESS)
			goto VICTIM_BLOCK_INFO_EXCEPTION_ERR;

		if ((*flashmem)->victim_block_info.victim_block_invalid_ratio == 1.0)
		{
			PBN2_meta_buffer = SPARE_read(flashmem, (PBN2 * BLOCK_PER_SECTOR));
			PBN2_meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] = false;
			SPARE_write(flashmem, (PBN2 * BLOCK_PER_SECTOR), &PBN2_meta_buffer);

			delete PBN2_meta_buffer;
			PBN2_meta_buffer = NULL;

			//PBN2를 블록 단위 매핑 테이블 상에서 Unlink(연결 해제)
			(*flashmem)->log_block_level_mapping_table[LBN][1] = DYNAMIC_MAPPING_INIT_VALUE;

			(*flashmem)->gc->scheduler(flashmem, mapping_method);
		}
		else //GC에 의해 Victim Block으로 선정되지 않도록 구조체 초기화
			(*flashmem)->victim_block_info.clear_all();
	}

	if (search_empty_offset_in_block(flashmem, PBN2, Poffset, &PBN2_meta_buffer) == FAIL)
	{
		//Merge를 위해서 PBN1, PBN2 모두 대응되어 있어야 한다.
		if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE || PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
			goto MERGE_COND_EXCEPTION_ERR;

		if (full_merge(flashmem, LBN, mapping_method) != SUCCESS)
			goto WRONG_META_ERR;
		
		goto HYBRID_LOG_DYNAMIC; //재 연산
	}

	(*flashmem)->offset_level_mapping_table[offset_level_table_index] = Poffset; //LSN에 해당하는 물리 블록 내의 물리적 오프셋 위치
	PSN = (PBN2 * BLOCK_PER_SECTOR) + Poffset; //기록 할 위치

	//해당 오프셋 위치에 기록
	PBN2_write_proc = true;
	goto HYBRID_LOG_DYNAMIC_COMMON_WRITE_PROC;

HYBRID_LOG_DYNAMIC_COMMON_WRITE_PROC: //하이브리드 매핑 공용 기록 처리 루틴
	if (PBN1_write_proc == true && PBN2_write_proc == false) //PBN1에 대한 기록
	{
		PBN1_write_proc = false;
		if (PBN1_meta_buffer == NULL)
			PBN1_meta_buffer = SPARE_read(flashmem, PSN);

		//해당 오프셋 위치에 기록
		if (Flash_write(flashmem, &PBN1_meta_buffer, PSN, src_data) == COMPLETE)
			goto OVERWRITE_ERR;

		delete PBN1_meta_buffer;
		PBN1_meta_buffer = NULL;

	}
	else if (PBN1_write_proc == false && PBN2_write_proc == true) //PBN2에 대한 기록
	{
		PBN2_write_proc = false;
		if (PBN2_meta_buffer == NULL)
			PBN2_meta_buffer = SPARE_read(flashmem, PSN);

		//해당 오프셋 위치에 기록
		if (Flash_write(flashmem, &PBN2_meta_buffer, PSN, src_data) == COMPLETE)
			goto OVERWRITE_ERR;

		delete PBN2_meta_buffer;
		PBN2_meta_buffer = NULL;
	}
	else
		goto WRITE_COND_EXCEPTION_ERR;

	goto END_SUCCESS; //종료


END_SUCCESS: //연산 성공
	if (meta_buffer != NULL || PBN1_meta_buffer != NULL || PBN2_meta_buffer != NULL)
		goto MEM_LEAK_ERR;

	/*** Block Invalid Ratio Threshold에 따른 Victim Block 선정을 위해 현재 쓰기가 발생한 논리 블록의 무효율 계산 및 갱신 ***/
	//블록 매핑은 Overwrite시 해당 블록은 항상 무효화되므로, 하이브리드 매핑의 경우에만 수행
	//문제발생 부분:Victim Block정보가 초기화되므로
	//switch (mapping_method)
	//{
	//case 3:
	//	update_victim_block_info(flashmem, true, LBN, mapping_method);
	//	break;

	//default:
	//	break;
	//}
	(*flashmem)->save_table(mapping_method);
	//(*flashmem)->gc->scheduler(flashmem, mapping_method);

	return SUCCESS;

END_NO_SPACE: //기록 가능 공간 부족
	fprintf(stderr, "기록 할 공간이 존재하지 않음 : 할당된 크기의 공간 모두 사용\n");
	return FAIL;

VICTIM_BLOCK_INFO_EXCEPTION_ERR:
	fprintf(stderr, "오류 : Victim Block 정보 갱신 예외 - 이미 사용 중 (FTL_write)\n");
	system("pause");
	exit(1);

SPARE_BLOCK_EXCEPTION_ERR: //Spare Block Table에 대한 예외
	if(VICTIM_BLOCK_QUEUE_RATIO != SPARE_BLOCK_RATIO)
		fprintf(stderr, "Spare Block Table에 할당된 크기의 공간 모두 사용 : 미구현, GC에 의해 처리되도록 해야한다.\n");
	else
	{
		fprintf(stderr, "오류 : Spare Block Table 및 GC Scheduler에 대한 예외 발생 (FTL_write)\n");
		system("pause");
		exit(1);
	}
	return FAIL;

WRONG_META_ERR: //잘못된 meta정보 오류
	fprintf(stderr, "오류 : 잘못된 meta 정보 (FTL_write)\n");
	system("pause");
	exit(1);

WRONG_TABLE_TYPE_ERR: //잘못된 테이블 타입
	fprintf(stderr, "오류 : 잘못된 테이블 타입 (FTL_write)\n");
	system("pause");
	exit(1);

WRONG_STATIC_TABLE_ERR: //잘못된 정적 테이블 오류
	fprintf(stderr, "오류 : 정적 테이블에 대한 대응되지 않은 테이블\n");
	system("pause");
	exit(1);

WRITE_COND_EXCEPTION_ERR:
	fprintf(stderr, "오류 : 쓰기 조건 오류\n");
	system("pause");
	exit(1);
	
MERGE_COND_EXCEPTION_ERR:
	fprintf(stderr, "오류 : Merge 조건 오류 (PBN 1 : %u , PBN2 : %u)\n", PBN1, PBN2);
	system("pause");
	exit(1);

OVERWRITE_ERR: //Overwrite 오류
	fprintf(stderr, "오류 : Overwrite에 대한 예외 발생 (FTL_write)\n");
	system("pause");
	exit(1);

MEM_LEAK_ERR:
	fprintf(stderr, "오류 : meta 정보에 대한 메모리 누수 발생 (FTL_write)\n");
	system("pause");
	exit(1);
}

int full_merge(FlashMem** flashmem, unsigned int LBN, int mapping_method) //특정 LBN에 대응된 PBN1과 PBN2에 대하여 Merge 수행
{
	unsigned int PBN1 = (*flashmem)->log_block_level_mapping_table[LBN][0]; //LBN에 대응된 물리 블록(PBN1)
	unsigned int PBN2 = (*flashmem)->log_block_level_mapping_table[LBN][1]; //LBN에 대응된 물리 블록(PBN2)
	unsigned int PSN = DYNAMIC_MAPPING_INIT_VALUE; //실제로 저장된 물리 섹터 번호

	__int8 Loffset = OFFSET_MAPPING_INIT_VALUE; //블록 내의 논리적 섹터 Offset = 0 ~ 31
	__int8 Poffset = OFFSET_MAPPING_INIT_VALUE; //블록 내의 물리적 섹터 Offset = 0 ~ 31
	unsigned int offset_level_table_index = DYNAMIC_MAPPING_INIT_VALUE; //오프셋 단위 테이블 인덱스

	unsigned int empty_spare_block = DYNAMIC_MAPPING_INIT_VALUE; //기록할 빈 Spare 블록
	unsigned int spare_block_table_index = DYNAMIC_MAPPING_INIT_VALUE; //기록할 빈 Spare 블록의  Spare 블록 테이블 상 인덱스
	unsigned int tmp = DYNAMIC_MAPPING_INIT_VALUE; //테이블 SWAP위한 임시 변수

	char read_buffer = NULL; //물리 섹터로부터 읽어들인 데이터
	char block_read_buffer[BLOCK_PER_SECTOR] = { NULL }; //한 블록 내의 데이터 임시 저장 변수
	__int8 read_buffer_index = 0; //데이터를 읽어서 임시저장하기 위한 read_buffer 인덱스 변수

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼

	switch (mapping_method) //실행 조건
	{
	case 3:
		break;

	default:
		return FAIL;
	}

	f_flash_info = (*flashmem)->get_f_flash_info();

	/***
		Merge 조건 :
		- PBN1과 PBN2는 유효한 값(초기값이 아닌 대응된 값)이어야 함
		- PBN1과 PBN2는 Spare 블록이 아닐 것
		========================================================================
		어떠한 데이터에 대해 기록을 할 때 먼저 PBN1에 기록되고, Overwrite시에 PBN2에 기록되므로
		- PBN1에서 어떤 논리 오프셋이 무효한 값이라면 PBN2에서 해당 논리 오프셋에 유효한 데이터가 존재
		- PBN1에서 어떤 논리 오프셋에 대해 유효한 값이라면 PBN2에서 해당 논리 오프셋에 대해 기록되어 있지 않음 (항상)
		= PBN1에서 어떤 논리 오프셋에 대해 유효한 값이라면 PBN2에서 해당 논리 오프셋에 대해 무효한 데이터이지 않고 항상 기록되어 있지 않음
		=> Overwrite시에 PBN2에 기록하므로, PBN1의 어떤 논리 오프셋이 유효하다면 PBN2의 같은 논리 오프셋은 비어있을 수 밖에 없다
		========================================================================
		PBN1과 PBN2로부터 논리 오프셋에 맞춰 유효 데이터들을 버퍼로 복사
		PBN1의 논리 오프셋 위치가 무효할 경우 PBN2에서 해당 논리 오프셋 위치를 읽는다
		=> PBN1로 재 할당함
	***/

	//Merge를 위해 PBN1과 PBN2이 양쪽 다 대응되어 있어야 함
	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
		return COMPLETE;

	printf("Performing Merge on PBN1 : %u and PBN2 : %u...\n", PBN1, PBN2);
#if DEBUG_MODE == 1
	/***
		각 블록의 첫 번째 페이지를 읽어 Spare 블록인지 판별(생략 가능)
		일반 블록 단위 테이블로부터 인덱싱하여 블록 번호(LBN => PBN1, PBN2)를 받으므로, 항상 Spare 블록이 아니지만, meta 정보 오류 검출을 위하여 검사한다
	***/
	PSN = (PBN1 * BLOCK_PER_SECTOR);
	meta_buffer = SPARE_read(flashmem, PSN);

	//PBN1이 Spare 블록이 아닌 경우에
	if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] == true)
	{
		delete meta_buffer;

		PSN = (PBN2 * BLOCK_PER_SECTOR);
		meta_buffer = SPARE_read(flashmem, PSN);
		//PBN2가 Spare 블록이 아닌 경우에
		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] == true)
			delete meta_buffer;
		else //Spare 블록인 경우
			goto WRONG_META_ERR;

	}
	else //Spare 블록인 경우
		goto WRONG_META_ERR;
#endif

	/***
		PBN1과 PBN2로부터 논리 오프셋에 맞춰 유효 데이터들을 버퍼로 복사
		1) PBN1의 논리 오프셋 위치가 무효할 경우
		2) PBN2에서 해당 논리 오프셋 위치를 읽는다
	***/
	for (Loffset = 0; Loffset < BLOCK_PER_SECTOR; Loffset++)
	{
		PSN = (PBN1 * BLOCK_PER_SECTOR) + Loffset; //PBN1 인덱싱
		meta_buffer = SPARE_read(flashmem, PSN);

		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true)
		{
			//PBN1에서 비어있지 않고, 유효하면
			Flash_read(flashmem, NULL, PSN, block_read_buffer[Loffset]);
		}

		else if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] == true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true)
		{
			//PBN1에서 비어있고, 유효하면 오프셋을 맞추기위해 빈 공간으로 기록 (즉, 한번도 기록되지 않은 위치)
			block_read_buffer[Loffset] = NULL;
		}
		else //PBN1에서 유효하지 않으면
		{
			//PBN2에서 해당 논리 오프셋 위치를 읽는다
			offset_level_table_index = (PBN2 * BLOCK_PER_SECTOR) + Loffset; //PBN2의 오프셋 단위 테이블 내에서의 PBN1에서의 Loffset에 해당하는 index값
			Poffset = (*flashmem)->offset_level_mapping_table[offset_level_table_index]; //물리 블록 내의 물리적 오프셋 위치
			PSN = (PBN2 * BLOCK_PER_SECTOR) + Poffset; //실제로 저장된 물리 섹터 번호

			Flash_read(flashmem, NULL, PSN, block_read_buffer[Loffset]);
		}

		delete meta_buffer;
	}

	/*** PBN1, PBN2 Erase후 하나를(PBN1) Spare 블록으로 설정 ***/
	Flash_erase(flashmem, PBN1); //PBN1을 erase
	Flash_erase(flashmem, PBN2); //PBN2을 erase
	PSN = PBN1 * BLOCK_PER_SECTOR; //해당 블록의 첫 번째 섹터(페이지)
	meta_buffer = SPARE_read(flashmem, PSN);
	meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] = false;
	SPARE_write(flashmem, PSN, &meta_buffer);
	delete meta_buffer;

	/*** 빈 Spare 블록을 찾아서 기록 ***/
	if ((*flashmem)->spare_block_table->rr_read(flashmem, empty_spare_block, spare_block_table_index) == FAIL)
		goto SPARE_BLOCK_EXCEPTION_ERR;

	for (Loffset = 0; Loffset < BLOCK_PER_SECTOR; Loffset++)
	{
		PSN = (empty_spare_block * BLOCK_PER_SECTOR) + Loffset; //데이터를 옮길 Spare 블록 인덱싱

		if (block_read_buffer[Loffset] != NULL) //블록 단위로 읽어들인 버퍼에서 해당 위치가 비어있지 않으면, 즉 유효한 데이터이면
		{
			meta_buffer = SPARE_read(flashmem, PSN); //섹터(페이지)의 Spare 정보 읽기

			if (PSN % BLOCK_PER_SECTOR == 0) //만약 기록 할 위치가 첫 번째 섹터라면
			{
				//해당 블록은 일반 블록화 될 것, 또한 빈 블록, 빈 섹터 여부 변경
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] = true;
				meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
				if (Flash_write(flashmem, &meta_buffer, PSN, block_read_buffer[Loffset]) == COMPLETE)
					goto OVERWRITE_ERR;
			}
			else
			{
				if (Flash_write(flashmem, &meta_buffer, PSN, block_read_buffer[Loffset]) == COMPLETE)
					goto OVERWRITE_ERR;
			}

			delete meta_buffer;
		}
		//무효하거나 비어있을 경우 기록하지 않는다 
	}
	if (block_read_buffer[0] == NULL) //만약 첫 번째 섹터(페이지)에 해당하는 블록 단위 버퍼의 index 0이 비어있어서 해당 블록 정보가 변경되지 않았을 경우 변경
	{
		//해당 블록은 일반 블록화 될 것, 또한 빈 블록 여부 변경
		PSN = empty_spare_block * BLOCK_PER_SECTOR; //해당 블록의 첫 번째 섹터(페이지)로 초기화
		meta_buffer = SPARE_read(flashmem, PSN);
		meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] = true;
		meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] = false;
		SPARE_write(flashmem, PSN, &meta_buffer);
		delete meta_buffer;
	}

	/*** PBN2 블록 단위 테이블, 오프셋 단위 테이블 초기화 ***/
	for (Loffset = 0; Loffset < BLOCK_PER_SECTOR; Loffset++)
	{
		offset_level_table_index = (PBN2 * BLOCK_PER_SECTOR) + Loffset; //오프셋 단위 테이블 내에서의 index값
		(*flashmem)->offset_level_mapping_table[offset_level_table_index] = OFFSET_MAPPING_INIT_VALUE;
	}
	PBN2 = (*flashmem)->log_block_level_mapping_table[LBN][1] = DYNAMIC_MAPPING_INIT_VALUE;

	/*** PBN1과 Spare 블록 테이블 변경 ***/
	//PBN1의 오프셋은 항상 일치시키도록 하였으므로, 블록 단위 테이블만 변경
	SWAP((*flashmem)->log_block_level_mapping_table[LBN][0], (*flashmem)->spare_block_table->table_array[spare_block_table_index], tmp); //PBN1과 Spare 블록 교체

	(*flashmem)->save_table(mapping_method);
	return SUCCESS;

SPARE_BLOCK_EXCEPTION_ERR:
	if (VICTIM_BLOCK_QUEUE_RATIO != SPARE_BLOCK_RATIO)
		fprintf(stderr, "Spare Block Table에 할당된 크기의 공간 모두 사용 : 미구현, GC에 의해 처리되도록 해야한다.\n");
	else
	{
		fprintf(stderr, "오류 : Spare Block Table 및 GC Scheduler에 대한 예외 발생 (FTL_write)\n");
		system("pause");
		exit(1);
	}
	return FAIL;

WRONG_META_ERR: //잘못된 meta정보 오류
	fprintf(stderr, "오류 : 잘못된 meta 정보 (full_merge)\n");
	system("pause");
	exit(1);

OVERWRITE_ERR: //Overwrite 오류
	fprintf(stderr, "오류 : Overwrite에 대한 예외 발생 (full_merge)\n");
	system("pause");
	exit(1);
}

int full_merge(FlashMem** flashmem, int mapping_method) //테이블내의 대응되는 모든 블록에 대해 Merge 수행
{
	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보

	switch (mapping_method) //실행 조건
	{
	case 3:
		break;

	default:
		return FAIL;
	}

	f_flash_info = (*flashmem)->get_f_flash_info();

	for (unsigned int LBN = 0; LBN < (f_flash_info.block_size - f_flash_info.spare_block_size); LBN++)
	{
		//일반 블록 단위 매핑 테이블내의 대응되는 모든 일반 블록에 대해 가능할 경우 Merge 수행
		if (full_merge(flashmem, LBN, mapping_method) == FAIL)
			return FAIL;
	}

	return SUCCESS;
}

int trace(FlashMem** flashmem, int mapping_method, int table_type) //특정 패턴에 의한 쓰기 성능을 측정하는 함수
{
	//전체 trace 를 수행하기 위해 수행된 read, write, erase 횟수 출력

	FILE* trace_file = NULL; //trace 위한 입력 파일
	
#if BLOCK_TRACE_MODE == 1 //Trace for Per Block Wear-leveling
	FILE* trace_per_block_output = NULL; //블록 당 trace 결과 출력
	F_FLASH_INFO f_flash_info;
	f_flash_info = (*flashmem)->get_f_flash_info();
#endif

	char file_name[2048]; //trace 파일 이름
	char op_code[2] = { 0, }; //연산코드(w,r,e) : '\0' 포함 2자리
	unsigned int LSN = UINT32_MAX; //LSN
	char dummy_data = 'A'; //trace를 위한 더미 데이터
	
	system("cls");
	std::cout << "< 현재 파일 목록 >" << std::endl;
	system("dir");
	std::cout << "trace 파일 이름 입력 (이름.확장자) >>";
	gets_s(file_name, 100);

	trace_file = fopen(file_name, "rt"); //읽기 + 텍스트 파일 모드

	if (trace_file == NULL)
	{
		fprintf(stderr, "File does not exist or cannot be opened.\n");
		return FAIL;
	}

	std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now(); //trace 시간 측정 시작
	while (!feof(trace_file))
	{
		memset(op_code, NULL, sizeof(op_code)); //연산코드 버퍼 초기화
		LSN = UINT32_MAX; //생성 가능한 플래시 메모리의 용량에서 존재 할 수가 없는 LSN 값으로 초기화

		fscanf(trace_file, "%s\t%u\n", &op_code, &LSN); //탭으로 분리된 파일 읽기
		if (strcmp(op_code, "w") == 0 || strcmp(op_code, "W") == 0)
		{
			FTL_write(flashmem, LSN, dummy_data, mapping_method, table_type);
		}
	}
	fclose(trace_file);
	std::chrono::system_clock::time_point end_time = std::chrono::system_clock::now(); //trace 시간 측정 끝
	std::chrono::milliseconds mill_end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
	std::cout << ">> Trace function ends : " << mill_end_time.count() <<"milliseconds elapsed"<< std::endl;

#if BLOCK_TRACE_MODE == 1 //Trace for Per Block Wear-leveling
	trace_per_block_output = fopen("trace_per_block_result.txt", "wt");
	if (trace_per_block_output == NULL)
	{
		fprintf(stderr, "블록 당 마모도 출력 파일 오류 (trace_per_block_result.txt)\n");
		return FAIL;
	}

	fprintf(trace_per_block_output, "PBN\tRead\tWrite\tErase\n");
	for (unsigned int PBN = 0; PBN < f_flash_info.block_size; PBN++)
	{
		//PBN [TAB] 읽기 횟수 [TAB] 쓰기 횟수 [TAB] 지우기 횟수 출력 및 다음 trace를 위한 초기화 동시 수행
		fprintf(trace_per_block_output, "%u\t%u\t%u\t%u\n", PBN, (*flashmem)->block_trace_info[PBN].block_read_count, (*flashmem)->block_trace_info[PBN].block_write_count, (*flashmem)->block_trace_info[PBN].block_erase_count);
		(*flashmem)->block_trace_info[PBN].clear_all(); //읽기, 쓰기, 지우기 횟수 초기화
	}
	fclose(trace_per_block_output);

	printf(">> 블록 당 마모도 정보 trace_per_block_result.txt\n");
	system("notepad trace_per_block_result.txt");

#endif

	return SUCCESS;
}