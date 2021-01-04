#include "Build_Options.h"

// Spare Area에 대한 비트 단위 처리, Meta-data 판독을 위한 함수 SPARE_init, SPARE_read, SPARE_write 정의
// 물리적 가용 가능 공간 관리와 Garbage Collection을 위한 SPARE_reads, update_victim_block_info, update_v_flash_info_for_reorganization, update_v_flash_info_for_erase, calc_block_invalid_ratio 정의
// Meta-data를 통한 빈 일반 물리 블록 탐색 및 특정 물리 블록 내의 빈 물리 오프셋 탐색 위한 search_empty_block, search_empty_offset_in_block 정의

int SPARE_init(class FlashMem*& flashmem, FILE*& storage_spare_pos) //물리 섹터(페이지)의 Spare Area에 대한 초기화
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	unsigned char* write_buffer = NULL; //전체 Spare Area에 기록할 버퍼

	if (storage_spare_pos != NULL)
	{
		write_buffer = new unsigned char[SPARE_AREA_BYTE];
		memset(write_buffer, SPARE_INIT_VALUE, SPARE_AREA_BYTE);

		/* 삭제
		for (__int8 byte_unit = 0; byte_unit < SPARE_AREA_BYTE; byte_unit++) //1바이트마다 초기화
		{
			write_buffer[byte_unit] = SPARE_INIT_VALUE;
		}
		*/
		fwrite(write_buffer, sizeof(unsigned char), SPARE_AREA_BYTE, storage_spare_pos);

		delete[] write_buffer;
	}
	else
		goto NULL_FILE_PTR_ERR;

	return SUCCESS;

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : nullptr (SPARE_init)\n");
	system("pause");
	exit(1);
}

int SPARE_read(class FlashMem*& flashmem, FILE*& storage_spare_pos, META_DATA*& dst_meta_buffer) //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 구조체 형태로 반환
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}
	
	unsigned char* read_buffer = NULL; //전체 Spare Area로부터 읽어들일 버퍼

	if (storage_spare_pos != NULL)
	{
		if (dst_meta_buffer != NULL)
			goto MEM_LEAK_ERR;

		dst_meta_buffer = new META_DATA; //목적지에 META_DATA 구조체 생성

		read_buffer = new unsigned char[SPARE_AREA_BYTE];
		fread(read_buffer, sizeof(unsigned char), SPARE_AREA_BYTE, storage_spare_pos);

		/*** Spare Area의 전체 16byte에 대해 첫 1byte의 블록 및 섹터(페이지)의 상태 정보에 대한 처리 시작 ***/
		unsigned char bits_8_buffer = read_buffer[0]; //1byte == 8bit크기의 블록 및 섹터(페이지) 정보에 관하여 Spare Area를 읽어들인 버퍼 
		//삭제 fread(&bits_8_buffer, sizeof(unsigned char), 1, storage_spare_pos);

		/*** 읽어들인 8비트(2^7 ~2^0)에 대해서 블록 상태(2^7 ~ 2^5) 판별 ***/
		switch ((((bits_8_buffer) >> (5)) & (0x7))) //추출 끝나는 2^5 자리가 LSB에 오도록 오른쪽으로 5번 쉬프트하여, 00000111(2)와 AND 수행
		{
		case (const unsigned)BLOCK_STATE::NORMAL_BLOCK_EMPTY:
			dst_meta_buffer->block_state = BLOCK_STATE::NORMAL_BLOCK_EMPTY;
			break;

		case (const unsigned)BLOCK_STATE::NORMAL_BLOCK_VALID:
			dst_meta_buffer->block_state = BLOCK_STATE::NORMAL_BLOCK_VALID;
			break;

		case (const unsigned)BLOCK_STATE::NORMAL_BLOCK_INVALID:
			dst_meta_buffer->block_state = BLOCK_STATE::NORMAL_BLOCK_INVALID;
			break;

		case (const unsigned)BLOCK_STATE::SPARE_BLOCK_EMPTY:
			dst_meta_buffer->block_state = BLOCK_STATE::SPARE_BLOCK_EMPTY;
			break;

		case (const unsigned)BLOCK_STATE::SPARE_BLOCK_VALID:
			dst_meta_buffer->block_state = BLOCK_STATE::SPARE_BLOCK_VALID;
			break;

		case (const unsigned)BLOCK_STATE::SPARE_BLOCK_INVALID:
			dst_meta_buffer->block_state = BLOCK_STATE::SPARE_BLOCK_INVALID;
			break;

		default:
			printf("Block State Err\n");
			bit_disp(bits_8_buffer, 7, 0);
			goto WRONG_META_ERR;
		}

		/*** 읽어들인 8비트(2^7 ~2^0)에 대해서 섹터 상태(2^4 ~ 2^3) 판별 ***/
		switch ((((bits_8_buffer) >> (3)) & (0x3))) //추출 끝나는 2^3 자리가 LSB에 오도록 오른쪽으로 3번 쉬프트하여, 00000011(2)와 AND 수행
		{
		case (const unsigned)SECTOR_STATE::EMPTY:
			dst_meta_buffer->sector_state = SECTOR_STATE::EMPTY;
			break;

		case (const unsigned)SECTOR_STATE::VALID:
			dst_meta_buffer->sector_state = SECTOR_STATE::VALID;
			break;

		case (const unsigned)SECTOR_STATE::INVALID:
			dst_meta_buffer->sector_state = SECTOR_STATE::INVALID;
			break;

		default:
			printf("Sector State Err\n");
			bit_disp(bits_8_buffer, 7,0);
			goto WRONG_META_ERR;
		}

		/*** DUMMY 3비트 처리 (2^2 ~ 2^0) ***/
		switch ((bits_8_buffer) &= (0x7)) //00000111(2)를 AND 수행
		{
		case (0x7): //111(2)가 아닐 경우 오류
			break;
			
		default:
			printf("DUMMY bit Err\n");
			bit_disp(bits_8_buffer, 7, 0);
			goto WRONG_META_ERR;
		}
		/*** Spare Area의 전체 16byte에 대해 첫 1byte의 블록 및 섹터(페이지)의 상태 정보에 대한 처리 종료 ***/

		//기타 Meta 정보 추가 시 읽어서 처리 할 코드 추가
	}
	else
		goto NULL_FILE_PTR_ERR;

	delete[] read_buffer;

	/*** trace위한 정보 기록 ***/
	flashmem->v_flash_info.flash_read_count++; //Global 플래시 메모리 읽기 카운트 증가

#ifdef PAGE_TRACE_MODE //Trace for Per Sector(Page) Wear-leveling
	/***
		현재 읽기가 발생한 PSN 계산, Spare Area에 대한 처리가 끝난 후
		ftell(현재 파일 포인터, 바이트 단위) - SECTOR_INC_SPARE_BYTE == Flash_read상의 read_pos
		PSN = read_pos / SECTOR_INC_SPARE_BYTE
	***/
	flashmem->page_trace_info[((ftell(storage_spare_pos) - SECTOR_INC_SPARE_BYTE) / SECTOR_INC_SPARE_BYTE)].read_count++; //해당 섹터(페이지)의 지우기 카운트 증가
#endif

#ifdef BLOCK_TRACE_MODE //Trace for Per Block Wear-leveling
	/***
		현재 읽기가 발생한 PBN 계산, Spare Area에 대한 처리가 끝난 후
		ftell(현재 파일 포인터, 바이트 단위) - SECTOR_INC_SPARE_BYTE == Flash_read상의 read_pos
		PSN = read_pos / SECTOR_INC_SPARE_BYTE
		PBN = PSN / BLOCK_PER_SECTOR
	***/
	flashmem->block_trace_info[((ftell(storage_spare_pos) - SECTOR_INC_SPARE_BYTE) / SECTOR_INC_SPARE_BYTE) / BLOCK_PER_SECTOR].read_count++; //해당 블록의 읽기 카운트 증가
#endif

	return SUCCESS;

WRONG_META_ERR: //잘못된 meta정보 오류
	fprintf(stderr, "치명적 오류 : 잘못된 meta 정보 (SPARE_read)\n");
	system("pause");
	exit(1);

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (SPARE_read)\n");
	system("pause");
	exit(1);

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : nullptr (SPARE_read)\n");
	system("pause");
	exit(1);
}

