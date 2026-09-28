#include "regAllocator.h"
#include <stdint.h>
#include <float.h>
#include <stdlib.h>
#include "../../tester/testGen.h"
#include "../../helper/algos.h"

#define stackStep 2048
sizedPool vRegStack; uint32_t curStackPos = 0;

arena edgeArena;
sizedPool ranges;

void makevRegStack(){ vRegStack = (sizedPool){.data = malloc(sizeof(range*) * stackStep), .size = stackStep}; }

void push(const range* r){
	if(curStackPos >= vRegStack.size){vRegStack.size *= 2; vRegStack.data = realloc(vRegStack.data, vRegStack.size * sizeof(range*));}
	((range**)vRegStack.data)[curStackPos++] = r;
}

void printRanges(){ const sizedPool p = ranges;
	for(uint32_t r = 0; r < p.size; r++){
		const range rt = ((range*)p.data)[r];
		if(rt.vReg == NULL){printfD("???\n"); continue;}
		if(rt.i1 == -1){printfD("GLOBAL<<"); printSymbol(*rt.vReg);
		printfD(">>\n"); continue;}
		printfD("RANGE(%d : %d)<<", rt.i1, rt.i2); printSymbol(*rt.vReg); printfD(">>");
		if(rt.vReg->preferredReg) printfD("PREF[x%d]", rt.vReg->preferredReg-1); 
		printfD(" COST<%d>", rt.vReg->spillCost);printfD("\n");
	}
}

#pragma pack(push, 0)
typedef struct{
	symbol o1, o2, o3;
}quad_packed;
#pragma pack(pop)

