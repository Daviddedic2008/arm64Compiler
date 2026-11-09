#include <stdint.h>
#include "arenaAlloc.h"

void insertionSortIMPL(void* data, const uint32_t elSz, const uint32_t numEl, int8_t (*compareEl)(void*, void*));

#define insertionSort(data, elSz, numEl, compareEl) do { \
    _Static_assert(__builtin_constant_p(elSz), "Error: elSz parameter must be a compile-time constant expression!"); \
    insertionSortIMPL((data), (elSz), (numEl), (compareEl)); \
} while(0)
	
typedef struct hashmapIntIntEl{
	uint32_t key;
	uint32_t value;
}hashmapIntIntEl;

typedef struct hashmapStrIntEl{
	char key[32];
	uint32_t value;
}hashmapStrIntEl;

typedef struct hashmap{
	arena elements; uint32_t* hashes; uint32_t* offsets; void* values; uint32_t keySize, elementSize, numElements;
	uint32_t (*hash)(void*, uint32_t);
}hashmap;

hashmap makeMap(uint32_t (*hash)(void* const, const uint32_t), const uint32_t keySize, const uint32_t elementSize, const int64_t expectedNumElements);