#include "FlashMem.h"

//명령어 목록 출력, 플래시 메모리의 생성, 매핑 테이블 불러오기 및 스토리지 용량을 계산하는 내부 함수 정의

void VICTIM_BLOCK_INFO::clear_all() //Victim Block 선정을 위한 블록 정보 구조체 초기화
{
	this->is_logical = true;

	//나올 수 없는 값으로 초기화
	this->victim_block_num = DYNAMIC_MAPPING_INIT_VALUE;
	this->victim_block_invalid_ratio = -1;
}

void VARIABLE_FLASH_INFO::clear_all() //모두 초기화
{
	/*** Variable Information ***/
	this->written_sector_count = 0;
	this->invalid_sector_count = 0;

	//for trace
	this->flash_write_count = 0;
	this->flash_erase_count = 0;
	this->flash_read_count = 0;
}

void VARIABLE_FLASH_INFO::clear_trace_info() //trace를 위한 정보 초기화
{
	//for trace
	this->flash_write_count = 0;
	this->flash_erase_count = 0;
	this->flash_read_count = 0;
}

void BLOCK_TRACE_INFO::clear_all() //블록 당 마모도 trace를 위한 정보 초기화
{
	this->block_write_count = 0;
	this->block_read_count = 0;
	this->block_erase_count = 0;
}

FlashMem::FlashMem()
{
	/*** Fixed information ***/
	this->f_flash_info.flashmem_size = 0; //플래시 메모리의 MB 크기
	this->f_flash_info.block_size = 0; //할당된 메모리 크기에 해당하는 전체 블록의 개수 (Spare Block 포함)
	this->f_flash_info.sector_size = 0; //할당된 메모리 크기에 해당하는 전체 섹터의 개수 (Spare Block 포함)
	//for ftl algorithm
	this->f_flash_info.data_only_storage_byte = 0; //할당된 메모리 크기에 해당하는 총 byte 크기 반환 (Spare Area 제외)
	this->f_flash_info.storage_byte = 0; //할당된 메모리 크기에 해당하는 총 byte 크기 (Spare Area 포함)
	this->f_flash_info.spare_area_byte = 0; //할당된 메모리 크기에 해당하는 전체 Spare Area의 byte 크기
	this->f_flash_info.spare_block_size = 0; //할당된 메모리 크기에 해당하는 시스템에서 관리하는 Spare Block 개수
	this->f_flash_info.spare_block_byte = 0; //할당된 메모리 크기에 해당하는 시스템에서 관리하는 Spare Block의 총 byte 크기

	/*** 테이블 초기화 : 할당 전이므로 포인터만 초기화 ***/
	this->block_level_mapping_table = NULL; //블록 매핑 테이블
	this->log_block_level_mapping_table = NULL; //1 : 2 블록 매핑 테이블 (Log Algorithm)
	this->offset_level_mapping_table = NULL; //오프셋 단위 매핑 테이블
	this->spare_block_table = NULL; //Spare 블록 테이블

	/*** Variable Information ***/
	this->v_flash_info.clear_all();

	/*** Victim Block 선정을 위한 블록 정보 구조체 ***/
	this->victim_block_info.clear_all();
	this->victim_block_queue = NULL;

	this->gc = NULL;

	this->block_trace_info = NULL;
}

FlashMem::FlashMem(unsigned short megabytes) //megabytes 크기의 플래시 메모리 생성
{
	/*** Fixed Information ***/
	this->f_flash_info.flashmem_size = megabytes; //플래시 메모리의 MB 크기
	this->f_flash_info.block_size = this->f_flash_info.flashmem_size * MB_PER_BLOCK; //할당된 메모리 크기에 해당하는 전체 블록의 개수 (Spare Block 포함)
	this->f_flash_info.sector_size = this->f_flash_info.block_size * BLOCK_PER_SECTOR; //할당된 메모리 크기에 해당하는 전체 섹터의 개수 (Spare Block 포함)
	//for ftl algorithm
	this->f_flash_info.data_only_storage_byte = this->f_flash_info.sector_size * SECTOR_PER_BYTE; //할당된 메모리 크기에 해당하는 총 byte 크기 반환 (Spare Area 제외)
	this->f_flash_info.storage_byte = this->f_flash_info.sector_size * SECTOR_INC_SPARE_BYTE; //할당된 메모리 크기에 해당하는 총 byte 크기 (Spare Area 포함)
	this->f_flash_info.spare_area_byte = this->f_flash_info.sector_size * SPARE_AREA_BYTE; //할당된 메모리 크기에 해당하는 전체 Spare Area의 byte 크기
	this->f_flash_info.spare_block_size = (unsigned int)round(this->f_flash_info.block_size * SPARE_BLOCK_RATIO); //할당된 메모리 크기에 해당하는 시스템에서 관리하는 Spare Block 개수
	this->f_flash_info.spare_block_byte = this->f_flash_info.spare_block_size * SECTOR_INC_SPARE_BYTE * BLOCK_PER_SECTOR; //할당된 메모리 크기에 해당하는 시스템에서 관리하는 Spare Block의 총 byte 크기

	/*** 테이블 초기화 : 할당 전이므로 포인터만 초기화 ***/
	this->block_level_mapping_table = NULL; //블록 매핑 테이블
	this->log_block_level_mapping_table = NULL; //1 : 2 블록 매핑 테이블 (Log Algorithm)
	this->offset_level_mapping_table = NULL; //오프셋 단위 매핑 테이블
	this->spare_block_table = NULL; //Spare 블록 테이블

	/*** Variable Information ***/
	this->v_flash_info.clear_all();

	/*** Victim Block 선정을 위한 블록 정보 구조체 ***/
	this->victim_block_info.clear_all();
	this->victim_block_queue = NULL;

	/*** 가비지 콜렉터 객체 생성 ***/
	this->gc = new GarbageCollector();

	/*** 블록 당 마모도 추적 ***/
	this->block_trace_info = NULL;

#if BLOCK_TRACE_MODE == 1
	this->block_trace_info = new BLOCK_TRACE_INFO[this->f_flash_info.block_size]; //전체 블록 수의 크기로 할당
	for (unsigned int PBN = 0; PBN < this->f_flash_info.block_size; PBN++)
		block_trace_info[PBN].clear_all(); //읽기, 쓰기, 지우기 횟수 초기화
#endif
}

