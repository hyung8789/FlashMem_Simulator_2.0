// Circular_Queue 클래스 템플릿 및 하위 클래스 기능 정의
// Round-Robin 기반의 Wear-leveling을 위한 Empty Block 대기열, Spare Block 대기열 정의
// Victim Block 처리에 효율성을 위한 Victim Block 대기열 정의

/*** Circual Queue 클래스 템플릿 정의 (생성, 소멸, 큐 상태 반환, 요소 수 반환) ***/

template <typename data_type, typename element_type>
Circular_Queue<data_type, element_type>::Circular_Queue()
{
	this->queue_array = NULL;
	this->queue_size = 0;
	this->front = this->rear = 0;
}

template <typename data_type, typename element_type>
Circular_Queue<data_type, element_type>::Circular_Queue(data_type queue_size)
{
	this->queue_array = NULL;
	this->queue_size = queue_size + 1; //데이터 삽입 시 증가 된 위치에 삽입 하므로 queue_size만큼 저장하기 위해 + 1
	this->queue_array = new element_type[this->queue_size];

	this->front = this->rear = 0;
}

template <typename data_type, typename element_type>
Circular_Queue<data_type, element_type>::~Circular_Queue()
{
	if (this->queue_array != NULL)
	{
		delete[] this->queue_array;
		this->queue_array = NULL;
	}
}

template <typename data_type, typename element_type>
bool Circular_Queue<data_type, element_type>::is_empty() //공백 상태 검출
{
	return (this->front == this->rear); //front와 rear의 값이 같으면 공백상태
}

template <typename data_type, typename element_type>
bool Circular_Queue<data_type, element_type>::is_full() //포화 상태 검출
{
	return ((this->rear + 1) % this->queue_size == this->front); //증가된 위치에서 front를 만날경우 포화상태
}

template <typename data_type, typename element_type>
data_type Circular_Queue<data_type, element_type>::get_count() //큐의 요소 개수 반환
{
	return (this->rear) - (this->front);
}

/*** Empty Block 대기열 기능 정의 (출력, 삽입, 삭제) ***/

inline void Empty_Block_Queue::print() //출력
{
	printf("QUEUE(front = %u rear = %u)\n", this->front, this->rear);
	if (this->is_empty() != true) //큐가 비어있지 않으면
	{
		unsigned int i = this->front;

		do {
			i = (i + 1) % (this->queue_size);

			std::cout << "INDEX : " << i << "|| ";
			std::cout << "empty_block_num : " << this->queue_array[i] << std::endl;
			std::cout << "----------------------------------" << std::endl;

			if (i == this->rear) //rear위치까지 도달 후 종료
				break;
		} while (i != this->front); //큐를 한 바퀴 돌때까지
	}
	printf("\n");
}

inline int Empty_Block_Queue::enqueue(empty_block_num src_block_num) //Empty Block 순차 할당
{
	if (this->is_full() == true) //가득 찼으면
		return FAIL;

	this->rear = (this->rear + 1) % this->queue_size;
	this->queue_array[this->rear] = src_block_num;

	return SUCCESS;
}

inline int Empty_Block_Queue::dequeue(empty_block_num& dst_block_num) //Empty Block 가져오기
{
	if (this->is_empty() == true) //비어있으면
		return FAIL;

	this->front = (this->front + 1) % this->queue_size;
	dst_block_num = this->queue_array[this->front];

	return SUCCESS;
}

/*** Spare Block 대기열 기능 정의 ***/

inline Spare_Block_Queue::Spare_Block_Queue(unsigned int queue_size) : Circular_Queue<unsigned int, spare_block_num>(queue_size)
{
	/***
		< Round-Robin Based Spare Block Table을 위한 기존 read_index 처리 방법 >

		1) 기존 플래시 메모리 제거하고 새로운 플래시 메모리 할당 시
		2) Bootloader에 의한 Reorganization시 기존 생성 된 플래시 메모리를 무시하고, 새로운 플래시 메모리 할당 시
		---
		=> Physical_func의 init함수 실행 시 기존 read_index 제거
	***/

	this->load_read_index();
}

inline void Spare_Block_Queue::print() //출력
{
	if (this->is_full()) //가득 차지 않았을 경우 불완전하므로 읽어서는 안됨
	{
		fprintf(stderr, "치명적 오류 : 불완전한 Spare Block Queue\n");
		system("pause");
		exit(1);
	}

	std::cout << "current read_index : " << this->front << std::endl;

	for (unsigned int i = 0; i < this->queue_size; i++)
	{
		std::cout << "INDEX : " << i << "|| ";
		std::cout << "Spare_block_num : " << this->queue_array[i] << std::endl;
	}
}

inline int Spare_Block_Queue::enqueue(spare_block_num src_block_num) //Spare Block 순차 할당
{
	if (this->is_full()) //가득 찼으면
		return FAIL;

	this->rear = (this->rear + 1) % this->queue_size;
	this->queue_array[this->rear] = src_block_num;

	return SUCCESS;
}