bool validType(const symbol s){
	switch(s.type){
		case local: case arg: case temp: case global: return 1;
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
        } if(sf->type == global){((range*)ret.data)[r] = (range){.vReg = sf, .i1 = -1, .i2 = -1}; continue;}
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

void findFreeRegister(symbol* const srcReg){
	bool registerTaken[numGPRegs] = {0};
	edge* e = (edge*)edgeArena.pool; for(uint32_t n = 0; n < edgeArena.used/sizeof(edge); n++, e++){
		int16_t r = -1;
		if(sameSymbol(*(srcReg), *(e->n1->vReg))){
			r = e->n2->vReg->physicalReg;
		}
		else if(sameSymbol(*(srcReg), *(e->n2->vReg))){
			r = e->n1->vReg->physicalReg;
		}
		if(r != -1) registerTaken[r] = true; 	
	} if(srcReg->preferredReg) if(!registerTaken[srcReg->preferredReg-1]){srcReg->physicalReg = srcReg->preferredReg-1; return;}
	for(uint32_t r = 0; r < numGPRegs; r++) if(!registerTaken[r]){srcReg->physicalReg = r; return;}
}

typedef enum quadCheckState{noUse, smblUsed, smblWrite} quadCheck;

quadCheck checkQuad(const quad q, const symbol s){
	if(q.o1.type != invalidSymbol && sameSymbol(s, q.o1)) return smblWrite;
	if((q.o2.type != invalidSymbol && sameSymbol(s, q.o2)) || 
		(q.o3.type != invalidSymbol && sameSymbol(s, q.o3))) return smblWrite;
	return noUse;
}

quad refactorQuad(quad q, const symbol s1, const symbol s2){
	if(sameSymbol(q.o1, s1)) q.o1 = s2; if(sameSymbol(q.o2, s1)) q.o2 = s2; if(sameSymbol(q.o3, s1)) q.o3 = s2;
	return q;
}

typedef struct{
	quad q; uint32_t sId;
}indexedQuad;

int8_t cmpIndexedQuads(void* q1, void* q2){
	const int8_t r = ((indexedQuad*)q2)->sId - ((indexedQuad*)q1)->sId;
	if(r >= 0) return 1; if(!r) return 0; return -1;
}

void writeQuad(arena* a, const quad q){
	writeElement(a, &q, sizeof(quad));
}

#define newStepQuad 2048
void chaitinPass(){
	curStackPos = 0;
	if(vRegStack.data == NULL) makevRegStack();
	// k is max physical regs
	// check for nodes of degree < k
	// pop them to stack
	// when all are popped if any are left degree >= k put them on the spill stack in heuristic order(most likely to spill last)
	// go through the stack element 0 to element end and assign physical regs
	// if spilling is required: add stores loads and split live ranges into smaller ranges. DEF -> STORE, LOAD -> USE
	// MAKE SURE TO CHECK FOR INFINITE SPILL LOOPS
	while(1){ bool ret = 1;
		for(uint32_t r = 0; r < ranges.size; r++){
			const range* rt = ((range*)ranges.data) + r;
			if(rt->numEdges < numGPRegs && !rt->offGraph){
				push(rt); ret = 0;
				decrementFromPool(rt);
			}
		} if(ret){float minWeight = FLT_MAX; int64_t bestIdx = -1; for(uint32_t r = 0; r < ranges.size; r++){
				const range rt = ((range*)ranges.data)[r]; const uint32_t rtsc = rt.numEdges ? (rt.vReg->spillCost/rt.numEdges) : 1<<30;
				if(rtsc < minWeight && !rt.offGraph) minWeight = rtsc, bestIdx = r;
			}
			if(bestIdx != -1){ret = 0; push((range*)ranges.data + bestIdx); decrementFromPool((range*)ranges.data + bestIdx);}
		}
		if(ret) break;
	} bool anySpill = 0;
	arena* quadArena = getQuadArena();
	arena addInstrs = newArena(sizeof(indexedQuad) * newStepQuad);
	for(int64_t r = ranges.size-1; r >= 0; r--){
		range* rt = ((range**)vRegStack.data)[r];
		findFreeRegister(rt->vReg);
		if(rt->vReg->physicalReg == -1){ anySpill = 1;
			const uint32_t spStackOff = 0; // idk how to do this yet, have to allocate stack space.
			// insert spill code & split ranges
			// at each spill, go through live range and keep separate pool where u store instructions to be added/replaced
			bool foundRewrite = 0;for(uint32_t qi = rt->i1; qi <= rt->i2; qi++){
				const quad cq = (((indexedQuad*)quadArena->pool) + qi)->q;
				const quadCheck qc = checkQuad(cq, *rt->vReg);
				uint32_t i1, i2; symbol newVR;
				switch(qc){
					case smblUsed:{
						newVR = newVReg(rt->vReg->varType.type); i1 = qi; i2 = qi + 1;
						indexedQuad refactoredQuad = (indexedQuad){.q = refactorQuad(cq, *rt->vReg, newVR), .sId = qi};
						refactoredQuad.q.refactorType = rfReplace;
						const indexedQuad newQuad = (indexedQuad){.q = (quad){.op = LOAD, .o1 = newVR, .o2 = (symbol){.type = literal, .vReg = spStackOff}}, .sId = qi};
						writeElement(&addInstrs, &newQuad, sizeof(indexedQuad)); writeElement(&addInstrs, &refactoredQuad, sizeof(indexedQuad));
						break;
					}
					case smblWrite:{
						i1 = qi; i2 = qi;
						newVR = newVReg(rt->vReg->varType.type);
						quad replaceQuad;
						replaceQuad = refactorQuad(cq, *rt->vReg, newVR); replaceQuad.refactorType = rfReplace;
						quad newQuad = {.op = INVALIDOP};
						if(cq.op != MOV){i2++; newQuad = (quad){.op = STORE, .o1 = (symbol){.type = literal, .vReg = spStackOff}, .o2 = newVR}; newQuad.refactorType = rfNew;}
						writeElement(&addInstrs, &replaceQuad, sizeof(indexedQuad)); if(newQuad.op) writeElement(&addInstrs, &newQuad, sizeof(indexedQuad));
						break;
					}
					default: goto skipFound;
				}
				if(foundRewrite){
					
					const range nr = (range){.vReg = addSymbolPre(newVR), .i1 = i1, .i2 = i2};
					addEl(&ranges, &nr, sizeof(range));
				} else{
					*rt = (range){.vReg = addSymbolPre(newVR), .i1 = i1, .i2 = i2};
					foundRewrite = 1;
				}
				skipFound:;
			}
			// at each use, ADD a load into new temporary vreg. copy spillCost. REPLACE use instruction to use new temp
			// at each writeback, either REPLACE with a store or ADD a store if its a compound instruction that isnt MOV
		}
	}
	for(uint32_t qi = 0; qi < quadArena->used/sizeof(quad); qi++){
		quad* q = (quad*)(quadArena->pool) + qi;
		if(validType(q->o1)) q->o1.physicalReg = ((range*)(ranges.data))[q->o1.vReg].vReg->physicalReg+1;
		if(validType(q->o2)) q->o2.physicalReg = ((range*)(ranges.data))[q->o2.vReg].vReg->physicalReg+1;
		if(validType(q->o3)) q->o3.physicalReg = ((range*)(ranges.data))[q->o3.vReg].vReg->physicalReg+1;
	}
	if(anySpill){
		// go through pool of ADD and REPLACE instructions. sort in order from first to last
		// iterate through og quad pool and REPLACE and ADD into new quad pool that has been allocated to hold all quads, free old one.
		// rerun buildin ranges, edges, and finally run chaitin pass again
		insertionSort(addInstrs.pool, sizeof(indexedQuad), addInstrs.used/sizeof(indexedQuad), cmpIndexedQuads);
		const uint32_t nnq = addInstrs.used/sizeof(indexedQuad);
		arena newQuads = newArena(quadArena->used + sizeof(quad) * nnq);
		uint32_t prevId = 0; for(uint32_t i = 0; i < nnq; i++){
			const indexedQuad pq = ((indexedQuad*)addInstrs.pool)[i];
			for(uint32_t q = prevId; q < pq.sId; q++){
				writeQuad(&newQuads, ((quad*)(quadArena->pool))[q]);
			} switch(pq.q.refactorType){
				case rfReplace:
				writeQuad(&newQuads, pq.q);
				break;
				case rfNew:
				writeQuad(&newQuads, ((quad*)(quadArena->pool))[pq.sId]);
				writeQuad(&newQuads, pq.q);
				break;
			}
		} freeArena((*quadArena)); *quadArena = newQuads; freeArena(addInstrs);
		constructRanges(newQuads);
		constructEdges();
		chaitinPass();
	}
}