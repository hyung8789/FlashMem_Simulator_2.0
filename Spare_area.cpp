#include "Spare_area.h"

// Spare Area에 대한 비트 단위 처리, Meta-data 판독을 위한 함수 SPARE_init, SPARE_read, SPARE_write 정의
// 물리적 가용 가능 공간 관리와 Garbage Collection을 위한 SPARE_reads, update_v_flash_info_for_reorganization, update_v_flash_info_for_erase, calc_block_invalid_ratio 정의
// Meta-data를 통한 빈 일반 물리 블록 탐색 및 특정 물리 블록 내의 빈 물리 오프셋 탐색 위한 search_empty_block, search_empty_offset_in_block 정의

META_DATA::META_DATA()
{
	this->is_full = false; //초기 비어 있음
	this->write_index = SPARE_AREA_BIT - 1; //다음에 기록 할 위치 (127 ~ 0)
	this->read_index = SPARE_AREA_BIT - 1; //다음에 읽을 위치 (127 ~ 0)
	this->meta_data_array = new bool[SPARE_AREA_BIT]; //(2^0 ~ 2^127 까지 자릿수에 해당하는 meta정보를 저장하는 배열 생성)
}

META_DATA::~META_DATA()
{
	if (this->meta_data_array != NULL)
	{
		delete [] this->meta_data_array;
		this->meta_data_array = NULL;
	}
}

int META_DATA::seq_write(unsigned flag_bit) //읽어들인 flag_bit로부터 뒤에서부터 순차적으로(sequential) 기록
{
	//뒤에서부터 기록(2^127 자릿수에 해당하는 인덱스부터)

	if (this->is_full == true) //더 이상 기록 불가
	{
		return FAIL;
	}

	switch (flag_bit)
	{
	case TRUE_bit: //0x1
		this->meta_data_array[write_index] = true;
		break;

	case FALSE_bit: //0x0
		this->meta_data_array[write_index] = false;
		break;

	default:
		fprintf(stderr, "flag_bit 판별 실패\n");
		system("pause");
		exit(1);
		break;
	}
	
	this->write_index--;
	/***
		현재 사용중인 meta정보의 비트 위치(META_DATA_BIT_POS::empty_sector)까지만 기록하도록 하였으므로
		추가적인 meta정보 기록을 하고자 한다면 META_DATA_BIT_POS에 (비트 용도) = (비트 자리 위치(127~0))를 추가,
		record 기록을 하고자 하는 scope에 대해 다시 지정해야함
	***/
	if (this->write_index < 0 || this->write_index < (__int8)META_DATA_BIT_POS::empty_sector)
	{
		this->is_full = true; //더 이상 기록 불가
		return COMPLETE; //write complete
	}
	
	return SUCCESS;
}

int META_DATA::seq_read(bool& dst_result) //뒤에서부터 순차적으로(sequential) 읽어서 값 전달
{
	//뒤에서부터 읽음(2^127 자릿수에 해당하는 인덱스부터)
	//매 호출시마다 순차적으로 인덱스를 감소시키면서 데이터 반환

	if (this->is_full != true) //가득 차지 않았을 경우 불완전하므로 읽어서는 안됨
	{
		return FAIL;
	}

	dst_result = this->meta_data_array[read_index];
	read_index--;
	
	/***
		현재 사용중인 meta정보의 비트 위치(META_DATA_BIT_POS::empty_sector)까지만 읽도록 하였으므로
		추가적으로 기록한 meta정보를 읽고자 하고자 한다면 META_DATA_BIT_POS에 (비트 용도) = (비트 자리 위치(127~0))를 추가,
		record 읽기를 하고자 하는 scope에 대해 다시 지정해야함
	***/
	if (this->read_index < 0 || this->read_index < (__int8)META_DATA_BIT_POS::empty_sector)
	{
		this->read_index = SPARE_AREA_BIT - 1; 
		return COMPLETE; //read complete
	}

	return SUCCESS;
}

int SPARE_init(class FlashMem** flashmem, FILE** storage_spare_pos) //물리 섹터(페이지)의 Spare Area에 대한 초기화
{
	unsigned char* write_buffer = NULL; //Spare Area에 기록하기 위한 버퍼

	if ((*storage_spare_pos) != NULL)
	{
		write_buffer = new unsigned char[SPARE_AREA_BYTE];

		for (int byte_unit = 0; byte_unit < SPARE_AREA_BYTE; byte_unit++) //1바이트마다 초기화
		{
			write_buffer[byte_unit] = SPARE_INIT_VALUE; //0xff(16) = 11111111(2) = 255(10) 로 초기화
		}

		fwrite(write_buffer, sizeof(unsigned char), SPARE_AREA_BYTE, (*storage_spare_pos));

		delete[] write_buffer;
	}
	else
		return FAIL;

	return SUCCESS;
}