int SPARE_read(class FlashMem*& flashmem, unsigned int PSN, META_DATA*& dst_meta_buffer) //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 구조체 형태로 반환
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	FILE* storage_spare_pos = NULL;

	unsigned int read_pos = 0; //읽고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점

	if (dst_meta_buffer != NULL)
		goto MEM_LEAK_ERR;

	if ((storage_spare_pos = fopen("storage.bin", "rb")) == NULL) //읽기 + 이진파일 모드
		goto NULL_FILE_PTR_ERR;

	read_pos = SECTOR_INC_SPARE_BYTE * PSN; //쓰고자 하는 위치
	spare_pos = read_pos + SECTOR_PER_BYTE; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)

	fseek(storage_spare_pos, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동

	if (SPARE_read(flashmem, storage_spare_pos, dst_meta_buffer) != SUCCESS)
	{
		/*** 플래시 메모리가 할당되지 않아 읽기가 실패할 경우 플래시 메모리 읽기 카운트 변동없이 FAIL return ***/
		fclose(storage_spare_pos);
		return FAIL;
	}

	fclose(storage_spare_pos);
	return SUCCESS;

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (SPARE_read)\n");
	system("pause");
	exit(1);

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 파일을 읽기모드로 열 수 없습니다. (SPARE_read)\n");
	system("pause");
	exit(1);
}

int SPARE_write(class FlashMem*& flashmem, FILE*& storage_spare_pos, META_DATA*& src_meta_buffer) //META_DATA에 대한 구조체 전달받아, 물리 섹터의 Spare Area에 기록
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	unsigned char* write_buffer = NULL; //전체 Spare Area에 기록 할 버퍼
	unsigned bits_8_buffer; //8비트(1바이트) 단위로 분리하여 판독 위한 버퍼

	if (storage_spare_pos != NULL)
	{
		if (src_meta_buffer == NULL)
			goto NULL_SRC_META_ERR;

		/*** for Remaining Space Management ***/
		//해당 섹터가 무효화 되었을 경우 무효 카운트를 증가시킨다
		if (src_meta_buffer->sector_state == SECTOR_STATE::INVALID)
			flashmem->v_flash_info.invalid_sector_count++; //무효 페이지 수 증가

		write_buffer = new unsigned char[SPARE_AREA_BYTE];
		memset(write_buffer, SPARE_INIT_VALUE, SPARE_AREA_BYTE);

		/*** Spare Area의 전체 16byte에 대해 첫 1byte의 블록 및 섹터(페이지)의 상태 정보에 대한 처리 시작 ***/
		//BLOCK_TYPE(Normal or Spare, 1bit) || IS_VALID (BLOCK, 1bit) || IS_EMPTY (BLOCK, 1bit) || IS_VALID (SECTOR, 1bit) || IS_EMPTY(SECTOR, 1bit) || DUMMY (3bit)
		bits_8_buffer = ~(SPARE_INIT_VALUE); //00000000(2)

		switch (src_meta_buffer->block_state) //1바이트 크기의 bits_8_buffer에 대하여
		{
		case BLOCK_STATE::NORMAL_BLOCK_EMPTY: //2^7, 2^6, 2^5 비트를 0x1으로 설정
			bits_8_buffer |= (0x7 << 5); //111(2)를 5번 왼쪽 쉬프트하여 11100000(2)를 OR 수행
			break;

		case BLOCK_STATE::NORMAL_BLOCK_VALID: //2^7, 2^6 비트를 0x1로 설정
			bits_8_buffer |= (0x6 << 5); //110(2)를 5번 왼쪽 쉬프트하여 11000000(2)를 OR 수행
			break;

		case BLOCK_STATE::NORMAL_BLOCK_INVALID: //2^7 비트를 0x1로 설정
			bits_8_buffer |= (TRUE_BIT << 7); //10000000(2)를 OR 수행
			break;

		case BLOCK_STATE::SPARE_BLOCK_EMPTY: //2^6, 2^5 비트를 0x1로 설정
			bits_8_buffer |= (0x6 << 4); //110(2)를 4번 왼쪽 쉬프트하여 01100000(2)를 OR 수행
			break;

		case BLOCK_STATE::SPARE_BLOCK_VALID: //2^6 비트를 0x1로 설정
			bits_8_buffer |= (TRUE_BIT << 6); //01000000(2)를 OR 수행
			break;

		case BLOCK_STATE::SPARE_BLOCK_INVALID: //do nothing
			break;

		default:
			printf("Block State Err\n");
			bit_disp(bits_8_buffer, 7, 0);
			goto WRONG_META_ERR;
		}

		switch (src_meta_buffer->sector_state) //1바이트 크기의 bits_8_buffer에 대하여
		{
		case SECTOR_STATE::EMPTY: //2^4, 2^3 비트를 0x1로 설정
			bits_8_buffer |= (0x3 << 3); //11(2)를 3번 왼쪽 쉬프트하여 00011000(2)를 OR 수행
			break;

		case SECTOR_STATE::VALID: //2^4비트를 0x1로 설정
			bits_8_buffer |= (TRUE_BIT << 4); //00010000(2)를 OR 수행
			break;

		case SECTOR_STATE::INVALID: //do nothing
			break;

		default:
			printf("Sector State Err\n");
			bit_disp(bits_8_buffer, 7, 0);
			goto WRONG_META_ERR;
		}

		//DUMMY 3비트 처리 (2^2 ~ 2^0)
		bits_8_buffer |= (0x7); //00000111(2)를 OR 수행

		write_buffer[0] = bits_8_buffer;

#ifdef DEBUG_MODE
		bit_disp(write_buffer[0], 7, 0);
#endif
		//삭제 fwrite(&bits_8_buffer, sizeof(unsigned char), 1, storage_spare_pos);
		
		/*** Spare Area의 전체 16byte에 대해 첫 1byte의 블록 및 섹터(페이지)의 상태 정보에 대한 처리 종료 ***/

		//기타 Meta 정보 추가 시 기록 할 코드 추가
	}
	else
		goto NULL_FILE_PTR_ERR;

	fwrite(write_buffer, sizeof(unsigned char), SPARE_AREA_BYTE, storage_spare_pos);
	delete[] write_buffer;

	/*** trace위한 정보 기록 ***/
	flashmem->v_flash_info.flash_write_count++; //Global 플래시 메모리 쓰기 카운트 증가

#ifdef PAGE_TRACE_MODE //Trace for Per Sector(Page) Wear-leveling
	/***
		현재 쓰기가 발생한 PSN 계산, Spare Area에 대한 처리가 끝난 후
		ftell(현재 파일 포인터, 바이트 단위) - SECTOR_INC_SPARE_BYTE == Flash_write상의 write_pos
		PSN = write_pos / SECTOR_INC_SPARE_BYTE
	***/
	flashmem->page_trace_info[((ftell(storage_spare_pos) - SECTOR_INC_SPARE_BYTE) / SECTOR_INC_SPARE_BYTE)].write_count++; //해당 섹터(페이지)의 지우기 카운트 증가
#endif

#ifdef BLOCK_TRACE_MODE //Trace for Per Block Wear-leveling
	/***
		현재 쓰기가 발생한 PBN 계산, Spare Area에 대한 처리가 끝난 후
		ftell(현재 파일 포인터, 바이트 단위) - SECTOR_INC_SPARE_BYTE == Flash_write상의 write_pos
		PSN = write_pos / SECTOR_INC_SPARE_BYTE
		PBN = PSN / BLOCK_PER_SECTOR
	***/
	flashmem->block_trace_info[((ftell(storage_spare_pos) - SECTOR_INC_SPARE_BYTE) / SECTOR_INC_SPARE_BYTE) / BLOCK_PER_SECTOR].write_count++; //해당 블록의 쓰기 카운트 증가
#endif

	return SUCCESS;

WRONG_META_ERR: //잘못된 meta정보 오류
	fprintf(stderr, "치명적 오류 : 잘못된 meta 정보 (SPARE_write)\n");
	system("pause");
	exit(1);

NULL_SRC_META_ERR:
	fprintf(stderr, "치명적 오류 : 기록 위한 meta 정보가 존재하지 않음 (SPARE_write)\n");
	system("pause");
	exit(1);

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : nullptr (SPARE_write)\n");
	system("pause");
	exit(1);
}