inline int Spare_Block_Queue::dequeue(class FlashMem*& flashmem, spare_block_num& dst_spare_block, unsigned int& dst_read_index) //빈 Spare Block, 해당 블록의 index 가져오기
{
	//현재 read_index에 따른 read_index 전달, Spare Block 번호 전달 후 다음 Spare Block 위치로 이동

	META_DATA* meta_buffer = NULL;

#ifdef DEBUG_MODE
	if (this->is_full()) //가득 차지 않았을 경우 불완전하므로 읽어서는 안됨
	{
		fprintf(stderr, "치명적 오류 : 불완전한 Spare Block Queue\n");
		system("pause");
		exit(1);
	}
#endif
	
	unsigned int end_read_index = this->front;
	do {
		SPARE_read(flashmem, (this->queue_array[this->front] * BLOCK_PER_SECTOR), meta_buffer); //PBN * BLOCK_PER_SECTOR == 해당 PBN의 meta 정보

		if (meta_buffer->block_state == BLOCK_STATE::SPARE_BLOCK_EMPTY) //유효하고 비어있는 블록일 경우 전달
		{
			dst_spare_block = this->queue_array[this->front]; //Spare Block의 PBN 전달
			dst_read_index = this->front; //SWAP을 위한 해당 PBN의 테이블 상 index 전달

			this->front = (this->front + 1) % this->queue_size;
			this->save_read_index();

			if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;

			return SUCCESS;
		}
		else //아직 GC에 의한 처리가 되지 않은 블록일 경우, 해당 블록은 Erase 수행 전에는 사용 불가
			this->front = (this->front + 1) % this->queue_size;

		if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
			goto MEM_LEAK_ERR;

	} while (this->front != end_read_index); //한 바퀴 돌떄까지

	this->save_read_index();

	return FAIL; //현재 사용 가능 한 Spare Block이 없으므로, Victim Block Queue에 대해 GC에서 처리를 수행하여야 함

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (rr_read)\n");
	system("pause");
	exit(1);
}

inline int Spare_Block_Queue::save_read_index()
{
	FILE* rr_read_index_output = NULL;

#ifdef DEBUG_MODE
	if (!(this->is_full())) //가득 차지 않았을 경우 불완전하므로 처리해서는 안됨
	{
		fprintf(stderr, "치명적 오류 : 불완전한 Spare Block Queue\n");
		system("pause");
		exit(1);
	}
#endif

	if ((rr_read_index_output = fopen("rr_read_index.txt", "wt")) == NULL) //쓰기 + 텍스트파일 모드
		return COMPLETE; //실패해도 계속 수행

	fprintf(rr_read_index_output, "%u", this->rear);
	fclose(rr_read_index_output);

	return SUCCESS;
}

inline int Spare_Block_Queue::load_read_index()
{
	FILE* rr_read_index_input = NULL;

	if ((rr_read_index_input = fopen("rr_read_index.txt", "rt")) == NULL) //읽기 + 텍스트파일 모드
	{
		this->front = 0;
		return COMPLETE;
	}

	fscanf(rr_read_index_input, "%u", &this->front);
	fclose(rr_read_index_input);

	return SUCCESS;
}

/*** Victim Block 대기열 기능 정의 ***/

inline void Victim_Block_Queue::print() //출력
{
	printf("QUEUE(front = %u rear = %u)\n", this->front, this->rear);
	if (this->is_empty() != true) //큐가 비어있지 않으면
	{
		unsigned int i = this->front;

		do {
			i = (i + 1) % (this->queue_size);

			switch (this->queue_array[i].is_logical)
			{
			case true:
				std::cout << "victim_LBN : ";
				break;

			case false:
				std::cout << "victim_PBN : ";
				break;
			}
			std::cout << this->queue_array[i].victim_block_num << std::endl;
			std::cout << "victim_block_invalid_ratio : " << this->queue_array[i].victim_block_invalid_ratio << std::endl;
			std::cout << "----------------------------------" << std::endl;

			if (i == this->rear) //rear위치까지 도달 후 종료
				break;
		} while (i != this->front); //큐를 한 바퀴 돌때까지
	}
	printf("\n");
}

inline int Victim_Block_Queue::enqueue(victim_block_element src_block_element) //삽입
{
	if (this->is_full()) //가득 찼으면
		return FAIL;

	this->rear = (this->rear + 1) % this->queue_size;
	this->queue_array[this->rear] = src_block_element;

	return SUCCESS;
}

inline int Victim_Block_Queue::dequeue(victim_block_element& dst_block_element) //삭제
{
	if (this->is_empty()) //비어있으면
		return FAIL;

	this->front = (this->front + 1) % this->queue_size;
	dst_block_element = this->queue_array[this->front];

	return SUCCESS;
}