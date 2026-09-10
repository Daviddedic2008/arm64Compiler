#include "regAllocator.h"
#include <stdint.h>
#include <stdlib.h>

void printRanges(const sizedPool p){
	for(uint32_t r = 0; r < p.size; r++){
		const range rt = ((range*)p.data)[r];
		if(rt.vReg == NULL){printf("???\n"); continue;}
		printf("RANGE(%d : %d)<<", rt.i1, rt.i2); printSymbol(*rt.vReg); printf(">>");
		if(rt.vReg->preferredReg) printf("PREF[x%d]", rt.vReg->preferredReg-1); printf("\n");
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
            quad* q_ptr = &((quad*)quadArena.pool)[q]; if(q_ptr->skippable) continue;
			uint32_t preReg = (sf != NULL) ? sf->preferredReg : 0;
            if(validType(q_ptr->o1) && q_ptr->o1.vReg == r){
                if(!start){ start = q + 1;} sf = &q_ptr->o1; 
                end = q;
            }
            if(validType(q_ptr->o2) && q_ptr->o2.vReg == r){
                if(!start){ start = q + 1;} sf = &q_ptr->o2; 
                end = q;
            }
            if(validType(q_ptr->o3) && q_ptr->o3.vReg == r){
                if(!start){ start = q + 1;} sf = &q_ptr->o3;
                end = q;
            } 
			if(preReg != 0 && sf != NULL) sf->preferredReg = preReg;
        }
        start -= 1;
        ((range*)ret.data)[r] = (range){.vReg = sf, .i1 = start, .i2 = end};
    }
    return ret;
}

typedef struct{
	range* n1; range* n2;
}edge;


#define startingEdges 2048

arena constructEdges(const sizedPool ranges){
	arena edgeArena = newArena(sizeof(edge) * 2048);
	 range* r = (range*)ranges.data; for(uint32_t n = 0; n < ranges.size; n++, r++){
		range* r2 = (range*)ranges.data; for(uint32_t ni = 0; ni < ranges.size; ni++, r2++){
			if((r->i1 < r2->i2) && (r2->i1 < r->i2) && (n-ni) && !r2->edgesFound){
				const edge e = (edge){.n1 = r, .n2 = r2};
				writeElement(&edgeArena, &e, sizeof(edge));
			}
		} r->edgesFound = 1;
	} return edgeArena;
}

void printEdges(const arena edgeArena){
	edge* e = (edge*)edgeArena.pool; for(uint32_t n = 0; n < edgeArena.used/sizeof(edge); n++, e++){
		printf("EDGE<<R(%d, %d) : R(%d, %d)>>\n", e->n1->i1, e->n1->i2, e->n2->i1, e->n2->i2);
	}
}

void precolor(){
	
}