int SPARE_write(class FlashMem*& flashmem, unsigned int PSN, META_DATA*& src_meta_buffer) //META_DATA에 대한 구조체 전달받아, 물리 섹터의 Spare Area에 기록
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	FILE* storage_spare_pos = NULL;

	unsigned int write_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점

	if (src_meta_buffer == NULL)
		goto NULL_SRC_META_ERR;

	if ((storage_spare_pos = fopen("storage.bin", "rb+")) == NULL) //읽고 쓰기 모드 + 이진파일 모드
		goto NULL_FILE_PTR_ERR;

	write_pos = SECTOR_INC_SPARE_BYTE * PSN; //PSN 위치
	spare_pos = write_pos + SECTOR_PER_BYTE; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)

	fseek(storage_spare_pos, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동

	if (SPARE_write(flashmem, storage_spare_pos, src_meta_buffer) != SUCCESS)
	{
		/*** 플래시 메모리가 할당되지 않아 쓰기가 실패할 경우 플래시 메모리 쓰기 카운트 변동없이 FAIL return ***/
		fclose(storage_spare_pos);
		return FAIL;
	}

	fclose(storage_spare_pos);
	return SUCCESS;

NULL_SRC_META_ERR:
	fprintf(stderr, "치명적 오류 : 기록 위한 meta 정보가 존재하지 않음 (SPARE_write)\n");
	system("pause");
	exit(1);

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 파일을 읽고 쓰기 모드로 열 수 없습니다. (SPARE_write)\n");
	system("pause");
	exit(1);
}

/*** Depending on Spare area processing function ***/
//for Remaining Space Management and Garbage Collection
int SPARE_reads(class FlashMem*& flashmem, unsigned int PBN, META_DATA**& dst_block_meta_buffer_array) //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 구조체 배열 형태로 반환
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	FILE* storage_spare_pos = NULL;

	unsigned int read_pos = 0; //읽고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점
	__int8 offset_index_dump = OFFSET_MAPPING_INIT_VALUE; //debug

	if (dst_block_meta_buffer_array != NULL)
		goto MEM_LEAK_ERR;

	dst_block_meta_buffer_array = new META_DATA * [BLOCK_PER_SECTOR]; //블록 당 섹터(페이지)수의 META_DATA 주소를 담을 수 있는 공간(row)

	if ((storage_spare_pos = fopen("storage.bin", "rb")) == NULL) //읽기 + 이진파일 모드
		goto NULL_FILE_PTR_ERR;

	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		dst_block_meta_buffer_array[offset_index] = NULL; //먼저 초기화

		read_pos = ((SECTOR_INC_SPARE_BYTE * PBN) * BLOCK_PER_SECTOR) + (SECTOR_INC_SPARE_BYTE * offset_index);
		spare_pos = read_pos + SECTOR_PER_BYTE; //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)
		fseek(storage_spare_pos, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동

		if (SPARE_read(flashmem, storage_spare_pos, dst_block_meta_buffer_array[offset_index]) != SUCCESS) //한 물리 블록 내의 각 물리 오프셋 위치(페이지)에 대해 순차적으로 저장(col)
		{
			offset_index_dump = offset_index;
			goto READ_BLOCK_META_ERR;
		}
	}

	fclose(storage_spare_pos);
	return SUCCESS;

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (SPARE_read)\n");
	system("pause");
	exit(1);

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 파일을 읽기모드로 열 수 없습니다. (SPARE_read)\n");
	system("pause");
	exit(1);

READ_BLOCK_META_ERR:
	fprintf(stderr, "치명적 오류 :  PBN : %u 의 Offset : %d 대한 meta 정보 읽기 실패 (SPARE_reads)\n", PBN, offset_index_dump);
	system("pause");
	exit(1);
}

int SPARE_writes(class FlashMem*& flashmem, unsigned int PBN, META_DATA**& src_block_meta_buffer_array) //한 물리 블록 내의 모든 섹터(페이지)에 대해 meta정보 기록
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	FILE* storage_spare_pos = NULL;

	unsigned int write_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점
	__int8 offset_index_dump = OFFSET_MAPPING_INIT_VALUE; //debug

	if (src_block_meta_buffer_array == NULL)
		goto NULL_SRC_META_ERR;

	if ((storage_spare_pos = fopen("storage.bin", "rb+")) == NULL) //읽고 쓰기 모드 + 이진파일 모드
		goto NULL_FILE_PTR_ERR;

	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		write_pos = ((SECTOR_INC_SPARE_BYTE * PBN) * BLOCK_PER_SECTOR) + (SECTOR_INC_SPARE_BYTE * offset_index);
		spare_pos = write_pos + SECTOR_PER_BYTE; //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)
		fseek(storage_spare_pos, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동

		if (SPARE_write(flashmem, storage_spare_pos, src_block_meta_buffer_array[offset_index]) != SUCCESS)
		{
			offset_index_dump = offset_index;
			goto WRITE_BLOCK_META_ERR;
		}
	
	}
	fclose(storage_spare_pos);
	return SUCCESS;

NULL_SRC_META_ERR:
	fprintf(stderr, "치명적 오류 : 기록 위한 meta 정보가 존재하지 않음 (SPARE_write)\n");
	system("pause");
	exit(1);

NULL_FILE_PTR_ERR:
	fprintf(stderr, "치명적 오류 : storage.bin 파일을 읽고 쓰기 모드로 열 수 없습니다. (SPARE_write)\n");
	system("pause");
	exit(1);

WRITE_BLOCK_META_ERR:
	fprintf(stderr, "치명적 오류 :  PBN : %u 의 Offset : %d 에 대한 meta 정보 쓰기 실패 (SPARE_writes)\n", PBN, offset_index_dump);
	system("pause");
	exit(1);
}


int update_victim_block_info(class FlashMem*& flashmem, bool is_logical, enum VICTIM_BLOCK_PROC_STATE proc_state, unsigned int src_block_num, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type) //Victim Block 선정을 위한 블록 정보 구조체 갱신 및 GC 스케줄러 실행
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	unsigned int LBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN1 = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN2 = DYNAMIC_MAPPING_INIT_VALUE;
	float LBN_invalid_ratio = -1;
	float PBN_invalid_ratio = -1; //PBN1 or PBN2 (단일 블록에 대한 무효율 계산)
	float PBN1_invalid_ratio = -1;
	float PBN2_invalid_ratio = -1;

	META_DATA** block_meta_buffer_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 구조체 배열 형태

	/***
		Victim Block 정보 구조체 초기값
		---
		victim_block_num = DYNAMIC_MAPPING_INIT_VALUE;
		victim_block_invalid_ratio = -1;
	***/

	//아직 처리되지 않은 Victim Block 정보가 존재하면 치명적 오류
	if (flashmem->victim_block_info.victim_block_num != DYNAMIC_MAPPING_INIT_VALUE && flashmem->victim_block_info.victim_block_invalid_ratio != -1)
		goto VICTIM_BLOCK_INFO_EXCEPTION_ERR;

	switch (proc_state)
	{
	case VICTIM_BLOCK_PROC_STATE::SPARE_LINKED:
		flashmem->victim_block_info.proc_state = VICTIM_BLOCK_PROC_STATE::SPARE_LINKED;
		break;

	case VICTIM_BLOCK_PROC_STATE::UNLINKED:
		flashmem->victim_block_info.proc_state = VICTIM_BLOCK_PROC_STATE::UNLINKED;
		break;

	default:
		goto WRONG_VICTIM_BLOCK_PROC_STATE;
	}

	/***
		< Block Mapping >

		- Block Mapping에서 Overwrite발생 시 해당 블록은 항상 무효화된다. 이에 따라, GC에서 적절한 시기에 PBN을 Erase수행하여야 함
		=> Overwrite발생 시 유효 데이터는 새로운 블록(여분의 빈 Spare Block을 사용)으로 copy되었고, 기존 블록은 더 이상 사용하지 않으므로,
		무효율 계산을 위한 스캐닝을 하지 않고 항상 무효율을 1.0 으로 설정

		------------------------------------------------------------------

		< Hybrid Mapping (Log algorithm) >

		- Hybrid Mapping (Log algorithm)에서 PBN1(Data Block) 또는 PBN2(Log Block) 중 하나가 완전 무효화되는 시점은
		한 블록 내의 특정 오프셋에 대한 반복적 Overwrite가 발생하지 않고, 모든 오프셋에 대하여 Overwrite가 발생한 경우

		- 이에 따라, Erase 시점은 LBN에 대응된 PBN1(Data Block) 또는 PBN2(Log Block) 중 하나가 완전 무효화되는 경우 Victim Block으로 선정되어 GC에 의해 처리

		- Merge를 하고자 한다면, LBN에 PBN1(Data Block)과 PBN2(Log Block) 모두 대응되어 있어야 하며(즉, 유효한 블록), 양쪽 모두 일부 유효 데이터를 포함하고 있어야 한다.

		- Merge 시점은,
			1) Overwrite가 발생하였지만 PBN1 혹은 PBN2에 새로운 데이터를 기록 할 공간이 없을 경우
			: 쓰기 작업 중에 FTL함수에 의하여 Merge 수행

			2) 물리적 가용 공간 확보
			: GC에 의해 현재 매핑 테이블을 통해 전체 논리 블록들 중 Merge 가능한 LBN에 대해서 Merge 수행

		- Hybrid Mapping (Log algorithm)에서 GC의 역할은 물리적 가용 공간(Physical Remaining Space)에 따른 Block Invalid Ratio Threshold에 따라
			1) 완전 무효화된 PBN일 경우 Erase 수행
			2) 일부 유효 데이터를 포함하고 있는 LBN에 대응된 PBN1과 PBN2에 대하여 Merge 수행
			=> 쓰기 작업이 발생한 LBN의 PBN1과 PBN2에 대해 통합된 무효율 값 계산
	***/

	switch (mapping_method)
	{
	case MAPPING_METHOD::BLOCK: //블록 매핑
		if (is_logical == true) //src_block_num이 LBN일 경우
			return FAIL;
		else //src_block_num이 PBN일 경우
		{
			flashmem->victim_block_info.is_logical = false;
			flashmem->victim_block_info.victim_block_num = PBN = src_block_num;
			flashmem->victim_block_info.victim_block_invalid_ratio = 1.0;

			goto BLOCK_MAPPING;
		}


	case MAPPING_METHOD::HYBRID_LOG: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
		if (is_logical == true) //src_block_num이 LBN일 경우
		{
			LBN = src_block_num;
			PBN1 = flashmem->log_block_level_mapping_table[LBN][0];
			PBN2 = flashmem->log_block_level_mapping_table[LBN][1];

			goto HYBRID_LOG_LBN;
		}
		else //src_block_num이 PBN일 경우
		{
			PBN = src_block_num;
			flashmem->victim_block_info.is_logical = false;

			goto HYBRID_LOG_PBN;
		}

	default:
		return FAIL;
	}