META_DATA* SPARE_read(class FlashMem** flashmem, FILE** storage_spare_pos) //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 클래스 형태로 반환
{
	META_DATA* meta_data = NULL; //Spare Area로부터 읽어들일 버퍼로부터 할당
	unsigned char* read_buffer = NULL; //Spare Area로부터 읽어들일 버퍼

	if ((*storage_spare_pos) != NULL)
	{
		read_buffer = new unsigned char[SPARE_AREA_BYTE];
		fread(read_buffer, sizeof(unsigned char), SPARE_AREA_BYTE, (*storage_spare_pos)); //바이트 단위로 읽어서 read_buffer에 할당

		meta_data = new META_DATA(); //읽어들인 버퍼로부터 할당하기 위한 META_DATA 생성

		__int8 META_DATA_BIT_POS = (__int8)META_DATA_BIT_POS::not_spare_block; //meta-data의 비트 위치 인덱싱

		for (int byte_unit = 0; byte_unit < SPARE_AREA_BYTE; byte_unit++) //read_buffer로부터 1byte씩 분리하여 비트단위로 판별
		{
			unsigned char bits_8; //1byte = 8bit
			bits_8 = read_buffer[byte_unit]; //1byte 크기만큼 read_buffer로부터 bits_8에 할당

			for (int bit_digits = 7; bit_digits >= 0; bit_digits--) //8비트(2^7 ~ 2^0)에 대해서 2^7 자리부터 한 자리씩 판별하여 META_DATA에 순차적으로 삽입
			{
				//0x1(16) = 1(10) = 1(2) 를 구하고자 하는 비트 자리 위치만큼 왼쪽으로 쉬프트 시키고, 값을 얻어내고자 하는 비트열과 & 연산을 통해 해당 자리를 검사
				meta_data->seq_write(((bits_8) & (0x1 << (bit_digits))) ? TRUE_bit : FALSE_bit);
			}

			/***
				추가적인 meta정보를 사용 하고자 한다면 META_DATA_BIT_POS에(비트 용도) = (비트 자리 위치(127~0))를 추가,
				읽고자 하는 Spare 영역의 비트 자리에 대해 다시 지정해야함
			***/
			META_DATA_BIT_POS -= 8; //처리된 1byte = 8bit만큼 감소
			if (META_DATA_BIT_POS < (__int8)META_DATA_BIT_POS::empty_sector) //Spare area에 대해 사용하지 않는 공간(122 ~ 0)일 경우 더 이상 처리 할 필요 없음
				break;
		}
	}
	else 
		return NULL;
	
	/*** trace위한 정보 기록 ***/
	(*flashmem)->v_flash_info.flash_read_count++; //플래시 메모리 읽기 카운트 증가

#if BLOCK_TRACE_MODE == 1 //Trace for Per Block Wear-leveling
	/***
		현재 읽기가 발생한 PBN 계산, Spare Area에 대한 처리가 끝난 후
		ftell(현재 파일 포인터, 바이트 단위) - SECTOR_INC_SPARE_BYTE == Flash_read상의 write_pos
		PSN = write_pos / SECTOR_INC_SPARE_BYTE
		PBN = PSN / BLOCK_PER_SECTOR
	***/
	(*flashmem)->block_trace_info[((ftell(*storage_spare_pos) - SECTOR_INC_SPARE_BYTE) / SECTOR_INC_SPARE_BYTE) / BLOCK_PER_SECTOR].block_read_count++; //해당 블록의 읽기 카운트 증가
#endif

	return meta_data;
}

META_DATA* SPARE_read(class FlashMem** flashmem, unsigned int PSN) //물리 섹터(페이지)의 Spare Area로부터 읽을 수 있는 META_DATA 클래스 형태로 반환
{
	FILE* storage_spare_pos = NULL;

	META_DATA* meta_data = NULL; //Spare Area로부터 읽어들일 버퍼로부터 할당

	unsigned int read_pos = 0; //읽고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점

	if ((storage_spare_pos = fopen("storage.bin", "rb")) == NULL) //읽기 + 이진파일 모드
	{
		fprintf(stderr, "storage.bin 파일을 읽기모드로 열 수 없습니다. (SPARE_read)");
		system("pause");
		exit(1);
	}

	read_pos = SECTOR_INC_SPARE_BYTE * PSN; //쓰고자 하는 위치
	spare_pos = read_pos + SECTOR_PER_BYTE; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)

	fseek(storage_spare_pos, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동
	
	/*** 읽기가 실패할 경우 단순 return되므로, 플래시 메모리 읽기 카운트는 변동이 없음 ***/
	meta_data = SPARE_read(flashmem, &storage_spare_pos);
	fclose(storage_spare_pos);

	return meta_data;
}

