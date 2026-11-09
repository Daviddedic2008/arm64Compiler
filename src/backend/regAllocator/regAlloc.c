#include "regAlloc.h"
#include <stdint.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include "../../tester/testGen.h"

#define BITVEC_WORDS 16

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

bool validType(const symbol s){
	switch(s.type){
		case defaultSymbol: case local: case arg: case temp: case global: return 1;
		default: return 0;
	}
}

bool sameSymbol(const symbol s1, const symbol s2){return s1.vReg == s2.vReg && s1.type == s2.type;}

typedef struct{
	range* n1; range* n2;
}edge;

#define startingEdges 2048

typedef struct{
	uint64_t w1 : 32;
	uint64_t w2 : 32;
}w64;

typedef struct{
	uint64_t arr[BITVEC_WORDS];
}bva;

// CHANGED: 1 -> 1ULL, (1 << 32+) is UB and dropped every vReg with bit%64 >= 31
#define SET_BIT_BVA(bvar, bit) (bvar).arr[(bit)/64] |= (1ULL << ((bit) % 64)), (bvar)
#define CHECK_BIT_BVA(bvar, bit) (((bvar).arr[(bit)/64] & (1ULL << ((bit) % 64))) != 0)

bva UBVA(bva bva1, const bva bva2){
	for(uint32_t u64 = 0; u64 < BITVEC_WORDS; u64++) bva1.arr[u64] |= bva2.arr[u64];
	return bva1;
}

bva ABVA(bva bva1, const bva bva2){
	for(uint32_t u64 = 0; u64 < BITVEC_WORDS; u64++) bva1.arr[u64] &= bva2.arr[u64];
	return bva1;
}

bva NBVA(bva bva1, const bva bva2){
	for(uint32_t u64 = 0; u64 < BITVEC_WORDS; u64++) bva1.arr[u64] &= ~bva2.arr[u64];
	return bva1;
}

uint8_t NZBVA(const bva b){for(uint32_t u64 = 0; u64 < BITVEC_WORDS; u64++) if(b.arr[u64]) return 1; return 0;}

bva SBBVA(const uint32_t bit){
	bva ret = (bva){0}; return SET_BIT_BVA(ret, bit);
}

bool EQBVA(const bva bva1, const bva bva2){
	for(uint32_t u64 = 0; u64 < BITVEC_WORDS; u64++) if(bva1.arr[u64] != bva2.arr[u64]) return 0;
	return 1;
}

void printBVA(const bva b){
	bool firstF = 1; printfD("{");
	for(uint32_t u64 = 0; u64 < BITVEC_WORDS; u64++){
		uint16_t off = 0; for(uint64_t mask = 1; mask; mask <<= 1, off++) if(b.arr[u64] & mask)
		{if(!firstF) printfD(", "); firstF = 0; printfD("%d", off);}
	}
	printfD("}\n");
}

typedef struct block{
	uint32_t q1, q2, lId, visitNum;
	uint32_t numPBranches, refVisit;
	struct block** pBranches;
	bva liveOut, liveIn;
	bva def, use;
	bva refIn, refOut;
}block;

block* activeBlocks;

block* findJmpBlock(const uint32_t lblId, const uint32_t numBlocks){
	const uint32_t qId = getLabelQID(lblId);
	uint32_t bl = 0;
	for(block* b = activeBlocks; bl < numBlocks; b++, bl++){
		if(b->q1 == qId) return b;
	} return NULL;
}
bool isConditionalJump(w64 w){return w.w2 == (0xFFFFFFFF - 1);}

void printBlock(const block* b){
	printfD("BLOCK[%d]<%d, %d>", b->lId, b->q1, b->q2);
	if(b->numPBranches){
		printfD("LINK{");
		for(uint32_t pbId = 0; pbId < b->numPBranches; pbId++){
			printfD("%d", b->pBranches[pbId]->lId);
			if(b->numPBranches - pbId - 1){
				printfD(", ");
			}
		} printfD("}");
	}
	printfD("\n");
	printfD("USE: "); printBVA(b->use);
	printfD("DEF: "); printBVA(b->def);
	printfD("LIVE IN: "); printBVA(b->liveIn);
	printfD("LIVE OUT: "); printBVA(b->liveOut);
}

