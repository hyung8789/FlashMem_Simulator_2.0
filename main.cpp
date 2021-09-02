#include "Build_Options.h"

FlashMem* flashmem = NULL;

MAPPING_METHOD mapping_method = MAPPING_METHOD::NONE; //현재 플래시 메모리의 매핑방식 (default : 사용하지 않음)
TABLE_TYPE table_type = TABLE_TYPE::STATIC; //매핑 테이블의 타입

void main() 
{
	flashmem->bootloader(flashmem, mapping_method, table_type);

	while (1)
	{
		flashmem->disp_command(mapping_method, table_type); //커맨드 출력
		flashmem->input_command(flashmem, mapping_method, table_type); //커맨드 입력
	}
}