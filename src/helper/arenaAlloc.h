#include <stdint.h>
#include <stdlib.h>

typedef struct{
	void* pool;
	uint32_t allocated, used;
}arena;

arena newArena(const uint32_t sz);
void* writeElement(arena* a, const void* data, const uint32_t wrSz);
void* blankElement(arena* a, const uint32_t wrSz);
void printArena(arena a, int(*printFunc)(void*));
#define freeArena(a) a.allocated = 0, a.used = 0, free(a.pool)
#define emptyArena(a) a.used = 0
#define reallocateArena(a, sz) {const uint32_t s = sz; (s <= a.used) ? (a.pool = realloc(a.pool, s), a.allocated = s, 0) : 1;}