FlashMem::~FlashMem()
{
	this->deallocate_table();
	delete this->gc;

#if BLOCK_TRACE_MODE == 1
	delete[] this->block_trace_info;
#endif
}

void FlashMem::bootloader(FlashMem** flashmem, int& mapping_method, int& table_type) //Reorganization process from initialized flash memory storage file
{
	/***
	- Reorganization process from initialized flash memory storage file :

	1) 기존에 생성한 플래시 메모리의 크기, 매핑 방식, 테이블 타입 로드하여 새로운 플래시 메모리 객체 생성
		1-1) 이에 따른 매핑 테이블 캐싱
	
	2) 기존의 스토리지 파일로부터 가변적 스토리지 정보를 갱신 (기록된 섹터 수, 무효화된 섹터 수)
		2-1) 가변적 스토리지 정보를 갱신 후 Victim Block 큐 재 구성을 위한 모든 블록 재 참조에 필요한 오버헤드가 상당히 크기 때문에
			 가변적 스토리지 정보를 갱신하면서 완전 무효화된 물리 블록(valid_block == false)만 Victim Block 큐에 삽입
		2-2) 갱신 완료된 가변적 스토리지 정보에 따라 GC에 의해 가변적 무효율 임계값 설정
	***/

	FILE* volume = NULL;  //생성한 플래시 메모리의 정보 (MB단위의 크기, 매핑 방식, 테이블 타입)를 저장하기 위한 파일 포인터

	META_DATA* meta_buffer = NULL; //단일 섹터(페이지)의 meta 정보
	META_DATA** block_meta_buffer_array = NULL; //한 물리 블록 내의 모든 섹터(페이지)에 대해 Spare Area로부터 읽을 수 있는 META_DATA 클래스 배열 형태
	F_FLASH_INFO f_flash_info; //플래시 메모리 생성 시 결정되는 고정된 정보

	if ((*flashmem) == NULL)
	{
		if ((volume = fopen("volume.txt", "rt")) == NULL) //읽기 + 텍스트파일 모드
			return;
		else
		{
			while (1)
			{
				system("cls");
				int input = 0;
				std::cout << "Already initialized continue?" << std::endl;
				std::cout << "0: Ignore, 1: Load" << std::endl;
				std::cout << ">>";
				std::cin >> input;
				if (input == 0)
				{
					std::cin.clear(); //오류스트림 초기화
					std::cin.ignore(INT_MAX, '\n'); //입력버퍼비우기
					return;
				}
				else if (input == 1) //기존에 저장된 플래시 메모리의 용량 및 테이블 타입 불러와서 재 할당
				{
					unsigned short megabytes;
					fscanf(volume, "%hd\n", &megabytes);
					(*flashmem) = new FlashMem(megabytes); //용량 설정하여 생성
					fscanf(volume, "%d\n", &mapping_method); //매핑 방식 설정
					fscanf(volume, "%d", &table_type); //테이블 타입 설정
					std::cin.clear(); //오류스트림 초기화
					std::cin.ignore(INT_MAX, '\n'); //입력버퍼비우기
					break;
				}
				else
				{

					std::cin.clear(); //오류스트림 초기화
					std::cin.ignore(INT_MAX, '\n'); //입력버퍼비우기
				}
			}
			
		}

		/*** 매핑 테이블 캐싱 ***/
		(*flashmem)->load_table(mapping_method);

		/*** 
			1) 기존의 스토리지 파일로부터 가변적 스토리지 정보를 갱신 (기록된 섹터 수, 무효화된 섹터 수)
			2) 가변적 스토리지 정보에 따라 Victim Block 선정 위한 가변적 무효율 임계값 설정
			----
			!! 가변적 스토리지 정보를 갱신 후 Victim Block 큐 재 구성을 위한 모든 블록 재 참조에 필요한 오버헤드가 상당히 크기 때문에
			가변적 스토리지 정보를 갱신하면서 완전 무효화된 물리 블록(valid_block == false)만 Victim Block 큐에 삽입한다.
		***/
		f_flash_info = (*flashmem)->get_f_flash_info();

		/*** 매핑 방식을 사용할 경우 GC를 위한 Victim Block 큐 생성 ***/
		switch (mapping_method)
		{
		case 0:
			break;

		default:
			(*flashmem)->victim_block_queue = new Victim_Queue(f_flash_info.block_size);
			break;
		}
		
		for (unsigned int PBN = 0; PBN < f_flash_info.block_size; PBN++)
		{
			printf("Reorganizing...(%.0f%%)\r", ((float)PBN / (float)(f_flash_info.block_size - 1)) * 100);
			block_meta_buffer_array = SPARE_reads(flashmem, PBN); //해당 블록의 모든 섹터(페이지)에 대해 meta정보를 읽어옴

			if (block_meta_buffer_array[0]->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] == false) //무효화된 블록이면
			{
				/***
					update_victim_block_info호출 시 meta정보를 중복하여 다시 읽으므로, 
					해당 소스의 일부분을 사용한다
				***/
				(*flashmem)->victim_block_info.is_logical = false; //PBN
				(*flashmem)->victim_block_info.victim_block_num = PBN;
				(*flashmem)->victim_block_info.victim_block_invalid_ratio = 1.0;
			}

			if (update_v_flash_info_for_reorganization(flashmem, block_meta_buffer_array) != SUCCESS)
				goto WRONG_META_ERR;

			/*** Deallocate block_meta_buffer_array ***/
			if (deallocate_block_meta_buffer_array(block_meta_buffer_array) != SUCCESS)
				goto MEM_LEAK_ERR;

			/*** 무효화된 블록 존재 시 Victim Block 큐 삽입, 이에 따라 큐가 가득 찰 시 처리 ***/
			(*flashmem)->gc->scheduler(flashmem, mapping_method);
		}

		(*flashmem)->gc->RDY_v_flash_info_for_set_invalid_ratio_threshold = true; //무효율 임계값 설정을 위한 가변적 스토리지 정보 갱신 완료 알림

		/*** 갱신 완료된 가변적 스토리지 정보에 따라 GC에 의해 가변적 무효율 임계값 설정 ***/
		(*flashmem)->gc->scheduler(flashmem, mapping_method);
		(*flashmem)->gc->print_invalid_ratio_threshold();

		printf("Reorganizing Process Success\n");
		system("pause");
	}

	if (volume != NULL)
		fclose(volume);
	return;

