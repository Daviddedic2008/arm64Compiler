#include "arenaAlloc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

arena newArena(const uint32_t sz){return (arena){.pool = malloc(sz), .allocated = sz, .used = 0};}

void* writeElement(arena* a, const void* data, const uint32_t wrSz){
	if(a->used + wrSz > a->allocated){a->pool = realloc(a->pool, a->allocated * 2);}
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