BLOCK_MAPPING:
	goto END_SUCCESS;

HYBRID_LOG_PBN: //PBN1 or PBN2 (단일 블록에 대한 무효율 계산)
	flashmem->victim_block_info.victim_block_num = PBN;

	/*** Calculate PBN Invalid Ratio ***/
	SPARE_reads(flashmem, PBN, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	calc_block_invalid_ratio(block_meta_buffer_array, PBN_invalid_ratio);
	
	/*** Deallocate block_meta_buffer_array ***/
	if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
		goto MEM_LEAK_ERR;

	try
	{
		if (PBN_invalid_ratio >= 0 && PBN_invalid_ratio <= 1)
			flashmem->victim_block_info.victim_block_invalid_ratio = PBN_invalid_ratio;
		else
			throw PBN_invalid_ratio;
	}
	catch (float& PBN_invalid_ratio)
	{
		fprintf(stderr, "치명적 오류 : 잘못된 무효율(%f)", PBN_invalid_ratio);
		system("pause");
		exit(1);
	}

	goto END_SUCCESS;

HYBRID_LOG_LBN:
	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //양쪽 다 대응되어 있지 않으면
		goto NON_ASSIGNED_LBN;

	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE || PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //하나라도 대응되어 있지 않으면, 단일 블록에 대한 연산 수행
	{
		flashmem->victim_block_info.is_logical = false;

		//대응되어 있는 블록에 대해 단일 블록 처리 루틴에서 사용할 PBN으로 할당
		if (PBN1 != DYNAMIC_MAPPING_INIT_VALUE)
			PBN = PBN1;
		else
			PBN = PBN2;

		//단일 블록 처리 루틴으로 이동
		goto HYBRID_LOG_PBN;
	}

	/*** Calculate PBN1 Invalid Ratio ***/
	SPARE_reads(flashmem, PBN1, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	calc_block_invalid_ratio(block_meta_buffer_array, PBN1_invalid_ratio);

	/*** Deallocate block_meta_buffer_array ***/
	if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
		goto MEM_LEAK_ERR;

	/*** Calculate PBN2 Invalid Ratio ***/
	SPARE_reads(flashmem, PBN2, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	calc_block_invalid_ratio(block_meta_buffer_array, PBN2_invalid_ratio);
	
	/*** Deallocate block_meta_buffer_array ***/
	if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
		goto MEM_LEAK_ERR;

	flashmem->victim_block_info.is_logical = true;
	flashmem->victim_block_info.victim_block_num = LBN;

	try
	{
		LBN_invalid_ratio = (float)((PBN1_invalid_ratio + PBN2_invalid_ratio) / 2); //LBN의 무효율 계산
		if (LBN_invalid_ratio >= 0 && LBN_invalid_ratio <= 1)
			flashmem->victim_block_info.victim_block_invalid_ratio = LBN_invalid_ratio;
		else
			throw LBN_invalid_ratio;
	}
	catch (float& LBN_invalid_ratio)
	{
		fprintf(stderr, "치명적 오류 : 잘못된 무효율(%f)", LBN_invalid_ratio);
		system("pause");
		exit(1);
	}

	goto END_SUCCESS;

END_SUCCESS:
	flashmem->gc->scheduler(flashmem, mapping_method, table_type); //갱신 완료된 Victim Block 정보 처리를 위한 GC 스케줄러 실행
	return SUCCESS;

NON_ASSIGNED_LBN:
	return COMPLETE;

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (update_victim_block_info)\n");
	system("pause");
	exit(1);

VICTIM_BLOCK_INFO_EXCEPTION_ERR:
	fprintf(stderr, "치명적 오류 : 아직 처리되지 않은 Victim Block 존재 (update_victim_block_info)\n");
	system("pause");
	exit(1);

WRONG_VICTIM_BLOCK_PROC_STATE:
	fprintf(stderr, "치명적 오류 : Wrong VICTIM_BLOCK_PROC_STATE (one_dequeue_job)\n");
	system("pause");
	exit(1);
}

/*** 이미 읽어들인 meta 정보를 이용하여 수행 ***/
int update_victim_block_info(class FlashMem*& flashmem, bool is_logical, enum VICTIM_BLOCK_PROC_STATE proc_state, unsigned int src_block_num, META_DATA**& src_block_meta_buffer_array, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type) //Victim Block 선정을 위한 블록 정보 구조체 갱신 및 GC 스케줄러 실행 (블록 매핑)
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	unsigned int PBN = DYNAMIC_MAPPING_INIT_VALUE;
	float PBN_invalid_ratio = -1; //PBN 무효율

	//상위 계층에서 src_block_meta_buffer_array 메모리 해제 수행
	if (src_block_meta_buffer_array == NULL)
		goto MEM_LEAK_ERR;

	/***
		Victim Block 정보 구조체 초기값
		---
		victim_block_num = DYNAMIC_MAPPING_INIT_VALUE;
		victim_block_invalid_ratio = -1;
	***/

	//아직 처리되지 않은 Victim Block 정보가 존재하면 치명적 오류
	if (flashmem->victim_block_info.victim_block_num != DYNAMIC_MAPPING_INIT_VALUE && flashmem->victim_block_info.victim_block_invalid_ratio != -1)
		goto VICTIM_BLOCK_INFO_EXCEPTION_ERR;

	switch (proc_state)
	{
	case VICTIM_BLOCK_PROC_STATE::SPARE_LINKED:
		flashmem->victim_block_info.proc_state = VICTIM_BLOCK_PROC_STATE::SPARE_LINKED;
		break;

	case VICTIM_BLOCK_PROC_STATE::UNLINKED:
		flashmem->victim_block_info.proc_state = VICTIM_BLOCK_PROC_STATE::UNLINKED;
		break;

	default:
		goto WRONG_VICTIM_BLOCK_PROC_STATE;
	}

	switch (mapping_method)
	{
	case MAPPING_METHOD::BLOCK: //블록 매핑
		if (is_logical == true) //src_block_num이 LBN일 경우
			return FAIL;
		else //src_block_num이 PBN일 경우
		{
			flashmem->victim_block_info.is_logical = false;
			flashmem->victim_block_info.victim_block_num = PBN = src_block_num;
			flashmem->victim_block_info.victim_block_invalid_ratio = 1.0;

			goto BLOCK_MAPPING;
		}

	default:
		return FAIL;
	}

BLOCK_MAPPING:
	goto END_SUCCESS;

END_SUCCESS:
	flashmem->gc->scheduler(flashmem, mapping_method, table_type); //갱신 완료된 Victim Block 정보 처리를 위한 GC 스케줄러 실행
	return SUCCESS;

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (update_victim_block_info)\n");
	system("pause");
	exit(1);

VICTIM_BLOCK_INFO_EXCEPTION_ERR:
	fprintf(stderr, "치명적 오류 : 아직 처리되지 않은 Victim Block 존재 (update_victim_block_info)\n");
	system("pause");
	exit(1);

WRONG_VICTIM_BLOCK_PROC_STATE:
	fprintf(stderr, "치명적 오류 : Wrong VICTIM_BLOCK_PROC_STATE (one_dequeue_job)\n");
	system("pause");
	exit(1);
}

int update_victim_block_info(class FlashMem*& flashmem, bool is_logical, enum VICTIM_BLOCK_PROC_STATE proc_state, unsigned int src_block_num, META_DATA**& src_PBN1_block_meta_buffer_array, META_DATA**& src_PBN2_block_meta_buffer_array, enum MAPPING_METHOD mapping_method, enum TABLE_TYPE table_type) //Victim Block 선정을 위한 블록 정보 구조체 갱신 및 GC 스케줄러 실행 (하이브리드 매핑)
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	unsigned int LBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN1 = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN2 = DYNAMIC_MAPPING_INIT_VALUE;
	float LBN_invalid_ratio = -1;
	float PBN1_invalid_ratio = -1;
	float PBN2_invalid_ratio = -1;

	//상위 계층에서 src_PBN1_block_meta_buffer_array 및 src_PBN2_block_meta_buffer_array 메모리 해제 수행
	if (src_PBN1_block_meta_buffer_array == NULL || src_PBN2_block_meta_buffer_array == NULL)
		goto MEM_LEAK_ERR;

	/***
		Victim Block 정보 구조체 초기값
		---
		victim_block_num = DYNAMIC_MAPPING_INIT_VALUE;
		victim_block_invalid_ratio = -1;
	***/

	//아직 처리되지 않은 Victim Block 정보가 존재하면 치명적 오류
	if (flashmem->victim_block_info.victim_block_num != DYNAMIC_MAPPING_INIT_VALUE && flashmem->victim_block_info.victim_block_invalid_ratio != -1)
		goto VICTIM_BLOCK_INFO_EXCEPTION_ERR;

	switch (proc_state)
	{
	case VICTIM_BLOCK_PROC_STATE::SPARE_LINKED:
		flashmem->victim_block_info.proc_state = VICTIM_BLOCK_PROC_STATE::SPARE_LINKED;
		break;
	case VICTIM_BLOCK_PROC_STATE::UNLINKED:
		flashmem->victim_block_info.proc_state = VICTIM_BLOCK_PROC_STATE::UNLINKED;
		break;

	default:
		goto WRONG_VICTIM_BLOCK_PROC_STATE;
	}

	switch (mapping_method)
	{
	case MAPPING_METHOD::HYBRID_LOG: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
		if (is_logical == true) //src_block_num이 LBN일 경우
		{
			LBN = src_block_num;
			PBN1 = flashmem->log_block_level_mapping_table[LBN][0];
			PBN2 = flashmem->log_block_level_mapping_table[LBN][1];

			goto HYBRID_LOG_LBN;
		}
		else //src_block_num이 PBN일 경우 : PBN1과 PBN2의 블록 단위 meta 정보를 모두 받았으므로 이 경우는 잘못 호출 한 것이다.
			return FAIL;

	default:
		return FAIL;
	}

HYBRID_LOG_LBN:
	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //양쪽 다 대응되어 있지 않으면
		goto NON_ASSIGNED_LBN;

	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE || PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //하나라도 대응되어 있지 않으면
		goto MISMATCH_BETWEEN_META_TBL_ERR; //meta 정보와 블록 단위 매핑 테이블 간의 불일치

	/*** Calculate PBN1 Invalid Ratio ***/
	calc_block_invalid_ratio(src_PBN1_block_meta_buffer_array, PBN1_invalid_ratio);

	/*** Calculate PBN2 Invalid Ratio ***/
	calc_block_invalid_ratio(src_PBN2_block_meta_buffer_array, PBN2_invalid_ratio);

	flashmem->victim_block_info.is_logical = true;
	flashmem->victim_block_info.victim_block_num = LBN;

	try
	{
		LBN_invalid_ratio = (float)((PBN1_invalid_ratio + PBN2_invalid_ratio) / 2); //LBN의 무효율 계산
		if (LBN_invalid_ratio >= 0 && LBN_invalid_ratio <= 1)
			flashmem->victim_block_info.victim_block_invalid_ratio = LBN_invalid_ratio;
		else
			throw LBN_invalid_ratio;
	}
	catch (float& LBN_invalid_ratio)
	{
		fprintf(stderr, "치명적 오류 : 잘못된 무효율(%f)", LBN_invalid_ratio);
		system("pause");
		exit(1);
	}

	goto END_SUCCESS;

END_SUCCESS:
	flashmem->gc->scheduler(flashmem, mapping_method, table_type); //갱신 완료된 Victim Block 정보 처리를 위한 GC 스케줄러 실행
	return SUCCESS;

NON_ASSIGNED_LBN:
	return COMPLETE;

MISMATCH_BETWEEN_META_TBL_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보와 블록 단위 매핑 테이블 간의 정보 불일치 (update_victim_block_info)\n");
	system("pause");
	exit(1);

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (update_victim_block_info)\n");
	system("pause");
	exit(1);

VICTIM_BLOCK_INFO_EXCEPTION_ERR:
	fprintf(stderr, "치명적 오류 : 아직 처리되지 않은 Victim Block 존재 (update_victim_block_info)\n");
	system("pause");
	exit(1);

WRONG_VICTIM_BLOCK_PROC_STATE:
	fprintf(stderr, "치명적 오류 : Wrong VICTIM_BLOCK_PROC_STATE (one_dequeue_job)\n");
	system("pause");
	exit(1);
}