void printBlocks(const uint32_t numBlocks){
	for(uint32_t i = 0; i < numBlocks; i++){
		printBlock(activeBlocks + i);
	}
}

bool isWriteQuad(uint32_t op){
	return op < STORE;
}

bool singleReadOp(const uint32_t op){
	switch(op){
		case RET: case PUSH: case POP: return 1;
		default: return 0;
	}
}

bva findReads(const quad q){
	bva ret = (bva){0};
	if(validType(q.o2)) SET_BIT_BVA(ret, q.o2.vReg);
	if(validType(q.o3)) SET_BIT_BVA(ret, q.o3.vReg);
	if(singleReadOp(q.op) && validType(q.o1)) SET_BIT_BVA(ret, q.o1.vReg);
	return ret;
}

bva findWrite(const quad q){
	bva write = (bva){0}; if(isWriteQuad(q.op) && validType(q.o1)) write = SBBVA(q.o1.vReg);
	return write;
}

symbol* rangeSyms;
uint8_t* edgeSeen; uint32_t edgeSeenDim;
uint32_t curFnStart, curFnEnd;
bool chaitinPass(const uint32_t fID, uint32_t* const endQ, uint32_t* const nq);

void buildRanges(const uint32_t startQ, const uint32_t endQ){
	const uint32_t nvr = getTotalVRegs();
	free(ranges.data); free(rangeSyms);
	ranges = (sizedPool){.data = calloc(nvr, sizeof(range)), .size = nvr};
	rangeSyms = (symbol*)calloc(nvr, sizeof(symbol));
	range* const r = (range*)ranges.data; for(uint32_t v = 0; v < nvr; v++) r[v].offGraph = 1;
	const quad* q = (quad*)(getQuadArena()->pool) + startQ; for(uint32_t qi = startQ; qi <= endQ; qi++, q++){
		const symbol* ops[3] = {&q->o1, &q->o2, &q->o3};
		for(uint32_t o = 0; o < 3; o++){ const symbol* s = ops[o];
			if(!validType(*s)) continue;
			range* rt = r + s->vReg;
			if(rt->vReg == NULL){
				rangeSyms[s->vReg] = *s; rangeSyms[s->vReg].physicalReg = 0;
				*rt = (range){.vReg = rangeSyms + s->vReg, .i1 = qi, .i2 = qi};
				if(s->type == global) rt->i1 = rt->i2 = -1;
			}
			if(s->preferredReg) rangeSyms[s->vReg].preferredReg = s->preferredReg;
			if(rt->i1 != -1) rt->i2 = qi;
		}
	}
}

void resetEdges(){
	if(edgeArena.pool) freeArena(edgeArena);
	edgeArena = newArena(sizeof(edge) * startingEdges);
	free(edgeSeen); edgeSeenDim = getTotalVRegs();
	edgeSeen = (uint8_t*)calloc((edgeSeenDim * edgeSeenDim) / 8 + 1, 1);
}

bool checkEdge(const uint32_t i1, const uint32_t i2){
	const uint32_t b = i1 * edgeSeenDim + i2; return (edgeSeen[b / 8] >> (b % 8)) & 1;
}
void markEdge(const uint32_t i1, const uint32_t i2){
	uint32_t b = i1 * edgeSeenDim + i2; edgeSeen[b / 8] |= 1 << (b % 8);
	b = i2 * edgeSeenDim + i1; edgeSeen[b / 8] |= 1 << (b % 8);
}