WRONG_META_ERR: //잘못된 meta정보 오류
	fprintf(stderr, "오류 : 잘못된 meta 정보 (bootloader)\n");
	system("pause");
	exit(1);

MEM_LEAK_ERR:
	fprintf(stderr, "오류 : meta 정보에 대한 메모리 누수 발생 (bootloader)\n");
	system("pause");
	exit(1);
}

void FlashMem::load_table(int mapping_method) //매핑방식에 따른 매핑 테이블 로드
{
	/***
		< 테이블 load 순서 >
	
		1) Block level(normal or log block) table
		2) Spare Block table
		3) Offset level table
	***/

	FILE* table = NULL;

	unsigned int* spare_block_table_buffer = NULL; //Spare_Block_Table의 seq_record()에 의한 테이블 값 순차 할당 위한 버퍼

	//저장된 기존 테이블로부터 할당 수행

	if ((table = fopen("table.bin", "rb")) == NULL) //읽기 + 이진파일 모드
	{
		fprintf(stderr, "table.bin 파일을 읽기모드로 열 수 없습니다. (load_table)");
		return;
	}

	switch (mapping_method) //매핑 방식에 따라 캐시할 공간할당 및 불러오기
	{
	case 1:
		return;

	case 2: //블록 매핑
		if (this->block_level_mapping_table == NULL && this->spare_block_table == NULL)
		{
			//블록 단위 매핑 테이블, Spare 블록 테이블
			this->block_level_mapping_table = new unsigned int[this->f_flash_info.block_size - this->f_flash_info.spare_block_size];
			fread(this->block_level_mapping_table, sizeof(unsigned int), this->f_flash_info.block_size - this->f_flash_info.spare_block_size, table);

			spare_block_table_buffer = new unsigned int[f_flash_info.spare_block_size];
			this->spare_block_table = new Spare_Block_Table(f_flash_info.spare_block_size);

			fread(spare_block_table_buffer, sizeof(unsigned int), this->f_flash_info.spare_block_size, table);
			/*** 버퍼에 한 번에 읽은 다음 seq_write에 의한 순차 할당 ***/
			for (unsigned int spare_block_table_buffer_index = 0; spare_block_table_buffer_index < this->f_flash_info.spare_block_size; spare_block_table_buffer_index++)
			{
				this->spare_block_table->seq_write(spare_block_table_buffer[spare_block_table_buffer_index]);
			}
		}
		else
			goto LOAD_FAIL;
		break;

	case 3: //하이브리드 매핑 (Log algorithm - 1:2 Block level mapping with Dynamic Table)
		if (this->log_block_level_mapping_table == NULL && this->spare_block_table == NULL && this->offset_level_mapping_table == NULL)
		{
			//블록 단위 1:2 매핑 테이블, Spare 블록 테이블, 오프셋 단위 테이블
			this->log_block_level_mapping_table = new unsigned int* [this->f_flash_info.block_size - this->f_flash_info.spare_block_size]; //row : 전체 PBN의 수

			for (unsigned int i = 0; i < this->f_flash_info.block_size - this->f_flash_info.spare_block_size; i++)
			{
				log_block_level_mapping_table[i] = new unsigned int[2]; //col : 두 공간은 각각 PBN1, PBN2를 나타냄
				//각 행을 별도로 할당했기 때문에 한 번에 전체 배열을 읽을 수 없다. 행 단위로 수행
				fread(this->log_block_level_mapping_table[i], sizeof(unsigned int), 2, table);
			}

			spare_block_table_buffer = new unsigned int[f_flash_info.spare_block_size];
			this->spare_block_table = new Spare_Block_Table(f_flash_info.spare_block_size);
			fread(spare_block_table_buffer, sizeof(unsigned int), this->f_flash_info.spare_block_size, table);
			/*** 버퍼에 한 번에 읽은 다음 seq_write에 의한 순차 할당 ***/
			for (unsigned int spare_block_table_buffer_index = 0; spare_block_table_buffer_index < this->f_flash_info.spare_block_size; spare_block_table_buffer_index++)
			{
				this->spare_block_table->seq_write(spare_block_table_buffer[spare_block_table_buffer_index]);
			}

			this->offset_level_mapping_table = new __int8[this->f_flash_info.block_size * BLOCK_PER_SECTOR];
			fread(this->offset_level_mapping_table, sizeof(__int8), this->f_flash_info.block_size * BLOCK_PER_SECTOR, table);
		}
		else
			goto LOAD_FAIL;
		break;

	default:
		return;
	}

	if (spare_block_table_buffer != NULL)
		delete[] spare_block_table_buffer;

	fclose(table);
	return;

LOAD_FAIL:
	fprintf(stderr, "오류 : 테이블 불러오기 실패\n");
	system("pause");
	exit(1);
}
void FlashMem::save_table(int mapping_method) //매핑방식에 따른 매핑 테이블 저장
{
	/***
		< 테이블 save 순서 >
		
		1) Block level(normal or log block) table
		2) Spare Block table
		3) Offset level table
	***/

	FILE* table = NULL;

	if ((table = fopen("table.bin", "wb")) == NULL) //쓰기 + 이진파일 모드
	{
		fprintf(stderr, "table.bin 파일을 쓰기모드로 열 수 없습니다. (save_table)");
		return;
	}

	switch (mapping_method) //매핑 방식에 따라 저장
	{
	case 2: //블록 매핑
		//블록 단위 매핑 테이블, Spare 블록 테이블
		if (this->block_level_mapping_table != NULL && this->spare_block_table != NULL)
		{
			fwrite(this->block_level_mapping_table, sizeof(unsigned int), this->f_flash_info.block_size - this->f_flash_info.spare_block_size, table);
			fwrite(this->spare_block_table->table_array, sizeof(unsigned int), this->f_flash_info.spare_block_size, table);
		}
		else
			goto SAVE_FAIL;
		break;

	case 3: //하이브리드 매핑 (Log algorithm - 1:2 Block level mapping with Dynamic Table)
		//블록 단위 1:2 매핑 테이블, Spare 블록 테이블, 오프셋 단위 테이블
		if (this->log_block_level_mapping_table != NULL && this->spare_block_table != NULL && this->offset_level_mapping_table != NULL)
		{
			//각 행을 별도로 할당했기 때문에 한 번에 전체 배열을 쓸 수 없다. 행 단위로 수행
			for (unsigned int i = 0; i < this->f_flash_info.block_size - this->f_flash_info.spare_block_size; i++)
			{
				fwrite(this->log_block_level_mapping_table[i], sizeof(unsigned int), 2, table);
			}
			fwrite(this->spare_block_table->table_array, sizeof(unsigned int), this->f_flash_info.spare_block_size, table); //Spare block 테이블 기록
			fwrite(this->offset_level_mapping_table, sizeof(__int8), this->f_flash_info.block_size * BLOCK_PER_SECTOR, table); //오프셋 단위 매핑 테이블 기록
		}
		else
			goto SAVE_FAIL;
		break;

	default:
		return;
	}

	fclose(table);
	return;

SAVE_FAIL:
	fprintf(stderr, "오류 : 테이블 저장 실패\n");
	system("pause");
	exit(1);
}

