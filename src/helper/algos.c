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
			const int8_t cr; if(compareEl == NULL) cr = (*((uint32_t*)e1) > *((uint32_t*)e2)) - (*((uint32_t*)e2) > *((uint32_t*)e1)); else 
			cr = compareEl(e1, e2); if(cr == -1){swap(e1, e2, elSz); e1 = e2;} else break;
		}
	}
}