void newEdge(bva write, bva curLive){
	for(uint32_t w = 0; w < BITVEC_WORDS; w++){ if(!write.arr[w]) continue;
		for(uint32_t wb = 0; wb < 64; wb++){ if(!(write.arr[w] & (1ULL << wb))) continue;
			const uint32_t i1 = w * 64 + wb;
			for(uint32_t l = 0; l < BITVEC_WORDS; l++){ if(!curLive.arr[l]) continue;
				for(uint32_t lb = 0; lb < 64; lb++){ if(!(curLive.arr[l] & (1ULL << lb))) continue;
					const uint32_t i2 = l * 64 + lb;
					if(i1 == i2 || i1 >= ranges.size || i2 >= ranges.size || checkEdge(i1, i2)) continue;
					range* r1 = (range*)ranges.data + i1; range* r2 = (range*)ranges.data + i2;
					if(!r1->vReg || !r2->vReg) continue;
					markEdge(i1, i2); r1->numEdges++; r2->numEdges++;
					const edge e = (edge){.n1 = r1, .n2 = r2}; writeElement(&edgeArena, &e, sizeof(edge));
				}
			}
		}
	}
}

typedef struct quadInsertion{
	uint32_t idx;
	quad instr;
}quadInsertion;

memC* quadInsertions;

void initInsertions(){quadInsertions = newChunk(sizeof(quadInsertion) * getNumQuads() / 100);}
void addInsertion(const quad q, const uint32_t idx){
	if(q.op == INVALIDOP){quadInsertions->used = 0; return;}
	*(quadInsertion*)expandChunk(&quadInsertions, sizeof(quadInsertion)) = (quadInsertion){.instr = q, .idx = idx};
}
#define resetInsertions() addInsertion((quad){.op = INVALIDOP}, 0)

void flushInsertions(uint32_t nqo){
	insertionSort(quadInsertions->data, sizeof(quadInsertion), quadInsertions->used/sizeof(quadInsertion), NULL);
	const uint32_t used = sizeof(quad) * nqo + quadInsertions->used/sizeof(quadInsertion) * sizeof(quad); arena newQuadArena = newArena(used); newQuadArena.used = used;
	quad* nqp = (quad*)newQuadArena.pool; uint32_t nqid = 0;
	const quad* curQ = (quad*)(getQuadArena()->pool); uint32_t curQO = 0; for(uint32_t ii = 0; ii < quadInsertions->used/sizeof(quadInsertion); ii++){
		quadInsertion* qi = (quadInsertion*)quadInsertions->data + ii;
		for(;curQO <= qi->idx; curQO++, nqp[nqid++] = *(curQ++)); nqp[nqid++] = qi->instr;
	} for(;curQO < nqo; curQO++, nqp[nqid++] = *(curQ++));
	arena qa = *getQuadArena();
	freeArena(qa); *getQuadArena() = newQuadArena;
	quadInsertions->used = 0; resetInsertions();
}

void compUseDefRef(block* b){
	b->use = (bva){0}; b->def = (bva){0};
	const quad* q = (quad*)(getQuadArena()->pool) + b->q1; for(uint32_t qo = b->q1; qo < b->q2; qo++, q++){
		if(q->op == REF){SET_BIT_BVA((b->refOut), (q->o2.vReg)); continue;}
		bva reads = findReads(*q);
		bva write = findWrite(*q);
		b->use = UBVA(b->use, NBVA(reads, b->def)); b->def = UBVA(b->def, write);
	}
}

uint32_t blocksTraversed = 0;
void postOrderBlocks(block** postOrderTraversal, block* curBlock, const uint32_t numBlocks){
	if(!curBlock || curBlock->visitNum) return;
	curBlock->visitNum++;
	for(uint32_t i = 0; i < curBlock->numPBranches; i++){
		postOrderBlocks(postOrderTraversal, curBlock->pBranches[i], numBlocks);
	}
	postOrderTraversal[blocksTraversed++] = curBlock;
}

