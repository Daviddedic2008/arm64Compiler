#include "../../frontend/parser/3opIrGen.h"

typedef struct{
	uint32_t i1, i2;
	const symbol* vReg;
	uint8_t edgesFound;
}range;

void printRanges(const sizedPool p);

sizedPool constructRanges(const arena quadArena);

arena constructEdges(const sizedPool ranges);

void printEdges(const arena edgeArena);