int update_v_flash_info_for_reorganization(class FlashMem*& flashmem, META_DATA**& src_block_meta_buffer_array) //특정 물리 블록 하나에 대한 META_DATA 구조체 배열을 통한 판별을 수행하여 물리적 가용 가능 공간 계산 위한 가변적 플래시 메모리 정보 갱신
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	if (src_block_meta_buffer_array != NULL)
	{
		for (__int8 Poffset = 0; Poffset < BLOCK_PER_SECTOR; Poffset++) //블록 내의 각 페이지에 대해 인덱싱
		{
			switch (src_block_meta_buffer_array[Poffset]->sector_state)
			{
			case SECTOR_STATE::EMPTY:  //비어있을 경우
				//do nothing
				break;

			case SECTOR_STATE::VALID: //비어있지 않고, 유효한 페이지이면
				flashmem->v_flash_info.written_sector_count++; //기록된 페이지 수 증가
				break;

			case SECTOR_STATE::INVALID: //비어있지 않고, 유효하지 않은 페이지이면
				flashmem->v_flash_info.written_sector_count++; //기록된 페이지 수 증가
				flashmem->v_flash_info.invalid_sector_count++; //무효 페이지 수 증가
				break;	
			}
		}
	}
	else
		goto NULL_SRC_META_ERR;

	return SUCCESS;

NULL_SRC_META_ERR:
	fprintf(stderr, "치명적 오류 : 가변적 플래시 메모리 정보 갱신 위한 meta 정보가 존재하지 않음 (update_v_flash_info_for_reorganization)\n");
	system("pause");
	exit(1);
}

int update_v_flash_info_for_erase(class FlashMem*& flashmem, META_DATA**& src_block_meta_buffer_array) //Erase하고자 하는 특정 물리 블록 하나에 대해 META_DATA 구조체 배열을 통한 판별을 수행하여 플래시 메모리의 가변적 정보 갱신
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	//for Remaining Space Management
	if (src_block_meta_buffer_array != NULL)
	{
		for (__int8 Poffset = 0; Poffset < BLOCK_PER_SECTOR; Poffset++) //블록 내의 각 페이지에 대해 인덱싱
		{
			switch (src_block_meta_buffer_array[Poffset]->sector_state)
			{
			case SECTOR_STATE::EMPTY:  //비어있을 경우
				//do nothing
				break;

			case SECTOR_STATE::VALID: //비어있지 않고, 유효한 페이지이면
				flashmem->v_flash_info.written_sector_count--; //기록된 페이지 수 감소
				break;

			case SECTOR_STATE::INVALID: //비어있지 않고, 유효하지 않은 페이지이면
				flashmem->v_flash_info.written_sector_count--; //기록된 페이지 수 감소
				flashmem->v_flash_info.invalid_sector_count--; //무효 페이지 수 감소
				break;
			}
		}
	}
	else
		goto NULL_SRC_META_ERR;

	return SUCCESS;