int SPARE_write(class FlashMem** flashmem, FILE** storage_spare_pos, META_DATA** src_data) //META_DATA에 대한 클래스 전달받아, 물리 섹터의 Spare Area에 기록
{
	unsigned char* write_buffer = NULL; //Spare Area에 기록하기 위한 버퍼

	if ((*storage_spare_pos) != NULL && (*src_data) != NULL)
	{
		/*** for Remaining Space Management ***/
		//해당 섹터가 무효화 되었을 경우 무효 카운트를 증가시킨다
		if ((*src_data)->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == false)
			(*flashmem)->v_flash_info.invalid_sector_count++; //무효 섹터 수 증가

		write_buffer = new unsigned char[SPARE_AREA_BYTE];

		unsigned char bits_8 = SPARE_INIT_VALUE; //1byte = 8bit, 0xff로 초기화된 비트열
		int write_buffer_idx_count = 0; //아래 연산에 의해 1바이트 단위로 기록된 write_buffer의 인덱스 카운터(다음에 기록될 인덱스 지정)
		int bit_digits = 7; //1byte의 2^7 자릿수 표현

		for (int bit_unit = 0; bit_unit < SPARE_AREA_BIT; bit_unit++) //Spare area의 128bit(16byte)에 대해 반복
		{
			bool result;
			if ((*src_data)->seq_read(result) != FAIL) //read SUCCESS or read COMPLETE
			{
				///0x1(16) = 1(10) = 1(2) 를 loc(설정하고자 하는 비트 자리 위치)만큼 왼쪽으로 쉬프트 시키고, data(값을 설정하고자 하는 비트열)과 |(or)연산을 통해 해당 위치를 1로 설정
				//(이미 해당 자리가 1인 경우 변동없음, or 연산 : 두 비트 모두 0일 경우에만 0)
				switch (result)
				{
				case true:
					//bits_8 |= 0x1 << bit_digits;
					bit_digits--; //이미 0x1이므로 자릿수만 감소
					break;

				case false:
					//해당 위치에 ^(exclusive not) 연산
					bits_8 ^= 0x1 << bit_digits;
					bit_digits--;
					break;
				}
			}

			if ((bit_unit + 1) % 8 == 0) //8bit(1byte)만큼 수행되었으면,
			{
				//1바이트만큼의 연산이 끝난 경우 write_buffer에 기록 후 bits_8 초기화
				write_buffer[write_buffer_idx_count] = bits_8; //해당 비트열을 쓰기 버퍼에 할당
				bits_8 = SPARE_INIT_VALUE; //0xff로 초기화
				bit_digits = 7; //다음 초기화된 비트열 bits_8의 비트 설정을 위해 자릿수 2^7로 초기화
				write_buffer_idx_count++; //write_buffer의 인덱스 카운터 증가(다음에 기록될 인덱스에 대해 카운트)
			}
		}

		fwrite(write_buffer, sizeof(unsigned char), SPARE_AREA_BYTE, (*storage_spare_pos));

		delete[] write_buffer;
	}
	else
		return FAIL;

	/*** trace위한 정보 기록 ***/
	(*flashmem)->v_flash_info.flash_write_count++; //플래시 메모리 쓰기 카운트 증가

#if BLOCK_TRACE_MODE == 1 //Trace for Per Block Wear-leveling
	(*flashmem)->block_trace_info[((ftell(*storage_spare_pos) - SECTOR_INC_SPARE_BYTE) / SECTOR_INC_SPARE_BYTE) / BLOCK_PER_SECTOR].block_write_count++; //해당 블록의 쓰기 카운트 증가
#endif

	return SUCCESS;
}

int SPARE_write(class FlashMem** flashmem, unsigned int PSN, META_DATA** src_data) //META_DATA에 대한 클래스 전달받아, 물리 섹터의 Spare Area에 기록
{
	FILE* storage_spare_pos = NULL;

	unsigned int write_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 위치
	unsigned int spare_pos = 0; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점

	if ((storage_spare_pos = fopen("storage.bin", "rb+")) == NULL) //읽고 쓰기 모드 + 이진파일 모드
	{
		fprintf(stderr, "storage.bin 파일을 읽고 쓰기 모드로 열 수 없습니다. (SPARE_write)");
		return FAIL;
	}

	write_pos = SECTOR_INC_SPARE_BYTE * PSN; //PSN 위치
	spare_pos = write_pos + SECTOR_PER_BYTE; //쓰고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점(데이터 영역을 건너뜀)

	fseek(storage_spare_pos, spare_pos, SEEK_SET); //읽고자 하는 물리 섹터(페이지)의 Spare Area 시작 지점으로 이동

	if (SPARE_write(flashmem, &storage_spare_pos, src_data) != SUCCESS)
	{
		/*** 쓰기가 실패할 경우 플래시 메모리 쓰기 카운트 변동없이 FAIL return ***/
		fclose(storage_spare_pos);
		return FAIL;
	}

	fclose(storage_spare_pos);
	return SUCCESS;
}

META_DATA** SPARE_reads(class FlashMem** flashmem, unsigned int PBN) //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태로 반환
{
	META_DATA** block_meta_data_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태
	block_meta_data_array = new META_DATA*[BLOCK_PER_SECTOR]; //블록 당 섹터(페이지)수의 META_DATA 주소를 담을 수 있는 공간 생성(row)

	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		unsigned int PSN = (PBN * BLOCK_PER_SECTOR) + offset_index;

		block_meta_data_array[offset_index] = new META_DATA; //각 공간에 대해 META_DATA형태를 담을 수 있는 공간 생성(col)
		block_meta_data_array[offset_index] = SPARE_read(flashmem, PSN); //한 물리 블록 내의 각 물리 오프셋 위치(페이지)에 대해 순차적으로 저장
	}
	/***
		< META_DATA 클래스 배열에 대한 메모리 해제 >
		플래시 메모리의 가변적 정보와 Victim Block 선정 위한 정보 갱신 위해 상위 계층의 함수에서 해제 수행
		---
		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
			delete block_meta_data_array[offset_index];
		delete[] block_meta_data_array;
	***/

	return block_meta_data_array;
}