void computeEdges(block* b, const bool isFirst, uint32_t* const endQ, uint32_t* const nq){
	bva curLive = b->liveOut;
	const quad* q = (quad*)getQuadArena()->pool + b->q2; for(int64_t i = b->q2; i >= b->q1; i--, q--){
		if(isFirst && q->op == CALL){
			for(uint32_t v = 0; v < BITVEC_WORDS * 64; v++) if(CHECK_BIT_BVA(curLive, v)){
				addInsertion((quad){.op = PUSH, .o1 = (symbol){.type = defaultSymbol, .vReg = v}}, i-1);
				addInsertion((quad){.op = POP, .o1 = (symbol){.type = defaultSymbol, .vReg = v}}, i);
				(*nq) += 2; (*endQ) += 2;
			}
		}
		bva write = findWrite(*q); newEdge(write, curLive);
		curLive = NBVA(curLive, write);
		curLive = UBVA(curLive, findReads(*q));
	}
}

bva constructRefCurrent(const block* b, const uint32_t fID){
	bva ret = {0};
	const frameDescriptor* fd = getFrameDescriptor(fID);
	symbol* s = fd->vars; for(uint32_t i = 0; i < fd->numVars; i++, s++){
		SET_BIT_BVA(ret, s->vReg);
	} return ret;
}

void fillRefBvas(block* b, const uint32_t fID){
	bva cr = UBVA(constructRefCurrent(b, fID), b->refIn);
	b->refOut = UBVA(b->refOut, cr); block** c = b->pBranches; for(uint32_t i = 0; i < b->numPBranches; i++, c++){
		(*c)->refIn = UBVA((*c)->refIn, b->refOut);
	}
}

enum insertTypes {INSERTSTORE, INSERTLOAD};

void insertWrapper(quad* q, const uint32_t qId, uint32_t* const endQ, uint32_t* const nq, const bva wrapBva, const uint8_t type){
	switch(type){
		case INSERTSTORE:{
			const symbol po1 = q->o1;
			addInsertion((quad){.op = STORE, .o1 = (symbol){.type = physical, .vReg = 31}, .o2 = q->o1, .o3 = (symbol){.type = refSymbol, .vReg = po1.vReg}}, qId);
			break;
		}
		case INSERTLOAD:{
			for(uint32_t b = 0; b < getTotalVRegs(); b++){
				symbol po = {0};
				if(CHECK_BIT_BVA(wrapBva, b)){
					if(validType(q->o2) && q->o2.vReg == b){
						po = q->o2; 
					}
					else if(validType(q->o3) && q->o3.vReg == b){
						po = q->o3;
					}
					else if(validType(q->o1) && q->o1.vReg == b){
						po = q->o1;
					}
					if(po.type) addInsertion((quad){.op = LOAD, .o1 = po, .o2 = (symbol){.type = physical, .vReg = 31}, .o3 = (symbol){.type = refSymbol, .vReg = po.vReg}}, qId-1);
				}
			}
			break;
		}
	}
}

