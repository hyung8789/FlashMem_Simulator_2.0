#include "FlashMem.h"

FlashMem* flashmem = NULL;

int mapping_method = 0; //현재 플래시 메모리의 매핑방식 (default : 0 - 사용하지 않음)
int table_type = 0; //매핑 테이블의 타입 (0 : Static Table, 1 : Dynamic Table)

void main() {
	flashmem->bootloader(&flashmem, mapping_method, table_type);

	while (1)
	{
		flashmem->disp_command(mapping_method, table_type); //커맨드 출력
		flashmem->input_command(&flashmem, mapping_method, table_type); //커맨드 입력
	}
}