int SPARE_writes(class FlashMem** flashmem, unsigned int PBN, META_DATA** block_meta_data_array) //한 물리 블록 내의 모든 섹터(페이지)에 대해 meta정보 기록
{
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		unsigned int PSN = (PBN * BLOCK_PER_SECTOR) + offset_index;

		if (SPARE_write(flashmem, PSN, &block_meta_data_array[offset_index]) != SUCCESS)
			return FAIL;
	}

	return SUCCESS;
}

int update_victim_block_info(class FlashMem** flashmem, bool is_logical, unsigned int src_Block_num, int mapping_method) //Victim Block 선정을 위한 블록 정보 구조체 갱신
{
	unsigned int LBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN1 = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN2 = DYNAMIC_MAPPING_INIT_VALUE;
	float LBN_invalid_ratio = -1;
	float PBN_invalid_ratio = -1; //PBN1 or PBN2 (단일 블록에 대한 무효율 계산)
	float PBN1_invalid_ratio = -1;
	float PBN2_invalid_ratio = -1;

	META_DATA** block_meta_data_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태

	/***
		Victim Block 정보 구조체 초기값
		---
		victim_block_num = DYNAMIC_MAPPING_INIT_VALUE;
		victim_block_invalid_ratio = -1;
	***/
	//초기값이 아니면, Victim Block 선정을 위해 건너뜀
	if ((*flashmem)->victim_block_info.victim_block_num != DYNAMIC_MAPPING_INIT_VALUE && (*flashmem)->victim_block_info.victim_block_invalid_ratio != -1)
		return FAIL;
	
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
			1) PBN1(Data Block)에 대한 Overwrite가 발생하여 PBN2(Log Block)에 기록하여야 하는데, PBN2(Log Block)에 더 이상 기록할 공간이 없을 경우
			: 쓰기 작업 중에 FTL함수에 의하여 Merge 수행

			2) 물리적 가용 공간 확보
			: GC에 의해 현재 매핑 테이블을 통해 전체 블록들 중 Merge 가능한 LBN에 대해서 Merge 수행
		
		- Hybrid Mapping (Log algorithm)에서 GC의 역할은 물리적 가용 공간(Physical Remaining Space)에 따른 Block Invalid Ratio Threshold에 따라
			1) 완전 무효화된 PBN일 경우 Erase 수행
			2) 일부 유효 데이터를 포함하고 있는 LBN에 대응된 PBN1과 PBN2에 대하여 Merge 수행
			=> 쓰기 작업이 발생한 LBN의 PBN1과 PBN2에 대해 통합된 무효율 값 계산
	***/

	switch (mapping_method)
	{
	case 2: //블록 매핑
		if (is_logical == true) //src_Block_num이 LBN일 경우
			return FAIL;
		else //src_Block_num이 PBN일 경우
		{
			(*flashmem)->victim_block_info.is_logical = false;
			(*flashmem)->victim_block_info.victim_block_num = PBN = src_Block_num;
			(*flashmem)->victim_block_info.victim_block_invalid_ratio = 1.0;
			
			goto BLOCK_MAPPING;
		}
			

	case 3: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
		if (is_logical == true) //src_Block_num이 LBN일 경우
		{
			LBN = src_Block_num;
			PBN1 = (*flashmem)->log_block_level_mapping_table[LBN][0];
			PBN2 = (*flashmem)->log_block_level_mapping_table[LBN][1];

			goto HYBRID_LOG_LBN;
		}
		else //src_Block_num이 PBN일 경우
		{
			PBN = src_Block_num;
			(*flashmem)->victim_block_info.is_logical = false;
			
			goto HYBRID_LOG_PBN;
		}

	default:
		return FAIL;
	}

BLOCK_MAPPING:
	goto END_SUCCESS;

HYBRID_LOG_PBN: //PBN1 or PBN2 (단일 블록에 대한 무효율 계산)
	(*flashmem)->victim_block_info.victim_block_num = PBN;

	/*** Calculate PBN Invalid Ratio ***/
	block_meta_data_array = SPARE_reads(flashmem, PBN); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	if (calc_block_invalid_ratio(block_meta_data_array, PBN_invalid_ratio) != SUCCESS)
	{
		fprintf(stderr, "오류 : nullptr (block_meta_data_array)");
		system("pause");
		exit(1);
	}
	/*** Deallocate block_meta_data_array ***/
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		delete block_meta_data_array[offset_index];
	delete[] block_meta_data_array;
	block_meta_data_array = NULL;

	try
	{
		if (PBN_invalid_ratio >= 0 && PBN_invalid_ratio <= 1)
			(*flashmem)->victim_block_info.victim_block_invalid_ratio = PBN_invalid_ratio;
		else
			throw PBN_invalid_ratio;
	}
	catch (float PBN_invalid_ratio)
	{
		fprintf(stderr, "오류 : 잘못된 무효율(%f)", &PBN_invalid_ratio);
		system("pause");
		exit(1);
	}

	goto END_SUCCESS;

