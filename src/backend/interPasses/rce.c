#include "rce.h"

uint64_t getPhysicalReads(const quad q){
	uint64_t mask = 0;
	if(validType(q.o2)) mask |= (1 << q.o2.physicalReg);
	if(validType(q.o3)) mask |= (1 << q.o3.physicalReg);
	if(q.op == RET && validType(q.o1)) mask |= (1 << q.o1.physicalReg);
	return mask;
}

int32_t getWrite(const quad q){
	if(isWriteQuad(q.op)) return q.o1.physicalReg;
	return 0;
}

bool eqOp(const quad q){
	const int32_t wr = getWrite(q);
	if(wr) switch(q.op){
		case MOV: return (q.o1.physicalReg && q.o2.physicalReg) && q.o1.physicalReg == q.o2.physicalReg;
	}
	return 0;
}

void rmRedundantWrites(){
	arena* quadArena = getQuadArena();
	quad* lastWrite[numGPRegs];
	quad* q = ((quad*)quadArena->pool); for(uint32_t qo = 0; qo < quadArena->used / sizeof(quad); qo++, q++){
		int32_t wr = getWrite(*q); if(wr){
			if(eqOp(*q)){q->skippable = 1; continue;}
		} 
	}
}