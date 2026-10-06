#include "arenaAlloc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

arena newArena(const uint32_t sz){return (arena){.pool = malloc(sz), .allocated = sz, .used = 0};}

arena scratchpadArena;

void initializeScratchpad(const uint32_t initSz){
	scratchpadArena = newArena(initSz);
}

arena* getScratchpad(){
	return &scratchpadArena;
}

void* writeElement(arena* a, const void* data, const uint32_t wrSz){
	if(a->used + wrSz > a->allocated){a->pool = realloc(a->pool, a->allocated * 2);}
	if(data == NULL) return a->pool + a->used;
	void* ptr = (char*)a->pool + a->used;
	memcpy(ptr, data, wrSz);
	a->used += wrSz;
	return ptr;
}

void* blankElement(arena* a, const uint32_t wrSz){
	if(a->used + wrSz > a->allocated){a->pool = realloc(a->pool, a->allocated * 2);}
	void* ptr = (char*)a->pool + a->used;
	a->used += wrSz;
	return ptr;
}

void printArena(arena a, int(*printFunc)(void*)){
	void* tmp = a.pool;
	while(tmp != a.pool + a.used){
		tmp += printFunc(tmp);
	}
}

void addEl(sizedPool* p, const void* data, const uint32_t wrSz){
	p->data = realloc(p->data, p->size + wrSz);
	memcpy(p->data + p->size, data, wrSz);
}

memC* scratchpadChunks;

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static inline void* raw_chunk_alloc_zero(size_t bytes) {
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
}

static inline void* raw_chunk_alloc(size_t bytes) {
	return HeapAlloc(GetProcessHeap(), 0, bytes);
}

static inline void* raw_chunk_realloc_in_place(void* ptr, size_t bytes) {
	return HeapReAlloc(GetProcessHeap(), HEAP_REALLOC_IN_PLACE_ONLY, ptr, bytes);
}

static inline void* raw_chunk_realloc(void* ptr, size_t bytes) {
	return HeapReAlloc(GetProcessHeap(), 0, ptr, bytes);
}

static inline void raw_chunk_free(void* ptr) {
	HeapFree(GetProcessHeap(), 0, ptr);
}
#else
static inline void* raw_chunk_alloc(size_t bytes) {
	return calloc(1, bytes);
}

static inline void* raw_chunk_realloc_in_place(void* ptr, size_t bytes) {
	(void)ptr; (void)bytes;
	return NULL; 
}

static inline void raw_chunk_free(void* ptr) {
	free(ptr);
}
#endif

memC* getLastChunk(scratchpad s){
	const memC* curC = s.startC;
	while(curC){
		curC = curC->nextC;
	}
}

scratchpad globalScratch;

#define minChunkSz 1024

memC* newChunk(const uint32_t initSz){
	const uint32_t is = initSz > minChunkSz ? initSz : minChunkSz;
	const memC* m = raw_chunk_alloc(sizeof(memC) + is);
	m->allocated = is; m->used = 0;
	m->nextC = NULL; m->prevC = NULL;
	m->offsetS = globalScratch.allocatedMem; m->offsetE = m->offsetS + initSz;
	globalScratch.allocatedMem += initSz;
	if(globalScratch.numC){
		m->prevC = globalScratch->endC;
		globalScratch->endC->nextC = m;
	} else{
		globalScratch->startC = m;
	} globalScratch->endC = m;
	globalScratch.numC++;
	return m;
}

void initializeScratchpad(const uint32_t initSz){
	const uint32_t is = initSz > minChunkSz ? initSz : minChunkSz;
	globalScratch = (scratchpad(){0};
	newChunk(initSz);
}

void freeScratchpad(){
	const void* mcp = globalScratch.startC;
	while(mcp){
		const void* mcpN = mcp->nextC;
		raw_chunk_free(mcp);
		mcp = mcpN;
	}
}

uint8_t* allocateOnScratchpad(const uint32_t allocSz){
	memC* lc = globalScratch.endC;
	uint8_t* ret = NULL;
	if((lc->used + allocSz) < lc->allocated){
		ret = lc->data; lc->used += allocSz; 
	} else{
		ret = raw_chunk_realloc_in_place(lc, sizeof(memC) + lc->used + allocSz);
		lc->offsetE += allocSz;
		if(!ret){
			const memC* nc = newChunk(allocSz);
			ret = nc->data;
		}
	} return ret;
}

uint8_t* writeToScratchpad(const uint8_t* data, const uint32_t wrSz){
	const uint8_t* allM = allocateOnScratchpad(wrSz);
	switch(wrSz){
		case 1: *allM = *data; break;
		case 2: *((uint16_t*)allM) = *((uint16_t*)data); break;
		case 4: *((uint32_t*)allM) = *((uint32_t*)data); break;
		case 8: *((uint64_t*)allM) = *((uint64_t*)data); break;
		default: memcpy(allM, data, wrSz);
	}
	return allM;
}

uint8_t* getScratchEl(const uint32_t offset){
	const memC* mcp = globalScratch.startC;
	while(mcp){
		if(mcp->offsetS <= offset && mcp->offsetE < offset) return mcp->data + (offset - mcp->offsetS);
		mcp = mcp->nextC;
	} return NULL;
}

uint8_t* resizeChunk(memC* c, const uint32_t allocSzRaw){
	const uint32_t allocSz = allocSzRaw < minChunkSz ? minChunkSz : allocSzRaw;
	if((c->used + allocSz) < c->allocated){c->used += allocSz; return c->data;}
	memC* newM = raw_chunk_realloc(c, allocSz);
	if(!newM){
		newM = raw_chunk_alloc(allocSz + sizeof(memC));
		*newM = *c;
		raw_chunk_free(c);
		c = newM;
	} return c->data;
}