void FlashMem::deallocate_table() //현재 캐시된 모든 테이블 해제
{
	//블록 매핑 테이블
	if (this->block_level_mapping_table != NULL)
	{
		delete[] this->block_level_mapping_table;
		this->block_level_mapping_table = NULL;
	}

	//1 : 2 블록 매핑 테이블 (Log Algorithm)
	if (this->log_block_level_mapping_table != NULL)
	{
		//Spare block 개수를 제외한 크기가 테이블의 크기
		for (unsigned int i = 0; i < this->f_flash_info.block_size - this->f_flash_info.spare_block_size; i++)
		{
			delete[] this->log_block_level_mapping_table[i];
		}
		delete[] this->log_block_level_mapping_table;
		this->log_block_level_mapping_table = NULL;
	}

	//오프셋 단위 매핑 테이블
	if (this->offset_level_mapping_table != NULL)
	{
		delete[] this->offset_level_mapping_table;
		this->offset_level_mapping_table = NULL;
	}

	//Spare 블록 테이블
	if (this->spare_block_table != NULL)
	{
		delete this->spare_block_table;
		this->spare_block_table = NULL;
	}
}

void FlashMem::disp_command(int mapping_method, int table_type) //커맨드 명령어 출력
{
	switch (mapping_method)
	{
	case 0:
		system("cls");
		std::cout << "========================================================" << std::endl;
		std::cout << "	플래시 메모리 시뮬레이터 - Non-FTL" << std::endl;
		std::cout << "========================================================" << std::endl;
		std::cout << " init x 또는 i x - x MB 만큼 플래시 메모리 생성 " << std::endl;
		std::cout << " read PSN 또는 r PSN - 물리 섹터의 데이터 읽기" << std::endl;
		std::cout << " write PSN data 또는 w PSN data - 물리 섹터의 data 기록" << std::endl;
		std::cout << " erase PBN 또는 e PBN - 물리 블록의 데이터 삭제" << std::endl;
		std::cout << "--------------------------------------------------------" << std::endl;
		std::cout << " change - 매핑 방식 변경" << std::endl;
		std::cout << "--------------------------------------------------------" << std::endl;
		std::cout << " pbninfo PBN - 해당 물리 블록의 모든 섹터의 meta정보 출력" << std::endl;
		std::cout << "--------------------------------------------------------" << std::endl;
		std::cout << " info - 플래시 메모리 정보 출력" << std::endl;
		std::cout << " exit - 종료" << std::endl;
		std::cout << "========================================================" << std::endl;
		break;

	case 1:
		break;

	case 2:
		system("cls");
		std::cout << "====================================================================" << std::endl;
		std::cout << "	플래시 메모리 시뮬레이터 - Block Mapping ";
		if(table_type == 0) std::cout << "(Static Table)" << std::endl;
		else std::cout << "(Dynamic Table)" << std::endl;
		std::cout << "====================================================================" << std::endl;
		std::cout << " init x 또는 i x - x MB 만큼 플래시 메모리 생성 " << std::endl;
		std::cout << " read LSN 또는 r LSN - 논리 섹터의 데이터 읽기" << std::endl;
		std::cout << " write LSN data 또는 w LSN data - 논리 섹터의 data 기록" << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		std::cout << " change - 매핑 방식 변경" << std::endl;
		std::cout << " print - 매핑 테이블 출력" << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		std::cout << " trace - trace파일로보터 쓰기 성능 측정  " << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		//std::cout << " gc - 강제로 가비지 컬렉션 실시  " << std::endl;
		std::cout << " vqprint - Victim Block 큐 출력  " << std::endl;
		std::cout << " lbninfo LBN - 해당 LBN에 대응된 물리 블록의 모든 섹터의 meta정보 출력" << std::endl;
		std::cout << " pbninfo PBN - 해당 물리 블록의 모든 섹터의 meta정보 출력" << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		std::cout << " info - 플래시 메모리 정보 출력" << std::endl;
		std::cout << " exit - 종료" << std::endl; //Victim Block 큐가 비어 있을 때만 종료 가능(GC가 작업중이 아닐 때)
		std::cout << "====================================================================" << std::endl;
		break;

	case 3:
		system("cls");
		std::cout << "====================================================================" << std::endl;
		std::cout << "	플래시 메모리 시뮬레이터 - Hybrid Mapping ";
		if (table_type == 0) std::cout << "(Static Table)" << std::endl;
		else std::cout << "(Dynamic Table)" << std::endl;
		std::cout << "====================================================================" << std::endl;
		std::cout << " init x 또는 i x - x MB 만큼 플래시 메모리 생성 " << std::endl;
		std::cout << " read LSN 또는 r LSN - 논리 섹터의 데이터 읽기" << std::endl;
		std::cout << " write LSN data 또는 w LSN data - 논리 섹터의 data 기록" << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		std::cout << " change - 매핑 방식 변경" << std::endl;
		std::cout << " print - 매핑 테이블 출력" << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		std::cout << " trace - trace파일로보터 쓰기 성능 측정  " << std::endl;
		//std::cout << " gc - 강제로 가비지 컬렉션 실시  " << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		std::cout << " vqprint - Victim Block 큐 출력  " << std::endl;
		std::cout << " lbninfo LBN - 해당 LBN에 대응된 물리 블록의 모든 섹터의 meta정보 출력" << std::endl;
		std::cout << " pbninfo PBN - 해당 물리 블록의 모든 섹터의 meta정보 출력" << std::endl;
		std::cout << "--------------------------------------------------------------------" << std::endl;
		std::cout << " info - 플래시 메모리 정보 출력" << std::endl;
		std::cout << " exit - 종료" << std::endl; //Victim Block 큐가 비어 있을 때만 종료 가능(GC가 작업중이 아닐 때)
		std::cout << "====================================================================" << std::endl;
		break;

	default:
		break;
	}
}

