#include "regAllocator.h"
#include <stdint.h>
#include <stdlib.h>
#include "../../tester/testGen.h"

#define stackStep 2048
sizedPool symbolStack; uint32_t curStackPos = 0;

arena edgeArena;
sizedPool ranges;

void makeSymbolStack(){ symbolStack = (sizedPool){.data = malloc(sizeof(symbol) * stackStep), .size = stackStep}; }

void push(const symbol s){
	if(curStackPos >= symbolStack.size){symbolStack.size *= 2; symbolStack.data = realloc(symbolStack.data, symbolStack.size);}
	((symbol*)symbolStack.data)[curStackPos++] = s;
}

symbol pop(){
	return ((symbol*)symbolStack.data)[--curStackPos];
}

void printRanges(){ const sizedPool p = ranges;
	for(uint32_t r = 0; r < p.size; r++){
		const range rt = ((range*)p.data)[r];
		if(rt.vReg == NULL){printfD("???\n"); continue;}
		printfD("RANGE(%d : %d)<<", rt.i1, rt.i2); printSymbol(*rt.vReg); printfD(">>");
		if(rt.vReg->preferredReg) printfD("PREF[x%d]", rt.vReg->preferredReg-1); printfD("\n");
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

bool sameSymbol(const symbol s1, const symbol s2){return s1.vReg == s2.vReg && s1.type == s2.type;}

void constructRanges(const arena quadArena){
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
    ranges = ret;
}

typedef struct{
	range* n1; range* n2;
}edge;


#define startingEdges 2048

void constructEdges(){
	edgeArena = newArena(sizeof(edge) * 2048);
	 range* r = (range*)ranges.data; for(uint32_t n = 0; n < ranges.size; n++, r++){
		range* r2 = (range*)ranges.data; for(uint32_t ni = 0; ni < ranges.size; ni++, r2++){
			if((r->i1 < r2->i2) && (r2->i1 < r->i2) && (n-ni) && !r2->edgesFound){
				const edge e = (edge){.n1 = r, .n2 = r2};
				r->numEdges++; r2->numEdges++; writeElement(&edgeArena, &e, sizeof(edge));
			}
		} r->edgesFound = 1;
	}
}

void printEdges(){
	edge* e = (edge*)edgeArena.pool; for(uint32_t n = 0; n < edgeArena.used/sizeof(edge); n++, e++){
		printfD("EDGE<<R(%d, %d) : R(%d, %d)>>\n", e->n1->i1, e->n1->i2, e->n2->i1, e->n2->i2);
	}
}

void decrementFromPool(range* const r){
	r->offGraph = 1;
	edge* e = (edge*)edgeArena.pool; for(uint32_t n = 0; n < edgeArena.used/sizeof(edge); n++, e++){
		if(sameSymbol(*(r->vReg), *(e->n1->vReg))) e->n2->numEdges--; 
		else if(sameSymbol(*(r->vReg), *(e->n2->vReg))) e->n1->numEdges--;
	}
}