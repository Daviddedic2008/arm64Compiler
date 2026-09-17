#include "../../frontend/parser/3opIrGen.h"

typedef struct{
	uint32_t i1, i2;
	const symbol* vReg;
	uint8_t edgesFound, degree;
	uint16_t physicalReg; uint8_t offGraph; uint16_t numEdges;
}range;

void printRanges();

void constructRanges(const arena quadArena);

void constructEdges();

void printEdges();

#define numGPRegs 30