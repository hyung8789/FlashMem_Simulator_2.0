#ifndef _RANDOM_GEN_H_
#define _RANDOM_GEN_H_

// Wear-leveling을 위한 무작위 오프셋 번호를 생성하는 난수 추출 함수 선언, 정의

static std::random_device rand_device; //non-deterministic generator
static std::mt19937 gen(rand_device()); //to seed mersenne twister
static std::uniform_int_distribution<short> dist(0, BLOCK_PER_SECTOR - 1); //0 ~ 31 분포

inline __int8 get_rand_offset() //0 ~ 31 분포 내 반환
{
	return (__int8)dist(gen); //분포 내의 메르센 트위스터에 의한 무작위 난수 반환
}
#endif