void FlashMem::input_command(FlashMem** flashmem, int& mapping_method, int& table_type) //커맨드 명령어 입력
{
	std::string user_input; //사용자로부터 명령어를 한 줄로 입력받는 변수
	
	/*** user_input으로부터 분리하는 값들 ***/
	std::string command; //명령어
	char data = NULL; //기록하고자 하는 데이터
	__int64 megabytes = -1; //생성하고자 하는 플래시 메모리 용량
	__int64 LSN = -1; //논리 섹터 번호
	__int64 LBN = -1; //논리 블록 번호
	__int64 PBN = -1; //물리 블록 번호
	__int64 PSN = -1; //물리 섹터 번호
	//↑ 예외 처리를 위해 __int64형으로 받는다

	char data_output = NULL; //물리 섹터로부터 읽어들인 데이터
	
	std::cout << "명령어 >> ";
	std::getline(std::cin, user_input); //한 줄로 입력받기
	std::stringstream ss(user_input); //분리
	ss >> command;

	switch (mapping_method)
	{
	case 0: //매핑 사용하지 않음
		if (command.compare("init") == 0 || command.compare("i") == 0) //megabytes 만큼 플래시 메모리 생성
		{
			ss >> megabytes;
			if (megabytes <= 0)
			{
				std::cout << "must over 0 megabytes" << std::endl;
				system("pause");
				break;
			}
			else if (megabytes > MAX_CAPACITY_MB)
			{
				std::cout << "최대 할당 가능 공간 초과" << std::endl;
				system("pause");
				break;
			}

			init(flashmem, megabytes, mapping_method, table_type);
			system("pause");
		}
		else if (command.compare("read") == 0 || command.compare("r") == 0) //물리 섹터의 데이터 읽기
		{
			ss >> PSN;
			if (PSN < 0)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			if(Flash_read(flashmem, NULL, PSN, data_output) != FAIL)
			{
				if(data_output != NULL)
					std::cout << data_output << std::endl;
				else
					std::cout << "no data" << std::endl;
			}
			
			system("pause");
			break;

		}
		else if (command.compare("write") == 0 || command.compare("w") == 0) //물리 섹터에 data 기록
		{
			ss >> PSN;
			ss >> data;
			if (PSN < 0 || data == NULL)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			Flash_write(flashmem, NULL, PSN, data);
			system("pause");
		}
		else if (command.compare("erase") == 0 || command.compare("e") == 0) //물리 블록 번호에 해당되는 블록의 데이터 삭제
		{
			ss >> PBN;
			if (PBN < 0)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			Flash_erase(flashmem, PBN);
			system("pause");
		}
		else if (command.compare("change") == 0) //매핑 방식 변경
		{
			(*flashmem)->switch_mapping_method(mapping_method, table_type);
			if(*flashmem != NULL)
				(*flashmem)->deallocate_table(); //기존 테이블 해제
		}
		else if (command.compare("pbninfo") == 0) //PBN meta 정보 출력
		{
			ss >> PBN;
			if (PBN < 0)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			print_block_meta_info(flashmem, false, PBN, mapping_method);
			system("pause");
		}
		else if (command.compare("info") == 0) //정보 출력
		{
			(*flashmem)->disp_flash_info(flashmem, mapping_method, table_type);
			system("pause");
		}
		else if (command.compare("exit") == 0) //종료
		{
			exit(1);
		}
		else if (command.compare("cleaner") == 0)
		{
			system("cleaner.cmd");
		}
		else
		{
			std::cout << "잘못된 명령어 입력" << std::endl;
			system("pause");
			break;
		}
		break;

	default: //블록, 하이브리드 매핑
		if (command.compare("init") == 0 || command.compare("i") == 0) //megabytes 만큼 플래시 메모리 생성
		{
			ss >> megabytes;
			if (megabytes <= 0)
			{
				std::cout << "must over 0 megabytes" << std::endl;
				system("pause");
				break;
			}
			init(flashmem, megabytes, mapping_method, table_type);
			system("pause");
		}
		else if (command.compare("read") == 0 || command.compare("r") == 0) //논리 섹터의 데이터 읽기
		{
			ss >> LSN;
			if (LSN < 0)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			FTL_read(flashmem, LSN, mapping_method, table_type);
			system("pause");
			break;

		}
		else if (command.compare("write") == 0 || command.compare("w") == 0) //논리 섹터에 data 기록
		{
			ss >> LSN;
			ss >> data;
			if (LSN < 0 || data == NULL)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			FTL_write(flashmem, LSN, data, mapping_method, table_type);
			system("pause");
		}
		else if (command.compare("change") == 0) //매핑 방식 변경
		{
			(*flashmem)->switch_mapping_method(mapping_method, table_type);
			
			if(*flashmem != NULL) //역참조하여 생성되어있는지 확인
				(*flashmem)->deallocate_table(); //기존 테이블 해제
		}
		else if (command.compare("print") == 0) //매핑 테이블 출력
		{
			Print_table(flashmem, mapping_method, table_type);
			system("pause");
		}
		
		else if (command.compare("vqprint") == 0) //Victim Block 큐 출력
		{
			(*flashmem)->victim_block_queue->print();
		}
		else if (command.compare("trace") == 0) //트레이스 실행
		{
			trace(flashmem, mapping_method, table_type);
			system("pause");
		}
		else if (command.compare("lbninfo") == 0) //LBN에 대응된 PBN meta 정보 출력
		{
			ss >> LBN;
			if (LBN < 0)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			print_block_meta_info(flashmem, true, LBN, mapping_method);
			system("pause");
		}
		else if (command.compare("pbninfo") == 0) //PBN meta 정보 출력
		{
			ss >> PBN;
			if (PBN < 0)
			{
				std::cout << "잘못된 명령어 입력" << std::endl;
				system("pause");
				break;
			}
			print_block_meta_info(flashmem, false, PBN, mapping_method);
			system("pause");
		}
		else if (command.compare("info") == 0) //정보 출력
		{
			(*flashmem)->disp_flash_info(flashmem, mapping_method, table_type);
			system("pause");
		}
		else if (command.compare("exit") == 0) //종료
		{
			if (*flashmem != NULL)
			{
				(*flashmem)->gc->RDY_terminate = true; //종료 대기 상태 알림
				(*flashmem)->gc->scheduler(flashmem, mapping_method);
			}
			else
				exit(1);
		}
		else if (command.compare("cleaner") == 0)
		{
			system("cleaner.cmd");
		}
		else
		{
			std::cout << "잘못된 명령어 입력" << std::endl;
			system("pause");
			break;
		}
		break;
	}
}