NULL_SRC_META_ERR:
	fprintf(stderr, "치명적 오류 : 가변적 플래시 메모리 정보 갱신 위한 meta 정보가 존재하지 않음 (update_v_flash_info_for_erase)\n");
	system("pause");
	exit(1);
}

int calc_block_invalid_ratio(META_DATA**& src_block_meta_buffer_array, float& dst_block_invalid_ratio) //특정 물리 블록 하나에 대한 META_DATA 구조체 배열을 통한 판별을 수행하여 무효율 계산 및 전달
{
	//for Calculate Block Invalid Ratio
	__int8 block_per_written_sector_count = 0;
	__int8 block_per_invalid_sector_count = 0;
	__int8 block_per_empty_sector_count = 0;

	if (src_block_meta_buffer_array != NULL)
	{
		for (__int8 Poffset = 0; Poffset < BLOCK_PER_SECTOR; Poffset++) //블록 내의 각 페이지에 대해 인덱싱
		{
			switch (src_block_meta_buffer_array[Poffset]->sector_state)
			{
			case SECTOR_STATE::EMPTY:  //비어있을 경우, 항상 유효한 페이지이다
				block_per_empty_sector_count++; //빈 페이지 수 증가
				break;

			case SECTOR_STATE::VALID: //비어있지 않고, 유효한 페이지이면
				block_per_written_sector_count++; //기록된 페이지 수 증가
				break;

			case SECTOR_STATE::INVALID: //비어있지 않고, 유효하지 않은 페이지이면
				block_per_written_sector_count++; //기록된 페이지 수 증가
				block_per_invalid_sector_count++; //무효 페이지 수 증가
				break;
			}
		}
	}
	else
		goto NULL_SRC_META_ERR;

	try
	{
		float block_invalid_ratio = (float)block_per_invalid_sector_count / (float)BLOCK_PER_SECTOR; //현재 블록의 무효율 계산
		if (block_invalid_ratio >= 0 && block_invalid_ratio <= 1)
			dst_block_invalid_ratio = block_invalid_ratio;
		else
			throw block_invalid_ratio;
	}
	catch (float& block_invalid_ratio)
	{
		fprintf(stderr, "치명적 오류 : 잘못된 무효율(%f)\n", block_invalid_ratio);
		fprintf(stderr, "block_per_written_sector_count : %d, block_per_invalid_sector_count : %d, block_per_empty_sector_count : %d\n", block_per_written_sector_count, block_per_invalid_sector_count, block_per_empty_sector_count);
		system("pause");
		exit(1);
	}

	return SUCCESS;

NULL_SRC_META_ERR:
	fprintf(stderr, "치명적 오류 : 블록 무효율 계산 위한 meta 정보가 존재하지 않음 (calc_block_invalid_ratio)\n");
	system("pause");
	exit(1);
}
		
/* 삭제
int search_empty_normal_block_for_dynamic_table(class FlashMem*& flashmem, unsigned int& dst_block_num, META_DATA*& dst_meta_buffer, MAPPING_METHOD mapping_method, TABLE_TYPE table_type) //빈 일반 물리 블록(PBN)을 순차적으로 탐색하여 PBN또는 테이블 상 LBN 값, 해당 PBN의 meta정보 전달
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	unsigned int PSN = DYNAMIC_MAPPING_INIT_VALUE; //실제로 저장된 물리 섹터 번호

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼

	f_flash_info = flashmem->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	switch (mapping_method) //매핑 방식에 따라 해당 처리 위치로 이동
	{
	case MAPPING_METHOD::BLOCK: //블록 매핑
		if (table_type == TABLE_TYPE::STATIC) //블록 매핑 Static Table
			goto BLOCK_MAPPING_STATIC_PROC;

		else if (table_type == TABLE_TYPE::DYNAMIC) //블록 매핑 Dynamic Table
			goto DYNAMIC_COMMON_PROC;

		else
			goto WRONG_TABLE_TYPE_ERR;

	case 3: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
		goto DYNAMIC_COMMON_PROC;

	default:
		goto WRONG_FUNC_CALL_ERR;
	}

BLOCK_MAPPING_STATIC_PROC: //블록 매핑 Static Table : 블록 단위 매핑 테이블을 통한 각 LBN에 대응된 PBN의 Spare 영역 판별, 빈 블록 순차탐색 후 LBN전달
	for (unsigned int table_index = 0; table_index < f_flash_info.block_size - f_flash_info.spare_block_size; table_index++) //순차적으로 비어있는 블록을 찾는다 (정적 테이블이므로 테이블 크기만큼 수행)
	{
		//정적 테이블이므로 LBN전달을 위하여 테이블을 통하여 검색
		PSN = (flashmem->block_level_mapping_table[table_index] * BLOCK_PER_SECTOR); //해당 블록의 첫 번째 페이지
		SPARE_read(flashmem, PSN, meta_buffer); //Spare 영역을 읽음

		if (meta_buffer->block_state == BLOCK_STATE::NORMAL_BLOCK_EMPTY)
			//일반 블록에 대하여, 비어있고, 유효한 블록이면
		{
			//LBN 및 meta 정보 전달
			dst_block_num = table_index;
			dst_meta_buffer = meta_buffer;

			return SUCCESS;
		}

		if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
			goto MEM_LEAK_ERR;
	}
	//만약 빈 블록을 찾지 못했으면
	return COMPLETE;

DYNAMIC_COMMON_PROC: //Dynamic Table 공용 처리 루틴 : 각 물리 블록의 Spare 영역 판별, 빈 블록 순차탐색 후 PBN전달
	for (unsigned int block_index = 0; block_index < f_flash_info.block_size; block_index++) //순차적으로 비어있는 블록을 찾는다 (Spare 블록의 위치는 쓰기가 일어남에 따라 가변적이므로 모든 블록에 대해 스캐닝)
	{
		PSN = (block_index * BLOCK_PER_SECTOR); //해당 블록의 첫 번째 페이지
		SPARE_read(flashmem, PSN, meta_buffer); //Spare 영역을 읽음

		if (meta_buffer->block_state == BLOCK_STATE::NORMAL_BLOCK_EMPTY)
			//일반 블록에 대하여, 비어있고, 유효한 블록이면
		{
			//PBN 및 meta 정보 전달
			dst_block_num = block_index;
			dst_meta_buffer = meta_buffer;

			return SUCCESS;
		}

		if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
			goto MEM_LEAK_ERR;
	}

	//만약 빈 블록을 찾지 못했으면
	return COMPLETE;

WRONG_TABLE_TYPE_ERR: //잘못된 테이블 타입
	fprintf(stderr, "치명적 오류 : 잘못된 테이블 타입\n");
	system("pause");
	exit(1);

WRONG_FUNC_CALL_ERR:
	fprintf(stderr, "치명적 오류 : 잘못된 함수 호출\n");
	system("pause");
	exit(1);

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (search_empty_normal_block)\n");
	system("pause");
	exit(1);
}
*/
int search_empty_offset_in_block(class FlashMem*& flashmem, unsigned int src_PBN, __int8& dst_Poffset, META_DATA*& dst_meta_buffer, enum MAPPING_METHOD mapping_method) //일반 물리 블록(PBN) 내부를 순차적인 비어있는 위치 탐색, Poffset 값, 해당 위치의 meta정보 전달
{
	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	unsigned int PSN = DYNAMIC_MAPPING_INIT_VALUE; //실제로 저장된 물리 섹터 번호
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼
	__int8 low, mid, high, current_empty_index;

	switch (flashmem->search_mode)
	{
	case SEARCH_MODE::SEQ_SEARCH: //순차 탐색
		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++) //순차적으로 비어있는 페이지를 찾는다
		{
			PSN = (src_PBN * BLOCK_PER_SECTOR) + offset_index;
			SPARE_read(flashmem, PSN, meta_buffer); //Spare 영역을 읽음

			if (meta_buffer->sector_state == SECTOR_STATE::EMPTY) //비어있고, 유효한 페이지이면
			{
				//Poffset 및 meta 정보 전달
				dst_Poffset = offset_index;
				dst_meta_buffer = meta_buffer;

				return SUCCESS;
			}

			if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;
		}
		break;

	case SEARCH_MODE::BINARY_SEARCH: //이진 탐색
		if (mapping_method != MAPPING_METHOD::HYBRID_LOG) //페이지 단위 매핑을 사용 할 경우에만, 이진 탐색 사용 가능
			goto WRONG_BINARY_SEARCH_MODE_ERR;

		low = 0;
		high = BLOCK_PER_SECTOR - 1;
		mid = get_rand_offset(); //Wear-leveling을 위한 초기 랜덤한 탐색 위치 지정
		current_empty_index = OFFSET_MAPPING_INIT_VALUE;

		while (low <= high)
		{
			PSN = (src_PBN * BLOCK_PER_SECTOR) + mid; //탐색 위치
			SPARE_read(flashmem, PSN, meta_buffer); //Spare 영역을 읽음

			if (meta_buffer->sector_state == SECTOR_STATE::EMPTY) //비어있으면
			{
				//왼쪽으로 탐색
				current_empty_index = mid;
				high = mid - 1;
			}
			else //비어있지 않으면
			{
				//왼쪽은 모두 비어있지 않음
				//오른쪽으로 탐색
				low = mid + 1;
			}
			
			mid = (low + high) / 2;

			if (low <= high && current_empty_index != OFFSET_MAPPING_INIT_VALUE) //탐색 완료 후 빈 섹터가 존재하는 경우, meta 정보 전달 위해 현재 meta_buffer 보존
				break;

			if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;
		}

		if (current_empty_index != OFFSET_MAPPING_INIT_VALUE)
		{
			//Poffset 및 meta 정보 전달
			dst_Poffset = current_empty_index;
			dst_meta_buffer = meta_buffer;

			return SUCCESS;
		}
		break;
	}

	//만약 빈 페이지를 찾지 못했으면
	return FAIL;

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (search_empty_offset_in_block)\n");
	system("pause");
	exit(1);