// return postordered blocks for stuff
enum readTypes{reconstructR, readR};
block** constructBlocks(const uint32_t startQ, const uint32_t endQ, const uint32_t fID, const bool readType, uint32_t* const numBlocks){
	// change quadgen to save a separate stack of labels and their related quad indices
	// will massively speed up pass 2
	// to construct blocks run 3 passes
	// pass 1, look for "leader" quads, which are either labels, returns, or jumps
	// make blocks based on sequential "leader" quads or quad0 and quadEND

	// pass 2 intertwined with previous pass, construct jump links.
	// for each block, if the end is a conditional jump add block it jumps to as a potential branch
	// if last quad is an absolute jump, the block it jumps to is THE ONLY POTENTIAL BRANCH
	// if no absolute jump is found, add next sequential block as potential branch(fallthrough)
	// each block also has to mark what blocks potentially branch TO it, so this goes both ways

	// BLOCK CONSTRUCTION DETAILS:
	// allocate pBranches and parentBlocks in quadArena
	// maintain 2 stacks, finished blocks(obv) and requested children blocks(store when a block needs to jump to another but the one being jumped to isnt constructed)
	// when you start making a block, check if its qids fall within any requested blocks. if so, update both block that requested and this one with children/parent pointers.
	// when you find a jump within your block and are looking at which block it will jump to, first call getLblQID to get quad ID.
	// check if quad ID is in any blocks that were already made. if so, just add that as your child. if not, push ur quad id to the requested blocks along with the pointer to the block being made rn
	// even if ur child is only requested, STILL ALLOCATE SPACE FOR IT!!!
	static block** postOrderTraversal; if(readType == readR) return postOrderTraversal;
	const arena* quadArena = getQuadArena(); blocksTraversed = 0;
	const lblInd* lbls = getLblInds(); uint32_t numLabels = 0; // only this function's labels
	for(uint32_t l = 0; l < getNumLbls(); l++) if(lbls[l].qId >= startQ && lbls[l].qId <= endQ) numLabels++;
	sizedPool jumpIds = (sizedPool){.data = allocateOnScratchpad((endQ - startQ + 1) * sizeof(w64)), .size = 0};
	for(uint32_t lId = 0, lc = 0; lId < numLabels; lId++, lbls++) if(lbls->qId >= startQ && lbls->qId <= endQ) ((lblInd*)jumpIds.data)[lc++] = *lbls;
	const quad* q = (quad*)quadArena->pool + startQ; uint32_t nqb = 0; for(uint32_t qi = startQ; qi <= endQ; qi++, q++){
		if(q->op == JMP || q->op == JMPCND || q->op == JMPABS || q->op == RET){
			((w64*)(jumpIds.data))[numLabels + nqb] = (w64){qi, 0xFFFFFFFF};
			jumpIds.size++; nqb++;
		}
	}
	const uint32_t numQids = numLabels + nqb;
	insertionSort(jumpIds.data, sizeof(w64), numQids, NULL);
	const uint32_t lastQid = numQids ? ((w64*)jumpIds.data)[numQids-1].w1 : startQ;
	const uint32_t firstQid = numQids ? ((w64*)jumpIds.data)[0].w1 : startQ;
	// now, go through sorted list to make initial block outlines.
	const uint32_t expectedNB = numQids ? (numQids + (lastQid != endQ) - (firstQid == startQ)) : 1;
	activeBlocks = (block*)allocateOnScratchpad(sizeof(block) * expectedNB);
	const w64* w64ids = ((w64*)jumpIds.data); uint32_t blocksCnstr = 0;
	int64_t i1 = 0, i2 = 0; do{
		block newBlock = (block){.q1 = (i1 == i2) ? (i1--, startQ) : w64ids[i1].w1, .q2 = numQids ? w64ids[i2].w1 : endQ-1, .lId = blocksCnstr};
		compUseDefRef(&newBlock);
		const quad endQ = ((quad*)(quadArena->pool))[newBlock.q2];
		const uint32_t npb = 1 + endQ.op == JMPCND;
		activeBlocks[blocksCnstr++] = newBlock;
		i2++, i1++;
	}while(i2 < numQids); if(blocksCnstr != expectedNB){
		block newBlock = (block){.q1 = lastQid, .q2 = endQ-1, .lId = blocksCnstr};
		compUseDefRef(&newBlock);
		activeBlocks[blocksCnstr++] = newBlock;
	}
	for(uint32_t bId = 0; bId < blocksCnstr; bId++){
		block* b = activeBlocks + bId;
		const quad qJmp = ((quad*)(quadArena->pool))[b->q2];
		switch(qJmp.op){
			case JMP:{
				const block* tempB = findJmpBlock(qJmp.o1.vReg, blocksCnstr);
				b->numPBranches = 1; b->pBranches = (block**)writeToScratchpad(&tempB, sizeof(block*));
				break;
			}
			case JMPCND:{
				const block* tempB = findJmpBlock(qJmp.o1.vReg, blocksCnstr);
				b->numPBranches = 1 + (bId != (blocksCnstr - 1));
				b->pBranches = (block**)allocateOnScratchpad(sizeof(block*) * b->numPBranches);
				*(b->pBranches) = tempB; if(bId != blocksCnstr - 1) *(b->pBranches + 1) = b+1;
				// fallthrough
				break;
			}
			case JMPABS:{
				// SWITCH STATEMENT, ALLOCATE FOR EACH CASE LABEL
				uint32_t numLbls = 0;
				const quad* q = (quad*)(quadArena->pool) + b->q2 - 1;
				while(q->op == WRITE_LABEL){
					q--; numLbls++;
				} b->numPBranches = numLbls; b->pBranches = (block**)allocateOnScratchpad(sizeof(block*) * numLbls);
				q = (quad*)(quadArena->pool) + b->q2 - 1;
				uint32_t pbId = 0; while(q->op == WRITE_LABEL){
					b->pBranches[pbId] = findJmpBlock((q--)->o1.vReg, blocksCnstr); pbId++;
				}
				break;
			}
			case RET:{
				b->pBranches = NULL;
				b->numPBranches = 0;
				break;
			}
			default:{
				const block* tempB = b + 1;
				b->pBranches = (block**)allocateOnScratchpad(sizeof(block*)); b->numPBranches = 1;
				*(b->pBranches) = tempB; // fallthrough
				break; // fallthrough
			}
		}
	}
	postOrderTraversal = (block**)allocateOnScratchpad(sizeof(block*) * blocksCnstr); initInsertions();
	postOrderBlocks(postOrderTraversal, activeBlocks, blocksCnstr); *numBlocks = blocksCnstr;
	return postOrderTraversal;
}