void FlashMem::disp_flash_info(FlashMem** flashmem, int mapping_method, int table_type) //현재 생성된 플래시 메모리의 정보 출력
{
	if (*flashmem != NULL) //현재 생성된 플래시 메모리의 정보 보여주기
	{
		F_FLASH_INFO f_flash_info = (*flashmem)->get_f_flash_info(); //생성된 플래시 메모리의 고정된 정보를 가져온다

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
		
		float physical_used_percent = ((float)physical_using_space / (float)f_flash_info.storage_byte) * 100;
		float logical_used_percent = ((float)logical_using_space / ((float)f_flash_info.storage_byte - (float)f_flash_info.spare_block_byte)) * 100;
		
		if (mapping_method != 0)
		{
			std::cout << "테이블 타입 : ";
			switch (table_type)
			{
			case 0:
				std::cout << "Static Table" << std::endl;
				break;

			case 1:
				std::cout << "Dynamic Table" << std::endl;
				break;
			default:
				break;
			}
		}
		std::cout << "현재 생성된 플래시 메모리의 용량 : " << f_flash_info.flashmem_size << "MB(" << f_flash_info.storage_byte << "bytes)" << std::endl;
		std::cout << "전체 블록 수 : " << f_flash_info.block_size << " [범위 : 0~" << f_flash_info.block_size - 1 << "]" << std::endl;
		std::cout << "전체 섹터 수 : " << f_flash_info.sector_size << " [범위 : 0~" << f_flash_info.sector_size - 1 << "]" << std::endl;
		std::cout << "-----------------------------------------------------" << std::endl;
		/*** 
			매핑 방식을 사용 안 할 경우, Overwrite가 불가능하므로, 섹터(페이지)단위 또는 블록 단위 Invalid 처리가 발생하지 않음
			따라서, 물리적으로 남아있는 실제 기록 가능 공간과 논리적으로 남아있는 기록 가능 공간 (사용자에게 보여지는 용량)은 항상 동일하다.
		***/
		std::cout << "물리적으로 남아있는 기록 가능 공간 (Spare Block 포함) : " << physical_free_space << "bytes" << std::endl;
		printf("사용 중 %ubytes / 전체 %ubytes (%.1f%% used)\n", physical_using_space, f_flash_info.storage_byte, physical_used_percent);
		std::cout << "논리적으로 남아있는 기록 가능 공간 (사용자에게 보여지는 용량) : " << logical_free_space << "bytes" << std::endl;
		printf("사용 중 %ubytes / 전체 %ubytes (%.1f%% used)\n", logical_using_space, f_flash_info.storage_byte - f_flash_info.spare_block_byte, logical_used_percent);
		std::cout << "-----------------------------------------------------" << std::endl;
		std::cout << "전체 Spare Area 공간 크기 : " << f_flash_info.spare_area_byte << "bytes" << std::endl;
		std::cout << "Spare Area 제외 전체 데이터 공간 크기 : " << f_flash_info.data_only_storage_byte << "bytes" << std::endl;
		std::cout << "-----------------------------------------------------" << std::endl;
		std::cout << "Current Flash read count : " << (*flashmem)->v_flash_info.flash_read_count << std::endl;
		std::cout << "Current Flash write count : " << (*flashmem)->v_flash_info.flash_write_count << std::endl;
		std::cout << "Current Flash erase count : " << (*flashmem)->v_flash_info.flash_erase_count << std::endl;
		std::cout << "-----------------------------------------------------" << std::endl;
		if (mapping_method != 0) //매핑 방식을 사용하면
		{
			std::cout << "전체 Spare Block 수 : " << f_flash_info.spare_block_size << "개" << std::endl;
			std::cout << "전체 Spare Block 크기 : " << f_flash_info.spare_block_byte << "bytes" << std::endl;
			std::cout << "-----------------------------------------------------" << std::endl;
			(*flashmem)->gc->print_invalid_ratio_threshold();
		}
	}
	else
	{
		std::cout << "not initialized" << std::endl;
	}

	return;
}