HYBRID_LOG_LBN:
	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //양쪽 다 대웅되어 있지 않으면,
		goto NON_ASSIGNED_TABLE;

	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE || PBN2 == DYNAMIC_MAPPING_INIT_VALUE) //하나라도 대응되어 있지 않으면, 단일 블록에 대한 연산 수행
	{
		(*flashmem)->victim_block_info.is_logical = false;
		
		//대응되어 있는 블록에 대해 단일 블록 처리 루틴에서 사용할 PBN으로 할당
		if (PBN1 != DYNAMIC_MAPPING_INIT_VALUE)
			PBN = PBN1;
		else
			PBN = PBN2;

		//단일 블록 처리 루틴으로 이동
		goto HYBRID_LOG_PBN;
	}

	/*** Calculate PBN1 Invalid Ratio ***/
	block_meta_data_array = SPARE_reads(flashmem, PBN1); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	if (calc_block_invalid_ratio(block_meta_data_array, PBN1_invalid_ratio) != SUCCESS)
	{
		fprintf(stderr, "오류 : nullptr (block_meta_data_array)");
		system("pause");
		exit(1);
	}
	/*** Deallocate block_meta_data_array ***/
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		delete block_meta_data_array[offset_index];
	delete[] block_meta_data_array;

	/*** Calculate PBN2 Invalid Ratio ***/
	block_meta_data_array = SPARE_reads(flashmem, PBN2); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	if (calc_block_invalid_ratio(block_meta_data_array, PBN2_invalid_ratio) != SUCCESS)
	{
		fprintf(stderr, "오류 : nullptr (block_meta_data_array)");
		system("pause");
		exit(1);
	}
	/*** Deallocate block_meta_data_array ***/
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		delete block_meta_data_array[offset_index];
	delete[] block_meta_data_array;
	block_meta_data_array = NULL;

	(*flashmem)->victim_block_info.is_logical = true;
	(*flashmem)->victim_block_info.victim_block_num = LBN;
	
	try
	{
		LBN_invalid_ratio = (float)((PBN1_invalid_ratio + PBN2_invalid_ratio) / 2); //LBN의 무효율 계산
		if (LBN_invalid_ratio >= 0 && LBN_invalid_ratio <= 1)
			(*flashmem)->victim_block_info.victim_block_invalid_ratio = LBN_invalid_ratio;
		else
			throw LBN_invalid_ratio;
	}
	catch (float LBN_invalid_ratio)
	{
		fprintf(stderr, "오류 : 잘못된 무효율(%f)", &LBN_invalid_ratio);
		system("pause");
		exit(1);
	}
	
	goto END_SUCCESS;
	
END_SUCCESS:
	return SUCCESS;

NON_ASSIGNED_TABLE:
	return COMPLETE;

}

int update_v_flash_info_for_erase(class FlashMem** flashmem, META_DATA** src_data) //Erase하고자 하는 특정 물리 블록 하나에 대해 META_DATA 클래스 배열을 통한 판별을 수행하여 플래시 메모리의 가변적 정보 갱신
{
	//for Remaining Space Management

	if (src_data != NULL)
	{
		for (__int8 Poffset = 0; Poffset < BLOCK_PER_SECTOR; Poffset++) //블록 내의 각 페이지에 대해 인덱싱
		{
			if (src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
				src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true)
				//비어있지 않고, 유효한 페이지이면
			{
				(*flashmem)->v_flash_info.written_sector_count--; //기록된 페이지 수 감소
			}
			else if (src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
				src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] != true)
			{
				//비어있지 않고, 유효하지 않은 페이지이면
				(*flashmem)->v_flash_info.written_sector_count--; //기록된 페이지 수 감소
				(*flashmem)->v_flash_info.invalid_sector_count--; //무효 페이지 수 감소
			}
			else //비어있을 경우, 항상 유효한 페이지이다
			{
				//do nothing
			}
		}
	}
	else
		return FAIL;

	return SUCCESS;
}

int update_v_flash_info_for_reorganization(class FlashMem** flashmem, META_DATA** src_data) //특정 물리 블록 하나에 대한 META_DATA 클래스 배열을 통한 판별을 수행하여 물리적 가용 가능 공간 계산 위한 가변적 플래시 메모리 정보 갱신
{
	if (src_data != NULL)
	{
		for (__int8 Poffset = 0; Poffset < BLOCK_PER_SECTOR; Poffset++) //블록 내의 각 페이지에 대해 인덱싱
		{
			if (src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
				src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true)
				//비어있지 않고, 유효한 페이지이면
			{
				(*flashmem)->v_flash_info.written_sector_count++; //기록된 페이지 수 증가
			}
			else if (src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
				src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] != true)
			{
				//비어있지 않고, 유효하지 않은 페이지이면
				(*flashmem)->v_flash_info.written_sector_count++; //기록된 페이지 수 증가
				(*flashmem)->v_flash_info.invalid_sector_count++; //무효 페이지 수 증가
			}
			else //비어있을 경우, 항상 유효한 페이지이다
			{
				//do nothing
			}
		}
	}
	else
		return FAIL;

	return SUCCESS;
}

