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
	if(data == NULL) return NULL;
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