WRONG_BINARY_SEARCH_MODE_ERR:
	fprintf(stderr, "치명적 오류 : 페이지 단위 매핑을 사용 할 경우에만, 이진 탐색 사용 가능 (search_empty_offset_in_block)\n");
	system("pause");
	exit(1);
}

int search_empty_offset_in_block(META_DATA**& src_block_meta_buffer_array, __int8& dst_Poffset, enum MAPPING_METHOD mapping_method) //일반 물리 블록(PBN)의 블록 단위 meta 정보를 순차적인 비어있는 위치 탐색, Poffset 값 전달
{
	return 0;//일단보류
}

int print_block_meta_info(class FlashMem*& flashmem, bool is_logical, unsigned int src_block_num, enum MAPPING_METHOD mapping_method) //블록 내의 모든 섹터(페이지)의 meta 정보 출력
{
	FILE* block_meta_output = NULL;

	META_DATA** block_meta_buffer_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태
	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	unsigned int LBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN1 = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN2 = DYNAMIC_MAPPING_INIT_VALUE;

	if (flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return FAIL;
	}

	f_flash_info = flashmem->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	if ((block_meta_output = fopen("block_meta_output.txt", "wt")) == NULL)
	{
		fprintf(stderr, "block_meta_output.txt 파일을 쓰기모드로 열 수 없습니다. (print_block_meta_info)");
		return FAIL; //의존성이 없으며 단순 출력만 수행 하므로, 실패만 반환
	}

	switch (mapping_method)
	{
	case MAPPING_METHOD::BLOCK: //블록 매핑
		if (is_logical == true) //src_block_num이 LBN일 경우
			goto BLOCK_LBN;
		else //src_block_num이 PBN일 경우
			goto COMMON_PBN;

	case MAPPING_METHOD::HYBRID_LOG: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
		if (is_logical == true) //src_block_num이 LBN일 경우
			goto HYBRID_LOG_LBN;
		else //src_block_num이 PBN일 경우
			goto COMMON_PBN;

	default:
		goto COMMON_PBN;
	}

COMMON_PBN: //PBN에 대한 공용 처리 루틴
	PBN = src_block_num;

	if (PBN == DYNAMIC_MAPPING_INIT_VALUE || PBN > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1))
		goto OUT_OF_RANGE;

	SPARE_reads(flashmem, PBN, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	fprintf(block_meta_output, "===== PBN : %u =====", PBN);
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		printf("\n< Offset : %d >\n", offset_index);
		fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);

		printf("Block State : ");
		switch (block_meta_buffer_array[offset_index]->block_state)
		{
		case BLOCK_STATE::NORMAL_BLOCK_EMPTY:
			fprintf(block_meta_output, "NORMAL_BLOCK_EMPTY\n");
			printf("NORMAL_BLOCK_EMPTY\n");
			break;

		case BLOCK_STATE::NORMAL_BLOCK_VALID:
			fprintf(block_meta_output, "NORMAL_BLOCK_VALID\n");
			printf("NORMAL_BLOCK_VALID\n");
			break;

		case BLOCK_STATE::NORMAL_BLOCK_INVALID:
			fprintf(block_meta_output, "NORMAL_BLOCK_INVALID\n");
			printf("NORMAL_BLOCK_INVALID\n");
			break;

		case BLOCK_STATE::SPARE_BLOCK_EMPTY:
			fprintf(block_meta_output, "SPARE_BLOCK_EMPTY\n");
			printf("SPARE_BLOCK_EMPTY\n");
			break;

		case BLOCK_STATE::SPARE_BLOCK_VALID:
			fprintf(block_meta_output, "SPARE_BLOCK_VALID\n");
			printf("SPARE_BLOCK_VALID\n");
			break;

		case BLOCK_STATE::SPARE_BLOCK_INVALID:
			fprintf(block_meta_output, "SPARE_BLOCK_INVALID\n");
			printf("SPARE_BLOCK_INVALID\n");
			break;
		}

		printf("Sector State : ");
		switch (block_meta_buffer_array[offset_index]->sector_state)
		{
		case SECTOR_STATE::EMPTY:
			fprintf(block_meta_output, "EMPTY\n");
			printf("EMPTY\n");
			break;

		case SECTOR_STATE::VALID:
			fprintf(block_meta_output, "VALID\n");
			printf("VALID\n");
			break;

		case SECTOR_STATE::INVALID:
			fprintf(block_meta_output, "INVALID\n");
			printf("INVALID\n");
			break;
		}
	}

	/*** Deallocate block_meta_buffer_array ***/
	if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
		goto MEM_LEAK_ERR;

	goto END_SUCCESS;

BLOCK_LBN: //블록 매핑 LBN 처리 루틴
	LBN = src_block_num;
	PBN = flashmem->block_level_mapping_table[LBN];

	//LBN 범위를 Spare Block 개수만큼 제외한 범위로 제한
	if (LBN == DYNAMIC_MAPPING_INIT_VALUE || LBN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - (f_flash_info.spare_block_size) - 1))
		goto OUT_OF_RANGE;

	if (PBN == DYNAMIC_MAPPING_INIT_VALUE)
		goto NON_ASSIGNED_LBN;

	if (PBN > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1)) //PBN이 잘못된 값으로 대응되어 있을 경우
		goto WRONG_ASSIGNED_LBN_ERR;

	fprintf(block_meta_output, "===== LBN : %u =====\n", LBN);
	SPARE_reads(flashmem, PBN, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	fprintf(block_meta_output, "===== PBN : %u =====", PBN);

	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		printf("\n< Offset : %d >\n", offset_index);
		fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);
		
		printf("Block State : ");
		switch (block_meta_buffer_array[offset_index]->block_state)
		{
		case BLOCK_STATE::NORMAL_BLOCK_EMPTY:
			fprintf(block_meta_output, "NORMAL_BLOCK_EMPTY\n");
			printf("NORMAL_BLOCK_EMPTY\n");
			break;

		case BLOCK_STATE::NORMAL_BLOCK_VALID:
			fprintf(block_meta_output, "NORMAL_BLOCK_VALID\n");
			printf("NORMAL_BLOCK_VALID\n");
			break;

		case BLOCK_STATE::NORMAL_BLOCK_INVALID:
			fprintf(block_meta_output, "NORMAL_BLOCK_INVALID\n");
			printf("NORMAL_BLOCK_INVALID\n");
			break;

		case BLOCK_STATE::SPARE_BLOCK_EMPTY:
			fprintf(block_meta_output, "SPARE_BLOCK_EMPTY\n");
			printf("SPARE_BLOCK_EMPTY\n");
			break;

		case BLOCK_STATE::SPARE_BLOCK_VALID:
			fprintf(block_meta_output, "SPARE_BLOCK_VALID\n");
			printf("SPARE_BLOCK_VALID\n");
			break;

		case BLOCK_STATE::SPARE_BLOCK_INVALID:
			fprintf(block_meta_output, "SPARE_BLOCK_INVALID\n");
			printf("SPARE_BLOCK_INVALID\n");
			break;
		}

		printf("Sector State : ");
		switch (block_meta_buffer_array[offset_index]->sector_state)
		{
		case SECTOR_STATE::EMPTY:
			fprintf(block_meta_output, "EMPTY\n");
			printf("EMPTY\n");
			break;

		case SECTOR_STATE::VALID:
			fprintf(block_meta_output, "VALID\n");
			printf("VALID\n");
			break;

		case SECTOR_STATE::INVALID:
			fprintf(block_meta_output, "INVALID\n");
			printf("INVALID\n");
			break;
		}
	}

	/*** Deallocate block_meta_buffer_array ***/
	if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
		goto MEM_LEAK_ERR;

	goto END_SUCCESS;