int calc_block_invalid_ratio(META_DATA** src_data, float& dst_block_invalid_ratio) //특정 물리 블록 하나에 대한 META_DATA 클래스 배열을 통한 판별을 수행하여 무효율 계산 및 전달
{
	//for Calculate Block Invalid Ratio
	__int8 block_per_written_sector_count = 0;
	__int8 block_per_invalid_sector_count = 0;
	__int8 block_per_empty_sector_count = 0;

	if (src_data != NULL)
	{
		for (__int8 Poffset = 0; Poffset < BLOCK_PER_SECTOR; Poffset++) //블록 내의 각 페이지에 대해 인덱싱
		{
			if (src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
				src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true)
				//비어있지 않고, 유효한 페이지이면
			{
				block_per_written_sector_count++; //기록된 페이지 수 증가
			}
			else if (src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] != true &&
				src_data[Poffset]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] != true)
			{
				//비어있지 않고, 유효하지 않은 페이지이면
				block_per_written_sector_count++; //기록된 페이지 수 증가
				block_per_invalid_sector_count++; //무효 페이지 수 증가
			}
			else //비어있을 경우, 항상 유효한 페이지이다
			{
				block_per_empty_sector_count++; //빈 페이지 수 증가
			}
		}
	}
	else
		return FAIL;

	try
	{
		float block_invalid_ratio = (float)block_per_invalid_sector_count / (float)BLOCK_PER_SECTOR; //현재 블록의 무효율 계산
		if (block_invalid_ratio >= 0 && block_invalid_ratio <= 1)
			dst_block_invalid_ratio = block_invalid_ratio;
		else 
			throw block_invalid_ratio;
	}
	catch (float block_invalid_ratio)
	{
		fprintf(stderr, "오류 : 잘못된 무효율(%f)", &block_invalid_ratio);
		system("pause");
		exit(1);
	}

	return SUCCESS;
}

int search_empty_normal_block(class FlashMem** flashmem, unsigned int& dst_Block_num, META_DATA** dst_data, int mapping_method, int table_type) //빈 일반 물리 블록(PBN)을 순차적으로 탐색하여 PBN또는 테이블 상 LBN 값, 해당 PBN의 meta정보 전달
{
	unsigned int PSN = DYNAMIC_MAPPING_INIT_VALUE; //실제로 저장된 물리 섹터 번호

	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼

	f_flash_info = (*flashmem)->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	switch (mapping_method) //매핑 방식에 따라 해당 처리 위치로 이동
	{
	case 2: //블록 매핑
		if (table_type == 0) //블록 매핑 Static Table
			goto BLOCK_MAPPING_STATIC_PROC;

		else if (table_type == 1) //블록 매핑 Dynamic Table
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
		PSN = ((*flashmem)->block_level_mapping_table[table_index] * BLOCK_PER_SECTOR); //해당 블록의 첫 번째 페이지
		meta_buffer = SPARE_read(flashmem, PSN); //Spare 영역을 읽음

		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] == true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] == true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] == true)
			//Spare 블록이 아니며, 비어있고, 유효한 블록이면
		{
			//LBN 및 meta 정보 전달
			dst_Block_num = table_index;
			(*dst_data) = meta_buffer; //dst_data 참조

			return SUCCESS;
		}
		delete meta_buffer;
		meta_buffer = NULL;
	}
	//만약 빈 블록을 찾지 못했으면
	return COMPLETE;

DYNAMIC_COMMON_PROC: //Dynamic Table 공용 처리 루틴 : 각 물리 블록의 Spare 영역 판별, 빈 블록 순차탐색 후 PBN전달
	for (unsigned int block_index = 0; block_index < f_flash_info.block_size; block_index++) //순차적으로 비어있는 블록을 찾는다 (Spare 블록의 위치는 쓰기가 일어남에 따라 가변적이므로 모든 블록에 대해 스캐닝)
	{
		PSN = (block_index * BLOCK_PER_SECTOR); //해당 블록의 첫 번째 페이지
		meta_buffer = SPARE_read(flashmem, PSN); //Spare 영역을 읽음

		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] == true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] == true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] == true)
			//Spare 블록이 아니며, 비어있고, 유효한 블록이면
		{
			//PBN 및 meta 정보 전달
			dst_Block_num = block_index;
			(*dst_data) = meta_buffer;

			return SUCCESS;
		}
		delete meta_buffer;
		meta_buffer = NULL;
	}
	
	//만약 빈 블록을 찾지 못했으면
	return COMPLETE;