void refInsertions(block** postOrderTraversal, uint32_t* const endQ, uint32_t* const nq){
	quad* quadArena = (quad*)(getQuadArena()->pool);
	bool refChanged = 1;
	while(refChanged){
		refChanged = 0;
		for(int64_t i = blocksTraversed - 1; i >= 0; i--){
			block* curBlock = postOrderTraversal[i];
			const bva ti = curBlock->refIn; const bva to = curBlock->refOut;
			curBlock->refOut = UBVA(ti, to);
			for(uint32_t c = 0; c < curBlock->numPBranches; c++){
				curBlock->pBranches[c]->refIn = UBVA(curBlock->refOut, curBlock->pBranches[c]->refIn);
			}
			if(!EQBVA(curBlock->refIn, ti) || !EQBVA(curBlock->refOut, to)) refChanged = 1;
		}
	} 
	block* curBlock = *postOrderTraversal; for(int64_t i = 0; i < blocksTraversed; i++, curBlock = postOrderTraversal[i]){
		bva curRef = curBlock->refIn;
		quad* q = quadArena + curBlock->q1; for(uint32_t qi = curBlock->q1; qi <= curBlock->q2; qi++, q++){
			const bva reads = findReads(*q); const bva write = findWrite(*q);
			bva bvarr = ABVA(reads, curRef);
			bva bvawr = ABVA(write, curRef);
			if(q->op == REF){SET_BIT_BVA(curRef, q->o2.vReg); qi++, q++; continue;}
			if(NZBVA(bvarr)){
				insertWrapper(q, qi, endQ, nq, bvarr, INSERTLOAD);
			}
			if(NZBVA(bvawr)){
				(*endQ)++; (*nq)++;
				insertWrapper(q, qi, endQ, nq, bvawr, INSERTSTORE);
			}
		}
	} flushInsertions(*nq);
}

