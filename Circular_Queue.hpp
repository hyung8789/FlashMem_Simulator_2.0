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
Circular_Queue<data_type, element_type>::Circular_Queue(data_type queue_size) //할당 크기 - 1개의 원소만 저장 가능, 따라서 생성 시 + 1 크기로 생성
{
	this->queue_array = NULL;
	this->queue_size = queue_size;
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
	FILE* ebq_output = NULL;

	if ((ebq_output = fopen("ebq_output.txt", "wt")) == NULL) //쓰기 + 텍스트파일 모드
		return;

	printf("QUEUE(front = %u rear = %u)\n", this->front, this->rear);
	if (!(this->is_empty())) //큐가 비어있지 않으면
	{
		unsigned int i = this->front;

		do {
			i = (i + 1) % (this->queue_size);

			fprintf(ebq_output, "INDEX : %u\n", i);
			fprintf(ebq_output, "empty_block_num : %u\n", this->queue_array[i]);
			fprintf(ebq_output, "----------------------------------\n");

			std::cout << "INDEX : " << i << std::endl;
			std::cout << "empty_block_num : " << this->queue_array[i] << std::endl;
			std::cout << "----------------------------------" << std::endl;

			if (i == this->rear) //rear위치까지 도달 후 종료
				break;
		} while (i != this->front); //큐를 한 바퀴 돌때까지
	}
	printf("\n");

	fclose(ebq_output);
	printf(">> ebq_output.txt\n");
	system("notepad ebq_output.txt");
}

inline int Empty_Block_Queue::enqueue(empty_block_num src_block_num) //Empty Block 삽입
{
	if (this->is_full()) //가득 찼으면
		return FAIL;

	this->rear = (this->rear + 1) % this->queue_size;
	this->queue_array[this->rear] = src_block_num;

	return SUCCESS;
}

inline int Empty_Block_Queue::dequeue(empty_block_num& dst_block_num) //Empty Block 가져오기
{
	if (this->is_empty()) //비어있으면
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

	this->init_mode = true;
	this->load_read_index();
}

inline void Spare_Block_Queue::print() //출력
{
#ifdef DEBUG_MODE
	if (this->init_mode) //가득 차지 않았을 경우 불완전하므로 읽어서는 안됨
	{
		fprintf(stderr, "치명적 오류 : 불완전한 Spare Block Queue\n");
		system("pause");
		exit(1);
	}
#endif

	FILE* sbq_output = NULL;

	if ((sbq_output = fopen("sbq_output.txt", "wt")) == NULL) //쓰기 + 텍스트파일 모드
		return;

	printf("QUEUE(front = %u rear = %u)\n", this->front, this->rear);

	for (unsigned int i = 1; i < this->queue_size; i++) //0번 인덱스 사용하지 않음
	{
		fprintf(sbq_output, "INDEX : %u\n", i);
		fprintf(sbq_output, "spare_block_num : %u\n", this->queue_array[i]);
		fprintf(sbq_output, "----------------------------------\n");
		std::cout << "INDEX : " << i << std::endl;
		std::cout << "spare_block_num : " << this->queue_array[i] << std::endl;
		std::cout << "----------------------------------" << std::endl;
	}

	fclose(sbq_output);
	printf(">> sbq_output.txt\n");
	system("notepad sbq_output.txt");
}

inline int Spare_Block_Queue::enqueue(spare_block_num src_block_num) //Spare Block 삽입
{
	if (!(this->init_mode)) //가득 찼을 경우 더 이상 기록 불가
		return FAIL;

	this->rear = (this->rear + 1) % this->queue_size;
	this->queue_array[this->rear] = src_block_num;

	if (this->is_full())
	{
		this->init_mode = false; //Spare Block 대기열은 SWAP 작업만 수행 가능
		return COMPLETE;
	}

	return SUCCESS;
}

inline int Spare_Block_Queue::dequeue(class FlashMem*& flashmem, spare_block_num& dst_spare_block, unsigned int& dst_read_index) //빈 Spare Block, 해당 블록의 index 가져오기
{
	//현재 read_index에 따른 read_index 전달, Spare Block 번호 전달 후 다음 Spare Block 위치로 이동

	META_DATA* meta_buffer = NULL;

#ifdef DEBUG_MODE
	if (this->init_mode)
	{
		fprintf(stderr, "치명적 오류 : 초기화 되지 않은 Spare Block Queue\n");
		system("pause");
		exit(1);
	}
#endif
	
	unsigned int end_read_index = this->front;
	do {
		this->front = (this->front + 1) % this->queue_size;

		if (this->front == 0) //0번 인덱스 위치는 사용하지 않으므로 읽지 않도록 해야 한다.
			this->front = (this->front + 1) % this->queue_size;

		SPARE_read(flashmem, (this->queue_array[this->front] * BLOCK_PER_SECTOR), meta_buffer); //PBN * BLOCK_PER_SECTOR == 해당 PBN의 meta 정보
		
		if (meta_buffer->get_block_state() == BLOCK_STATE::SPARE_BLOCK_EMPTY) //유효하고 비어있는 블록일 경우 전달
		{
			dst_spare_block = this->queue_array[this->front]; //Spare Block의 PBN 전달
			dst_read_index = this->front; //SWAP을 위한 해당 PBN의 테이블 상 index 전달

			this->save_read_index();

			if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
				goto MEM_LEAK_ERR;

			return SUCCESS;
		}
		//아직 GC에 의한 처리가 되지 않은 블록일 경우, 해당 블록은 Erase 수행 전에는 사용 불가

		if (deallocate_single_meta_buffer(meta_buffer) != SUCCESS)
			goto MEM_LEAK_ERR;

	} while (this->front != end_read_index); //한 바퀴 돌 때까지 (Spare Block 대기열은 항상 가득 차 있어야 한다)

	this->save_read_index();

	return FAIL; //현재 사용 가능 한 Spare Block이 없으므로, Victim Block Queue에 대해 GC에서 처리를 수행하여야 함

MEM_LEAK_ERR:
	fprintf(stderr, "치명적 오류 : meta 정보에 대한 메모리 누수 발생 (rr_read)\n");
	system("pause");
	exit(1);
}