WRONG_TABLE_TYPE_ERR: //잘못된 테이블 타입
	fprintf(stderr, "오류 : 잘못된 테이블 타입\n");
	system("pause");
	exit(1);

WRONG_FUNC_CALL_ERR:
	fprintf(stderr, "오류 : 잘못된 함수 호출\n");
	system("pause");
	exit(1);
}

int search_empty_offset_in_block(class FlashMem** flashmem, unsigned int src_PBN, __int8& dst_Poffset, META_DATA** dst_data) //일반 물리 블록(PBN) 내부를 순차적으로 탐색하여 Poffset 값, 해당 위치의 meta정보 전달
{
	unsigned int PSN = DYNAMIC_MAPPING_INIT_VALUE; //실제로 저장된 물리 섹터 번호

	META_DATA* meta_buffer = NULL; //Spare area에 기록된 meta-data에 대해 읽어들일 버퍼

	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++) //순차적으로 비어있는 페이지를 찾는다
	{
		PSN = (src_PBN * BLOCK_PER_SECTOR) + offset_index;
		meta_buffer = SPARE_read(flashmem, PSN); //Spare 영역을 읽음

		if (
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] == true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] == true)
			//비어있고, 유효한 페이지이면
		{
			//Poffset 및 meta 정보 전달
			dst_Poffset = offset_index;
			(*dst_data) = meta_buffer;

			return SUCCESS;
		}
		delete meta_buffer;
		meta_buffer = NULL;
	}

	//만약 빈 페이지를 찾지 못했으면
	return FAIL;
}