bool cfgEdges(block** postOrderTraversal, const uint32_t startQ, uint32_t* const endQ, uint32_t* const nq, const uint32_t fID, const uint32_t blocksCnstr, bool isFirst){
	// REFACTOR REFACTOR REFACTOR REFACTOR REFACTOR REFACTOR REFACTOR
	// HAS TO RUN ON A PER-FUNCTION BASIS, THEN DESTROY CFG AFTER CHAITIN BRIGGS
	// FOR CHAITIN BRIGGS, MAKE SURE THE BACKWARDS LIVENESS ANALYSIS DEALS W LOOPS PROPERLY

	// EDGE CONSTRUCTION:
	// make a reversed post order list of blocks(same as DFS just dont include circular connections)
	// traverse this reversed list depth first(bottom up) constructing live_in and live_out as u go
	// to compute live ins and outs do this:
	// have to do multiple passes until live_in and live_out stop changing for all blocks
	// compute live_out first, it should be SUM(live_in of potential branch nodes)
	// compute live_in second, it should be: use | (live_out NBVA def)
	arena* quadArena = getQuadArena();
	bool changed = 1;
	while(changed){
		changed = 0;
		for(uint32_t i = 0; i < blocksTraversed; i++){
			block* curBlock = postOrderTraversal[i];
			const bva to = curBlock->liveOut; const bva ti = curBlock->liveIn;
			for(uint32_t c = 0; c < curBlock->numPBranches; c++){
				curBlock->liveOut = UBVA(curBlock->liveOut, curBlock->pBranches[c]->liveIn);
			} curBlock->liveIn = UBVA(curBlock->use, NBVA(curBlock->liveOut, curBlock->def));
			if(!EQBVA(curBlock->liveIn, ti) || !EQBVA(curBlock->liveOut, to)) changed = 1;
		}
	} for(uint32_t i = 0; i < blocksTraversed; i++){
		block* curBlock = postOrderTraversal[i]; computeEdges(curBlock, isFirst, endQ, nq);
	}

	bool s = chaitinPass(fID, endQ, nq);
	printfD("\nFRAME ["); printSymbol(((quad*)(quadArena->pool))[startQ].o1); printfD("]\n");
	printBlocks(blocksCnstr);
	printfD("FRAME END\n");
	return s;
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
	uint8_t registerTaken[numGPRegs + 1] = {0}; // 1-based like physicalReg/preferredReg, [0] = none
	edge* e = (edge*)edgeArena.pool; for(uint32_t n = 0; n < edgeArena.used/sizeof(edge); n++, e++){
		int16_t r = 0, pr = 0;
		if(sameSymbol(*(srcReg), *(e->n1->vReg))){
			r = e->n2->vReg->physicalReg; pr = e->n2->vReg->preferredReg;
		}
		else if(sameSymbol(*(srcReg), *(e->n2->vReg))){
			r = e->n1->vReg->physicalReg; pr = e->n1->vReg->preferredReg;
		}
		if(pr && !r) if(!registerTaken[pr]) registerTaken[pr] = 2;
		if(r) registerTaken[r] = true;
	} if(srcReg->preferredReg) if(registerTaken[srcReg->preferredReg] != 1){srcReg->physicalReg = srcReg->preferredReg; return;}
	for(uint32_t r = 1; r <= numGPRegs; r++) if(!registerTaken[r]){srcReg->physicalReg = r; return;}
	for(uint32_t r = 1; r <= numGPRegs; r++) if(registerTaken[r] == 2){srcReg->physicalReg = r; return;}
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
	const int r = ((indexedQuad*)q2)->sId - ((indexedQuad*)q1)->sId;
	if(r >= 0) return 1; if(!r) return 0; return -1;
}

void writeQuad(arena* a, const quad q){
	writeElement(a, &q, sizeof(quad));
}

