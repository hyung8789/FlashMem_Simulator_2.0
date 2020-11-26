#include "Random_gen.h"

// Wear-leveling을 위한 무작위 오프셋 번호를 생성하는 난수 추출 함수 정의

__int8 get_rand_offset() //0 ~ 31 범위 내 반환
{ 
	return dist(gen); //분포 내의 메르센 트위스터에 의한 무작위 난수 반환
}