HYBRID_LOG_LBN: //하이브리드 매핑 LBN 처리 루틴
	LBN = src_block_num;
	PBN1 = flashmem->log_block_level_mapping_table[LBN][0];
	PBN2 = flashmem->log_block_level_mapping_table[LBN][1];

	//LBN 범위를 Spare Block 개수만큼 제외한 범위로 제한
	if (LBN == DYNAMIC_MAPPING_INIT_VALUE || LBN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - (f_flash_info.spare_block_size) - 1))
		goto OUT_OF_RANGE;

	//모두 대응되어 있지 않으면
	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
		goto NON_ASSIGNED_LBN;

	if (PBN1 != DYNAMIC_MAPPING_INIT_VALUE && PBN1 > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1) ||
		PBN2 != DYNAMIC_MAPPING_INIT_VALUE && PBN2 > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1)) //PBN1 또는 PBN2가 잘못된 값으로 대응되어 있을 경우
		goto WRONG_ASSIGNED_LBN_ERR;

	fprintf(block_meta_output, "===== LBN : %u =====\n", LBN);
	if (PBN1 != DYNAMIC_MAPPING_INIT_VALUE)
	{
		SPARE_reads(flashmem, PBN1, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
		fprintf(block_meta_output, "===== PBN1 : %u =====", PBN1);

		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		{
			printf("\n< Offset : %d >\n", offset_index);
			fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);

			printf("Block State : ");
			switch (block_meta_buffer_array[offset_index]->block_state)
			{
			case BLOCK_STATE::NORMAL_BLOCK_EMPTY:
				fprintf(block_meta_output, "NORMAL_BLOCK_EMPTY\n");
				printf("NORMAL_BLOCK_EMPTY\n");
				break;

			case BLOCK_STATE::NORMAL_BLOCK_VALID:
				fprintf(block_meta_output, "NORMAL_BLOCK_VALID\n");
				printf("NORMAL_BLOCK_VALID\n");
				break;

			case BLOCK_STATE::NORMAL_BLOCK_INVALID:
				fprintf(block_meta_output, "NORMAL_BLOCK_INVALID\n");
				printf("NORMAL_BLOCK_INVALID\n");
				break;

			case BLOCK_STATE::SPARE_BLOCK_EMPTY:
				fprintf(block_meta_output, "SPARE_BLOCK_EMPTY\n");
				printf("SPARE_BLOCK_EMPTY\n");
				break;

			case BLOCK_STATE::SPARE_BLOCK_VALID:
				fprintf(block_meta_output, "SPARE_BLOCK_VALID\n");
				printf("SPARE_BLOCK_VALID\n");
				break;

			case BLOCK_STATE::SPARE_BLOCK_INVALID:
				fprintf(block_meta_output, "SPARE_BLOCK_INVALID\n");
				printf("SPARE_BLOCK_INVALID\n");
				break;
			}

			printf("Sector State : ");
			switch (block_meta_buffer_array[offset_index]->sector_state)
			{
			case SECTOR_STATE::EMPTY:
				fprintf(block_meta_output, "EMPTY\n");
				printf("EMPTY\n");
				break;

			case SECTOR_STATE::VALID:
				fprintf(block_meta_output, "VALID\n");
				printf("VALID\n");
				break;

			case SECTOR_STATE::INVALID:
				fprintf(block_meta_output, "INVALID\n");
				printf("INVALID\n");
				break;
			}
		}

		/*** Deallocate block_meta_buffer_array ***/
		if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
			goto MEM_LEAK_ERR;
	}
	else
		fprintf(block_meta_output, "===== PBN1 : non-assigned =====");

	if (PBN2 != DYNAMIC_MAPPING_INIT_VALUE)
	{
		SPARE_reads(flashmem, PBN2, block_meta_buffer_array); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
		fprintf(block_meta_output, "\n===== PBN2 : %u =====", PBN2);

		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		{
			printf("\n< Offset : %d >\n", offset_index);
			fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);
			
			printf("Block State : ");
			switch (block_meta_buffer_array[offset_index]->block_state)
			{
			case BLOCK_STATE::NORMAL_BLOCK_EMPTY:
				fprintf(block_meta_output, "NORMAL_BLOCK_EMPTY\n");
				printf("NORMAL_BLOCK_EMPTY\n");
				break;

			case BLOCK_STATE::NORMAL_BLOCK_VALID:
				fprintf(block_meta_output, "NORMAL_BLOCK_VALID\n");
				printf("NORMAL_BLOCK_VALID\n");
				break;

			case BLOCK_STATE::NORMAL_BLOCK_INVALID:
				fprintf(block_meta_output, "NORMAL_BLOCK_INVALID\n");
				printf("NORMAL_BLOCK_INVALID\n");
				break;

			case BLOCK_STATE::SPARE_BLOCK_EMPTY:
				fprintf(block_meta_output, "SPARE_BLOCK_EMPTY\n");
				printf("SPARE_BLOCK_EMPTY\n");
				break;

			case BLOCK_STATE::SPARE_BLOCK_VALID:
				fprintf(block_meta_output, "SPARE_BLOCK_VALID\n");
				printf("SPARE_BLOCK_VALID\n");
				break;

			case BLOCK_STATE::SPARE_BLOCK_INVALID:
				fprintf(block_meta_output, "SPARE_BLOCK_INVALID\n");
				printf("SPARE_BLOCK_INVALID\n");
				break;
			}

			printf("Sector State : ");
			switch (block_meta_buffer_array[offset_index]->sector_state)
			{
			case SECTOR_STATE::EMPTY:
				fprintf(block_meta_output, "EMPTY\n");
				printf("EMPTY\n");
				break;

			case SECTOR_STATE::VALID:
				fprintf(block_meta_output, "VALID\n");
				printf("VALID\n");
				break;

			case SECTOR_STATE::INVALID:
				fprintf(block_meta_output, "INVALID\n");
				printf("INVALID\n");
				break;
			}
		}

		/*** Deallocate block_meta_buffer_array ***/
		if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
			goto MEM_LEAK_ERR;
	}
	else
		fprintf(block_meta_output, "\n===== PBN2 : non-assigned =====");

	goto END_SUCCESS;

END_SUCCESS:
	fclose(block_meta_output);
	printf(">> block_meta_output.txt\n");
	system("notepad block_meta_output.txt");
	return SUCCESS;

OUT_OF_RANGE: //범위 초과
	fclose(block_meta_output);
	printf("out of range\n");
	return FAIL;

NON_ASSIGNED_LBN:
	fclose(block_meta_output);
	printf("non-assigned LBN\n");
	return FAIL;

WRONG_ASSIGNED_LBN_ERR:
	fclose(block_meta_output);
	fprintf(stderr, "치명적 오류 : 잘못 할당 된 LBN (print_block_meta_info)\n");
	system("pause");
	exit(1);

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (print_block_meta_info)\n");
	system("pause");
	exit(1);
}

int deallocate_single_meta_buffer(META_DATA*& src_meta_buffer)
{
	if (src_meta_buffer != NULL)
	{
		delete src_meta_buffer;
		src_meta_buffer = NULL;

		return SUCCESS;
	}

	return FAIL; //할당 해제할 meta 정보가 없는 경우, 상위 계층에서 메모리 누수 검사
}

int deallocate_block_meta_buffer_array(META_DATA**& src_block_meta_buffer_array)
{
	if (src_block_meta_buffer_array != NULL)
	{
		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		{
			delete src_block_meta_buffer_array[offset_index];
		}
		delete[] src_block_meta_buffer_array;

		src_block_meta_buffer_array = NULL;

		return SUCCESS;
	}

	return FAIL; //할당 해제할 meta 정보가 없는 경우, 상위 계층에서 메모리 누수 검사
}