void print_block_meta_info(class FlashMem** flashmem, bool is_logical, unsigned int src_Block_num, int mapping_method) //블록 내의 모든 섹터(페이지)의 meta 정보 출력
{
	FILE* block_meta_output = NULL;

	META_DATA** block_meta_data_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태
	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보
	unsigned int LBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN1 = DYNAMIC_MAPPING_INIT_VALUE;
	unsigned int PBN2 = DYNAMIC_MAPPING_INIT_VALUE;

	if (*flashmem == NULL) //플래시 메모리가 할당되지 않았을 경우
	{
		fprintf(stderr, "not initialized\n");
		return;
	}
	f_flash_info = (*flashmem)->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

	if ((block_meta_output = fopen("block_meta_output.txt", "wt")) == NULL)
	{
		fprintf(stderr, "block_meta_output.txt 파일을 쓰기모드로 열 수 없습니다. (print_block_meta_info)");
		return;
	}
	
	switch (mapping_method)
	{
	case 2: //블록 매핑
		if (is_logical == true) //src_Block_num이 LBN일 경우
			goto BLOCK_LBN;
		else //src_Block_num이 PBN일 경우
			goto COMMON_PBN;

	case 3: //하이브리드 매핑(Log algorithm - 1:2 Block level mapping with Dynamic Table)
		if (is_logical == true) //src_Block_num이 LBN일 경우
			goto HYBRID_LOG_LBN;
		else //src_Block_num이 PBN일 경우
			goto COMMON_PBN;

	default:
		goto COMMON_PBN;
	}

COMMON_PBN: //PBN에 대한 공용 처리 루틴
	PBN = src_Block_num;

	if (PBN == DYNAMIC_MAPPING_INIT_VALUE || PBN > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1))
		goto OUT_OF_RANGE;

	block_meta_data_array = SPARE_reads(flashmem, PBN); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	fprintf(block_meta_output, "===== PBN : %u =====", PBN);
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);
		fprintf(block_meta_output, "not_spare_block : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] ? "true" : "false");
		fprintf(block_meta_output, "\nvalid_block : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] ? "true" : "false");
		fprintf(block_meta_output, "\nempty_block : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] ? "true" : "false");
		fprintf(block_meta_output, "\nvalid_sector : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] ? "true" : "false");
		fprintf(block_meta_output, "\nempty_sector : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] ? "true" : "false");
	}

	/*** Deallocate block_meta_data_array ***/
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		delete block_meta_data_array[offset_index];
	delete[] block_meta_data_array;

	goto END_SUCCESS;

BLOCK_LBN: //블록 매핑 LBN 처리 루틴
	LBN = src_Block_num;
	PBN = (*flashmem)->block_level_mapping_table[LBN];
	
	//LBN 범위를 Spare Block 개수만큼 제외한 범위로 제한
	if (LBN == DYNAMIC_MAPPING_INIT_VALUE || LBN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - (f_flash_info.spare_block_size) - 1))
		goto OUT_OF_RANGE;

	if (PBN == DYNAMIC_MAPPING_INIT_VALUE)
		goto NON_ASSIGNED_TABLE;

	if (PBN > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1)) //PBN이 잘못된 값으로 대응되어 있을 경우
		goto WRONG_ASSIGNED_TABLE;

	fprintf(block_meta_output, "===== LBN : %u =====\n", LBN);
	block_meta_data_array = SPARE_reads(flashmem, PBN); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
	fprintf(block_meta_output, "===== PBN : %u =====", PBN);

	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
	{
		fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);
		fprintf(block_meta_output, "not_spare_block : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] ? "true" : "false");
		fprintf(block_meta_output, "\nvalid_block : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] ? "true" : "false");
		fprintf(block_meta_output, "\nempty_block : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] ? "true" : "false");
		fprintf(block_meta_output, "\nvalid_sector : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] ? "true" : "false");
		fprintf(block_meta_output, "\nempty_sector : ");
		fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] ? "true" : "false");
	}

	/*** Deallocate block_meta_data_array ***/
	for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		delete block_meta_data_array[offset_index];
	delete[] block_meta_data_array;

	goto END_SUCCESS;

HYBRID_LOG_LBN: //하이브리드 매핑 LBN 처리 루틴
	LBN = src_Block_num;
	PBN1 = (*flashmem)->log_block_level_mapping_table[LBN][0];
	PBN2 = (*flashmem)->log_block_level_mapping_table[LBN][1];

	//LBN 범위를 Spare Block 개수만큼 제외한 범위로 제한
	if (LBN == DYNAMIC_MAPPING_INIT_VALUE || LBN > (unsigned int)((MB_PER_SECTOR * f_flash_info.flashmem_size) - (f_flash_info.spare_block_size) - 1))
		goto OUT_OF_RANGE;

	//모두 대응되어 있지 않으면
	if (PBN1 == DYNAMIC_MAPPING_INIT_VALUE && PBN2 == DYNAMIC_MAPPING_INIT_VALUE)
		goto NON_ASSIGNED_TABLE;
	
	if (PBN1 != DYNAMIC_MAPPING_INIT_VALUE && PBN1 > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1) ||
		PBN2 != DYNAMIC_MAPPING_INIT_VALUE && PBN2 > (unsigned int)((MB_PER_BLOCK * f_flash_info.flashmem_size) - 1)) //PBN1 또는 PBN2가 잘못된 값으로 대응되어 있을 경우
		goto WRONG_ASSIGNED_TABLE;

	fprintf(block_meta_output, "===== LBN : %u =====\n", LBN);
	if (PBN1 != DYNAMIC_MAPPING_INIT_VALUE)
	{
		block_meta_data_array = SPARE_reads(flashmem, PBN1); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
		fprintf(block_meta_output, "===== PBN1 : %u =====", PBN1);

		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		{
			fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);
			fprintf(block_meta_output, "not_spare_block : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] ? "true" : "false");
			fprintf(block_meta_output, "\nvalid_block : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] ? "true" : "false");
			fprintf(block_meta_output, "\nempty_block : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] ? "true" : "false");
			fprintf(block_meta_output, "\nvalid_sector : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] ? "true" : "false");
			fprintf(block_meta_output, "\nempty_sector : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] ? "true" : "false");
		}

		/*** Deallocate block_meta_data_array ***/
		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
			delete block_meta_data_array[offset_index];
		delete[] block_meta_data_array;
		block_meta_data_array = NULL;
	}
	else
		fprintf(block_meta_output, "===== PBN1 : non-assigned =====");

	if (PBN2 != DYNAMIC_MAPPING_INIT_VALUE)
	{
		block_meta_data_array = SPARE_reads(flashmem, PBN2); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴
		fprintf(block_meta_output, "\n===== PBN2 : %u =====", PBN2);

		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
		{
			fprintf(block_meta_output, "\n< Offset : %d >\n", offset_index);
			fprintf(block_meta_output, "not_spare_block : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::not_spare_block] ? "true" : "false");
			fprintf(block_meta_output, "\nvalid_block : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] ? "true" : "false");
			fprintf(block_meta_output, "\nempty_block : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] ? "true" : "false");
			fprintf(block_meta_output, "\nvalid_sector : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_sector] ? "true" : "false");
			fprintf(block_meta_output, "\nempty_sector : ");
			fprintf(block_meta_output, block_meta_data_array[offset_index]->meta_data_array[(__int8)META_DATA_BIT_POS::empty_sector] ? "true" : "false");
		}
		/*** Deallocate block_meta_data_array ***/
		for (__int8 offset_index = 0; offset_index < BLOCK_PER_SECTOR; offset_index++)
			delete block_meta_data_array[offset_index];
		delete[] block_meta_data_array;
	}
	else
		fprintf(block_meta_output, "\n===== PBN2 : non-assigned =====");

	goto END_SUCCESS;

END_SUCCESS:
	fclose(block_meta_output);
	printf(">> block_meta_output.txt\n");
	system("notepad block_meta_output.txt");
	return;

OUT_OF_RANGE: //범위 초과
	fclose(block_meta_output);
	printf("out of range\n");
	return;

NON_ASSIGNED_TABLE: //대응되지 않은 테이블
	fclose(block_meta_output);
	printf("non-assigned table\n");
	return;

WRONG_ASSIGNED_TABLE: //LBN에 잘못 대응된 PBN 오류
	fclose(block_meta_output);
	fprintf(stderr, "오류 : WRONG_ASSIGNED_TABLE\n");
	system("pause");
	exit(1);
}