void FlashMem::switch_mapping_method(int& mapping_method, int& table_type) //현재 플래시 메모리의 매핑 방식 및 테이블 타입 변경
{
	while (1)
	{
		int input_mapping_method = -1;
		int input_table_type = -1;

		system("cls");
		std::cout << "=============================================================================" << std::endl;
		std::cout << "!! 매핑 방식 및 테이블 타입 변경 시 새로 플래시 메모리를 생성(init)하여야 함" << std::endl;
		std::cout << "=============================================================================" << std::endl;
		std::cout << "0 : Do not use any Mapping Method" << std::endl;
		std::cout << "2 : Block Mapping Method (Static Table, Dynamic Table)" << std::endl;
		std::cout << "3 : Hybrid Mapping Method (log algorithm - 1:2 block level mapping)" << std::endl;
		std::cout << "=============================================================================" << std::endl;
		std::cout << ">>";
		std::cin >> input_mapping_method;

		switch (input_mapping_method)
		{
		case 0: //non-ftl
			break;

		case 2: //block mapping
			std::cin.clear(); //오류스트림 초기화
			std::cin.ignore(INT_MAX, '\n'); //입력버퍼비우기

			std::cout << "=============================================================================" << std::endl;
			std::cout << "!! 매핑 방식 및 테이블 타입 변경 시 새로 플래시 메모리를 생성(init)하여야 함" << std::endl;
			std::cout << "=============================================================================" << std::endl;
			std::cout << "0 : Static Table" << std::endl;
			std::cout << "1 : Dynamic Table" << std::endl;
			std::cout << "=============================================================================" << std::endl;
			std::cout << ">>";
			std::cin >> input_table_type;

			if (input_table_type != 0 && input_table_type != 1)
			{
				std::cin.clear(); //오류스트림 초기화
				std::cin.ignore(INT_MAX, '\n'); //입력버퍼비우기
				continue;
			}

			break;

		case 3: //hybrid mapping
			input_table_type = 1; //Dynamic Table
			break;

		default:
			break;
		}

		std::cin.clear(); //오류스트림 초기화
		std::cin.ignore(INT_MAX, '\n'); //입력버퍼비우기

		if(input_mapping_method != -1)
			mapping_method = input_mapping_method;
	
		if(input_table_type != -1)
			table_type = input_table_type;

		break;
	}
}

FIXED_FLASH_INFO FlashMem::get_f_flash_info() //플래시 메모리의 고정된 정보 반한
{
	return this->f_flash_info;
}