#include "../../frontend/parser/3opIrGen.h"

typedef struct{
	uint32_t i1, i2;
	const symbol* vReg;
}range;

void printRanges(const sizedPool p);

sizedPool constructRanges(const arena quadArena);