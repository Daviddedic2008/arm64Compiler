#include "algos.h"
#include <string.h>
#include <stdlib.h>

static inline void swap(uint8_t* d1, uint8_t* d2, const uint32_t elSz){
	if(elSz > 256) return;
	uint8_t buffer[256];
	memcpy(buffer, d1, elSz);
	memcpy(d1, d2, elSz);
	memcpy(d2, buffer, elSz);
}

void insertionSortIMPL(void* data, const uint32_t elSz, const uint32_t numEl, int8_t (*compareEl)(void*, void*)){
	uint8_t* e1; uint8_t* e2;for(uint32_t i = 1; i < numEl; i++){
		e1 = (uint8_t*)data + i * elSz;
		for(int64_t i2 = i-1; i2 >= 0; i2--){ e2 = (uint8_t*)data + i2 * elSz;
			int8_t cr; if(compareEl == NULL) cr = (*((uint32_t*)e1) > *((uint32_t*)e2)) - (*((uint32_t*)e2) > *((uint32_t*)e1)); else 
			cr = compareEl(e1, e2); if(cr == -1){swap(e1, e2, elSz); e1 = e2;} else break;
		}
	}
}

hashmap makeMap(uint32_t (*hash)(void* const, const uint32_t), const uint32_t keySize, const uint32_t elementSize, const int64_t expectedNumElements){
	const uint32_t entrySize = keySize + elementSize;
	uint32_t mp2 = 1; for(; mp2 < expectedNumElements; mp2 << 1);
	hashmap ret = (hashmap){.elements = newArena(mp2 * entrySize + mp2 * 8), .elementSize = elementSize, .hash = hash};
	ret.hashes = ret.elements.pool, ret.offsets = ret.elements.pool + mp2 * 4, ret.values = ret.elements.pool + mp2 * 8;
	return ret;
}

void writeIntoHashmap(hashmap* const map, void* const keyData, void* const elementData){
}