inline void Spare_Block_Queue::manual_init(unsigned int spare_block_size) //수동 재 구성
{
	if (this->queue_array == NULL) //queue_array는 수동 재 구성을 위해 먼저 할당되어야 한다.
		goto MANUAL_INIT_ERR;

	switch (this->init_mode)
	{
	case true:
		this->init_mode = false;
		this->rear = spare_block_size;
		break;

	case false: //이미 init_mode가 아닐 경우, 잘못 호출
		goto MANUAL_INIT_ERR;
	}

	return;

MANUAL_INIT_ERR:
	fprintf(stderr, "치명적 오류 : Spare Block 대기열 수동 재 구성 예외 발생 (manual_init)\n");
	system("pause");
	exit(1);
}

inline int Spare_Block_Queue::save_read_index()
{
	FILE* rr_read_index_output = NULL;

	if ((rr_read_index_output = fopen("rr_read_index.txt", "wt")) == NULL) //쓰기 + 텍스트파일 모드
		return COMPLETE; //실패해도 계속 수행

	fprintf(rr_read_index_output, "%u", this->front);
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
	FILE* vbq_output = NULL;

	if ((vbq_output = fopen("vbq_output.txt", "wt")) == NULL) //쓰기 + 텍스트파일 모드
		return;

	printf("QUEUE(front = %u rear = %u)\n", this->front, this->rear);
	if (!(this->is_empty())) //큐가 비어있지 않으면
	{
		unsigned int i = this->front;

		do {
			i = (i + 1) % (this->queue_size);

			switch (this->queue_array[i].is_logical)
			{
			case true:
				fprintf(vbq_output, "victim_LBN : ");
				std::cout << "victim_LBN : ";
				break;

			case false:
				fprintf(vbq_output, "victim_PBN : ");
				std::cout << "victim_PBN : ";
				break;
			}

			fprintf(vbq_output, "%u\n", this->queue_array[i].victim_block_num);
			std::cout << this->queue_array[i].victim_block_num << std::endl;
			
			switch (this->queue_array[i].proc_state)
			{
			case VICTIM_BLOCK_PROC_STATE::UNLINKED:
				fprintf(vbq_output, "VICTIM_BLOCK_PROC_STATE::UNLINKED\n");
				std::cout << "VICTIM_BLOCK_PROC_STATE::UNLINKED" << std::endl;
				break;

			case VICTIM_BLOCK_PROC_STATE::SPARE_LINKED:
				fprintf(vbq_output, "VICTIM_BLOCK_PROC_STATE::SPARE_LINKED\n");
				std::cout << "VICTIM_BLOCK_PROC_STATE::SPARE_LINKED" << std::endl;
				break;

			case VICTIM_BLOCK_PROC_STATE::UNPROCESSED_FOR_MERGE:
				fprintf(vbq_output, "VICTIM_BLOCK_PROC_STATE::UNPROCESSED_FOR_MERGE\n");
				std::cout << "VICTIM_BLOCK_PROC_STATE::UNPROCESSED_FOR_MERGE" << std::endl;
				break;
			}
			fprintf(vbq_output, "----------------------------------\n");
			std::cout << "----------------------------------" << std::endl;

			if (i == this->rear) //rear위치까지 도달 후 종료
				break;
		} while (i != this->front); //큐를 한 바퀴 돌때까지
	}
	printf("\n");

	fclose(vbq_output);
	printf(">> vbq_output.txt\n");
	system("notepad vbq_output.txt");
}

inline int Victim_Block_Queue::enqueue(victim_block_element src_block_element) //Victim Block 삽입
{
	if (this->is_full()) //가득 찼으면
		return FAIL;

	this->rear = (this->rear + 1) % this->queue_size;
	this->queue_array[this->rear] = src_block_element;

	return SUCCESS;
}

inline int Victim_Block_Queue::dequeue(victim_block_element& dst_block_element) //Victim Block 가져오기
{
	if (this->is_empty()) //비어있으면
		return FAIL;

	this->front = (this->front + 1) % this->queue_size;
	dst_block_element = this->queue_array[this->front];

	return SUCCESS;
}