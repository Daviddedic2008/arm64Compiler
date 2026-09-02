#include "regAllocator.h"
#include <stdint.h>
#include <stdlib.h>

void printRanges(const sizedPool p){
	for(uint32_t r = 0; r < p.size; r++){
		const range rt = ((range*)p.data)[r];
		if(rt.vReg == NULL){printf("???\n"); continue;}
		printf("RANGE(%d : %d)<<", rt.i1, rt.i2); printSymbol(*rt.vReg); printf(">>\n");
	}
}

#pragma pack(push, 0)
typedef struct{
	symbol o1, o2, o3;
}quad_packed;
#pragma pack(pop)

bool validType(const symbol s){
	switch(s.type){
		case local: case global: case arg: return 1;
		default: return 0;
	}
}

sizedPool constructRanges(const arena quadArena){
    const uint32_t nq = quadArena.used / sizeof(quad);
    const uint32_t vr = getTotalVRegs();
    sizedPool ret = {.data = malloc(sizeof(range) * vr), .size = vr};
    for(uint32_t r = 0; r < vr; r++){
        uint32_t start = 0, end = 0;
        symbol* sf = NULL;
        for(uint32_t q = 0; q < nq; q++){
            quad* q_ptr = &((quad*)quadArena.pool)[q];
            if(validType(q_ptr->o1) && q_ptr->o1.vReg == r){
                if(!start){ start = q + 1; sf = &q_ptr->o1; }
                end = q;
            }
            if(validType(q_ptr->o2) && q_ptr->o2.vReg == r){
                if(!start){ start = q + 1; sf = &q_ptr->o2; }
                end = q;
            }
            if(validType(q_ptr->o3) && q_ptr->o3.vReg == r){
                if(!start){ start = q + 1; sf = &q_ptr->o3; }
                end = q;
            }
        }
        start -= 1;
        ((range*)ret.data)[r] = (range){.vReg = sf, .i1 = start, .i2 = end};
    }
    return ret;
}