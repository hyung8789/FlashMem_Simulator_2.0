#include "Spare_block_table.h"

// Round-Robin 기반의 Wear-leveling을 위한 Spare Block Selection Algorithm을 적용
// 원형 형태의 Spare Block 테이블 정의

Spare_Block_Table::Spare_Block_Table()
{
	this->is_full = false; //초기 비어 있음

	this->write_index = 0;
	this->read_index = 0;
	this->table_size = 0;
	this->table_array = NULL;
}

Spare_Block_Table::Spare_Block_Table(unsigned int spare_block_size)
{
	this->is_full = false; //초기 비어 있음
	
	this->write_index = 0;
	this->read_index = 0;
	this->table_size = spare_block_size;
	this->table_array = NULL;

	this->init();
}

Spare_Block_Table::~Spare_Block_Table()
{
	if (this->table_array != NULL)
	{
		delete[] this->table_array;
		this->table_array = NULL;
	}
}

void Spare_Block_Table::init()
{
	if (this->table_array == NULL)
		this->table_array = new spare_block_element[this->table_size];

	this->load_read_index(); //초기 생성 시 Round-Robin 기반의 Wear-leveling을 위해 기존 read_index 값 재 할당 (존재 할 경우)
}


void Spare_Block_Table::print() //Spare Block에 대한 원형 배열 출력 함수(debug)
{
	if (this->is_full != true) //가득 차지 않았을 경우 불완전하므로 읽어서는 안됨
	{
		fprintf(stderr, "오류 : 불완전한 Spare Block Table\n");
		system("pause");
		exit(1);
	}

	std::cout << "current read_index : " << this->read_index << std::endl;

	for (unsigned int i = 0; i < this->table_size; i++)
	{
		std::cout << "INDEX : " << i << "|| ";
		std::cout << "Spare_block : " << this->table_array[i] << std::endl;
	}
}

int Spare_Block_Table::rr_read(class FlashMem** flashmem, spare_block_element& dst_spare_block, unsigned int& dst_read_index) //현재 read_index에 따른 read_index 전달, Spare Block 번호 전달 후 다음 Spare Block 위치로 이동
{
	META_DATA* meta_buffer = NULL;

	if (this->is_full != true) //가득 차지 않았을 경우 불완전하므로 읽어서는 안됨
	{
		fprintf(stderr, "오류 : 불완전한 Spare Block Table\n");
		system("pause");
		exit(1);
	}

	/*** 일반 블록과 SWAP이 발생하였지만, 아직 GC에 의해 처리가 되지 않은 Spare Block에 대하여 사용 할 수 없도록 예외처리 ***/
	unsigned int end_read_index = this->read_index;
	do {
		meta_buffer = SPARE_read(flashmem, (this->table_array[this->read_index] * BLOCK_PER_SECTOR)); //PBN * BLOCK_PER_SECTOR
		
		if (meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::valid_block] == true &&
			meta_buffer->meta_data_array[(__int8)META_DATA_BIT_POS::empty_block] == true) //유효하고 비어있는 블록일 경우 전달
		{
			dst_spare_block = this->table_array[this->read_index];
			dst_read_index = this->read_index;
			
			this->read_index = (this->read_index + 1) % this->table_size;
			this->save_read_index();
			
			if (deallocate_single_meta_buffer(&meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;

			return SUCCESS;
		}
		else //아직 GC에 의한 처리가 되지 않은 블록
			this->read_index = (this->read_index + 1) % this->table_size;

		if (deallocate_single_meta_buffer(&meta_buffer) != SUCCESS)
			goto MEM_LEAK_ERR;

	} while (this->read_index != end_read_index); //한 바퀴 돌떄까지

	this->save_read_index();

	return FAIL; // Victim Block Queue에 대해 GC에서 처리를 수행하여야 함

MEM_LEAK_ERR:
	fprintf(stderr, "오류 : meta 정보에 대한 메모리 누수 발생 (rr_read)\n");
	system("pause");
	exit(1);
}

int Spare_Block_Table::seq_write(spare_block_element src_spare_block) //테이블 값 순차 할당
{
	if (this->is_full == true) //가득 찼을 경우 더 이상 기록 불가
		return FAIL;

	this->table_array[this->write_index] = src_spare_block;
	this->write_index++;
	
	if (this->write_index == this->table_size)
	{
		this->is_full = true;
		return COMPLETE;
	}

	return SUCCESS;
}

int Spare_Block_Table::save_read_index() //Reorganization을 위해 현재 read_index 값 저장
{
	FILE* rr_read_index = NULL;

	if (this->is_full != true) //가득 차지 않았을 경우 불완전하므로 처리해서는 안됨
	{
		fprintf(stderr, "오류 : 불완전한 Spare Block Table\n");
		system("pause");
		exit(1);
	}

	if ((rr_read_index = fopen("rr_read_index.txt", "wt")) == NULL) //쓰기 + 텍스트파일 모드
	{
		fprintf(stderr, "rr_read_index.txt 파일을 쓰기모드로 열 수 없습니다. (save_read_index)");

		return FAIL;
	}

	fprintf(rr_read_index, "%u", this->read_index);
	fclose(rr_read_index);

	return SUCCESS;
}

int Spare_Block_Table::load_read_index() //Reorganization을 위해 기존 read_index 값 불러옴
{
	FILE* rr_read_index = NULL;

	if ((rr_read_index = fopen("rr_read_index.txt", "rt")) == NULL) //읽기 + 텍스트파일 모드
	{
		this->read_index = 0;
		return COMPLETE;
	}

	fscanf(rr_read_index, "%u", &this->read_index);
	fclose(rr_read_index);

	return SUCCESS;
}