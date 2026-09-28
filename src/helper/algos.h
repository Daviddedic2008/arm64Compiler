#include <stdint.h>

void insertionSortIMPL(void* data, const uint32_t elSz, const uint32_t numEl, int8_t (*compareEl)(void*, void*));

#define insertionSort(data, elSz, numEl, compareEl) do { \
    _Static_assert(__builtin_constant_p(elSz), "Error: elSz parameter must be a compile-time constant expression!"); \
    insertionSortIMPL((data), (elSz), (numEl), (compareEl)); \
} while(0)