#define newStepQuad 2048
bool chaitinPass(const uint32_t fID, uint32_t* const endQ, uint32_t* const nq){
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
	for(int64_t r = (int64_t)curStackPos-1; r >= 0; r--){
		range* rt = ((range**)vRegStack.data)[r];
		findFreeRegister(rt->vReg);
		if(rt->vReg->physicalReg == 0){ anySpill = 1;
			const uint32_t spStackOff = 0; // idk how to do this yet, have to allocate stack space.
			// insert spill code & split ranges
			// at each spill, go through live range and keep separate pool where u store instructions to be added/replaced
			bool foundRewrite = 0;for(uint32_t qi = rt->i1; qi <= rt->i2; qi++){
				const quad cq = *(((quad*)quadArena->pool) + qi);
				const quadCheck qc = checkQuad(cq, *rt->vReg);
				uint32_t i1, i2; symbol newVR;
				switch(qc){
					case smblUsed:{
						newVR = newVReg(rt->vReg->vType); i1 = qi; i2 = qi + 1;
						indexedQuad refactoredQuad = (indexedQuad){.q = refactorQuad(cq, *rt->vReg, newVR), .sId = qi};
						refactoredQuad.q.refactorType = rfReplace;
						const indexedQuad newQuad = (indexedQuad){.q = (quad){.op = LOAD, .o1 = newVR, .o2 = (symbol){.type = literal, .vReg = spStackOff}}, .sId = qi};
						writeElement(&addInstrs, &newQuad, sizeof(indexedQuad)); writeElement(&addInstrs, &refactoredQuad, sizeof(indexedQuad));
						break;
					}
					case smblWrite:{
						i1 = qi; i2 = qi;
						newVR = newVReg(rt->vReg->vType);
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
	if(anySpill){
		// go through pool of ADD and REPLACE instructions. sort in order from first to last
		// iterate through og quad pool and REPLACE and ADD into new quad pool that has been allocated to hold all quads, free old one.
		// rerun buildin ranges, edges, and finally run chaitin pass again
		insertionSort(addInstrs.pool, sizeof(indexedQuad), addInstrs.used/sizeof(indexedQuad), cmpIndexedQuads);
		const uint32_t nnq = addInstrs.used/sizeof(indexedQuad);
		const uint32_t oldNQ = quadArena->used/sizeof(quad); // CHANGED: needed to find the function's new end
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
				(*nq)++; (*endQ)++;
				break;
			}
		} freeArena((*quadArena)); *quadArena = newQuads; freeArena(addInstrs);
		return 1;
	}
	return 0;
}

void materializeRegisters(const uint32_t curFnStart, const uint32_t curFnEnd){
	for(uint32_t qi = curFnStart; qi <= curFnEnd; qi++){
		quad* q = (quad*)(getQuadArena()->pool) + qi;
		if(validType(q->o1)) q->o1.physicalReg = ((range*)(ranges.data))[q->o1.vReg].vReg->physicalReg;
		if(validType(q->o2) && q->op != REF) q->o2.physicalReg = ((range*)(ranges.data))[q->o2.vReg].vReg->physicalReg;
		if(validType(q->o3)) q->o3.physicalReg = ((range*)(ranges.data))[q->o3.vReg].vReg->physicalReg;
	}
}

void allocFunction(const uint32_t startQ, uint32_t* const endQ, uint32_t* const nq, const uint32_t fID){
	uint32_t numBlocks;
	scratchState ps = saveState();
	block** postorder = constructBlocks(startQ, *endQ, fID, reconstructR, &numBlocks);
	refInsertions(postorder, endQ, nq);
	bool spilled = 0; do{
		scratchState s = saveState();
		curFnStart = startQ; curFnEnd = *endQ;
		buildRanges(startQ, *endQ);
		resetEdges(); postorder = constructBlocks(startQ, *endQ, fID, spilled ? reconstructR : readR, &numBlocks);
		spilled = cfgEdges(postorder, startQ, endQ, nq, fID, numBlocks, !spilled);
		flushInsertions(*nq);
		materializeRegisters(startQ, *endQ);
		restoreState(s);
	}while(spilled);
	flushInsertions(*nq);
	restoreState(ps);
}

// main func here
void cfgPassTest(){
	arena* quadArena = getQuadArena(); uint32_t nq = getNumQuads(); uint32_t cfID = 0;
	int64_t qs = -1, qe = -1; uint32_t qi = 0; for(quad* q = (quad*)(quadArena->pool); qi < nq; qi++, q++){
		if(q->op == FNCDEF) qs = qi;
		else if(q->op == RET) qe = qi;
		if((qs+1) && (qe+1)){
			allocFunction(qs, (uint32_t*)&qe, &nq, cfID++);
			qi = qe;
			quadArena = getQuadArena(); q = (quad*)(quadArena->pool) + qi;
			qs = -1; qe = -1;
		}
	}
}
	